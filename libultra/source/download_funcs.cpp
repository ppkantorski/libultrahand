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

size_t DOWNLOAD_BUFFER_SIZE = 4096*4;
size_t UNZIP_BUFFER_SIZE = 131072*4;//4096*4;

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

const std::string userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36";

// Definition of CurlDeleter
void CurlDeleter::operator()(CURL* curl) const {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

//// Definition of MzZipReaderDeleter
//void MzZipReaderDeleter::operator()(void* reader) const {
//    if (reader) {
//        mz_zip_reader_delete(&reader);
//    }
//}

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

// Your C function
extern "C" int progressCallback(void *ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded) {
    if (!ptr) return 1;
    
    auto percentage = static_cast<std::atomic<int>*>(ptr);

    if (totalToDownload > 0) {
        int newProgress = static_cast<int>((static_cast<double>(nowDownloaded) / static_cast<double>(totalToDownload)) * 100.0);
        percentage->store(newProgress, std::memory_order_release);
    }

    if (abortDownload.load(std::memory_order_acquire)) {
        percentage->store(-1, std::memory_order_release);
        return 1;  // Abort the download
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

    downloadPercentage.store(0, std::memory_order_release);

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
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS); // Enable HTTP/2
    curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2); // Force TLS 1.2

    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_BUFFERSIZE, DOWNLOAD_BUFFER_SIZE); // Increase buffer size

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
        downloadPercentage.store(-1, std::memory_order_release);
        return false;
    }

#ifndef NO_FSTREAM_DIRECTIVE
    std::ifstream checkFile(tempFilePath);
    if (!checkFile || checkFile.peek() == std::ifstream::traits_type::eof()) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error downloading file: Empty file");
        #endif
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(-1, std::memory_order_release);
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
        downloadPercentage.store(-1, std::memory_order_release);
        return false;
    }
#endif

    downloadPercentage.store(100, std::memory_order_release);
    moveFile(tempFilePath, destination);
    return true;
}



/**
 * @brief Ultra-optimized ZIP extraction with micro-optimizations
 * 
 * Optimizations:
 * - Single progress tracking point (no skipped percentages)
 * - Branchless progress calculation
 * - Optimized string operations
 * - Reduced function call overhead
 * - Better memory access patterns
 */
