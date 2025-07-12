/********************************************************************************
 * File: list_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file contains function declarations and utility functions related
 *   to working with lists and vectors of strings. These functions are used in the
 *   Ultrahand Overlay project to perform various operations on lists, such as
 *   removing entries, filtering, and more.
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

#include <list_funcs.hpp>
#include <mutex>

namespace ult {
    // Thread-safe file access mutex
    static std::mutex file_access_mutex;
    
    std::vector<std::string> splitIniList(const std::string& value) {
        std::vector<std::string> result;
        std::string trimmed = value;
        trim(trimmed);
        if (trimmed.size() > 2 && trimmed.front() == '(' && trimmed.back() == ')') {
            trimmed = trimmed.substr(1, trimmed.size() - 2);
            ult::StringStream ss(trimmed);
            std::string token;
            while (ss.getline(token, ',')) {
                trim(token);
                result.push_back(token);
            }
        }
        return result;
    }
    
    std::string joinIniList(const std::vector<std::string>& list) {
        std::string result = "";
        for (size_t i = 0; i < list.size(); ++i) {
            result += list[i];
            if (i + 1 < list.size()) {
                result += ", ";
            }
        }
        return result;
    }


    /**
     * @brief Removes entries from a vector of strings that match a specified entry.
     *
     * This function removes entries from the `itemsList` vector of strings that match the `entry`.
     *
     * @param entry The entry to be compared against the elements in `itemsList`.
     * @param itemsList The vector of strings from which matching entries will be removed.
     */
    void removeEntryFromList(const std::string& entry, std::vector<std::string>& itemsList) {
        itemsList.erase(std::remove_if(itemsList.begin(), itemsList.end(), [&](const std::string& item) {
            return item.compare(0, entry.length(), entry) == 0;
        }), itemsList.end());
    }
    
    /**
     * @brief Filters a list of strings based on a specified filter list.
     *
     * This function filters a list of strings (`itemsList`) by removing entries that match any
     * of the criteria specified in the `filterList`. It uses the `removeEntryFromList` function
     * to perform the removal.
     *
     * @param filterList The list of entries to filter by. Entries in `itemsList` matching any entry in this list will be removed.
     * @param itemsList The list of strings to be filtered.
     */
    void filterItemsList(const std::vector<std::string>& filterList, std::vector<std::string>& itemsList) {
        for (const auto& entry : filterList) {
            removeEntryFromList(entry, itemsList);
        }
    }
    
    
    // Function to read file into a vector of strings with optional cap
    std::vector<std::string> readListFromFile(const std::string& filePath, size_t maxLines) {
        std::lock_guard<std::mutex> lock(file_access_mutex);
        std::vector<std::string> lines;
    
    #if !USING_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "r");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines;
        }
    
        constexpr size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        size_t len;
        while (fgets(buffer, BUFFER_SIZE, file)) {
            // Check cap before adding
            if (maxLines > 0 && lines.size() >= maxLines) {
                break;
            }
            
            // Remove newline character from the buffer if present
            len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            lines.emplace_back(buffer);
        }
    
        fclose(file);
    #else
        std::ifstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines;
        }
    
        std::string line;
        while (std::getline(file, line)) {
            // Check cap before adding
            if (maxLines > 0 && lines.size() >= maxLines) {
                break;
            }
            lines.push_back(std::move(line));
        }
    
        file.close();
    #endif
    
        return lines;
    }
    
    
    // Function to get an entry from the list based on the index
    std::string getEntryFromListFile(const std::string& listPath, size_t listIndex) {
        std::lock_guard<std::mutex> lock(file_access_mutex);
        
    #if !USING_FSTREAM_DIRECTIVE
        FILE* file = fopen(listPath.c_str(), "r");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + listPath);
            #endif
            return "";
        }
    
        constexpr size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        std::string line;

        size_t len;

        // Read lines until reaching the desired index
        for (size_t i = 0; i <= listIndex; ++i) {
            if (!fgets(buffer, BUFFER_SIZE, file)) {
                fclose(file);
                return ""; // Index out of bounds
            }
            if (i == listIndex) {
                line = buffer;
                // Remove newline character if present
                len = line.length();
                if (len > 0 && line[len - 1] == '\n') {
                    line.pop_back();
                }
                break;
            }
        }
    
        fclose(file);
    #else
        std::ifstream file(listPath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + listPath);
            #endif
            return "";
        }
    
        std::string line;
        for (size_t i = 0; i <= listIndex; ++i) {
            if (!std::getline(file, line)) {
                return ""; // Index out of bounds
            }
        }
    
        file.close();
    #endif
    
        return line;
    }


    /**
     * @brief Splits a string into a vector of strings using a delimiter.
     *
     * This function splits the input string into multiple strings using the specified delimiter.
     *
     * @param str The input string to split.
     * @return A vector of strings containing the split values.
     */
    std::vector<std::string> stringToList(const std::string& str) {
        std::vector<std::string> result;
        
        if (str.empty()) {
            return result;
        }
        
        // Check if the input string starts and ends with '(' and ')' or '[' and ']'
        if ((str.front() == '(' && str.back() == ')') || (str.front() == '[' && str.back() == ']')) {
            // Remove the parentheses or brackets
            std::string values = str.substr(1, str.size() - 2);
            
            size_t start = 0;
            size_t end = 0;
            
            std::string item;
            // Iterate through the string manually to split by commas
            while ((end = values.find(',', start)) != std::string::npos) {
                item = values.substr(start, end - start);
                
                // Trim leading and trailing spaces
                trim(item);
                
                // Remove quotes from each token if necessary
                removeQuotes(item);
                
                result.push_back(std::move(item));
                start = end + 1;
            }
            
            // Handle the last item after the last comma
            if (start < values.size()) {
                item = values.substr(start);
                trim(item);
                removeQuotes(item);
                result.push_back(std::move(item));
            }
        }
        
        return result;
    }

    
    
    // Function to read file into a set of strings
    std::unordered_set<std::string> readSetFromFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(file_access_mutex);
        std::unordered_set<std::string> lines;
    
    #if !USING_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "r");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines;
        }
    
        constexpr size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        size_t len;
        while (fgets(buffer, BUFFER_SIZE, file)) {
            // Remove trailing newline character if it exists
            len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            lines.insert(buffer);
        }
    
        fclose(file);
    #else
        std::ifstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines;
        }
    
        std::string line;
        while (std::getline(file, line)) {
            lines.insert(std::move(line));
        }
    
        file.close();
    #endif
    
        return lines;
    }
    
    
    // Function to write a set to a file
    void writeSetToFile(const std::unordered_set<std::string>& fileSet, const std::string& filePath) {
        std::lock_guard<std::mutex> lock(file_access_mutex);
        
    #if !USING_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "w");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to open file: " + filePath);
            #endif
            return;
        }
    
        for (const auto& entry : fileSet) {
            fprintf(file, "%s\n", entry.c_str());
        }
    
        fclose(file);
    #else
        std::ofstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to open file: " + filePath);
            #endif
            return;
        }
        
        for (const auto& entry : fileSet) {
            file << entry << '\n';
        }
        
        file.close();
    #endif
    }

    
    // Function to compare two file lists and save duplicates to an output file
    void compareFilesLists(const std::string& txtFilePath1, const std::string& txtFilePath2, const std::string& outputTxtFilePath) {
        // Read files into sets
        std::unordered_set<std::string> fileSet1 = readSetFromFile(txtFilePath1);
        std::unordered_set<std::string> fileSet2 = readSetFromFile(txtFilePath2);
        std::unordered_set<std::string> duplicateFiles;
    
        // Find intersection (common elements) between the two sets
        for (const auto& entry : fileSet1) {
            if (fileSet2.find(entry) != fileSet2.end()) {
                duplicateFiles.insert(entry);
            }
        }
    
        // Write the duplicates to the output file
        writeSetToFile(duplicateFiles, outputTxtFilePath);
    }
    
    // Helper function to read a text file and process each line with a callback
    void processFileLines(const std::string& filePath, const std::function<void(const std::string&)>& callback) {
        std::lock_guard<std::mutex> lock(file_access_mutex);
        
    #if !USING_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "r");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return;
        }
    
        constexpr size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        size_t len;
        while (fgets(buffer, BUFFER_SIZE, file)) {
            // Remove newline character, if present
            len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            callback(buffer);
        }
    
        fclose(file);
    #else
        std::ifstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return;
        }
    
        std::string line;
        while (std::getline(file, line)) {
            callback(line);
        }
    #endif
    }

    
    void compareWildcardFilesLists(
        const std::string& wildcardPatternFilePath,
        const std::string& txtFilePath,
        const std::string& outputTxtFilePath
    ) {
        // 1) Find *all* files matching the wildcard (e.g. both ".../Graphics Pack/location_on.txt"
        //    and ".../Graphics Pack 2/location_on.txt")
        std::vector<std::string> wildcardFiles = getFilesListByWildcards(wildcardPatternFilePath);
    
        // 2) Read every matching file's *contents* into one big set of strings,
        //    but first skip any entry that is exactly the same as txtFilePath.
        std::unordered_set<std::string> allWildcardLines;
        std::unordered_set<std::string> thisFileLines;

        for (const auto& singlePath : wildcardFiles) {
            // Skip the case where the wildcard match is literally the same path we're comparing to:
            if (singlePath == txtFilePath) {
                continue;
            }
    
            // readSetFromFile loads every line of 'singlePath' into a set
            thisFileLines = readSetFromFile(singlePath);
            allWildcardLines.insert(thisFileLines.begin(), thisFileLines.end());
        }
    
        // 3) Read the second file (txtFilePath) line by line, checking if each line also
        //    exists in allWildcardLines.
        std::unordered_set<std::string> duplicateLines;
        processFileLines(txtFilePath, [&](const std::string& entry) {
            if (allWildcardLines.find(entry) != allWildcardLines.end()) {
                duplicateLines.insert(entry);
            }
        });
    
        // 4) Write any "duplicate" lines to outputTxtFilePath
        writeSetToFile(duplicateLines, outputTxtFilePath);
    }
}