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

namespace ult {
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
     * @param itemsList The list of stringsto be filtered.
     */
    void filterItemsList(const std::vector<std::string>& filterList, std::vector<std::string>& itemsList) {
        for (const auto& entry : filterList) {
            removeEntryFromList(entry, itemsList);
        }
    }
    
    
    // Function to read file into a vector of strings
    std::vector<std::string> readListFromFile(const std::string& filePath) {
        std::vector<std::string> lines;
    
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "r");  // Open the file in text mode
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines; // Return empty vector
        }
    
        char buffer[4096];  // Buffer to hold each line
        while (fgets(buffer, sizeof(buffer), file)) {
            // Remove newline character from the buffer if present
            buffer[strcspn(buffer, "\n")] = '\0';  // Replace newline with null terminator
            lines.emplace_back(buffer);  // Add line to vector
        }
    
        fclose(file);  // Close the file after reading
    #else
        std::ifstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines; // Return empty vector
        }
    
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(std::move(line));
        }
    
        file.close();  // Close the file
    #endif
    
        return lines;  // Return the vector of lines
    }

    
    // Function to get an entry from the list based on the index
    std::string getEntryFromListFile(const std::string& listPath, size_t listIndex) {
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(listPath.c_str(), "r");  // Open the file in text mode
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + listPath);
            #endif
            return ""; // Return an empty string if the file cannot be opened
        }
    
        std::string line;
        // Read lines until reaching the desired index
        for (size_t i = 0; i <= listIndex; ++i) {
            char buffer[4096];  // Buffer to hold each line
            if (!fgets(buffer, sizeof(buffer), file)) {
                fclose(file);  // Close the file before returning
                return ""; // Return an empty string if the index is out of bounds
            }
            line = buffer;  // Store the line (it will include a newline character if present)
        }
    
        fclose(file);  // Close the file after reading
    #else
        std::ifstream file(listPath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + listPath);
            #endif
            return ""; // Return an empty string if the file cannot be opened
        }
    
        std::string line;
        for (size_t i = 0; i <= listIndex; ++i) {
            if (!std::getline(file, line)) {
                return ""; // Return an empty string if the index is out of bounds
            }
        }
    
        file.close();  // Close the file
    #endif
    
        return line;  // Return the line at the specified index
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
                
                result.push_back(item);
                start = end + 1;
            }
            
            // Handle the last item after the last comma
            if (start < values.size()) {
                item = values.substr(start);
                trim(item);
                removeQuotes(item);
                result.push_back(item);
            }
        }
        
        return result;
    }

    
    
    // Function to read file into a set of strings
    std::unordered_set<std::string> readSetFromFile(const std::string& filePath) {
        std::unordered_set<std::string> lines;
    
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "r");  // Open the file in text mode
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines; // Return empty set if the file cannot be opened
        }
    
        char buffer[4096];  // Buffer to hold each line
        while (fgets(buffer, sizeof(buffer), file)) {
            // Remove trailing newline character if it exists
            buffer[strcspn(buffer, "\n")] = 0;  
            lines.insert(buffer);  // Insert the line into the set
        }
    
        fclose(file);  // Close the file after reading
    #else
        std::ifstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return lines; // Return empty set if the file cannot be opened
        }
    
        std::string line;
        while (std::getline(file, line)) {
            lines.insert(std::move(line));  // Insert lines into the set
        }
    
        file.close();  // Close the file
    #endif
    
        return lines;  // Return the set of lines
    }
    
    
    // Function to write a set to a file
    void writeSetToFile(const std::unordered_set<std::string>& fileSet, const std::string& filePath) {
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "w");  // Open the file in write mode
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to open file: " + filePath);
            #endif
            return;  // Exit if the file cannot be opened
        }
    
        for (const auto& entry : fileSet) {
            fprintf(file, "%s\n", entry.c_str());  // Write each entry followed by a newline
        }
    
        fclose(file);  // Close the file after writing
    #else
        std::ofstream file(filePath);
        if (file.is_open()) {
            for (const auto& entry : fileSet) {
                file << entry << '\n';  // Write each entry followed by a newline
            }
            file.close();  // Close the file after writing
        } else {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to open file: " + filePath);
            #endif
        }
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
    #if NO_FSTREAM_DIRECTIVE
        FILE* file = fopen(filePath.c_str(), "r");  // Open the file in read mode
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return;  // Exit if the file cannot be opened
        }
    
        size_t len;
        char buffer[1024];  // Buffer to store each line
        while (fgets(buffer, sizeof(buffer), file)) {
            // Remove newline character, if present
            len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            callback(buffer);  // Call the provided callback function
        }
    
        fclose(file);  // Close the file after processing
    #else
        std::ifstream file(filePath);
        if (!file.is_open()) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Unable to open file: " + filePath);
            #endif
            return;  // Exit if the file cannot be opened
        }
    
        std::string line;
        while (std::getline(file, line)) {
            callback(line);  // Call the provided callback function
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
            // Skip the case where the wildcard match is literally the same path we’re comparing to:
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
    
        // 4) Write any “duplicate” lines to outputTxtFilePath
        writeSetToFile(duplicateLines, outputTxtFilePath);
    }
}
