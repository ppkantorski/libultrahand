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


namespace ult {

size_t DOWNLOAD_READ_BUFFER = 64 * 1024;//4096*10;
size_t DOWNLOAD_WRITE_BUFFER = 64 * 1024;
size_t UNZIP_READ_BUFFER = 64 * 1024;//131072*2;//4096*4;
size_t UNZIP_WRITE_BUFFER = 64 * 1024;//131072*2;//4096*4;

// Path to the CA certificate
constexpr char cacertPath[] = "sdmc:/config/ultrahand/cacert.pem";
//const std::string cacertURL = "https://curl.se/ca/cacert.pem";

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

// Progress tracking structure for optimized callbacks
struct ProgressData {
    std::atomic<int>* percentage;
    int lastReportedProgress;
    u64 lastAbortCheck;
    curl_off_t bytesPerPercent;
    curl_off_t nextPercentBoundary;
};

// Optimized write callback - minimal overhead
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    // Quick validation - single branch
    if (!ptr || !userdata) return 0;
    
    FILE* stream = static_cast<FILE*>(userdata);
    
    // Direct write - let fwrite handle the multiplication
    return fwrite(ptr, size, nmemb, stream);
}

// Optimized progress callback - minimal branching and calculations
int progressCallback(void *ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, 
                               curl_off_t totalToUpload, curl_off_t nowUploaded) {
    // Early exit for invalid state
    if (!ptr || totalToDownload <= 0) return 0;
    
    auto* progressData = static_cast<ProgressData*>(ptr);
    
    // Time-based abort check - only check every 2 seconds
    const u64 currentTick = armGetSystemTick();
    
    // Single comparison for time check
    if ((currentTick - progressData->lastAbortCheck) >= 2000000000ULL) { // Pre-calculated nanoseconds
        if (abortDownload.load(std::memory_order_relaxed)) {
            progressData->percentage->store(-1, std::memory_order_relaxed);
            return 1;  // Abort
        }
        progressData->lastAbortCheck = currentTick;
    }
    
    // Smart percentage calculation - only when crossing boundaries
    if (nowDownloaded >= progressData->nextPercentBoundary) {
        // Calculate new percentage
        int newProgress = static_cast<int>((nowDownloaded * 100LL) / totalToDownload);
        newProgress = (newProgress > 99) ? 99 : newProgress; // Branchless min
        
        // Update if changed
        if (newProgress > progressData->lastReportedProgress) {
            // Fill any gaps to ensure smooth progress
            while (progressData->lastReportedProgress < newProgress) {
                progressData->lastReportedProgress++;
                progressData->percentage->store(progressData->lastReportedProgress, std::memory_order_relaxed);
            }
            
            // Calculate next boundary
            progressData->nextPercentBoundary = ((progressData->lastReportedProgress + 1) * totalToDownload) / 100;
            
            #if USING_LOGGING_DIRECTIVE
            // Log at 10% intervals only
            if ((progressData->lastReportedProgress % 10) == 0) {
                logMessage("Download: " + std::to_string(progressData->lastReportedProgress) + "%");
            }
            #endif
        }
    }
    
    return 0;  // Continue
}

// Global initialization function
void initializeCurl() {
    std::lock_guard<std::mutex> lock(curlInitMutex);
    if (!curlInitialized.exchange(true, std::memory_order_acq_rel)) {
        CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (res != CURLE_OK) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("curl_global_init() failed: " + std::string(curl_easy_strerror(res)));
            #endif
            curlInitialized.store(false, std::memory_order_release);
        }
    }
}

// Global cleanup function
void cleanupCurl() {
    std::lock_guard<std::mutex> lock(curlInitMutex);
    if (curlInitialized.exchange(false, std::memory_order_acq_rel)) {
        curl_global_cleanup();
    }
}

/**
 * @brief Downloads a file from a URL to a specified destination.
 *
 * Ultra-optimized with FILE* only, smart progress, and micro-optimizations.
 *
 * @param url The URL of the file to download.
 * @param toDestination The destination path where the file should be saved.
 * @return True if the download was successful, false otherwise.
 */
