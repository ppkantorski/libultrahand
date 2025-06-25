/********************************************************************************
 * File: download_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file provides implementations for the functions declared in
 *   download_funcs.hpp. These functions utilize libcurl for downloading files
 *   from the internet and minizip-ng for extracting ZIP archives with proper
 *   64-bit file support.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 * 
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2024 ppkantorski
 ********************************************************************************/

#include "download_funcs.hpp"
#include <chrono>
#include <thread>

namespace ult {

size_t DOWNLOAD_BUFFER_SIZE = 65536;//4096*10;
size_t UNZIP_BUFFER_SIZE = 131072*2;//4096*4;

// Path to the CA certificate
const std::string cacertPath = "sdmc:/config/ultrahand/cacert.pem";
const std::string cacertURL = "https://curl.se/ca/cacert.pem";

// Shared atomic flag to indicate whether to abort the download operation
std::atomic<bool> abortDownload(false);
// Define an atomic bool for interpreter completion
std::atomic<bool> abortUnzip(false);
std::atomic<int> downloadPercentage(-1);
std::atomic<int> unzipPercentage(-1);

// Thread-safe curl initialization
static std::mutex curlInitMutex;
static std::atomic<bool> curlInitialized(false);

// Plain C-string (null-terminated at compile time)
constexpr char userAgent[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/91.0.4472.124 Safari/537.36";

// Definition of CurlDeleter
void CurlDeleter::operator()(CURL* curl) const {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

// Callback function to write received data to a file.
#if NO_FSTREAM_DIRECTIVE
// Using stdio.h functions (FILE*, fwrite)
size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || !stream) return 0;
    size_t totalBytes = size * nmemb;
    size_t writtenBytes = fwrite(ptr, 1, totalBytes, stream);
    return writtenBytes;
}
#else
// Using std::ofstream for writing
size_t writeCallback(void* ptr, size_t size, size_t nmemb, std::ostream* stream) {
    if (!ptr || !stream) return 0;
    auto& file = *static_cast<std::ofstream*>(stream);
    size_t totalBytes = size * nmemb;
    file.write(static_cast<const char*>(ptr), totalBytes);
    return totalBytes;
}
#endif

// Optimized progress callback - ensures no integer percentages are missed
extern "C" int progressCallback(void *ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded) {
    // Check for abort first - most critical check
    if (abortDownload.load(std::memory_order_acquire)) {
        if (ptr) static_cast<std::atomic<int>*>(ptr)->store(-1, std::memory_order_relaxed);
        return 1;  // Abort the download
    }
    
    if (!ptr || totalToDownload <= 0) return 0;
    
    auto percentage = static_cast<std::atomic<int>*>(ptr);
    int newProgress = static_cast<int>((static_cast<double>(nowDownloaded) / static_cast<double>(totalToDownload)) * 100.0);
    int currentProgress = percentage->load(std::memory_order_relaxed);
    
    // Fill in any missing integer percentages to ensure none are skipped
    if (newProgress > currentProgress) {
        // Step through each integer percentage to ensure none are missed
        for (int i = currentProgress + 1; i <= newProgress; ++i) {
            percentage->store(i, std::memory_order_relaxed);
        }
    }
    
    return 0;  // Continue the download
}

// Global initialization function
void initializeCurl() {
    std::lock_guard<std::mutex> lock(curlInitMutex);
    if (!curlInitialized.load(std::memory_order_acquire)) {
        CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (res != CURLE_OK) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("curl_global_init() failed: " + std::string(curl_easy_strerror(res)));
            #endif
            // Handle error appropriately, possibly exit the program
        } else {
            curlInitialized.store(true, std::memory_order_release);
        }
    }
}

// Global cleanup function
void cleanupCurl() {
    std::lock_guard<std::mutex> lock(curlInitMutex);
    if (curlInitialized.load(std::memory_order_acquire)) {
        curl_global_cleanup();
        curlInitialized.store(false, std::memory_order_release);
    }
}