bool unzipFile(const std::string& zipFilePath, const std::string& toDestination) {
    abortUnzip.store(false, std::memory_order_release);
    unzipPercentage.store(0, std::memory_order_release);

    createDirectory(toDestination);
    
    std::string destination;
    destination.reserve(toDestination.size() + 1);
    destination = toDestination;
    if (!destination.empty() && destination.back() != '/') {
        destination += '/';
    }

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    
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
        return false;
    }

    // Optimized file info with better memory layout
    struct FileInfo {
        mz_uint index;
        uint64_t size;
        std::string name;
        std::string extract_path;
        
        // Move constructor for better performance
        FileInfo(mz_uint idx, uint64_t sz, std::string&& n, std::string&& path) 
            : index(idx), size(sz), name(std::move(n)), extract_path(std::move(path)) {}
    };
    
    std::vector<FileInfo> files;
    files.reserve(num_files);
    
    uint64_t totalUncompressedSize = 0;
    
    // Pre-allocate strings with likely max sizes to avoid reallocations
    std::string fileName, extractedFilePath;
    fileName.reserve(256);  // Most filenames are under 256 chars
    extractedFilePath.reserve(512); // Most paths under 512
    
    // Optimized character replacement lookup table
    static constexpr auto invalid_chars = []{
        std::array<bool, 256> tbl{};           // zero-initialised
        tbl[':'] = tbl['*'] = tbl['?'] = tbl['"'] =
        tbl['<'] = tbl['>'] = tbl['|'] = true;
        return tbl;
    }();
    
    // Single-pass file collection with optimized string handling
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            continue;
        }

        const char* filename = file_stat.m_filename;
        const mz_uint64 uncompressed_size = file_stat.m_uncomp_size;
        
        if (!filename || filename[0] == '\0' || uncompressed_size == 0) continue;
        
        const size_t nameLen = strlen(filename);
        if (nameLen > 0 && filename[nameLen - 1] == '/') continue;
        
        // Optimized string assignment avoiding extra allocations
        fileName.assign(filename, nameLen);
        
        // Pre-calculate total path size to avoid string reallocations
        extractedFilePath.clear();
        extractedFilePath.reserve(destination.size() + nameLen);
        extractedFilePath.append(destination);
        extractedFilePath.append(fileName);
        
        // Optimized character cleaning using lookup table
        for (char& c : extractedFilePath) {
            if (static_cast<unsigned char>(c) < 256 && invalid_chars[static_cast<unsigned char>(c)]) {
                c = '_';
            }
        }
        
        files.emplace_back(i, uncompressed_size, std::move(fileName), std::move(extractedFilePath));
        totalUncompressedSize += uncompressed_size;
    }

    if (files.empty()) {
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    #if USING_LOGGING_DIRECTIVE
    logMessage("Extracting " + std::to_string(files.size()) + " files, " + 
               std::to_string(totalUncompressedSize) + " bytes total");
    #endif

    // Ultra-optimized extraction context
    struct UltraOptimizedContext {
        uint64_t totalProcessed;
        const uint64_t totalSize;
        std::atomic<int>* const progressAtomic;
        std::atomic<bool>* const abortFlag;
        
        #if NO_FSTREAM_DIRECTIVE
        FILE* outputFile;
        #else
        std::ofstream* outputFile;
        #endif
        
        bool writeError;
        uint32_t callCounter;
        
        // Pre-calculated percentage thresholds for ultra-fast lookup
        std::array<uint64_t, 101> percentageThresholds;
        int currentPercentage;
        
        UltraOptimizedContext(uint64_t total, std::atomic<int>* prog, std::atomic<bool>* abort) 
            : totalProcessed(0), totalSize(total), progressAtomic(prog), abortFlag(abort), 
              writeError(false), callCounter(0), currentPercentage(0) 
        {
            // Pre-calculate all percentage thresholds (0-100%)
            for (int i = 0; i <= 100; ++i) {
                percentageThresholds[i] = (static_cast<uint64_t>(i) * total) / 100;
            }
        }
    };


    // Micro-optimized callback with minimal branching
    auto ultraCallback = [](void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n) -> size_t {
        UltraOptimizedContext* ctx = static_cast<UltraOptimizedContext*>(pOpaque);
        
        // Ultra-fast abort check
        if (__builtin_expect((++ctx->callCounter & 0x3FF) == 0, false)) {
            if (__builtin_expect(ctx->abortFlag->load(std::memory_order_relaxed), false)) {
                return 0;
            }
        }
        
        // Fast write
        #if NO_FSTREAM_DIRECTIVE
        const size_t written = fwrite(pBuf, 1, n, ctx->outputFile);
        if (__builtin_expect(written != n, false)) {
            ctx->writeError = true;
            return 0;
        }
        #else
        ctx->outputFile->write(static_cast<const char*>(pBuf), n);
        if (__builtin_expect(!ctx->outputFile->good(), false)) {
            ctx->writeError = true;
            return 0;
        }
        #endif
        
        const uint64_t oldProcessed = ctx->totalProcessed;
        ctx->totalProcessed += n;
        
        // Ultra-fast percentage calculation using bit shifts (when total is power of 2)
        // Or use pre-calculated lookup for common sizes
        const int oldPct = static_cast<int>((oldProcessed * 100) / ctx->totalSize);
        const int newPct = static_cast<int>((ctx->totalProcessed * 100) / ctx->totalSize);
        
        // Emit all percentages between old and new (guarantees no skips)
        if (__builtin_expect(newPct > oldPct, false)) {
            const int clampedNewPct = newPct > 100 ? 100 : newPct;
            for (int pct = oldPct + 1; pct <= clampedNewPct; ++pct) {
                ctx->progressAtomic->store(pct, std::memory_order_relaxed);
            }
        }
        
        return n;
    };
    
    bool success = true;
    UltraOptimizedContext ctx(totalUncompressedSize, &unzipPercentage, &abortUnzip);
    
    // Pre-allocate directory string with generous size
    std::string directoryPath;
    directoryPath.reserve(512);
    
    // Optimized extraction loop with minimal overhead
    for (size_t fileIdx = 0; fileIdx < files.size(); ++fileIdx) {
        const FileInfo& file = files[fileIdx];  // const reference to avoid copying
        
        // Quick abort check
        if (__builtin_expect(abortUnzip.load(std::memory_order_relaxed), false)) {
            success = false;
            break;
        }

        // Optimized directory creation - find last slash using reverse search
        const size_t lastSlash = file.extract_path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            // Use assign instead of substr for better performance
            directoryPath.assign(file.extract_path, 0, lastSlash + 1);
            createDirectory(directoryPath);
        }

        // Optimized file opening
        #if NO_FSTREAM_DIRECTIVE
        FILE* outputFile = fopen(file.extract_path.c_str(), "wb");
        if (__builtin_expect(!outputFile, false)) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + file.extract_path);
            #endif
            continue;
        }
        setvbuf(outputFile, nullptr, _IOFBF, UNZIP_BUFFER_SIZE);
        ctx.outputFile = outputFile;
        #else
        std::ofstream outputFile(file.extract_path, std::ios::binary);
        if (__builtin_expect(!outputFile.is_open(), false)) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + file.extract_path);
            #endif
            continue;
        }
        outputFile.rdbuf()->pubsetbuf(nullptr, UNZIP_BUFFER_SIZE);
        ctx.outputFile = &outputFile;
        #endif

        // Reset context for new file
        ctx.writeError = false;
        ctx.callCounter = 0;

        // Extract with optimized callback
        const mz_bool extract_result = mz_zip_reader_extract_file_to_callback(
            &zip_archive, file.name.c_str(), ultraCallback, &ctx, 0);

        // Close file immediately
        #if NO_FSTREAM_DIRECTIVE
        fclose(outputFile);
        #else
        outputFile.close();
        #endif

        // Handle extraction result
        if (__builtin_expect(!extract_result || ctx.writeError, false)) {
            deleteFileOrDirectory(file.extract_path);
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to extract: " + file.name);
            #endif
            
            if (abortUnzip.load(std::memory_order_relaxed)) {
                success = false;
                break;
            }
            continue;
        }

        // Yield occasionally for very large archives only
        if (__builtin_expect((fileIdx & 0x3F) == 0 && files.size() > 500, false)) {
            #ifdef __SWITCH__
            svcSleepThread(100000); // 0.1ms
            #else
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            #endif
        }
    }

    mz_zip_reader_end(&zip_archive);

    // Final progress update
    if (success && ctx.totalProcessed > 0) {
        unzipPercentage.store(100, std::memory_order_release);
        
        #if USING_LOGGING_DIRECTIVE
        logMessage("Extraction completed: " + std::to_string(files.size()) + " files, " + 
                  std::to_string(ctx.totalProcessed) + " bytes");
        #endif
        
        return true;
    } else {
        unzipPercentage.store(-1, std::memory_order_release);
        return false;
    }
}
}
