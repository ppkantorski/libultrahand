/********************************************************************************
 * File: download_funcs.hpp
 * Author: ppkantorski
 * Description:
 *   This header file contains functions for downloading and extracting files
 *   using libcurl and miniz. It includes functions for downloading files from URLs,
 *   writing received data to a file, and extracting files from ZIP archives.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 * 
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2023-2026 ppkantorski
 ********************************************************************************/

#pragma once

#include <stdio.h>
#include <sys/stat.h>
#include <switch.h>

#define CURL_DISABLE_DEFLATE
#include <curl/curl.h>
#include <zlib.h>
#include <minizip/unzip.h>

#include <atomic>
#include <memory>
#include <string>
#include <mutex>
#include <cstring>
#include <algorithm>

#include "global_vars.hpp"
#include "string_funcs.hpp"
#include "get_funcs.hpp"
#include "path_funcs.hpp"
#include "debug_funcs.hpp"

namespace ult {
    // Constants for buffer sizes
    extern size_t DOWNLOAD_READ_BUFFER;
    extern size_t DOWNLOAD_WRITE_BUFFER;
    extern size_t UNZIP_READ_BUFFER;
    extern size_t UNZIP_WRITE_BUFFER;
    
    // Thread-safe atomic flags for operation control
    extern std::atomic<bool> abortDownload;
    extern std::atomic<bool> abortUnzip;
    extern std::atomic<int> downloadPercentage;
    extern std::atomic<int> unzipPercentage;
    
    // Main API functions - thread-safe and optimized
    bool downloadFile(const std::string& url, const std::string& toDestination, bool noSocketInit=false, bool noPercentagePolling=false);
    bool unzipFile(const std::string& zipFilePath, const std::string& extractTo);
}