bool downloadFile(const std::string& url, const std::string& toDestination) {
    // Reset state
    abortDownload.store(false, std::memory_order_release);
    downloadPercentage.store(0, std::memory_order_release);

    // Quick URL validation - single check
    if (url.find_first_of("{}") != std::string::npos) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Invalid URL: " + url);
        #endif
        return false;
    }

    // Pre-allocate strings to avoid reallocations
    std::string destination;
    destination.reserve(toDestination.size() + 256); // Extra space for filename
    destination = toDestination;
    
    // Handle directory destination
    if (destination.back() == '/') {
        createDirectory(destination);
        size_t lastSlash = url.find_last_of('/');
        if (lastSlash == std::string::npos) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Invalid URL: " + url);
            #endif
            return false;
        }
        destination.append(url, lastSlash + 1, std::string::npos);
    } else {
        // Create parent directory - avoid substr allocation
        size_t lastSlash = destination.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string parentDir(destination, 0, lastSlash);
            createDirectory(parentDir);
        }
    }

    // Build temp file path efficiently
    std::string tempFilePath;
    tempFilePath.reserve(destination.size() + 16);
    
    size_t lastSlash = destination.find_last_of('/');
    if (lastSlash != std::string::npos) {
        tempFilePath.append(destination, 0, lastSlash + 1);
        tempFilePath += '.';
        tempFilePath.append(destination, lastSlash + 1, std::string::npos);
    } else {
        tempFilePath = '.';
        tempFilePath += destination;
    }
    tempFilePath += ".tmp";

    // Allocate write buffer - 256KB for optimal throughput
    size_t WRITE_BUFFER_SIZE = DOWNLOAD_WRITE_BUFFER;
    std::unique_ptr<char[]> writeBuffer = std::make_unique<char[]>(WRITE_BUFFER_SIZE);

    // Open file with write buffer
    FILE* file = fopen(tempFilePath.c_str(), "wb");
    if (!file) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error opening file: " + tempFilePath);
        #endif
        return false;
    }
    
    // Set write buffer for optimal performance
    setvbuf(file, writeBuffer.get(), _IOFBF, WRITE_BUFFER_SIZE);

    // Ensure curl is initialized
    //initializeCurl(); // already initialized in main.cpp of ultrahand

    // Initialize CURL
    std::unique_ptr<CURL, CurlDeleter> curl(curl_easy_init());
    if (!curl) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error initializing curl.");
        #endif
        fclose(file);
        deleteFileOrDirectory(tempFilePath);
        return false;
    }

    // Initialize progress tracking - all data in one struct
    ProgressData progressData = {
        &downloadPercentage,
        0,
        armGetSystemTick(),
        0,
        0
    };

    // Set CURL options - grouped by category for cache efficiency
    CURL* curlPtr = curl.get(); // Cache pointer
    
    // Core transfer options
    curl_easy_setopt(curlPtr, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlPtr, CURLOPT_WRITEFUNCTION, +writeCallback);
    curl_easy_setopt(curlPtr, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curlPtr, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_MAXREDIRS, 5L);
    
    // Progress tracking
    curl_easy_setopt(curlPtr, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curlPtr, CURLOPT_XFERINFOFUNCTION, +progressCallback);
    curl_easy_setopt(curlPtr, CURLOPT_XFERINFODATA, &progressData);
    
    // Buffer size - 256KB for optimal throughput
    curl_easy_setopt(curlPtr, CURLOPT_BUFFERSIZE, static_cast<long>(DOWNLOAD_READ_BUFFER));
    
    // Protocol settings
    curl_easy_setopt(curlPtr, CURLOPT_USERAGENT, userAgent);
    curl_easy_setopt(curlPtr, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curlPtr, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    
    // TCP optimizations - grouped together
    curl_easy_setopt(curlPtr, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_FASTOPEN, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_KEEPINTVL, 60L);
    
    // HTTP/2 and connection reuse
    curl_easy_setopt(curlPtr, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_HTTP09_ALLOWED, 0L);
    curl_easy_setopt(curlPtr, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curlPtr, CURLOPT_MAXAGE_CONN, 300L);
    
    // Timeouts
    curl_easy_setopt(curlPtr, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curlPtr, CURLOPT_LOW_SPEED_LIMIT, 1000L);
    curl_easy_setopt(curlPtr, CURLOPT_LOW_SPEED_TIME, 30L);
    
    // SSL optimizations
    curl_easy_setopt(curlPtr, CURLOPT_SSL_ENABLE_ALPN, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_SSL_ENABLE_NPN, 0L);
    curl_easy_setopt(curlPtr, CURLOPT_SSL_FALSESTART, 1L);
    
    // Set CA cert if exists - avoid repeated string construction
    static bool cacertChecked = false;
    static bool cacertExists = false;
    if (!cacertChecked) {
        cacertExists = isFileOrDirectory(std::string(cacertPath));
        cacertChecked = true;
    }
    if (cacertExists) {
        curl_easy_setopt(curlPtr, CURLOPT_CAINFO, cacertPath);
    }

    #if USING_LOGGING_DIRECTIVE
    logMessage("Downloading: " + url);
    #endif

    // Perform download
    CURLcode result = curl_easy_perform(curlPtr);

    // Ensure all data is written and close file
    fflush(file);
    fclose(file);

    // Handle errors
    if (result != CURLE_OK) {
        #if USING_LOGGING_DIRECTIVE
        const char* errorStr = curl_easy_strerror(result);
        switch(result) {
            case CURLE_OPERATION_TIMEDOUT:
                logMessage("Download timed out: " + url);
                break;
            case CURLE_COULDNT_CONNECT:
                logMessage("Could not connect to: " + url);
                break;
            case CURLE_ABORTED_BY_CALLBACK:
                logMessage("Download aborted by user");
                break;
            default:
                logMessage(std::string("Download error: ") + errorStr);
        }
        #endif
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(abortDownload.load() ? -2 : -1, std::memory_order_relaxed);
        return false;
    }

    // Verify file exists and is not empty - single syscall
    struct stat fileStat;
    if (stat(tempFilePath.c_str(), &fileStat) != 0 || fileStat.st_size == 0) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Downloaded file is empty or missing");
        #endif
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }

    // Final abort check
    if (abortDownload.load(std::memory_order_acquire)) {
        deleteFileOrDirectory(tempFilePath);
        downloadPercentage.store(-2, std::memory_order_relaxed);
        return false;
    }

    // Success - update progress and move file
    downloadPercentage.store(100, std::memory_order_relaxed);
    moveFile(tempFilePath, destination);
    
    #if USING_LOGGING_DIRECTIVE
    logMessage("Download complete: " + destination);
    #endif
    
    return true;
}


