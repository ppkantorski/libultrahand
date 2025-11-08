/********************************************************************************
 * File: debug_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file contains the implementation of debugging functions for the
 *   Ultrahand Overlay project.
 ********************************************************************************/

#include "debug_funcs.hpp"

namespace ult {
    #if USING_LOGGING_DIRECTIVE
    // Define static variables
    const std::string defaultLogFilePath = "sdmc:/switch/.packages/log.txt";
    std::string logFilePath = defaultLogFilePath; 
    bool disableLogging = true;
    std::mutex logMutex;
    
    void logMessage(const char* message) {
        std::time_t currentTime = std::time(nullptr);
        std::tm* timeInfo = std::localtime(&currentTime);
        char timestamp[30];
        strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", timeInfo);
        
        #if !USING_FSTREAM_DIRECTIVE
        {
            std::lock_guard<std::mutex> lock(logMutex);
            
            FILE* file = fopen(logFilePath.c_str(), "a");
            if (file != nullptr) {
                fputs(timestamp, file);
                fputs(message, file);
                fputc('\n', file);
                fclose(file);
            }
        }
        #else
        {
            std::lock_guard<std::mutex> lock(logMutex);
            
            std::ofstream file(logFilePath.c_str(), std::ios::app);
            if (file.is_open()) {
                file << timestamp << message << "\n";
            }
        }
        #endif
    }
    
    // Overload for std::string
    void logMessage(const std::string& message) {
        logMessage(message.c_str());
    }
    #endif
}