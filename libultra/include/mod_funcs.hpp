/********************************************************************************
 * File: mod_funcs.hpp
 * Author: ppkantorski
 * Description:
 *   This header file contains function declarations and utility functions for IPS
 *   binary generations. These functions are used in the Ultrahand Overlay project
 *   to convert `.pchtxt` mods into `.ips` binaries.
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

#ifndef MOD_FUNCS_HPP
#define MOD_FUNCS_HPP

#include <fstream>
//#include <sstream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include "debug_funcs.hpp"
#include "path_funcs.hpp"
#include "hex_funcs.hpp"
#include <iomanip> // Include this header for std::setw and std::setfill

namespace ult {
    extern const char* IPS32_HEAD_MAGIC;
    extern const char* IPS32_FOOT_MAGIC;
    //const std::string CHEAT_HEADER = "// auto generated by pchtxt2cheat\n\n";
    extern const std::string CHEAT_TYPE;
    extern const std::string CHEAT_EXT;
    extern const std::string CHEAT_ENCODING;
    
    /**
     * @brief Checks if a cheat already exists in the cheat file.
     * @param cheatFilePath The path to the cheat file.
     * @param newCheat The new cheat to check.
     * @return True if the cheat exists, otherwise false.
     */
    bool cheatExists(const std::string &cheatFilePath, const std::string &newCheat);
    
    /**
     * @brief Appends a new cheat to the cheat file.
     * @param cheatFilePath The path to the cheat file.
     * @param newCheat The new cheat to append.
     */
    void appendCheatToFile(const std::string &cheatFilePath, const std::string &newCheat);
    
    /**
     * @brief Extracts the cheat name from the given file path.
     * @param filePath The full file path.
     * @return The extracted cheat name.
     */
    std::string extractCheatName(const std::string &filePath);
    
    // Helper function to determine if a string is a valid title ID
    bool isValidTitleID(const std::string &str);
    
    // Function to find the title ID in the text, avoiding the @nsobid- line
    std::string findTitleID(const std::string &text);
    
    /**
     * @brief Converts a .pchtxt file to a cheat file.
     * @param pchtxtPath The file path to the .pchtxt file.
     * @param cheatName The name of the cheat.
     * @param outCheatPath The file path for the output cheat file.
     * @return True if the conversion was successful, false otherwise.
     */
    bool pchtxt2cheat(const std::string &pchtxtPath, std::string cheatName = "", std::string outCheatPath = "");
    
    // Corrected helper function to convert values to big-endian format
    uint32_t toBigEndian(uint32_t value);
    
    uint16_t toBigEndian(uint16_t value);
    
    // Helper function to convert a vector of bytes to a hex string for logging
    
    std::string hexToString(const std::vector<uint8_t>& bytes);
    
    
    /**
     * @brief Converts a .pchtxt file to an IPS file using fstream.
     *
     * This function reads the contents of a .pchtxt file, extracts the address-value pairs,
     * and generates an IPS file with the provided output folder.
     *
     * @param pchtxtPath The file path to the .pchtxt file.
     * @param outputFolder The folder path for the output IPS file.
     * @return True if the conversion was successful, false otherwise.
     */
    bool pchtxt2ips(const std::string& pchtxtPath, const std::string& outputFolder);
}

#endif