/**
 * @brief Custom I/O function for opening files with larger buffer
 */
static voidpf ZCALLBACK fopen64_file_func_custom(voidpf opaque, const void* filename, int mode) {
    FILE* file = nullptr;
    const char* mode_fopen = nullptr;
    
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename != nullptr) && (mode_fopen != nullptr)) {
        file = fopen((const char*)filename, mode_fopen);
        if (file && ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)) {
            // Set 64KB buffer for reading the ZIP file - reduces syscalls
            static const size_t zipReadBufferSize = UNZIP_READ_BUFFER;
            setvbuf(file, nullptr, _IOFBF, zipReadBufferSize);
        }
    }
    return file;
}

/**
 * @brief Extracts files from a ZIP archive to a specified destination.
 *
 * Ultra-optimized single-pass extraction with smooth byte-based progress reporting
 * using miniz with proper 64-bit file support and streaming extraction
 * 
 * @param zipFilePath The path to the ZIP archive file.
 * @param toDestination The destination directory where files should be extracted.
 * @return True if the extraction was successful, false otherwise.
 */
bool unzipFile(const std::string& zipFilePath, const std::string& toDestination) {
    abortUnzip.store(false, std::memory_order_release);
    unzipPercentage.store(0, std::memory_order_release);

    // Set up custom I/O with 64KB buffer for ZIP reading
    zlib_filefunc64_def ffunc;
    fill_fopen64_filefunc(&ffunc);
    ffunc.zopen64_file = fopen64_file_func_custom; // Override open function for larger buffer

    // Open ZIP file using custom I/O
    unzFile zipFile = unzOpen2_64(zipFilePath.c_str(), &ffunc);
    if (!zipFile) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Failed to open zip file: " + zipFilePath);
        #endif
        return false;
    }

    // Get global info about the ZIP file
    unz_global_info64 globalInfo;
    if (unzGetGlobalInfo64(zipFile, &globalInfo) != UNZ_OK) {
        unzClose(zipFile);
        #if USING_LOGGING_DIRECTIVE
        logMessage("Failed to get zip file info");
        #endif
        return false;
    }

    const uLong numFiles = globalInfo.number_entry;
    if (numFiles == 0) {
        unzClose(zipFile);
        #if USING_LOGGING_DIRECTIVE
        logMessage("No files found in archive");
        #endif
        return false;
    }

    // ALWAYS calculate total size for accurate byte-based progress
    ZPOS64_T totalUncompressedSize = 0;
    char tempFilenameBuffer[512];
    unz_file_info64 fileInfo;
    
    // First pass: calculate total uncompressed size
    int result = unzGoToFirstFile(zipFile);
    while (result == UNZ_OK) {
        if (unzGetCurrentFileInfo64(zipFile, &fileInfo, tempFilenameBuffer, sizeof(tempFilenameBuffer), 
                                   nullptr, 0, nullptr, 0) == UNZ_OK) {
            const size_t nameLen = strlen(tempFilenameBuffer);
            if (nameLen > 0 && tempFilenameBuffer[nameLen - 1] != '/') {
                totalUncompressedSize += fileInfo.uncompressed_size;
            }
        }
        result = unzGoToNextFile(zipFile);
    }

    // Fallback to 1 if no actual data (avoid division by zero)
    if (totalUncompressedSize == 0) {
        totalUncompressedSize = 1;
    }

    #if USING_LOGGING_DIRECTIVE
    logMessage("Processing " + std::to_string(numFiles) + " files, " + 
              std::to_string(totalUncompressedSize) + " total bytes from archive");
    #endif

    // Pre-allocate ALL reusable strings and variables outside the main loop
    std::string fileName, extractedFilePath, directoryPath;
    fileName.reserve(512);
    extractedFilePath.reserve(1024);
    directoryPath.reserve(1024);
    
    // Single large buffer for extraction - reused for all files
    const size_t bufferSize = UNZIP_WRITE_BUFFER;//std::max(UNZIP_BUFFER_SIZE, static_cast<size_t>(512 * 1024)); // At least 512KB
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(bufferSize);
    char filenameBuffer[512]; // Stack allocated for filename reading
    
    // Progress tracking variables - OPTIMIZED for smooth byte-based tracking
    ZPOS64_T totalBytesProcessed = 0;
    uLong filesProcessed = 0;
    int currentProgress = 0;  // Current percentage (0-100)
    
    // Calculate bytes per percentage point for smooth updates
    const ZPOS64_T bytesPerPercent = totalUncompressedSize / 100;
    ZPOS64_T nextPercentBoundary = bytesPerPercent;  // Next byte count that triggers a percentage update
    
    // Time-based abort checking - pre-calculated constants
    const u64 abortCheckInterval = armNsToTicks(2000000000ULL); // 2 seconds in ticks
    u64 lastAbortCheck = armGetSystemTick();
    u64 currentTick; // Reused for all tick operations
    
    // File operation variables - moved outside loop
    #if NO_FSTREAM_DIRECTIVE
    FILE* outputFile = nullptr;
    #else
    std::ofstream outputFile;
    #endif
    
    // Loop variables moved outside
    bool success = true;
    bool extractSuccess;
    ZPOS64_T fileBytesProcessed;
    int bytesRead;
    
    // String operation variables
    const char* filename;
    size_t nameLen;
    size_t lastSlashPos;
    size_t invalid_pos;
    size_t start_pos;
    
    // Ensure destination directory exists
    createDirectory(toDestination);
    
    // Ensure destination ends with '/' - pre-allocate final string
    std::string destination;
    destination.reserve(toDestination.size() + 1);
    destination = toDestination;
    if (!destination.empty() && destination.back() != '/') {
        destination += '/';
    }
    
    // Extract files
    result = unzGoToFirstFile(zipFile);
    while (result == UNZ_OK && success) {
        // Time-based abort check at start of each file (only if 2+ seconds have passed)
        currentTick = armGetSystemTick();
        if ((currentTick - lastAbortCheck) >= abortCheckInterval) {
            if (abortUnzip.load(std::memory_order_relaxed)) {
                success = false;
                break;
            }
            lastAbortCheck = currentTick;
        }
        
        // Get current file info - reuse fileInfo variable
        if (unzGetCurrentFileInfo64(zipFile, &fileInfo, filenameBuffer, sizeof(filenameBuffer), 
                                   nullptr, 0, nullptr, 0) != UNZ_OK) {
            result = unzGoToNextFile(zipFile);
            continue;
        }

        filename = filenameBuffer;
        
        // Quick filename validation
        if (!filename || filename[0] == '\0') {
            result = unzGoToNextFile(zipFile);
            continue;
        }
        
        nameLen = strlen(filename);
        if (nameLen > 0 && filename[nameLen - 1] == '/') { // Skip directories
            result = unzGoToNextFile(zipFile);
            continue;
        }
        
        // Build extraction path - reuse allocated strings
        fileName.assign(filename, nameLen);
        extractedFilePath.clear();
        extractedFilePath = destination;
        extractedFilePath += fileName;
        
        // Optimized character cleaning - only if needed
        invalid_pos = extractedFilePath.find_first_of(":*?\"<>|");
        if (invalid_pos != std::string::npos) {
            start_pos = std::min(extractedFilePath.find(ROOT_PATH) + 5, extractedFilePath.size());
            auto it = extractedFilePath.begin() + start_pos;
            extractedFilePath.erase(std::remove_if(it, extractedFilePath.end(), [](char c) {
                return c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|';
            }), extractedFilePath.end());
        }

        // Open the current file in the ZIP
        if (unzOpenCurrentFile(zipFile) != UNZ_OK) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Could not open file in ZIP: " + fileName);
            #endif
            result = unzGoToNextFile(zipFile);
            continue;
        }

        // Create directory if needed
        lastSlashPos = extractedFilePath.find_last_of('/');
        if (lastSlashPos != std::string::npos) {
            directoryPath.assign(extractedFilePath, 0, lastSlashPos + 1);
            createDirectory(directoryPath);
        }

        // Open output file
        #if NO_FSTREAM_DIRECTIVE
        outputFile = fopen(extractedFilePath.c_str(), "wb");
        if (!outputFile) {
            unzCloseCurrentFile(zipFile);
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + extractedFilePath);
            #endif
            result = unzGoToNextFile(zipFile);
            continue;
        }
        // Set buffer for consistent performance
        setvbuf(outputFile, buffer.get(), _IOFBF, bufferSize);
        #else
        outputFile.open(extractedFilePath, std::ios::binary);
        if (!outputFile.is_open()) {
            unzCloseCurrentFile(zipFile);
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + extractedFilePath);
            #endif
            result = unzGoToNextFile(zipFile);
            continue;
        }
        // Set larger buffer
        outputFile.rdbuf()->pubsetbuf(nullptr, UNZIP_WRITE_BUFFER);
        #endif

        // Extract file data in chunks
        extractSuccess = true;
        fileBytesProcessed = 0;
        
        while ((bytesRead = unzReadCurrentFile(zipFile, buffer.get(), bufferSize)) > 0) {
            // Time-based abort check - only every 2 seconds for optimal performance
            currentTick = armGetSystemTick();
            if ((currentTick - lastAbortCheck) >= abortCheckInterval) {
                if (abortUnzip.load(std::memory_order_relaxed)) {
                    extractSuccess = false;
                    break;
                }
                lastAbortCheck = currentTick;
            }
            
            // Write data to file
            #if NO_FSTREAM_DIRECTIVE
            if (fwrite(buffer.get(), 1, bytesRead, outputFile) != static_cast<size_t>(bytesRead)) {
                extractSuccess = false;
                break;
            }
            #else
            outputFile.write(buffer.get(), bytesRead);
            if (!outputFile.good()) {
                extractSuccess = false;
                break;
            }
            #endif
            
            // Update progress tracking
            fileBytesProcessed += bytesRead;
            totalBytesProcessed += bytesRead;
            
            // OPTIMIZED: Check if we've crossed any percentage boundaries
            // This ensures we never skip a percentage point
            while (totalBytesProcessed >= nextPercentBoundary && currentProgress < 99) {
                currentProgress++;
                nextPercentBoundary = static_cast<ZPOS64_T>(currentProgress + 1) * bytesPerPercent;
                
                // Update the atomic progress value
                unzipPercentage.store(currentProgress, std::memory_order_relaxed);
                
                #if USING_LOGGING_DIRECTIVE
                // Only log at 10% intervals to avoid spam
                if (currentProgress % 10 == 0) {
                    logMessage("Progress: " + std::to_string(currentProgress) + "% (" + 
                              std::to_string(totalBytesProcessed) + "/" + 
                              std::to_string(totalUncompressedSize) + " bytes)");
                }
                #endif
            }
        }

        // Check for read errors
        if (bytesRead < 0) {
            extractSuccess = false;
        }

        // Close files
        #if NO_FSTREAM_DIRECTIVE
        if (outputFile) {
            fclose(outputFile);
            outputFile = nullptr;
        }
        #else
        outputFile.close();
        #endif
        
        unzCloseCurrentFile(zipFile);

        if (!extractSuccess) {
            deleteFileOrDirectory(extractedFilePath);
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to extract: " + fileName);
            #endif
            
            if (abortUnzip.load(std::memory_order_relaxed)) {
                success = false;
                break;
            }
        } else {
            filesProcessed++;
        }

        // Move to next file
        result = unzGoToNextFile(zipFile);
    }

    // Clean up
    unzClose(zipFile);

    if (abortUnzip.load(std::memory_order_relaxed)) {
        unzipPercentage.store(-2, std::memory_order_release);
        #if USING_LOGGING_DIRECTIVE
        logMessage("Extraction aborted by user");
        #endif
        return false;
    }

    if (success && filesProcessed > 0) {
        unzipPercentage.store(100, std::memory_order_release);
        
        #if USING_LOGGING_DIRECTIVE
        logMessage("Extraction completed: " + std::to_string(filesProcessed) + " files, " + 
                  std::to_string(totalBytesProcessed) + " bytes");
        #endif
        
        return true;
    } else {
        unzipPercentage.store(-1, std::memory_order_release);
        return false;
    }
}
}
