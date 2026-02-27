/********************************************************************************
 * File: hex_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file implements the functions declared in hex_funcs.hpp.
 *   These functions provide support for manipulating hexadecimal data,
 *   including conversions between ASCII and hexadecimal strings,
 *   locating specific hex patterns within files, and editing file contents
 *   at hex offsets.
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

#include "hex_funcs.hpp"

namespace ult {
    size_t HEX_BUFFER_SIZE = 4096;//65536/4;
    
    // Thread-safe cache and file operation mutexes
    std::shared_mutex cacheMutex;  // Allows multiple readers, single writer
    std::mutex fileWriteMutex;     // Protects file write operations
    
    // For improving the speed of hexing consecutively with the same file and asciiPattern.
    std::unordered_map<std::string, std::string> hexSumCache;
    
    
    /**
     * @brief Thread-safe cache management functions
     */
    void clearHexSumCache() {
        std::lock_guard<std::shared_mutex> writeLock(cacheMutex);
        hexSumCache = {};
    }

    size_t getHexSumCacheSize() {
        std::shared_lock<std::shared_mutex> readLock(cacheMutex);
        return hexSumCache.size();
    }

    /**
     * @brief Converts an ASCII string to a hexadecimal string.
     *
     * This function takes an ASCII string as input and converts it into a hexadecimal string.
     *
     * @param asciiStr The ASCII string to convert.
     * @return The corresponding hexadecimal string.
     */
    std::string asciiToHex(const std::string& asciiStr) {
        std::string hexStr;
    
        for (unsigned char c : asciiStr) {
            hexStr.push_back(hexLookup[c >> 4]); // High nibble
            hexStr.push_back(hexLookup[c & 0x0F]); // Low nibble
        }
    
        return hexStr;
    }
    
    /**
     * @brief Converts a decimal string to a fixed-width hexadecimal string.
     *
     * @param decimalStr The decimal string to convert.
     * @param byteGroupSize The number of hex digits to output (must be even for byte alignment).
     * @return Hex string of exactly 'byteGroupSize' digits, or empty string if value doesn't fit.
     */
    std::string decimalToHex(const std::string& decimalStr, int byteGroupSize) {
        const int decimalValue = ult::stoi(decimalStr);
        if (decimalValue < 0 || byteGroupSize <= 0 || (byteGroupSize % 2) != 0) {
            // Invalid input: negative number, or byteGroupSize <= 0, or odd byteGroupSize
            return "";
        }
    
        // Special case: zero
        if (decimalValue == 0) {
            return std::string(byteGroupSize, '0');
        }
    
        // Convert decimalValue to hex (uppercase, minimal length)
        std::string hex;
        int tempValue = decimalValue;
        int remainder;
        char hexChar;
        while (tempValue > 0) {
            remainder = tempValue % 16;
            hexChar = (remainder < 10) ? ('0' + remainder) : ('A' + remainder - 10);
            hex.insert(hex.begin(), hexChar);
            tempValue /= 16;
        }
    
        // Ensure hex length is even by adding leading zero if needed
        if (hex.length() % 2 != 0) {
            hex.insert(hex.begin(), '0');
        }
    
        // Minimum size needed to fit hex string
        const size_t hexLen = hex.length();
    
        // Adjust minimum byteGroupSize to be at least hexLen
        size_t minByteGroupSize = std::max(static_cast<size_t>(byteGroupSize), hexLen);
    
        // If byteGroupSize was too small, adjust to hex length (must be even)
        if (minByteGroupSize % 2 != 0) {
            minByteGroupSize++;
        }
    
        // If minByteGroupSize is less than hex length, number doesn't fit
        if (minByteGroupSize < hexLen) {
            return ""; // can't fit
        }
    
        // Pad with leading zeros to match minByteGroupSize
        if (hexLen < minByteGroupSize) {
            hex.insert(hex.begin(), minByteGroupSize - hexLen, '0');
        }
    
        return hex;
    }
    
    
    /**
     * @brief Converts a hexadecimal string to a decimal string.
     *
     * This function takes a hexadecimal string as input and converts it into a decimal string.
     *
     * @param hexStr The hexadecimal string to convert.
     * @return The corresponding decimal string.
     */
    std::string hexToDecimal(const std::string& hexStr) {
        // Convert hexadecimal string to integer
        int decimalValue = 0;
        const size_t len = hexStr.length();
        
        char hexChar;
        int value;

        // Iterate over each character in the hexadecimal string
        for (size_t i = 0; i < len; ++i) {
            hexChar = hexStr[i];
            
            // Convert hex character to its decimal value
            if (hexChar >= '0' && hexChar <= '9') {
                value = hexChar - '0';
            } else if (hexChar >= 'A' && hexChar <= 'F') {
                value = 10 + (hexChar - 'A');
            } else if (hexChar >= 'a' && hexChar <= 'f') {
                value = 10 + (hexChar - 'a');
            } else {
                break;
            }
    
            // Update the decimal value
            decimalValue = decimalValue * 16 + value;
        }
    
        // Convert the decimal value to a string
        return ult::to_string(decimalValue);
    }
    
    
    
    std::string hexToReversedHex(const std::string& hexadecimal, int order) {
        // Reverse the hexadecimal string in groups of order
        std::string reversedHex;
        for (int i = hexadecimal.length() - order; i >= 0; i -= order) {
            reversedHex += hexadecimal.substr(i, order);
        }
        
        return reversedHex;
    }
    
    /**
     * @brief Converts a decimal string to a reversed hexadecimal string.
     *
     * This function takes a decimal string as input, converts it into a hexadecimal
     * string, and reverses the resulting hexadecimal string in groups of byteGroupSize.
     *
     * @param decimalStr The decimal string to convert.
     * @param byteGroupSize The grouping byteGroupSize for reversing the hexadecimal string.
     * @return The reversed hexadecimal string.
     */
    std::string decimalToReversedHex(const std::string& decimalStr, int byteGroupSize) {
        return hexToReversedHex(decimalToHex(decimalStr, byteGroupSize));
    }
    
    
    
    /**
     * @brief Finds the offsets of hexadecimal data in a file.
     *
     * This function searches for occurrences of hexadecimal data in a binary file
     * and returns the file offsets where the data is found.
     *
     * @param filePath The path to the binary file.
     * @param hexData The hexadecimal data to search for.
     * @return A vector of strings containing the file offsets where the data is found.
     */
    std::vector<std::string> findHexDataOffsets(const std::string& filePath, const std::string& hexData) {
        std::vector<std::string> offsets;

        FILE* file = fopen(filePath.c_str(), "rb");
        if (!file) {
            return offsets;
        }
    
        fseek(file, 0, SEEK_END);
        const size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
    
        if (hexData.length() % 2 != 0) {
            fclose(file);
            return offsets;
        }
        
        const size_t hexLen = hexData.length();
        const size_t patternLen = hexLen / 2;
        
        // Use heap allocation for the buffer to avoid stack overflow with large buffer sizes
        std::unique_ptr<unsigned char[]> binaryData(new unsigned char[patternLen]);
        const unsigned char* hexPtr = reinterpret_cast<const unsigned char*>(hexData.c_str());
        
        // Unrolled hex conversion loop
        size_t i = 0;
        for (; i + 4 <= hexLen; i += 4) {
            binaryData[i/2] = (hexTable[hexPtr[i]] << 4) | hexTable[hexPtr[i + 1]];
            binaryData[i/2 + 1] = (hexTable[hexPtr[i + 2]] << 4) | hexTable[hexPtr[i + 3]];
        }
        // Handle remaining bytes
        for (; i < hexLen; i += 2) {
            binaryData[i/2] = (hexTable[hexPtr[i]] << 4) | hexTable[hexPtr[i + 1]];
        }
    
        // Optimized search variables
        const unsigned char* patternPtr = binaryData.get();
        const unsigned char firstByte = patternPtr[0];
        
        // Use heap allocation for the buffer to avoid stack overflow with large buffer sizes
        std::unique_ptr<unsigned char[]> buffer(new unsigned char[HEX_BUFFER_SIZE]);
        size_t bytesRead = 0;
        size_t offset = 0;
        

        while ((bytesRead = fread(buffer.get(), 1, HEX_BUFFER_SIZE, file)) > 0) {
            const unsigned char* bufPtr = buffer.get();
            
            // Optimized search with first-byte filtering and loop unrolling
            i = 0;
            const size_t searchEnd = bytesRead;
            
            // Process 4 bytes at a time for better cache usage
            for (; i + 4 <= searchEnd; i += 4) {
                // Check 4 positions at once
                if (bufPtr[i] == firstByte) {
                    if (offset + i + patternLen <= fileSize && 
                        memcmp(bufPtr + i, patternPtr, patternLen) == 0) {
                        offsets.emplace_back(ult::to_string(offset + i));
                    }
                }
                if (bufPtr[i + 1] == firstByte) {
                    if (offset + i + 1 + patternLen <= fileSize && 
                        memcmp(bufPtr + i + 1, patternPtr, patternLen) == 0) {
                        offsets.emplace_back(ult::to_string(offset + i + 1));
                    }
                }
                if (bufPtr[i + 2] == firstByte) {
                    if (offset + i + 2 + patternLen <= fileSize && 
                        memcmp(bufPtr + i + 2, patternPtr, patternLen) == 0) {
                        offsets.emplace_back(ult::to_string(offset + i + 2));
                    }
                }
                if (bufPtr[i + 3] == firstByte) {
                    if (offset + i + 3 + patternLen <= fileSize && 
                        memcmp(bufPtr + i + 3, patternPtr, patternLen) == 0) {
                        offsets.emplace_back(ult::to_string(offset + i + 3));
                    }
                }
            }
            
            // Handle remaining bytes
            for (; i < searchEnd; ++i) {
                if (bufPtr[i] == firstByte) {
                    if (offset + i + patternLen <= fileSize && 
                        memcmp(bufPtr + i, patternPtr, patternLen) == 0) {
                        offsets.emplace_back(ult::to_string(offset + i));
                    }
                }
            }
            
            offset += bytesRead;
        }
    
        fclose(file);
    
        return offsets;
    }

    
    
    /**
     * @brief Edits hexadecimal data in a file at a specified offset.
     *
     * This function opens a binary file, seeks to a specified offset, and replaces
     * the data at that offset with the provided hexadecimal data.
     *
     * @param filePath The path to the binary file.
     * @param offsetStr The offset in the file to perform the edit.
     * @param hexData The hexadecimal data to replace at the offset.
     */
    void hexEditByOffset(const std::string& filePath, const std::string& offsetStr, const std::string& hexData) {
        // Lock file writes to prevent concurrent modifications to the same file
        std::lock_guard<std::mutex> fileWriteLock(fileWriteMutex);
        const std::streampos offset = std::stoll(offsetStr);
        
        // Open the file for both reading and writing in binary mode
        FILE* file = fopen(filePath.c_str(), "rb+");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to open the file.");
            #endif
            return;
        }
    
        // Retrieve the file size
        fseek(file, 0, SEEK_END);
        const std::streampos fileSize = ftell(file);
    
        if (offset >= fileSize) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Offset exceeds file size.");
            #endif
            fclose(file);
            return;
        }
    
        // Validate hex data length
        const size_t hexLen = hexData.length();
        if (hexLen % 2 != 0) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Invalid hex data length.");
            #endif
            fclose(file);
            return;
        }
    
        // Convert the hex string to binary data using optimized lookup table
        const size_t dataLen = hexLen / 2;
        std::unique_ptr<unsigned char[]> binaryData(new unsigned char[dataLen]);
        const unsigned char* hexPtr = reinterpret_cast<const unsigned char*>(hexData.c_str());
        
        // Unrolled hex conversion loop (same as findHexDataOffsets)
        size_t i = 0;
        for (; i + 4 <= hexLen; i += 4) {
            binaryData[i/2] = (hexTable[hexPtr[i]] << 4) | hexTable[hexPtr[i + 1]];
            binaryData[i/2 + 1] = (hexTable[hexPtr[i + 2]] << 4) | hexTable[hexPtr[i + 3]];
        }
        // Handle remaining bytes
        for (; i < hexLen; i += 2) {
            binaryData[i/2] = (hexTable[hexPtr[i]] << 4) | hexTable[hexPtr[i + 1]];
        }
    
        // Move to the specified offset and write the binary data directly to the file
        fseek(file, offset, SEEK_SET);
        const size_t bytesWritten = fwrite(binaryData.get(), sizeof(unsigned char), dataLen, file);
        if (bytesWritten != dataLen) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to write data to the file.");
            #endif
            fclose(file);
            return;
        }
    
        fclose(file);
    }
    
    
    /**
     * @brief Edits a specific offset in a file with custom hexadecimal data.
     *
     * This function searches for a custom pattern in the file and calculates a new offset
     * based on user-provided offsetStr and the found pattern. It then replaces the data
     * at the calculated offset with the provided hexadecimal data.
     *
     * @param filePath The path to the binary file.
     * @param offsetStr The user-provided offset for the edit.
     * @param customPattern The custom pattern to search for in the file.
     * @param hexDataReplacement The hexadecimal data to replace at the calculated offset.
     * @param occurrence The occurrence/index of the data to replace (default is "0" to replace all occurrences).
     */
    void hexEditByCustomOffset(const std::string& filePath, const std::string& customAsciiPattern, const std::string& offsetStr, const std::string& hexDataReplacement, size_t occurrence) {
        
        // Create a cache key based on filePath and customAsciiPattern
        const std::string cacheKey = filePath + '?' + customAsciiPattern + '?' + ult::to_string(occurrence);
        
        int hexSum = -1;
        
        // Thread-safe cache access
        {
            std::shared_lock<std::shared_mutex> readLock(cacheMutex);
            const auto cachedResult = hexSumCache.find(cacheKey);
            if (cachedResult != hexSumCache.end()) {
                hexSum = ult::stoi(cachedResult->second);
            }
        }
        
        if (hexSum == -1) {
            std::string customHexPattern;
            if (customAsciiPattern[0] == '#') {
                // remove #
                customHexPattern = customAsciiPattern.substr(1);
            } else {
                // Convert custom ASCII pattern to a custom hex pattern
                customHexPattern = asciiToHex(customAsciiPattern);
            }
            
            
            // Find hex data offsets in the file
            const std::vector<std::string> offsets = findHexDataOffsets(filePath, customHexPattern);
            
            if (!offsets.empty()) {
                hexSum = ult::stoi(offsets[occurrence]);
                
                // Thread-safe cache write
                {
                    std::lock_guard<std::shared_mutex> writeLock(cacheMutex);
                    hexSumCache[cacheKey] = ult::to_string(hexSum);
                }
            } else {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Offset not found.");
                #endif
                return;
            }
        }
        
        
        if (hexSum != -1) {
            // Calculate the total offset to seek in the file
            hexEditByOffset(filePath, ult::to_string(hexSum + ult::stoi(offsetStr)), hexDataReplacement);
        } else {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to find " + customAsciiPattern + ".");
            #endif
        }
    }
    
    /**
     * @brief Finds and replaces hexadecimal data in a file.
     *
     * This function searches for occurrences of hexadecimal data in a binary file
     * and replaces them with a specified hexadecimal replacement data.
     *
     * @param filePath The path to the binary file.
     * @param hexDataToReplace The hexadecimal data to search for and replace.
     * @param hexDataReplacement The hexadecimal data to replace with.
     * @param occurrence The occurrence/index of the data to replace (default is "0" to replace all occurrences).
     */
    void hexEditFindReplace(const std::string& filePath, const std::string& hexDataToReplace, const std::string& hexDataReplacement, size_t occurrence) {
        const std::vector<std::string> offsetStrs = findHexDataOffsets(filePath, hexDataToReplace);
        if (!offsetStrs.empty()) {
            if (occurrence == 0) {
                // Replace all occurrences
                for (const std::string& offsetStr : offsetStrs) {
                    hexEditByOffset(filePath, offsetStr, hexDataReplacement);
                }
            } else {
                // Convert the occurrence string to an integer
                if (occurrence > 0 && occurrence <= offsetStrs.size()) {
                    // Replace the specified occurrence/index
                    hexEditByOffset(filePath, offsetStrs[occurrence - 1], hexDataReplacement);
                } else {
                    // Invalid occurrence/index specified
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Invalid hex occurrence/index specified.");
                    #endif
                }
            }
        }
    }
    
    /**
     * @brief Finds and replaces hexadecimal data in a file.
     *
     * This function searches for occurrences of hexadecimal data in a binary file
     * and replaces them with a specified hexadecimal replacement data.
     *
     * @param filePath The path to the binary file.
     * @param hexDataToReplace The hexadecimal data to search for and replace.
     * @param hexDataReplacement The hexadecimal data to replace with.
     * @param occurrence The occurrence/index of the data to replace (default is "0" to replace all occurrences).
     */
    std::string parseHexDataAtCustomOffset(const std::string& filePath, const std::string& customAsciiPattern, 
                                         const std::string& offsetStr, size_t length, size_t occurrence) {
        const std::string cacheKey = filePath + '?' + customAsciiPattern + '?' + ult::to_string(occurrence);
        int hexSum = -1;

        // Thread-safe cache read
        {
            std::shared_lock<std::shared_mutex> readLock(cacheMutex);
            const auto cachedResult = hexSumCache.find(cacheKey);
            if (cachedResult != hexSumCache.end()) {
                hexSum = ult::stoi(cachedResult->second);
            }
        }

        if (hexSum == -1) {
            const std::string customHexPattern = asciiToHex(customAsciiPattern);
            const std::vector<std::string> offsets = findHexDataOffsets(filePath, customHexPattern);

            if (!offsets.empty() && offsets.size() > occurrence) {
                hexSum = ult::stoi(offsets[occurrence]);
                
                // Thread-safe cache write
                {
                    std::lock_guard<std::shared_mutex> writeLock(cacheMutex);
                    hexSumCache[cacheKey] = ult::to_string(hexSum);
                }
            } else {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Offset not found.");
                #endif
                return "";
            }
        }

        const std::streampos totalOffset = hexSum + std::stoll(offsetStr);
        
        // Pre-allocate final string size to avoid reallocation
        std::string result;
        result.reserve(length * 2);

        FILE* file = fopen(filePath.c_str(), "rb");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to open the file.");
            #endif
            return "";
        }

        if (fseek(file, totalOffset, SEEK_SET) != 0) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Error seeking to offset.");
            #endif
            fclose(file);
            return "";
        }

        // Use heap allocation for the buffer to avoid stack overflow with large buffer sizes
        std::unique_ptr<unsigned char[]> buffer(new unsigned char[length]);
        const size_t bytesRead = fread(buffer.get(), 1, length, file);
        fclose(file);
        
        if (bytesRead != length) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Error reading data from file or end of file reached.");
            #endif
            return "";
        }

        // Optimized hex conversion - directly build uppercase string
        static constexpr char hexDigits[] = "0123456789ABCDEF";
        result.resize(length * 2);
        
        // Unrolled loop for better performance
        size_t i = 0;
        for (; i + 4 <= length; i += 4) {
            result[i * 2]     = hexDigits[(buffer[i] >> 4) & 0xF];
            result[i * 2 + 1] = hexDigits[buffer[i] & 0xF];
            result[i * 2 + 2] = hexDigits[(buffer[i + 1] >> 4) & 0xF];
            result[i * 2 + 3] = hexDigits[buffer[i + 1] & 0xF];
            result[i * 2 + 4] = hexDigits[(buffer[i + 2] >> 4) & 0xF];
            result[i * 2 + 5] = hexDigits[buffer[i + 2] & 0xF];
            result[i * 2 + 6] = hexDigits[(buffer[i + 3] >> 4) & 0xF];
            result[i * 2 + 7] = hexDigits[buffer[i + 3] & 0xF];
        }
        // Handle remaining bytes
        for (; i < length; ++i) {
            result[i * 2]     = hexDigits[(buffer[i] >> 4) & 0xF];
            result[i * 2 + 1] = hexDigits[buffer[i] & 0xF];
        }
        

        return result;
    }
    
    
    
    /**
     * @brief Finds and replaces hexadecimal data in a file.
     *
     * This function searches for occurrences of hexadecimal data in a binary file
     * and replaces them with a specified hexadecimal replacement data.
     *
     * @param filePath The path to the binary file.
     * @param hexDataToReplace The hexadecimal data to search for and replace.
     * @param hexDataReplacement The hexadecimal data to replace with.
     * @param occurrence The occurrence/index of the data to replace (default is "0" to replace all occurrences).
     */
    
    std::string replaceHexPlaceholder(const std::string& arg, const std::string& hexPath) {
        std::string replacement = arg;
        const std::string searchString = "{hex_file(";
        
        const size_t startPos = replacement.find(searchString);
        const size_t endPos = replacement.find(")}");
        
        if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos) {
            const std::string placeholderContent = replacement.substr(startPos + searchString.length(), endPos - startPos - searchString.length());
            
            // Split the placeholder content into its components (customAsciiPattern, offsetStr, length)
            std::vector<std::string> components;
            
            // Use StringStream instead of std::istringstream
            StringStream componentStream(placeholderContent);
            std::string component;
            
            while (componentStream.getline(component, ',')) {
                trim(component);
                components.push_back(component);
            }
            
            if (components.size() == 3) {
                // Extract individual components
                const std::string customAsciiPattern = components[0];
                const std::string offsetStr = components[1];
                const size_t length = std::stoul(components[2]);
                
                // Call the parsing function and replace the placeholder
                const std::string parsedResult = parseHexDataAtCustomOffset(hexPath, customAsciiPattern, offsetStr, length);
                
                // Only replace if parsedResult returns a non-empty string
                if (!parsedResult.empty()) {
                    // Replace the entire placeholder with the parsed result
                    replacement.replace(startPos, endPos - startPos + searchString.length() + 2, parsedResult);
                }
            }
        }
        
        return replacement;
    }

    
    
        
    /**
     * @brief Extracts the version string from a binary file.
     *
     * This function reads a binary file and searches for a version pattern
     * in the format "v#.#.#" (e.g., "v1.2.3").
     *
     * @param filePath The path to the binary file.
     * @return The version string if found; otherwise, an empty string.
     */
    std::string extractVersionFromBinary(const std::string &filePath) {
        // Step 1: Open the binary file
        FILE* file = fopen(filePath.c_str(), "rb");
        if (!file) {
            return ""; // Return empty string if file cannot be opened
        }
    
        // Get the file size
        fseek(file, 0, SEEK_END);
        const std::streamsize size = ftell(file);
        fseek(file, 0, SEEK_SET);
    
        // Read the entire file into a buffer
        std::vector<uint8_t> buffer(size);
        const size_t bytesRead = fread(buffer.data(), sizeof(uint8_t), size, file);
        fclose(file); // Close the file after reading
        
        if (bytesRead != static_cast<size_t>(size)) {
            return ""; // Return empty string if reading fails
        }
    
        // Step 2: Search for the pattern "v#.#.#"
        const char* data = reinterpret_cast<const char*>(buffer.data());
        for (std::streamsize i = 0; i < size; ++i) {
            if (data[i] == 'v' && i + 5 < size && 
                std::isdigit(data[i + 1]) && data[i + 2] == '.' && 
                std::isdigit(data[i + 3]) && data[i + 4] == '.' && 
                std::isdigit(data[i + 5])) {
    
                // Extract the version string
                return std::string(data + i, 6); // Return the version string
            }
        }
    
        return "";  // Return empty string if no match is found
    }

    // 1. Table optimization: Mark as constexpr for compile-time evaluation
    static constexpr uint8_t b64_table[256] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,62,  0xFF,0xFF,0xFF,63,
        52,53,54,55,56,57,58,59,60,61,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };
    
    // 2. Optimized decode: Pre-calculate output size, reduce bounds checking
    static size_t base64_decode(const char* src, uint8_t* out) {
        size_t outLen = 0;
        const char* p = src;
        
        // Process 4 chars at a time (unrolled loop for better instruction pipelining)
        while (*p) {
            uint8_t a = b64_table[static_cast<uint8_t>(*p++)];
            if (a == 0xFF) break;
            
            uint8_t b = b64_table[static_cast<uint8_t>(*p++)];
            if (b == 0xFF) break;
            
            out[outLen++] = (a << 2) | (b >> 4);
            
            uint8_t cChar = *p++;
            if (cChar == '=' || cChar == '\0') break;
            
            uint8_t c = b64_table[cChar];
            if (c == 0xFF) break;
            
            out[outLen++] = (b << 4) | (c >> 2);
            
            uint8_t dChar = *p++;
            if (dChar == '=' || dChar == '\0') break;
            
            uint8_t d = b64_table[dChar];
            if (d == 0xFF) break;
            
            out[outLen++] = (c << 6) | d;
        }
        
        return outLen;
    }
    
    // 3. Optimized wrapper: Pre-calculate exact output size, avoid vector overhead
    std::string decodeBase64ToString(const std::string& b64) {
        // Base64 decodes to ~3/4 original size
        const size_t maxOutSize = (b64.size() * 3) / 4 + 3;
        std::string result(maxOutSize, '\0');
        
        const size_t len = base64_decode(b64.c_str(), reinterpret_cast<uint8_t*>(result.data()));
        result.resize(len);
        
        return result;
    }
}
