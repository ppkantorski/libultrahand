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

size_t DOWNLOAD_READ_BUFFER = 64 * 1024;//64 * 1024;//4096*10;
size_t DOWNLOAD_WRITE_BUFFER = 16 * 1024;//64 * 1024;
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
};

// Optimized write callback - minimal overhead
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {

    // Quick validation - single branch
    if (!ptr || !userdata || abortDownload.load(std::memory_order_relaxed)) return 0;

    FILE* stream = static_cast<FILE*>(userdata);
    
    // Direct write - let fwrite handle the multiplication
    return fwrite(ptr, size, nmemb, stream);
}

// Optimized progress callback - minimal branching and calculations
int progressCallback(void *ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, 
                     curl_off_t totalToUpload, curl_off_t nowUploaded) {
    if (!ptr) return 0;
    
    auto* progressData = static_cast<ProgressData*>(ptr);
    
    if (abortDownload.load(std::memory_order_relaxed)) {
        progressData->percentage->store(-1, std::memory_order_relaxed);
        return 1;  // Abort transfer
    }
    
    if (totalToDownload > 0) {
        int currentPercent = static_cast<int>((nowDownloaded * 100LL) / totalToDownload);
        int newProgress = std::min(currentPercent, 99);
        
        if (newProgress > progressData->lastReportedProgress) {
            progressData->lastReportedProgress = newProgress;
            progressData->percentage->store(newProgress, std::memory_order_relaxed);
            
            #if USING_LOGGING_DIRECTIVE
            if ((newProgress % 10) == 0) {
                logMessage("Download: " + std::to_string(newProgress) + "%");
            }
            #endif
        }
    }
    
    return 0;
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
 * Fixed memory leaks with RAII for proper resource cleanup on abort.
 *
 * @param url The URL of the file to download.
 * @param toDestination The destination path where the file should be saved.
 * @return True if the download was successful, false otherwise.
 */
bool downloadFile(const std::string& url, const std::string& toDestination) {
    // CRITICAL: Don't reset abort flag - check if we should abort immediately
    if (abortDownload.load(std::memory_order_acquire)) {
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }
    
    // Only reset percentage
    downloadPercentage.store(0, std::memory_order_release);

    // URL validation
    if (url.find_first_of("{}") != std::string::npos) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Invalid URL: " + url);
        #endif
        return false;
    }

    // Prepare destination path
    std::string destination;
    destination.reserve(toDestination.size() + 256);
    destination = toDestination;
    
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
        size_t lastSlash = destination.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string parentDir(destination, 0, lastSlash);
            createDirectory(parentDir);
        }
    }

    // Build temp file path
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

    // RAII file manager
    struct FileManager {
        FILE* file = nullptr;
        std::string tempPath;
        std::unique_ptr<char[]> buffer;
        
        FileManager(const std::string& path, size_t bufSize) : tempPath(path) {
            file = fopen(path.c_str(), "wb");
            if (file && bufSize > 0) {
                buffer = std::make_unique<char[]>(bufSize);
                setvbuf(file, buffer.get(), _IOFBF, bufSize);
            }
        }
        
        ~FileManager() {
            if (file) {
                fclose(file);
                file = nullptr;
            }
            if (!tempPath.empty()) {
                deleteFileOrDirectory(tempPath);
            }
        }
        
        bool is_valid() const { return file != nullptr; }
        void release_temp() { tempPath.clear(); }
    };

    FileManager fileManager(tempFilePath, DOWNLOAD_WRITE_BUFFER);
    if (!fileManager.is_valid()) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error opening file: " + tempFilePath);
        #endif
        return false;
    }

    // Initialize CURL
    std::unique_ptr<CURL, CurlDeleter> curl(curl_easy_init());
    if (!curl) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Error initializing curl.");
        #endif
        return false;
    }

    // Progress tracking data
    ProgressData progressData = {
        &downloadPercentage,
        -1
    };

    CURL* curlPtr = curl.get();
    
    // Core options
    curl_easy_setopt(curlPtr, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlPtr, CURLOPT_WRITEFUNCTION, +writeCallback);
    curl_easy_setopt(curlPtr, CURLOPT_WRITEDATA, fileManager.file);
    curl_easy_setopt(curlPtr, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_MAXREDIRS, 5L);
    
    // Progress tracking
    curl_easy_setopt(curlPtr, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curlPtr, CURLOPT_XFERINFOFUNCTION, +progressCallback);
    curl_easy_setopt(curlPtr, CURLOPT_XFERINFODATA, &progressData);
    
    // Small buffer for responsive abort
    curl_easy_setopt(curlPtr, CURLOPT_BUFFERSIZE, static_cast<long>(DOWNLOAD_READ_BUFFER));
    
    // Protocol settings
    curl_easy_setopt(curlPtr, CURLOPT_USERAGENT, userAgent);
    curl_easy_setopt(curlPtr, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curlPtr, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    
    // TCP optimizations
    curl_easy_setopt(curlPtr, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_FASTOPEN, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(curlPtr, CURLOPT_TCP_KEEPINTVL, 60L);
    
    // Connection settings
    curl_easy_setopt(curlPtr, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_HTTP09_ALLOWED, 0L);
    curl_easy_setopt(curlPtr, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curlPtr, CURLOPT_MAXAGE_CONN, 300L);
    
    // Timeouts
    curl_easy_setopt(curlPtr, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curlPtr, CURLOPT_LOW_SPEED_LIMIT, 1000L);
    curl_easy_setopt(curlPtr, CURLOPT_LOW_SPEED_TIME, 30L);
    
    // SSL settings
    curl_easy_setopt(curlPtr, CURLOPT_SSL_ENABLE_ALPN, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_SSL_ENABLE_NPN, 0L);
    curl_easy_setopt(curlPtr, CURLOPT_SSL_FALSESTART, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_NOSIGNAL, 1L);
    
    // CA cert
    static const bool cacertExists = isFileOrDirectory(cacertPath);
    if (cacertExists) {
        curl_easy_setopt(curlPtr, CURLOPT_CAINFO, cacertPath);
    }

    #if USING_LOGGING_DIRECTIVE
    logMessage("Downloading: " + url);
    #endif

    // Final abort check before starting
    if (abortDownload.load(std::memory_order_acquire)) {
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }

    // Perform download
    CURLcode result = curl_easy_perform(curlPtr);

    // Ensure data is written
    if (fileManager.file) {
        fflush(fileManager.file);
    }

    // Check if aborted even if curl says OK
    if (abortDownload.load(std::memory_order_acquire)) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Download aborted by user");
        #endif
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }

    // Handle curl result
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
            case CURLE_WRITE_ERROR:
                logMessage("Write error (possibly aborted)");
                break;
            default:
                logMessage(std::string("Download error: ") + errorStr);
        }
        #endif
        downloadPercentage.store(-1, std::memory_order_relaxed);
        return false;
    }

    // Verify file
    struct stat fileStat;
    if (stat(tempFilePath.c_str(), &fileStat) != 0 || fileStat.st_size == 0) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Downloaded file is empty or missing");
        #endif
        downloadPercentage.store(-1, std::memory_order_release);
        return false;
    }

    // Success
    downloadPercentage.store(100, std::memory_order_relaxed);
    fileManager.release_temp();
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
 * using miniz with proper 64-bit file support and streaming extraction.
 * Fixed memory leaks with RAII for proper resource cleanup on abort.
 * 
 * @param zipFilePath The path to the ZIP archive file.
 * @param toDestination The destination directory where files should be extracted.
 * @return True if the extraction was successful, false otherwise.
 */
