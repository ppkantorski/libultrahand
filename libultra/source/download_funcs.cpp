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

    // Open ZIP file using switch-zlib
    unzFile zipFile = unzOpen64(zipFilePath.c_str());
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
    const size_t bufferSize = std::max(UNZIP_BUFFER_SIZE, static_cast<size_t>(512 * 1024)); // At least 512KB
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
        outputFile.rdbuf()->pubsetbuf(nullptr, UNZIP_BUFFER_SIZE);
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
                //if (currentProgress % 10 == 0) {
                //    logMessage("Progress: " + std::to_string(currentProgress) + "% (" + 
                //              std::to_string(totalBytesProcessed) + "/" + 
                //              std::to_string(totalUncompressedSize) + " bytes)");
                //}
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