/**
 * @brief Downloads a file from a URL to a specified destination.
 *
 * @param url The URL of the file to download.
 * @param toDestination The destination path where the file should be saved.
 * @return True if the download was successful, false otherwise.
 */
bool downloadFile(const std::string& url, const std::string& toDestination) {
    abortDownload.store(false, std::memory_order_release);

    if (url.find_first_of("{}") != std::string::npos) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Invalid URL: " + url);
        #endif
        return false;
    }

    std::string destination = toDestination;
    if (destination.back() == '/') {
        createDirectory(destination);
        size_t lastSlash = url.find_last_of('/');
        if (lastSlash != std::string::npos) {
            destination += url.substr(lastSlash + 1);
        } else {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Invalid URL: " + url);
            #endif
            return false;
        }
    } else {
        createDirectory(destination.substr(0, destination.find_last_of('/')));
    }

    std::string tempFilePath = getParentDirFromPath(destination) + "." + getFileName(destination) + ".tmp";

#ifndef NO_FSTREAM_DIRECTIVE
    // Use ofstream if NO_FSTREAM_DIRECTIVE is not defined
    std::ofstream file(tempFilePath, std::ios::binary);
    if (!file.is_open()) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error opening file: " + tempFilePath);
        #endif
        return false;
    }
#else
    // Alternative method of opening file (depending on your platform, like using POSIX open())
    FILE* file = fopen(tempFilePath.c_str(), "wb");
    if (!file) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error opening file: " + tempFilePath);
        #endif
        return false;
    }
#endif

    // Ensure curl is initialized
    initializeCurl();

    std::unique_ptr<CURL, CurlDeleter> curl(curl_easy_init());
    if (!curl) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error initializing curl.");
        #endif
#ifndef NO_FSTREAM_DIRECTIVE
        file.close();
#else
        fclose(file);
#endif
        return false;
    }

    downloadPercentage.store(0, std::memory_order_relaxed);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeCallback);
#ifndef NO_FSTREAM_DIRECTIVE
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &file);
#else
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file);
#endif
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &downloadPercentage);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, userAgent);
    curl_easy_setopt(curl.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS); // Enable HTTP/2
    curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2); // Force TLS 1.2

    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_BUFFERSIZE, DOWNLOAD_BUFFER_SIZE); // Increase buffer size

    // Performance optimizations
    curl_easy_setopt(curl.get(), CURLOPT_TCP_NODELAY, 1L);        // Disable Nagle's algorithm for faster small transfers
    curl_easy_setopt(curl.get(), CURLOPT_TCP_FASTOPEN, 1L);       // Enable TCP Fast Open if supported
    curl_easy_setopt(curl.get(), CURLOPT_PIPEWAIT, 1L);          // Enable HTTP/2 multiplexing
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);         // Limit redirects to prevent infinite loops
    curl_easy_setopt(curl.get(), CURLOPT_DNS_CACHE_TIMEOUT, 300L); // Cache DNS for 5 minutes

    // Add timeout options
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);   // 10 seconds to connect
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1L);   // 1 byte/s (virtually any progress)
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 60L);  // 1 minutes of no progress

    CURLcode result = curl_easy_perform(curl.get());

#ifndef NO_FSTREAM_DIRECTIVE
    file.close();
#else
    fclose(file);
#endif

    if (result != CURLE_OK) {
        #if USING_LOGGING_DIRECTIVE
        if (result == CURLE_OPERATION_TIMEDOUT) {
            logMessage("Download timed out: " + url);
        } else if (result == CURLE_COULDNT_CONNECT) {
            logMessage("Could not connect to: " + url);
        } else {
            logMessage("Error downloading file: " + std::string(curl_easy_strerror(result)));
        }
        #endif
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }

#ifndef NO_FSTREAM_DIRECTIVE
    std::ifstream checkFile(tempFilePath);
    if (!checkFile || checkFile.peek() == std::ifstream::traits_type::eof()) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error downloading file: Empty file");
        #endif
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(-1, std::memory_order_relaxed);
        checkFile.close();
        return false;
    }
    checkFile.close();