bool unzipFile(const std::string& zipFilePath, const std::string& toDestination) {
    abortUnzip.store(false, std::memory_order_release);
    unzipPercentage.store(0, std::memory_order_release);

    // Time-based abort checking - pre-calculated constants
    u64 lastAbortCheck = armTicksToNs(armGetSystemTick());
    u64 currentNanos; // Reused for all tick operations
    bool success = true;

    // RAII wrapper for unzFile
    struct UnzFileManager {
        unzFile file = nullptr;
        
        UnzFileManager(const std::string& path) {
            zlib_filefunc64_def ffunc;
            fill_fopen64_filefunc(&ffunc);
            ffunc.zopen64_file = fopen64_file_func_custom;
            file = unzOpen2_64(path.c_str(), &ffunc);
        }
        
        ~UnzFileManager() {
            if (file) {
                unzClose(file);
                file = nullptr;
            }
        }
        
        bool is_valid() const { return file != nullptr; }
        operator unzFile() const { return file; }
    };

    // RAII wrapper for output file
    struct OutputFileManager {
        #if NO_FSTREAM_DIRECTIVE
        FILE* file = nullptr;
        std::unique_ptr<char[]> buffer;
        size_t bufferSize;
        
        OutputFileManager(size_t bufSize) : bufferSize(bufSize) {
            buffer = std::make_unique<char[]>(bufferSize);
        }
        
        bool open(const std::string& path) {
            close();
            file = fopen(path.c_str(), "wb");
            if (file) {
                setvbuf(file, buffer.get(), _IOFBF, bufferSize);
            }
            return file != nullptr;
        }
        
        void close() {
            if (file) {
                fclose(file);
                file = nullptr;
            }
        }
        
        bool is_open() const { return file != nullptr; }
        
        size_t write(const void* data, size_t size) {
            return file ? fwrite(data, 1, size, file) : 0;
        }
        
        ~OutputFileManager() { close(); }
        #else
        std::ofstream file;
        
        OutputFileManager(size_t bufSize) {
            // Constructor for consistency with FILE* version
        }
        
        bool open(const std::string& path) {
            close();
            file.open(path, std::ios::binary);
            if (file.is_open()) {
                file.rdbuf()->pubsetbuf(nullptr, UNZIP_WRITE_BUFFER);
            }
            return file.is_open();
        }
        
        void close() {
            if (file.is_open()) {
                file.close();
            }
        }
        
        bool is_open() const { return file.is_open(); }
        
        size_t write(const void* data, size_t size) {
            if (file.is_open()) {
                file.write(static_cast<const char*>(data), size);
                return file.good() ? size : 0;
            }
            return 0;
        }
        #endif
    };

    UnzFileManager zipFile(zipFilePath);
    if (!zipFile.is_valid()) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Failed to open zip file: " + zipFilePath);
        #endif
        return false;
    }

    // Get global info about the ZIP file
    unz_global_info64 globalInfo;
    if (unzGetGlobalInfo64(zipFile, &globalInfo) != UNZ_OK) {
        #if USING_LOGGING_DIRECTIVE
        logMessage("Failed to get zip file info");
        #endif
        return false;
    }

    const uLong numFiles = globalInfo.number_entry;
    if (numFiles == 0) {
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
        // Time-based abort check at start of each file (only if 2+ seconds have passed)
        currentNanos = armTicksToNs(armGetSystemTick());
        if ((currentNanos - lastAbortCheck) >= 2000000000ULL) {
            if (abortUnzip.load(std::memory_order_relaxed)) {
                unzipPercentage.store(-1, std::memory_order_release);
                #if USING_LOGGING_DIRECTIVE
                logMessage("Extraction aborted during size calculation");
                #endif
                abortUnzip.store(false, std::memory_order_release);
                return false;
            }
            lastAbortCheck = currentNanos;
        }

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
    const size_t bufferSize = UNZIP_WRITE_BUFFER;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(bufferSize);
    char filenameBuffer[512]; // Stack allocated for filename reading
    
    // Progress tracking variables - OPTIMIZED for smooth byte-based tracking
    ZPOS64_T totalBytesProcessed = 0;
    uLong filesProcessed = 0;
    int currentProgress = 0;  // Current percentage (0-100)
    
    // Calculate bytes per percentage point for smooth updates - FIXED CALCULATION
    const ZPOS64_T bytesPerPercent = (totalUncompressedSize + 99) / 100;  // Round up to avoid missing last percent
    ZPOS64_T nextPercentBoundary = bytesPerPercent;  // First boundary at 1%
    
    
    // Create output file manager
    OutputFileManager outputFile(bufferSize);
    
    // Loop variables moved outside
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
        currentNanos = armTicksToNs(armGetSystemTick());
        if ((currentNanos - lastAbortCheck) >= 2000000000ULL) {
            if (abortUnzip.load(std::memory_order_relaxed)) {
                success = false;
                break; // RAII will handle cleanup
            }
            lastAbortCheck = currentNanos;
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
        if (!outputFile.open(extractedFilePath)) {
            unzCloseCurrentFile(zipFile);
            #if USING_LOGGING_DIRECTIVE
            logMessage("Error creating file: " + extractedFilePath);
            #endif
            result = unzGoToNextFile(zipFile);
            continue;
        }

        // Extract file data in chunks
        extractSuccess = true;
        fileBytesProcessed = 0;
        
        while ((bytesRead = unzReadCurrentFile(zipFile, buffer.get(), bufferSize)) > 0) {
            // Time-based abort check - only every 2 seconds for optimal performance
            currentNanos = armTicksToNs(armGetSystemTick());
            if ((currentNanos - lastAbortCheck) >= 2000000000ULL) {
                if (abortUnzip.load(std::memory_order_relaxed)) {
                    extractSuccess = false;
                    break; // RAII will handle cleanup
                }
                lastAbortCheck = currentNanos;
            }
            
            // Write data to file
            if (outputFile.write(buffer.get(), bytesRead) != static_cast<size_t>(bytesRead)) {
                extractSuccess = false;
                break;
            }
            
            // Update progress tracking
            fileBytesProcessed += bytesRead;
            totalBytesProcessed += bytesRead;
            
            // FIXED: Check if we've crossed the next percentage boundary
            if (totalBytesProcessed >= nextPercentBoundary && currentProgress < 99) {
                currentProgress++;
                nextPercentBoundary = (static_cast<ZPOS64_T>(currentProgress + 1) * totalUncompressedSize) / 100;
                
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

        // Close current file handles
        outputFile.close();
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

    // Check final abort state
    if (abortUnzip.load(std::memory_order_relaxed)) {
        unzipPercentage.store(-1, std::memory_order_release);
        #if USING_LOGGING_DIRECTIVE
        logMessage("Extraction aborted by user");
        #endif
        abortUnzip.store(false, std::memory_order_release);
        return false;
    }

    if (success && filesProcessed > 0) {
        abortUnzip.store(false, std::memory_order_release);
        unzipPercentage.store(100, std::memory_order_release);
        
        #if USING_LOGGING_DIRECTIVE
        logMessage("Extraction completed: " + std::to_string(filesProcessed) + " files, " + 
                  std::to_string(totalBytesProcessed) + " bytes");
        #endif
        
        return true;
    } else {
        abortUnzip.store(false, std::memory_order_release);
        unzipPercentage.store(-1, std::memory_order_release);
        return false;
    }
}

}
