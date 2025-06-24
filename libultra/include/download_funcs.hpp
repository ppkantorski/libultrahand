/********************************************************************************
 * File: download_funcs.hpp
 * Author: ppkantorski
 * Description:
 *   This header file contains functions for downloading and extracting files
 *   using libcurl and zlib. It includes functions for downloading files from URLs,
 *   writing received data to a file, and extracting files from ZIP archives.
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
#pragma once
#ifndef DOWNLOAD_FUNCS_HPP
#define DOWNLOAD_FUNCS_HPP

#if NO_FSTREAM_DIRECTIVE
#include <stdio.h>
#include <sys/stat.h>
#else
#include <fstream>
#endif

#include <switch.h>
#include <curl/curl.h>
#include <zlib.h>
#include <zzip/zzip.h>
#include <atomic>
#include <memory>
#include <string>
#include <mutex>

#include "global_vars.hpp"
#include "string_funcs.hpp"
#include "get_funcs.hpp"
#include "path_funcs.hpp"
#include "debug_funcs.hpp"

namespace ult {
    // Constants for buffer sizes
    extern size_t DOWNLOAD_BUFFER_SIZE;
    extern size_t UNZIP_BUFFER_SIZE;
    
    // Path to the CA certificate
    extern const std::string cacertPath;
    extern const std::string cacertURL;
    
    // Thread-safe atomic flags for operation control
    extern std::atomic<bool> abortDownload;
    extern std::atomic<bool> abortUnzip;
    extern std::atomic<int> downloadPercentage;
    extern std::atomic<int> unzipPercentage;
    
    // User agent string for curl requests
    extern const std::string userAgent;
    
    // Custom deleters for CURL and ZZIP handles
    struct CurlDeleter {
        void operator()(CURL* curl) const;
    };
    
    struct ZzipDirDeleter {
        void operator()(ZZIP_DIR* dir) const;
    };
    
    struct ZzipFileDeleter {
        void operator()(ZZIP_FILE* file) const;
    };
    
    // Thread-safe callback functions
    #if NO_FSTREAM_DIRECTIVE
    size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream);
    #else
    size_t writeCallback(void* ptr, size_t size, size_t nmemb, std::ostream* stream);
    #endif
    
    extern "C" int progressCallback(void* ptr, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded);
    
    // Thread-safe initialization and cleanup functions
    void initializeCurl();
    void cleanupCurl();
    
    // Main API functions - thread-safe and memory leak resistant
    bool downloadFile(const std::string& url, const std::string& toDestination);
    bool unzipFile(const std::string& zipFilePath, const std::string& extractTo);
}

#endif // DOWNLOAD_FUNCS_HPP
