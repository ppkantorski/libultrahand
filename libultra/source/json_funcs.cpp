/********************************************************************************
 * File: json_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file provides functions for working with JSON files in C++ using
 *   the `jansson` library. It includes a function to read JSON data from a file.
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

#include "json_funcs.hpp"


namespace ult {

    /**
     * @brief Reads JSON data from a file and returns it as a `json_t` object.
     *
     * @param filePath The path to the JSON file.
     * @return A `json_t` object representing the parsed JSON data. Returns `nullptr` on error.
     */
    json_t* readJsonFromFile(const std::string& filePath) {
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "rb");  // Open the file in binary mode
        if (!file) {
            // Optionally log: Failed to open file
            return nullptr;
        }
    
        // Move to the end of the file to determine its size
        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);  // Move back to the start of the file
    
        std::string content;
        content.resize(fileSize);  // Reserve space in the string to avoid multiple allocations
    
        // Read the file into the string's buffer
        size_t bytesRead = fread(&content[0], 1, fileSize, file);
        if (bytesRead != fileSize) {
            // Optionally log: Failed to read file
            fclose(file);  // Close the file before returning
            return nullptr;
        }
    
        fclose(file);  // Close the file after reading
    #else
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            // Optionally log: Failed to open file
            return nullptr;
        }
    
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
    
        std::string content;
        content.resize(fileSize);  // Reserve space in the string to avoid multiple allocations
    
        file.read(&content[0], fileSize);  // Read directly into the string's buffer
        if (file.fail() && !file.eof()) {
            // Optionally log: Failed to read file
            return nullptr;
        }
    
        file.close();  // Close the file after reading
    #endif
    
        // Parse the JSON content
        json_error_t error;
        json_t* root = json_loads(content.c_str(), 0, &error);
        if (!root) {
            // Optionally log: JSON parsing error at line `error.line`: `error.text`
            return nullptr;
        }
    
        // Optionally log: JSON file successfully parsed
        return root;
    }

    
    /**
     * @brief Parses a JSON string into a json_t object.
     *
     * This function takes a JSON string as input and parses it into a json_t object using Jansson library's `json_loads` function.
     * If parsing fails, it logs the error and returns nullptr.
     *
     * @param input The input JSON string to parse.
     * @return A json_t object representing the parsed JSON, or nullptr if parsing fails.
     */
    json_t* stringToJson(const std::string& input) {
        json_error_t error;
        json_t* jsonObj = json_loads(input.c_str(), 0, &error);
    
        if (!jsonObj) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to parse JSON: " + std::string(error.text) + " at line " + to_string(error.line));
            #endif
            return nullptr; // Return nullptr to indicate failure clearly
        }
    
        return jsonObj;
    }
    
    
    
    
    
    // Function to get a string from a JSON object
    std::string getStringFromJson(const json_t* root, const char* key) {
        const json_t* value = json_object_get(root, key);
        if (value && json_is_string(value)) {
            return json_string_value(value);
        } else {
            return ""; // Key not found or not a string, return empty string/char*
        }
    }
    
    
    /**
     * @brief Loads a JSON file from the specified path and retrieves a string value for a given key.
     *
     * This function combines the functionality of loading a JSON file and retrieving a string value associated
     * with a given key in a single step.
     *
     * @param filePath The path to the JSON file.
     * @param key The key whose associated string value is to be retrieved.
     * @return A string containing the value associated with the given key, or an empty string if the key is not found.
     */
    std::string getStringFromJsonFile(const std::string& filePath, const std::string& key) {
        // Load JSON from file using a smart pointer
        std::unique_ptr<json_t, JsonDeleter> root(readJsonFromFile(filePath), JsonDeleter());
        if (!root) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to load JSON file from path: " + filePath);
            #endif
            return "";
        }
    
        // Retrieve the string value associated with the key
        json_t* jsonKey = json_object_get(root.get(), key.c_str());
        const char* value = json_is_string(jsonKey) ? json_string_value(jsonKey) : nullptr;
    
        // Check if the value was found and return it
        if (value) {
            return std::string(value);
        } else {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Key not found or not a string in JSON: " + key);
            #endif
            return "";
        }
    }


    /**
     * @brief Sets a value in a JSON file, creating the file if it doesn't exist.
     *
     * @param filePath The path to the JSON file.
     * @param key The key to set.
     * @param value The value to set (auto-detected type).
     * @param createIfNotExists Whether to create the file if it doesn't exist.
     * @return true if successful, false otherwise.
     */
    bool setJsonValue(const std::string& filePath, const std::string& key, const std::string& value, bool createIfNotExists) {
        // Try to load existing file
        std::unique_ptr<json_t, JsonDeleter> root(readJsonFromFile(filePath), JsonDeleter());
        
        // If file doesn't exist, create new JSON object if allowed
        if (!root) {
            if (!createIfNotExists) {
                return false;
            }
            root.reset(json_object());
            if (!root) {
                return false;
            }
        }

        // Determine value type and create appropriate JSON value
        json_t* jsonValue = nullptr;
        if (value == "true") {
            jsonValue = json_true();
        } else if (value == "false") {
            jsonValue = json_false();
        } else {
            // Try parsing as integer
            std::size_t pos = 0;
            int intValue = ult::stoi(value, &pos, 10);
            if (pos == value.length() && !value.empty()) {
                jsonValue = json_integer(intValue);
            } else {
                jsonValue = json_string(value.c_str());
            }
        }

        if (!jsonValue) {
            return false;
        }

        // Set the value
        int result = json_object_set(root.get(), key.c_str(), jsonValue);
        json_decref(jsonValue);
        
        if (result != 0) {
            return false;
        }

        // Save to file
        char* jsonString = json_dumps(root.get(), JSON_INDENT(2) | JSON_PRESERVE_ORDER);
        if (!jsonString) {
            return false;
        }

        bool success = false;
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "wb");
        if (file) {
            size_t jsonLength = strlen(jsonString);
            size_t bytesWritten = fwrite(jsonString, 1, jsonLength, file);
            success = (bytesWritten == jsonLength);
            fclose(file);
        }
    #else
        std::ofstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            file << jsonString;
            success = !file.fail();
            file.close();
        }
    #endif

        free(jsonString);
        return success;
    }

    /**
     * @brief Renames a key in a JSON file.
     *
     * @param filePath The path to the JSON file.
     * @param oldKey The current key name.
     * @param newKey The new key name.
     * @return true if successful, false otherwise.
     */
    bool renameJsonKey(const std::string& filePath, const std::string& oldKey, const std::string& newKey) {
        // Try to load existing file
        std::unique_ptr<json_t, JsonDeleter> root(readJsonFromFile(filePath), JsonDeleter());
        
        if (!root) {
            return false; // File doesn't exist or couldn't be loaded
        }
    
        // Check if old key exists
        json_t* value = json_object_get(root.get(), oldKey.c_str());
        if (!value) {
            return false; // Old key doesn't exist
        }
    
        // Increment reference count since we're going to use this value twice
        json_incref(value);
    
        // Set the new key with the old value
        int setResult = json_object_set(root.get(), newKey.c_str(), value);
        
        // Delete the old key
        int delResult = json_object_del(root.get(), oldKey.c_str());
        
        // Decrement reference count
        json_decref(value);
        
        if (setResult != 0 || delResult != 0) {
            return false;
        }
    
        // Save to file
        char* jsonString = json_dumps(root.get(), JSON_INDENT(2) | JSON_PRESERVE_ORDER);
        if (!jsonString) {
            return false;
        }
    
        bool success = false;
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "wb");
        if (file) {
            size_t jsonLength = strlen(jsonString);
            size_t bytesWritten = fwrite(jsonString, 1, jsonLength, file);
            success = (bytesWritten == jsonLength);
            fclose(file);
        }
    #else
        std::ofstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            file << jsonString;
            success = !file.fail();
            file.close();
        }
    #endif
    
        free(jsonString);
        return success;
    }
}