#else
    // Alternative method for checking if the file is empty (POSIX example)
    struct stat fileStat;
    if (stat(tempFilePath.c_str(), &fileStat) != 0 || fileStat.st_size == 0) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error downloading file: Empty file");
        #endif
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }
#endif

    downloadPercentage.store(100, std::memory_order_relaxed);
    moveFile(tempFilePath, destination);
    return true;
}



/**
 * @brief Extracts files from a ZIP archive to a specified destination.
 *
 * Ultra-optimized single-pass extraction with smooth progress reporting using miniz
 * with proper 64-bit file support and streaming extraction
 * 
 * @param zipFilePath The path to the ZIP archive file.
 * @param toDestination The destination directory where files should be extracted.
 * @return True if the extraction was successful, false otherwise.
 */
bool unzipFile(const std::string& zipFilePath, const std::string& toDestination) {
    abortUnzip.store(false, std::memory_order_release);
    unzipPercentage.store(0, std::memory_order_release);

    // Ensure destination directory exists
    createDirectory(toDestination);
    
    // Ensure destination ends with '/' - pre-allocate final string
    std::string destination;
    destination.reserve(toDestination.size() + 1);
    destination = toDestination;
    if (!destination.empty() && destination.back() != '/') {
        destination += '/';
    }

    // Single ZIP archive handle - no reopening
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    
    // Open with ZIP64 support and disable sorting for faster init
    if (!mz_zip_reader_init_file(&zip_archive, zipFilePath.c_str(), 
                                MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY)) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Failed to open zip file: " + zipFilePath);
        #endif
        return false;
    }

    const mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    if (num_files == 0) {
        mz_zip_reader_end(&zip_archive);
        #if USING_LOGGING_DIRECTIVE
        logMessage("No files found in archive");
        #endif
        return false;
    }

    // Pre-allocate all containers to avoid reallocations
    std::vector<mz_uint> file_indices;
    std::vector<uint64_t> file_sizes;
    std::vector<std::string> file_names;  // Cache filenames to avoid repeated stat calls
    std::vector<std::string> extract_paths; // Pre-compute all extraction paths
    
    file_indices.reserve(num_files);
    file_sizes.reserve(num_files);
    file_names.reserve(num_files);
    extract_paths.reserve(num_files);
    
    uint64_t totalUncompressedSize = 0;
    
    // Pre-allocate strings for path operations
    std::string fileName, extractedFilePath, directoryPath;
    fileName.reserve(512);
    extractedFilePath.reserve(1024);
    directoryPath.reserve(1024);
    
    // Single pass: collect file info and pre-compute paths
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            continue;
        }

        const char* filename = file_stat.m_filename;
        const mz_uint64 uncompressed_size = file_stat.m_uncomp_size;
        
        // Quick filename validation
        if (!filename || filename[0] == '\0' || uncompressed_size == 0) continue;
        
        const size_t nameLen = strlen(filename);
        if (nameLen > 0 && filename[nameLen - 1] == '/') continue; // Skip directories
        
        // Pre-compute extraction path
        fileName.assign(filename, nameLen);
        extractedFilePath.clear();
        extractedFilePath.reserve(destination.size() + nameLen + 1);
        extractedFilePath = destination;
        extractedFilePath += fileName;
        
        // Optimized character cleaning - only if needed
        const size_t invalid_pos = extractedFilePath.find_first_of(":*?\"<>|");
        if (invalid_pos != std::string::npos) {
            const size_t start_pos = std::min(extractedFilePath.find(ROOT_PATH) + 5, extractedFilePath.size());
            auto it = extractedFilePath.begin() + start_pos;
            extractedFilePath.erase(std::remove_if(it, extractedFilePath.end(), [](char c) {
                return c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|';
            }), extractedFilePath.end());
        }
        
        file_indices.push_back(i);
        file_sizes.push_back(uncompressed_size);
        file_names.emplace_back(std::move(fileName));
        extract_paths.emplace_back(std::move(extractedFilePath));
        totalUncompressedSize += uncompressed_size;
    }

    if (file_indices.empty()) {
        mz_zip_reader_end(&zip_archive);
        #if USING_LOGGING_DIRECTIVE
        logMessage("No extractable files found");
        #endif
        return false;
    }

    #if USING_LOGGING_DIRECTIVE
    logMessage("Extracting " + std::to_string(file_indices.size()) + " files, " + 
               std::to_string(totalUncompressedSize) + " bytes total");
    #endif

    // Ultra-optimized extraction context with smoother progress tracking
    struct UltraOptimizedExtractionContext {
        uint64_t completedSize;
        const uint64_t totalSize;
        uint64_t currentFileProcessed;
        std::atomic<int>* const progressAtomic;
        std::atomic<bool>* const abortFlag;
        
        #if NO_FSTREAM_DIRECTIVE
        FILE* outputFile;
        #else
        std::ofstream* outputFile;
        #endif
        
        bool writeError;
        uint32_t updateCounter; // Reduce atomic operations
        int lastReportedProgress; // Track last reported progress to avoid duplicate updates
        
        UltraOptimizedExtractionContext(uint64_t total, std::atomic<int>* prog, std::atomic<bool>* abort) 
            : completedSize(0), totalSize(total), currentFileProcessed(0), 
              progressAtomic(prog), abortFlag(abort), writeError(false), updateCounter(0), lastReportedProgress(-1) {}
    };

    // Maximum performance callback with minimal atomic overhead
    auto maxPerformanceCallback = [](void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n) -> size_t {
        UltraOptimizedExtractionContext* ctx = static_cast<UltraOptimizedExtractionContext*>(pOpaque);
        
        // Write data first - most common path, optimize for it
        #if NO_FSTREAM_DIRECTIVE
        if (fwrite(pBuf, 1, n, ctx->outputFile) != n) [[unlikely]] {
            ctx->writeError = true;
            return 0;
        }
        #else
        ctx->outputFile->write(static_cast<const char*>(pBuf), n);
        if (!ctx->outputFile->good()) [[unlikely]] {
            ctx->writeError = true;
            return 0;
        }
        #endif
        
        ctx->currentFileProcessed += n;
        const uint32_t counter = ++ctx->updateCounter;
        
        // Check abort much less frequently - every 2048 calls (was 256)
        if ((counter & 0x7FF) == 0) [[unlikely]] { // Check every 2048 calls
            if (ctx->abortFlag->load(std::memory_order_relaxed)) [[unlikely]] {
                return 0;
            }
            
            // Combine abort check with progress update for efficiency
            const uint64_t totalProcessed = ctx->completedSize + ctx->currentFileProcessed;
            const int progress = static_cast<int>((totalProcessed * 100ULL) / ctx->totalSize);
            const int clampedProgress = std::min(progress, 99);
            
            // Only update if progress actually changed
            if (clampedProgress != ctx->lastReportedProgress) [[unlikely]] {
                ctx->progressAtomic->store(clampedProgress, std::memory_order_relaxed);
                ctx->lastReportedProgress = clampedProgress;
            }
        }
        
        return n;
    };

    bool success = true;
    UltraOptimizedExtractionContext ctx(totalUncompressedSize, &unzipPercentage, &abortUnzip);
    
    // Pre-declare variables outside loop for maximum performance
    uint64_t fileSize;
    const char* cachedFilename;
    const std::string* extractedFilePathPtr;
    std::string* directoryPathPtr = &directoryPath;
    size_t lastSlashPos;
    
    #if NO_FSTREAM_DIRECTIVE
    FILE* outputFile;
    char* buffer = nullptr;
    // Allocate a single large buffer for all file operations
    const size_t bufferSize = std::max(UNZIP_BUFFER_SIZE, static_cast<size_t>(256 * 1024)); // At least 256KB
    buffer = static_cast<char*>(malloc(bufferSize));
    #else
    std::ofstream outputFile;
    #endif
    
    mz_bool extract_result;
    
    // Process files with minimal overhead
    for (size_t fileIdx = 0; fileIdx < file_indices.size() && success; ++fileIdx) {
        fileSize = file_sizes[fileIdx];
        cachedFilename = file_names[fileIdx].c_str();
        extractedFilePathPtr = &extract_paths[fileIdx];
        
        // Quick abort check with relaxed ordering
        if (abortUnzip.load(std::memory_order_relaxed)) {
            success = false;
            break;
        }

        // Create directory only if needed - use pre-computed path
        lastSlashPos = extractedFilePathPtr->find_last_of('/');
        if (lastSlashPos != std::string::npos) {
            directoryPathPtr->assign(*extractedFilePathPtr, 0, lastSlashPos + 1);
            createDirectory(*directoryPathPtr);
        }

        // Open file with maximum performance settings
        #if NO_FSTREAM_DIRECTIVE
        outputFile = fopen(extractedFilePathPtr->c_str(), "wb");
        if (!outputFile) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + *extractedFilePathPtr);
            #endif
            continue;
        }
        // Use our pre-allocated buffer for consistent performance
        if (buffer) {
            setvbuf(outputFile, buffer, _IOFBF, bufferSize);
        }
        ctx.outputFile = outputFile;
        #else
        outputFile.open(*extractedFilePathPtr, std::ios::binary);
        if (!outputFile.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + *extractedFilePathPtr);
            #endif
            continue;
        }
        // Set larger buffer
        outputFile.rdbuf()->pubsetbuf(nullptr, UNZIP_BUFFER_SIZE);
        ctx.outputFile = &outputFile;
        #endif

        // Reset context for this file
        ctx.currentFileProcessed = 0;
        ctx.writeError = false;

        // Extract with streaming
        extract_result = mz_zip_reader_extract_file_to_callback(&zip_archive, cachedFilename, maxPerformanceCallback, &ctx, 0);

        // Close file immediately
        #if NO_FSTREAM_DIRECTIVE
        fclose(outputFile);
        #else
        outputFile.close();
        #endif

        if (!extract_result || ctx.writeError) {
            deleteFileOrDirectory(*extractedFilePathPtr);
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to extract: " + file_names[fileIdx]);
            #endif
            
            if (abortUnzip.load(std::memory_order_relaxed)) {
                success = false;
                break;
            }
            continue;
        }

        // Update completed size
        ctx.completedSize += fileSize;

        // Update progress for completed file - ensure smooth progression
        const int progress = static_cast<int>((ctx.completedSize * 100ULL) / totalUncompressedSize);
        const int clampedProgress = std::min(progress, 99);
        
        // Only update if progress changed to reduce atomic operations
        if (clampedProgress != ctx.lastReportedProgress) {
            unzipPercentage.store(clampedProgress, std::memory_order_relaxed);
            ctx.lastReportedProgress = clampedProgress;
        }

        // Minimize thread yields even further for maximum throughput
        if ((fileIdx & 0x7F) == 0 && file_indices.size() > 1000) { // Every 128 files, only for 1000+ files
            #ifdef __SWITCH__
            svcSleepThread(100000); // 0.1ms - minimal yield
            #else
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            #endif
        }
    }

    #if NO_FSTREAM_DIRECTIVE
    if (buffer) {
        free(buffer);
    }
    #endif

    mz_zip_reader_end(&zip_archive);

    if (success && ctx.completedSize > 0) {
        unzipPercentage.store(100, std::memory_order_release);
        
        #if USING_LOGGING_DIRECTIVE
        logMessage("Extraction completed: " + std::to_string(file_indices.size()) + " files, " + 
                  std::to_string(ctx.completedSize) + " bytes");
        #endif
        
        return true;
    } else {
        unzipPercentage.store(-1, std::memory_order_release);
        return false;
    }
}
}
