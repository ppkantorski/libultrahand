/********************************************************************************
 * File: path_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file implements the functions declared in path_funcs.hpp.
 *   These utility functions are focused on file and directory path manipulation
 *   for the Ultrahand Overlay project. Functionality includes creating directories,
 *   checking existence of files or paths, moving or copying files, and normalizing
 *   file system paths for cross-platform compatibility.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 * 
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2023-2025 ppkantorski
 ********************************************************************************/

#include "path_funcs.hpp"


namespace ult {
    std::atomic<bool> abortFileOp(false);
    
    size_t COPY_BUFFER_SIZE = 8192; // Back to non-const as requested

    std::atomic<int> copyPercentage(-1);
    
    std::mutex logMutex2; // Mutex for thread-safe logging (defined here, declared as extern in header)

    static std::vector<std::string> fileList;

    // RAII wrapper for FILE* to ensure proper cleanup
    class FileGuard {
    private:
        FILE* file;
    public:
        FileGuard(FILE* f) : file(f) {}
        ~FileGuard() { if (file) fclose(file); }
        FILE* get() { return file; }
        FILE* release() { FILE* f = file; file = nullptr; return f; }
    };

    /**
     * @brief Checks if a path points to a directory.
     *
     * This function checks if the specified path points to a directory.
     *
     * @param path The path to check.
     * @return True if the path is a directory, false otherwise.
     */
    bool isDirectory(const std::string& path) {
        struct stat pathStat;
        return (stat(path.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode));
    }
    
    /**
     * @brief Checks if a path points to a file.
     *
     * This function checks if the specified path points to a file.
     *
     * @param path The path to check.
     * @return True if the path is a file, false otherwise.
     */
    bool isFile(const std::string& path) {
        struct stat pathStat;
        return (stat(path.c_str(), &pathStat) == 0 && S_ISREG(pathStat.st_mode));
    }
    
    /**
     * @brief Checks if a path points to a file or directory.
     *
     * This function checks if the specified path points to either a file or a directory.
     *
     * @param path The path to check.
     * @return True if the path points to a file or directory, false otherwise.
     */
    bool isFileOrDirectory(const std::string& path) {
        struct stat pathStat;
        return (stat(path.c_str(), &pathStat) == 0);
    }
    
    // Helper function to check if directory is empty
    bool isDirectoryEmpty(const std::string& dirPath) {
        DIR* dir = opendir(dirPath.c_str());
        if (!dir) return false; // Can't open, assume not empty
        
        dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                closedir(dir);
                return false; // Found a file/directory
            }
        }
        closedir(dir);
        return true; // Empty directory
    }


    /**
     * @brief Creates a single directory if it doesn't exist.
     *
     * This function checks if the specified directory exists, and if not, it creates the directory.
     *
     * @param directoryPath The path of the directory to be created.
     */
    void createSingleDirectory(const std::string& directoryPath) {
        // mkdir is generally thread-safe, returns EEXIST if already exists
        if (mkdir(directoryPath.c_str(), 0777) != 0) {
            // Only log error if it's not EEXIST
            if (errno != EEXIST) {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Failed to create directory: " + directoryPath + " - " + std::string(strerror(errno)));
                #endif
            }
        }
    }
    
    /**
     * @brief Creates a directory and its parent directories if they don't exist.
     *
     * This function creates a directory specified by `directoryPath` and also creates any parent directories
     * if they don't exist. It handles nested directory creation.
     *
     * @param directoryPath The path of the directory to be created.
     */
    void createDirectory(const std::string& directoryPath) {
        const std::string volume = ROOT_PATH;
        std::string path = directoryPath;
    
        // Remove leading "sdmc:/" if present
        if (path.compare(0, volume.size(), volume) == 0) {
            path = path.substr(volume.size());
        }
    
        std::string parentPath = volume;
        size_t pos = 0, nextPos;
    
        // Iterate through the path and create each directory level if it doesn't exist
        while ((nextPos = path.find('/', pos)) != std::string::npos) {
            if (nextPos != pos) {
                parentPath.append(path, pos, nextPos - pos);
                parentPath += '/';
                createSingleDirectory(parentPath); // Create the parent directory
            }
            pos = nextPos + 1;
        }
    
        // Create the final directory level if it doesn't exist
        if (pos < path.size()) {
            parentPath += path.substr(pos);
            createSingleDirectory(parentPath); // Create the final directory
        }
    }
    
    #if !USING_FSTREAM_DIRECTIVE
    void writeLog(FILE* logFile, const std::string& line) {
        if (logFile) {
            std::lock_guard<std::mutex> lock(logMutex2);
            fprintf(logFile, "%s\n", line.c_str());
            fflush(logFile); // Ensure data is written immediately
        } else {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to write to log file.");
            #endif
        }
    }
    #else
    void writeLog(std::ofstream& logFile, const std::string& line) {
        if (logFile.is_open()) {
            std::lock_guard<std::mutex> lock(logMutex2);
            logFile << line << std::endl;
            logFile.flush(); // Ensure data is written immediately
        } else {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to write to log file.");
            #endif
        }
    }
    #endif
    
    /**
     * @brief Creates a text file with the specified content.
     *
     * This function creates a text file specified by `filePath` and writes the given `content` to the file.
     *
     * @param filePath The path of the text file to be created.
     * @param content The content to be written to the text file.
     */
    void createTextFile(const std::string& filePath, const std::string& content) {
        // Create parent directory first
        createDirectory(getParentDirFromPath(filePath));
        
    #if !USING_FSTREAM_DIRECTIVE
        FileGuard file(fopen(filePath.c_str(), "w"));
        if (file.get()) {
            fputs(content.c_str(), file.get());
        } else {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Error: Unable to create file " + filePath);
            #endif
        }
    #else
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << content;
            file.close();
        } else {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Error: Unable to create file " + filePath);
            #endif
        }
    #endif
    }
    
    /**
     * @brief Deletes a file or directory without using an explicit stack.
     *
     * This function deletes the file or directory specified by `pathToDelete`.
     * It uses a state-machine approach with a current directory pointer instead
     * of an explicit stack data structure.
     *
     * @param pathToDelete The path of the file or directory to be deleted.
     * @param logSource The path to the log file where deletions are recorded.
     */
    void deleteFileOrDirectory(const std::string& pathToDelete, const std::string& logSource) {
        if (pathToDelete.empty()) return;
        
        std::vector<std::string> successfulDeletions;
        const bool needsLogging = !logSource.empty();
        
        // Handle single file case
        const bool pathIsFile = pathToDelete.back() != '/';
        
        if (pathIsFile) {
            if (isFile(pathToDelete)) {
                if (remove(pathToDelete.c_str()) == 0) {
                    if (needsLogging) {
                        successfulDeletions.push_back(pathToDelete);
                    }
                } else {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Failed to delete file: " + pathToDelete);
                    #endif
                }
            }
            
            // Write log for single file deletion if needed
            if (needsLogging && !successfulDeletions.empty()) {
    #if !USING_FSTREAM_DIRECTIVE
                createDirectory(getParentDirFromPath(logSource));
                if (FILE* logFile = fopen(logSource.c_str(), "a")) {
                    writeLog(logFile, pathToDelete);
                    fclose(logFile);
                }
                #if USING_LOGGING_DIRECTIVE
                else {
                    if (!disableLogging)
                        logMessage("Failed to open source log file: " + logSource);
                }
                #endif
    #else
                createDirectory(getParentDirFromPath(logSource));
                std::ofstream logSourceFile(logSource, std::ios::app);
                if (logSourceFile.is_open()) {
                    writeLog(logSourceFile, pathToDelete);
                    logSourceFile.close();
                }
                #if USING_LOGGING_DIRECTIVE
                else {
                    if (!disableLogging)
                        logMessage("Failed to open source log file: " + logSource);
                }
                #endif
    #endif
            }
            return;
        }
        
        // Directory deletion - stackless approach
        // Normalize root path
        std::string rootPath = pathToDelete;
        if (rootPath.back() != '/') rootPath += '/';
        
        std::string currentDir = rootPath;
        struct stat st;
        
        while (true) {
            DIR* directory = opendir(currentDir.c_str());
            if (!directory) {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Failed to open directory: " + currentDir);
                #endif
                
                // Can't open - either deleted or error. Try to move up.
                if (currentDir == rootPath) break; // Done with root
                
                size_t pos = currentDir.find_last_of('/', currentDir.length() - 2);
                if (pos != std::string::npos && pos >= rootPath.length() - 1) {
                    currentDir = currentDir.substr(0, pos + 1);
                } else {
                    break;
                }
                continue;
            }
            
            bool foundSubdir = false;
            bool foundFile = false;
            dirent* entry;
            
            while ((entry = readdir(directory)) != nullptr) {
                const char* name = entry->d_name;
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
                
                std::string childPath = currentDir + name;
                if (stat(childPath.c_str(), &st) != 0) continue;
                
                if (S_ISDIR(st.st_mode)) {
                    // Found a subdirectory - dive into it
                    currentDir = childPath + "/";
                    foundSubdir = true;
                    break;
                } else if (S_ISREG(st.st_mode)) {
                    // Delete file immediately
                    if (remove(childPath.c_str()) == 0) {
                        if (needsLogging) {
                            successfulDeletions.push_back(childPath);
                        }
                        foundFile = true;
                    } else {
                        #if USING_LOGGING_DIRECTIVE
                        if (!disableLogging)
                            logMessage("Failed to delete file: " + childPath);
                        #endif
                    }
                } else {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Unknown file type: " + childPath);
                    #endif
                }
            }
            closedir(directory);
            
            if (foundSubdir) {
                // Continue with the subdirectory
                continue;
            }
            
            if (foundFile) {
                // We deleted files, re-scan this directory to check if empty
                continue;
            }
            
            // Directory is empty - delete it and move up
            if (rmdir(currentDir.c_str()) != 0) {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Failed to delete directory: " + currentDir);
                #endif
            }
            
            // Check if we just deleted the root directory
            if (currentDir == rootPath) break;
            
            // Move up, but NEVER go above rootPath
            size_t pos = currentDir.find_last_of('/', currentDir.length() - 2);
            if (pos != std::string::npos && pos >= rootPath.length() - 1) {
                currentDir = currentDir.substr(0, pos + 1);
            } else {
                break; // Safety: don't go above root
            }
        }
        
        // Batch write all successful deletions to log file at the end
        if (needsLogging && !successfulDeletions.empty()) {
    #if !USING_FSTREAM_DIRECTIVE
            createDirectory(getParentDirFromPath(logSource));
            if (FILE* logFile = fopen(logSource.c_str(), "a")) {
                for (const auto& deletedPath : successfulDeletions) {
                    writeLog(logFile, deletedPath);
                }
                fclose(logFile);
            }
            #if USING_LOGGING_DIRECTIVE
            else {
                if (!disableLogging)
                    logMessage("Failed to open source log file: " + logSource);
            }
            #endif
    #else
            createDirectory(getParentDirFromPath(logSource));
            std::ofstream logSourceFile(logSource, std::ios::app);
            if (logSourceFile.is_open()) {
                for (const auto& deletedPath : successfulDeletions) {
                    writeLog(logSourceFile, deletedPath);
                }
                logSourceFile.close();
            }
            #if USING_LOGGING_DIRECTIVE
            else {
                if (!disableLogging)
                    logMessage("Failed to open source log file: " + logSource);
            }
            #endif
    #endif
        }
    }
    
    /**
     * @brief Deletes files or directories that match a specified pattern.
     *
     * This function deletes files or directories specified by `pathPattern` by matching against a pattern.
     * It identifies files or directories that match the pattern and deletes them.
     * Files/directories in the filterSet will be skipped.
     *
     * @param pathPattern The pattern used to match and delete files or directories.
     * @param logSource Optional log source identifier.
     * @param filterSet Optional set of paths to exclude from deletion (nullptr to delete all).
     */
    void deleteFileOrDirectoryByPattern(const std::string& pathPattern, const std::string& logSource, 
        const std::unordered_set<std::string>* filterSet) {
        
        fileList = getFilesListByWildcards(pathPattern);
        
        for (auto& path : fileList) {
            // Check filter before deleting
            const bool shouldDelete = !filterSet || filterSet->find(path) == filterSet->end();
            
            if (shouldDelete) {
                deleteFileOrDirectory(path, logSource);
            }
            path = "";
        }
        
        fileList.clear();
        fileList.shrink_to_fit();
    }

    // Helper function to reverse a log file safely
    void reverseLogFile(const std::string& logFilePath) {
        // First pass: count lines and build offset index
        FILE* file = fopen(logFilePath.c_str(), "rb");
        if (!file) return;
        
        setvbuf(file, nullptr, _IOFBF, 8192);
        
        std::vector<long> lineOffsets;
        
        lineOffsets.push_back(0); // First line starts at offset 0
        
        static constexpr size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        long currentOffset = 0;
        
        while (fgets(buffer, BUFFER_SIZE, file)) {
            currentOffset = ftell(file);
            lineOffsets.push_back(currentOffset);
        }
        
        // Remove the last offset (it's past EOF)
        if (!lineOffsets.empty()) lineOffsets.pop_back();
        
        if (lineOffsets.empty()) {
            fclose(file);
            return;
        }
        
        // Create temp file
        std::string tempPath = logFilePath + ".tmp";
        FILE* outFile = fopen(tempPath.c_str(), "wb");
        if (!outFile) {
            fclose(file);
            return;
        }
        
        setvbuf(outFile, nullptr, _IOFBF, 8192);
        
        // Second pass: write lines in reverse order
        for (auto it = lineOffsets.rbegin(); it != lineOffsets.rend(); ++it) {
            fseek(file, *it, SEEK_SET);
            if (fgets(buffer, BUFFER_SIZE, file)) {
                // Remove trailing newline if present
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';
                
                fprintf(outFile, "%s\n", buffer);
            }
        }
        
        fflush(outFile);
        fclose(outFile);
        fclose(file);
        
        // Replace original with temp
        remove(logFilePath.c_str());
        rename(tempPath.c_str(), logFilePath.c_str());
    }
    
    void moveDirectory(const std::string& sourcePath, const std::string& destinationPath,
                       const std::string& logSource, const std::string& logDestination) {
    
        struct stat sourceInfo;
        if (stat(sourcePath.c_str(), &sourceInfo) != 0) {
    #if USING_LOGGING_DIRECTIVE
            if (!disableLogging) logMessage("Source directory doesn't exist: " + sourcePath);
    #endif
            return;
        }
    
        if (mkdir(destinationPath.c_str(), 0777) != 0 && errno != EEXIST) {
    #if USING_LOGGING_DIRECTIVE
            if (!disableLogging) logMessage("Failed to create destination directory: " + destinationPath);
    #endif
            return;
        }
    
        bool needsLogging = !logSource.empty() || !logDestination.empty();
        
        // Open log files once at the start
        FILE* logSrcFile = nullptr;
        FILE* logDestFile = nullptr;
    
        if (needsLogging && !logSource.empty()) {
            createDirectory(getParentDirFromPath(logSource));
            logSrcFile = fopen(logSource.c_str(), "w");
            if (logSrcFile) setvbuf(logSrcFile, nullptr, _IOFBF, 8192);
        }
        if (needsLogging && !logDestination.empty()) {
            createDirectory(getParentDirFromPath(logDestination));
            logDestFile = fopen(logDestination.c_str(), "w");
            if (logDestFile) setvbuf(logDestFile, nullptr, _IOFBF, 8192);
        }
    
        // Recursive helper that moves files/dirs and logs in post-order (deepest first)
        std::function<void(const std::string&, const std::string&)> moveRecursive = 
            [&](const std::string& srcPath, const std::string& dstPath) {
            
            DIR* dir = opendir(srcPath.c_str());
            if (!dir) {
    #if USING_LOGGING_DIRECTIVE
                if (!disableLogging) logMessage("Failed to open source directory: " + srcPath);
    #endif
                return;
            }
    
            //bool hasContent = false;
            dirent* entry;
            
            while ((entry = readdir(dir)) != nullptr) {
                const char* name = entry->d_name;
                if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
                    continue;
                }
    
                //hasContent = true;
    
                // Build full paths
                std::string fullSrcPath = srcPath;
                if (!fullSrcPath.empty() && fullSrcPath.back() != '/') fullSrcPath += '/';
                fullSrcPath += name;
    
                std::string fullDstPath = dstPath;
                if (!fullDstPath.empty() && fullDstPath.back() != '/') fullDstPath += '/';
                fullDstPath += name;
    
                // Use d_type if available
    #ifdef _DIRENT_HAVE_D_TYPE
                if (entry->d_type == DT_DIR) {
                    // Create destination directory
                    if (mkdir(fullDstPath.c_str(), 0777) != 0 && errno != EEXIST) {
    #if USING_LOGGING_DIRECTIVE
                        if (!disableLogging) logMessage("Failed to create destination directory: " + fullDstPath);
    #endif
                        continue;
                    }
    
                    // Recurse into subdirectory
                    moveRecursive(fullSrcPath, fullDstPath);
    
                    // Remove the now-empty source directory
                    if (rmdir(fullSrcPath.c_str()) != 0) {
    #if USING_LOGGING_DIRECTIVE
                        if (!disableLogging) logMessage("Failed to delete source directory: " + fullSrcPath);
    #endif
                    }
    
                    // Log after processing (post-order)
                    if (needsLogging) {
                        if (logSrcFile) {
                            fprintf(logSrcFile, "%s/\n", fullSrcPath.c_str());
                        }
                        if (logDestFile) {
                            fprintf(logDestFile, "%s/\n", fullDstPath.c_str());
                        }
                    }
                    continue;
                } else if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_REG) {
                    // Skip non-regular files (symlinks, devices, etc.)
                    continue;
                }
    #endif
                // Move regular file
                remove(fullDstPath.c_str());
                if (rename(fullSrcPath.c_str(), fullDstPath.c_str()) == 0) {
                    if (needsLogging) {
                        if (logSrcFile) {
                            fprintf(logSrcFile, "%s\n", fullSrcPath.c_str());
                        }
                        if (logDestFile) {
                            fprintf(logDestFile, "%s\n", fullDstPath.c_str());
                        }
                    }
                } else {
    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging) logMessage("Failed to move: " + fullSrcPath);
    #endif
                }
            }
            closedir(dir);
        };
    
        // Start the recursive move
        moveRecursive(sourcePath, destinationPath);
    
        // Close log files
        if (logSrcFile) fclose(logSrcFile);
        if (logDestFile) fclose(logDestFile);
    
        // Remove the top-level source directory
        if (rmdir(sourcePath.c_str()) != 0) {
    #if USING_LOGGING_DIRECTIVE
            if (!disableLogging) logMessage("Failed to delete source directory: " + sourcePath);
    #endif
        }
    
        // Reverse logs to get shallowest-first order
        if (needsLogging) {
            if (!logSource.empty()) reverseLogFile(logSource);
            if (!logDestination.empty()) reverseLogFile(logDestination);
        }
    }

    
    bool moveFile(const std::string& sourcePath,
                  const std::string& destinationPath,
                  const std::string& logSource,
                  const std::string& logDestination) {
        if (!isFileOrDirectory(sourcePath)) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Source file doesn't exist or is not a regular file: " + sourcePath);
            #endif
            return false;
        }
    
        std::string finalDestPath;
        bool moveSuccess = false;
    
        if (destinationPath.back() == '/') {
            // Destination is a directory - construct full destination path
            if (!isDirectory(destinationPath)) {
                createDirectory(destinationPath);
            }
            
            finalDestPath = destinationPath + getFileName(sourcePath);
            remove(finalDestPath.c_str());
            
            if (rename(sourcePath.c_str(), finalDestPath.c_str()) == 0) {
                moveSuccess = true;
            } else {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Failed to move file to directory: " + sourcePath);
                #endif
            }
        } else {
            // Destination is a file path - directly rename the file
            finalDestPath = destinationPath;
            remove(finalDestPath.c_str());
            createDirectory(getParentDirFromPath(finalDestPath));
            
            if (rename(sourcePath.c_str(), finalDestPath.c_str()) == 0) {
                moveSuccess = true;
            } else {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging) {
                    logMessage("Failed to move file: " + sourcePath + " -> " + finalDestPath);
                    logMessage("Error: " + std::string(strerror(errno)));
                }
                #endif
            }
        }
    
        // Only write to log files if the move was successful
        // This is the key optimization - logs are only opened when actually needed!
        if (moveSuccess) {
    #if !USING_FSTREAM_DIRECTIVE
            if (!logSource.empty()) {
                createDirectory(getParentDirFromPath(logSource));
                if (FILE* logFile = fopen(logSource.c_str(), "a")) {
                    writeLog(logFile, sourcePath);
                    fclose(logFile);
                }
                #if USING_LOGGING_DIRECTIVE
                else {
                    if (!disableLogging)
                        logMessage("Failed to open source log file: " + logSource);
                }
                #endif
            }
    
            if (!logDestination.empty()) {
                createDirectory(getParentDirFromPath(logDestination));
                if (FILE* logFile = fopen(logDestination.c_str(), "a")) {
                    writeLog(logFile, finalDestPath);
                    fclose(logFile);
                }
                #if USING_LOGGING_DIRECTIVE
                else {
                    if (!disableLogging)
                        logMessage("Failed to open destination log file: " + logDestination);
                }
                #endif
            }
    #else
            if (!logSource.empty()) {
                createDirectory(getParentDirFromPath(logSource));
                std::ofstream logSourceFile(logSource, std::ios::app);
                if (logSourceFile.is_open()) {
                    writeLog(logSourceFile, sourcePath);
                    logSourceFile.close();
                }
                #if USING_LOGGING_DIRECTIVE
                else {
                    if (!disableLogging)
                        logMessage("Failed to open source log file: " + logSource);
                }
                #endif
            }
    
            if (!logDestination.empty()) {
                createDirectory(getParentDirFromPath(logDestination));
                std::ofstream logDestFile(logDestination, std::ios::app);
                if (logDestFile.is_open()) {
                    writeLog(logDestFile, finalDestPath);
                    logDestFile.close();
                }
                #if USING_LOGGING_DIRECTIVE
                else {
                    if (!disableLogging)
                        logMessage("Failed to open destination log file: " + logDestination);
                }
                #endif
            }
    #endif
        }
    
        return moveSuccess;
    }
    
    /**
     * @brief Moves a file or directory to a new destination.
     *
     * This function moves a file or directory from the `sourcePath` to the `destinationPath`. It can handle both
     * files and directories and ensures that the destination directory exists before moving.
     *
     * @param sourcePath The path of the source file or directory.
     * @param destinationPath The path of the destination where the file or directory will be moved.
     */
    void moveFileOrDirectory(const std::string& sourcePath, const std::string& destinationPath,
        const std::string& logSource, const std::string& logDestination) {
        if (sourcePath.back() == '/' && destinationPath.back() == '/') {
            moveDirectory(sourcePath, destinationPath, logSource, logDestination);
        } else {
            moveFile(sourcePath, destinationPath, logSource, logDestination);
        }
    }
    
    
    /**
     * @brief Moves files or directories matching a specified pattern to a destination directory.
     *
     * This function identifies files or directories that match the `sourcePathPattern` and moves them to the `destinationPath`.
     * It processes each matching entry in the source directory pattern and moves them to the specified destination.
     * Files/directories in the filterSet will be skipped.
     *
     * @param sourcePathPattern The pattern used to match files or directories to be moved.
     * @param destinationPath The destination directory where matching files or directories will be moved.
     * @param logSource Optional log source identifier.
     * @param logDestination Optional log destination identifier.
     * @param filterSet Optional set of paths to exclude from moving (nullptr to move all).
     */
    void moveFilesOrDirectoriesByPattern(const std::string& sourcePathPattern, const std::string& destinationPath,
        const std::string& logSource, const std::string& logDestination, const std::unordered_set<std::string>* filterSet) {
        
        fileList = getFilesListByWildcards(sourcePathPattern);
        
        std::string folderName, fixedDestinationPath;
        
        // Iterate through the file list
        for (std::string& sourceFileOrDirectory : fileList) {
            // Check filter before moving
            const bool shouldMove = !filterSet || filterSet->find(sourceFileOrDirectory) == filterSet->end();
            
            if (shouldMove) {
                // if sourceFile is a file
                if (!isDirectory(sourceFileOrDirectory)) {
                    moveFileOrDirectory(sourceFileOrDirectory, destinationPath, logSource, logDestination);
                } else if (isDirectory(sourceFileOrDirectory)) {
                    // if sourceFile is a directory
                    folderName = getNameFromPath(sourceFileOrDirectory);
                    fixedDestinationPath = destinationPath + folderName + "/";
                    moveFileOrDirectory(sourceFileOrDirectory, fixedDestinationPath, logSource, logDestination);
                }
            }
            sourceFileOrDirectory = "";
        }
    
        fileList.clear();
        fileList.shrink_to_fit();
    }
    
    /**
     * @brief Copies a single file from the source path to the destination path.
     *
     * This function copies a single file specified by `fromFile` to the location specified by `toFile`.
     *
     * @param fromFile The path of the source file to be copied.
     * @param toFile The path of the destination where the file will be copied.
     */
    void copySingleFile(const std::string& fromFile, const std::string& toFile, long long& totalBytesCopied, 
                        const long long totalSize, const std::string& logSource, const std::string& logDestination) {
        static constexpr size_t maxRetries = 10;
        const size_t bufferSize = COPY_BUFFER_SIZE;
        
        // Create destination directory once
        createDirectory(getParentDirFromPath(toFile));
        
        // Use heap allocation for the buffer to avoid stack overflow with large buffer sizes
        std::unique_ptr<char[]> buffer(new char[bufferSize]);
    
    #if !USING_FSTREAM_DIRECTIVE
        FILE* srcFile = nullptr;
        FILE* destFile = nullptr;
        
        // Retry loop for file opening
        for (size_t retryCount = 0; retryCount <= maxRetries; ++retryCount) {
            srcFile = fopen(fromFile.c_str(), "rb");
            if (!srcFile) {
                if (retryCount == maxRetries) {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Error: Failed to open source file after " + std::to_string(maxRetries) + " retries");
                    #endif
                    return;
                }
                continue;
            }
            
            destFile = fopen(toFile.c_str(), "wb");
            if (!destFile) {
                fclose(srcFile);
                if (retryCount == maxRetries) {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Error: Failed to open destination file after " + std::to_string(maxRetries) + " retries");
                    #endif
                    return;
                }
                continue;
            }
            
            break; // Both files opened successfully
        }
        
        // RAII cleanup
        FileGuard srcGuard(srcFile);
        FileGuard destGuard(destFile);
        
        // Main copy loop - optimized for performance
        size_t bytesRead;
        char* bufferPtr = buffer.get();
        size_t remainingBytes, written;
        while ((bytesRead = fread(bufferPtr, 1, bufferSize, srcFile)) > 0) {
            if (abortFileOp.load(std::memory_order_acquire)) {
                remove(toFile.c_str());
                copyPercentage.store(-1, std::memory_order_release);
                return;
            }
            
            // Write all bytes - handle partial writes
            char* writePtr = bufferPtr;
            remainingBytes = bytesRead;
            
            while (remainingBytes > 0) {
                written = fwrite(writePtr, 1, remainingBytes, destFile);
                if (written == 0) {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Error writing to destination file");
                    #endif
                    remove(toFile.c_str());
                    copyPercentage.store(-1, std::memory_order_release);
                    return;
                }
                writePtr += written;
                remainingBytes -= written;
            }
            
            totalBytesCopied += bytesRead;
            if (totalSize > 0) {
                copyPercentage.store(static_cast<int>(100 * totalBytesCopied / totalSize), std::memory_order_release);
            }
        }
        
        // Check for read errors
        if (ferror(srcFile)) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Error reading from source file");
            #endif
            remove(toFile.c_str());
            copyPercentage.store(-1, std::memory_order_release);
            return;
        }
    
    #else
        std::ifstream srcFile;
        std::ofstream destFile;
        
        // Retry loop for file opening
        for (size_t retryCount = 0; retryCount <= maxRetries; ++retryCount) {
            srcFile.open(fromFile, std::ios::binary);
            destFile.open(toFile, std::ios::binary);
            
            if (srcFile.is_open() && destFile.is_open()) {
                break;
            }
            
            srcFile.close();
            destFile.close();
            
            if (retryCount == maxRetries) {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Error: Failed to open files after " + std::to_string(maxRetries) + " retries");
                #endif
                return;
            }
        }
        
        // Main copy loop
        char* bufferPtr = buffer.get();
        while (srcFile.read(bufferPtr, bufferSize) || srcFile.gcount() > 0) {
            if (abortFileOp.load(std::memory_order_acquire)) {
                srcFile.close();
                destFile.close();
                remove(toFile.c_str());
                copyPercentage.store(-1, std::memory_order_release);
                return;
            }
            
            std::streamsize bytesToWrite = srcFile.gcount();
            destFile.write(bufferPtr, bytesToWrite);
            
            if (!destFile.good()) {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Error writing to destination file");
                #endif
                srcFile.close();
                destFile.close();
                remove(toFile.c_str());
                copyPercentage.store(-1, std::memory_order_release);
                return;
            }
            
            totalBytesCopied += bytesToWrite;
            if (totalSize > 0) {
                copyPercentage.store(static_cast<int>(100 * totalBytesCopied / totalSize), std::memory_order_release);
            }
        }
        
        srcFile.close();
        destFile.close();
    #endif
        
        // Only open and write to log files if they're needed - this is the key optimization!
        if (!logSource.empty()) {
            createDirectory(getParentDirFromPath(logSource));
            if (FILE* logFile = fopen(logSource.c_str(), "a")) {
                writeLog(logFile, fromFile);
                fclose(logFile);
            }
            #if USING_LOGGING_DIRECTIVE
            else {
                if (!disableLogging)
                    logMessage("Failed to open source log file: " + logSource);
            }
            #endif
        }
        
        if (!logDestination.empty()) {
            createDirectory(getParentDirFromPath(logDestination));
            if (FILE* logFile = fopen(logDestination.c_str(), "a")) {
                writeLog(logFile, toFile);
                fclose(logFile);
            }
            #if USING_LOGGING_DIRECTIVE
            else {
                if (!disableLogging)
                    logMessage("Failed to open destination log file: " + logDestination);
            }
            #endif
        }
    }

    
    /**
     * Recursively calculates the total size of the given file or directory.
     * @param path The path to the file or directory.
     * @return The total size in bytes of all files within the directory or the size of a file.
     */
    long long getTotalSize(const std::string& path) {
        struct stat statbuf;
        if (lstat(path.c_str(), &statbuf) != 0) {
            return 0; // Cannot stat file
        }
    
        if (S_ISREG(statbuf.st_mode)) {
            return statbuf.st_size;
        }
    
        if (S_ISDIR(statbuf.st_mode)) {
            long long totalSize = 0;
    
            DIR* dir = opendir(path.c_str());
            if (!dir) {
                return 0; // Cannot open directory
            }
    
            // Calculate base path with slash for efficient concatenation
            bool needsSlash = (!path.empty() && path.back() != '/');
            
            dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                // Fast check for "." and ".."
                if (entry->d_name[0] == '.') {
                    if (entry->d_name[1] == '\0' || 
                        (entry->d_name[1] == '.' && entry->d_name[2] == '\0')) {
                        continue;
                    }
                }
                
                // Build the full path
                std::string fullPath = path;
                if (needsSlash) fullPath += '/';
                fullPath += entry->d_name;
    
                // Use d_type if available (much faster than lstat)
    #ifdef _DIRENT_HAVE_D_TYPE
                if (entry->d_type != DT_UNKNOWN) {
                    if (entry->d_type == DT_REG) {
                        // Regular file - get size
                        if (lstat(fullPath.c_str(), &statbuf) == 0) {
                            totalSize += statbuf.st_size;
                        }
                    } else if (entry->d_type == DT_DIR) {
                        // Directory - recurse
                        totalSize += getTotalSize(fullPath);
                    }
                    // Ignore other types (symlinks, devices, etc.)
                    continue;
                }
    #endif
                // Fallback to lstat if d_type unavailable or unknown
                if (lstat(fullPath.c_str(), &statbuf) != 0) {
                    continue; // Cannot stat file, skip it
                }
    
                if (S_ISREG(statbuf.st_mode)) {
                    totalSize += statbuf.st_size;
                } else if (S_ISDIR(statbuf.st_mode)) {
                    totalSize += getTotalSize(fullPath);
                }
            }
            closedir(dir);
    
            return totalSize;
        }
    
        return 0; // Non-file/directory entries
    }
        
    /**
     * @brief Copies a file or directory from the source path to the destination path.
     *
     * This function copies a file or directory specified by `fromFileOrDirectory` to the location specified by `toFileOrDirectory`.
     * If the source is a regular file, it copies the file to the destination. If the source is a directory, it recursively copies
     * the entire directory and its contents to the destination.
     *
     * @param fromPath The path of the source file or directory to be copied.
     * @param toPath The path of the destination where the file or directory will be copied.
     */
    void copyFileOrDirectory(const std::string& fromPath, const std::string& toPath, long long* totalBytesCopied, long long totalSize,
        const std::string& logSource, const std::string& logDestination) {
        bool isTopLevelCall = totalBytesCopied == nullptr;
        long long tempBytesCopied = 0;
    
        bool needsLogging = !logSource.empty() || !logDestination.empty();
    
        if (isTopLevelCall) {
            totalSize = getTotalSize(fromPath);
            totalBytesCopied = &tempBytesCopied;
        }
    
        // Check abort flag
        if (abortFileOp.load(std::memory_order_acquire)) {
            copyPercentage.store(-1, std::memory_order_release);
            return;
        }
    
        struct stat fromStat;
        if (stat(fromPath.c_str(), &fromStat) != 0) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Failed to get stat of " + fromPath);
            #endif
            return;
        }
    
        if (S_ISREG(fromStat.st_mode)) {
            // It's a regular file
            if (toPath.back() == '/') {
                // toPath is a directory, copy file into it
                std::string filename = getNameFromPath(fromPath);
                std::string toFilePath = toPath + filename;
                
                createDirectory(toPath);
                copySingleFile(fromPath, toFilePath, *totalBytesCopied, totalSize, logSource, logDestination);
            } else {
                // toPath is a file path, copy directly
                createDirectory(getParentDirFromPath(toPath));
                copySingleFile(fromPath, toPath, *totalBytesCopied, totalSize, logSource, logDestination);
            }
    
            if (totalSize > 0) {
                copyPercentage.store(static_cast<int>((*totalBytesCopied * 100) / totalSize), std::memory_order_release);
            }
        } else if (S_ISDIR(fromStat.st_mode)) {
            // It's a directory
            std::string actualToPath = toPath;
            
            // Ensure toPath ends with /
            if (actualToPath.back() != '/') {
                actualToPath += '/';
            }
            
            // Create the destination directory
            createDirectory(actualToPath);
    
            // Open and iterate through the directory
            DIR* dir = opendir(fromPath.c_str());
            if (!dir) {
                #if USING_LOGGING_DIRECTIVE
                if (!disableLogging)
                    logMessage("Failed to open directory: " + fromPath);
                #endif
                return;
            }
    
            bool hasContent = false;
            dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (abortFileOp.load(std::memory_order_acquire)) {
                    closedir(dir);
                    copyPercentage.store(-1, std::memory_order_release);
                    return;
                }
    
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                
                hasContent = true;
                
                // Build paths for recursion - ensure fromPath has trailing slash
                std::string fromWithSlash = fromPath;
                if (fromWithSlash.back() != '/') {
                    fromWithSlash += '/';
                }
                
                std::string subFromPath = fromWithSlash + entry->d_name;
                std::string subToPath = actualToPath + entry->d_name;
                
                // Check if this entry is a directory and add trailing slash if so
                struct stat entryStat;
                if (stat(subFromPath.c_str(), &entryStat) == 0 && S_ISDIR(entryStat.st_mode)) {
                    subToPath += '/';
                }
                
                // Recursive call - this will write logs in DFS order (deepest first)
                copyFileOrDirectory(subFromPath, subToPath, totalBytesCopied, totalSize, logSource, logDestination);
            }
            closedir(dir);
            
            // Log this directory AFTER processing its contents (post-order traversal)
            if (hasContent && needsLogging) {
                std::string logFromPath = fromPath;
                if (logFromPath.back() != '/') logFromPath += '/';
                
                std::string logToPath = actualToPath;
                
    #if !USING_FSTREAM_DIRECTIVE
                if (!logSource.empty()) {
                    createDirectory(getParentDirFromPath(logSource));
                    if (FILE* logFile = fopen(logSource.c_str(), "a")) {
                        writeLog(logFile, logFromPath);
                        fclose(logFile);
                    }
                    #if USING_LOGGING_DIRECTIVE
                    else {
                        if (!disableLogging)
                            logMessage("Failed to open source log file: " + logSource);
                    }
                    #endif
                }
    
                if (!logDestination.empty()) {
                    createDirectory(getParentDirFromPath(logDestination));
                    if (FILE* logFile = fopen(logDestination.c_str(), "a")) {
                        writeLog(logFile, logToPath);
                        fclose(logFile);
                    }
                    #if USING_LOGGING_DIRECTIVE
                    else {
                        if (!disableLogging)
                            logMessage("Failed to open destination log file: " + logDestination);
                    }
                    #endif
                }
    #else
                if (!logSource.empty()) {
                    createDirectory(getParentDirFromPath(logSource));
                    std::ofstream logSourceFile(logSource, std::ios::app);
                    if (logSourceFile.is_open()) {
                        writeLog(logSourceFile, logFromPath);
                        logSourceFile.close();
                    }
                    #if USING_LOGGING_DIRECTIVE
                    else {
                        if (!disableLogging)
                            logMessage("Failed to open source log file: " + logSource);
                    }
                    #endif
                }
    
                if (!logDestination.empty()) {
                    createDirectory(getParentDirFromPath(logDestination));
                    std::ofstream logDestFile(logDestination, std::ios::app);
                    if (logDestFile.is_open()) {
                        writeLog(logDestFile, logToPath);
                        logDestFile.close();
                    }
                    #if USING_LOGGING_DIRECTIVE
                    else {
                        if (!disableLogging)
                            logMessage("Failed to open destination log file: " + logDestination);
                    }
                    #endif
                }
    #endif
            }
        }
    
        if (isTopLevelCall) {
            copyPercentage.store(100, std::memory_order_release);
            
            // Reverse the log files to match original behavior (shallowest directories first)
            if (needsLogging) {
                if (!logSource.empty()) {
                    reverseLogFile(logSource);
                }
                if (!logDestination.empty()) {
                    reverseLogFile(logDestination);
                }
            }
        }
    }


    
    /**
     * @brief Copies files or directories matching a specified pattern to a destination directory.
     *
     * This function identifies files or directories that match the `sourcePathPattern` and copies them to the `toDirectory`.
     * It processes each matching entry in the source directory pattern and copies them to the specified destination.
     * Files/directories in the filterSet will be skipped.
     *
     * @param sourcePathPattern The pattern used to match files or directories to be copied.
     * @param toDirectory The destination directory where matching files or directories will be copied.
     * @param logSource Optional log source identifier.
     * @param logDestination Optional log destination identifier.
     * @param filterSet Optional set of paths to exclude from copying (nullptr to copy all).
     */
    void copyFileOrDirectoryByPattern(const std::string& sourcePathPattern, const std::string& toDirectory,
        const std::string& logSource, const std::string& logDestination, const std::unordered_set<std::string>* filterSet) {
        
        fileList = getFilesListByWildcards(sourcePathPattern);
        
        // Calculate total size only for files that will actually be copied
        long long totalSize = 0;
        for (const std::string& path : fileList) {
            const bool shouldCopy = !filterSet || filterSet->find(path) == filterSet->end();
            if (shouldCopy) {
                totalSize += getTotalSize(path);
            }
        }
    
        long long totalBytesCopied = 0;
        for (std::string& sourcePath : fileList) {
            // Check filter before copying
            const bool shouldCopy = !filterSet || filterSet->find(sourcePath) == filterSet->end();
            
            if (shouldCopy) {
                copyFileOrDirectory(sourcePath, toDirectory, &totalBytesCopied, totalSize, logSource, logDestination);
            }
            sourcePath = "";
        }
        
        fileList.clear();
        fileList.shrink_to_fit();
    }


    
    /**
     * @brief Mirrors the deletion of files from a source directory to a target directory.
     *
     * This function mirrors the deletion of files from a `sourcePath` directory to a `targetPath` directory.
     * It deletes corresponding files in the `targetPath` that match the source directory structure.
     *
     * @param sourcePath The path of the source directory.
     * @param targetPath The path of the target directory where files will be mirrored and deleted.
     *                   Default is "sdmc:/". You can specify a different target path if needed.
     */
    void mirrorFiles(const std::string& sourcePath, const std::string targetPath, const std::string mode) {
        fileList = getFilesListFromDirectory(sourcePath);
        std::string updatedPath;
        long long totalSize = 0;
        long long totalBytesCopied = 0;
        
        if (mode == "copy") {
            // Calculate total size for progress tracking
            for (const auto& path : fileList) {
                if (path != targetPath + path.substr(sourcePath.size())) {
                    totalSize += getTotalSize(path);
                }
            }
        }
        
        for (auto& path : fileList) {
            // Generate the corresponding path in the target directory by replacing the source path
            updatedPath = targetPath + path.substr(sourcePath.size());
            //logMessage("mirror-delete: "+path+" "+updatedPath);
            if (mode == "delete")
                deleteFileOrDirectory(updatedPath);
            else if (mode == "copy") {
                if (path != updatedPath)
                    copyFileOrDirectory(path, updatedPath, &totalBytesCopied, totalSize);
            }
            path = "";
        }
        //fileList.clear();
        fileList.clear();
        fileList.shrink_to_fit();
    }
    
    /**
     * @brief For each match of the wildcard pattern, creates an empty text file
     *        named basename.txt inside the output directory.
     *        Uses FILE* if !USING_FSTREAM_DIRECTIVE is defined, otherwise uses std::ofstream.
     *
     * @param wildcardPattern A path with a wildcard, such as /some/path/[*].
     *                        Each match results in a file named after the basename.
     * @param outputDir       Directory where the output files will be written.
     *                        Created if it doesn't already exist.
     */
    void createFlagFiles(const std::string& wildcardPattern, const std::string& outputDir) {
        // 1) Gather all matches from the wildcard pattern
        fileList = ult::getFilesListByWildcards(wildcardPattern);
        if (fileList.empty()) {
            return; // No matches, nothing to do
        }
    
        // 2) Ensure the output directory exists
        createDirectory(outputDir);
    
        // 3) Generate empty .txt files for each matched path
        std::string outputPrefix = outputDir;
        if (!outputPrefix.empty() && outputPrefix.back() != '/')
            outputPrefix.push_back('/');
        
        std::string baseName, outFile;
        for (auto& fullPath : fileList) {
            baseName = ult::getNameFromPath(fullPath);
            if (baseName.empty()) {
                fullPath = "";
                continue;
            }
    
            outFile = outputPrefix + baseName;
    
        #if !USING_FSTREAM_DIRECTIVE
            FileGuard fp(std::fopen(outFile.c_str(), "wb"));
            // File automatically closed by FileGuard destructor
        #else
            std::ofstream ofs(outFile, std::ios::binary | std::ios::trunc);
            ofs.close();
        #endif
            fullPath = "";
        }
        fileList.clear();
        fileList.shrink_to_fit();
    }
        
    /**
     * @brief Removes all files starting with "._" from a directory and its subdirectories.
     *
     * This function recursively scans the specified directory and removes all files
     * whose names start with "._" (commonly macOS metadata files). It processes
     * all subdirectories recursively.
     *
     * @param sourcePath The path of the directory to clean.
     */
    void dotCleanDirectory(const std::string& sourcePath) {
        DIR* dir = opendir(sourcePath.c_str());
        if (!dir) {
            #if USING_LOGGING_DIRECTIVE
            if (!disableLogging)
                logMessage("Path is not a directory or cannot open: " + sourcePath);
            #endif
            return;
        }
    
        struct dirent* entry;
        struct stat pathStat;
        bool needsSlash = (!sourcePath.empty() && sourcePath.back() != '/');
    
        while ((entry = readdir(dir)) != nullptr) {
            const char* fileName = entry->d_name;
    
            // Fast skip for "." and ".."
            if (fileName[0] == '.' &&
                (fileName[1] == '\0' || (fileName[1] == '.' && fileName[2] == '\0'))) {
                continue;
            }
    
            // Build full path
            std::string fullPath = sourcePath;
            if (needsSlash) fullPath += '/';
            fullPath += fileName;
    
            // Handle directories - recurse into them
            if (entry->d_type == DT_DIR) {
                dotCleanDirectory(fullPath);
                continue;
            }
    
            // Only care about "._" files and ".DS_Store"
            bool isDotUnderscore = (fileName[0] == '.' && fileName[1] == '_');
            bool isDSStore = (fileName[0] == '.' && fileName[1] == 'D' && fileName[2] == 'S' &&
                             fileName[3] == '_' && fileName[4] == 'S' && fileName[5] == 't' &&
                             fileName[6] == 'o' && fileName[7] == 'r' && fileName[8] == 'e' &&
                             fileName[9] == '\0');
            
            if (!isDotUnderscore && !isDSStore) {
                continue;
            }
    
            // Process regular files or unknown types
            if (entry->d_type == DT_REG) {
                if (remove(fullPath.c_str()) == 0) {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Removed dot-underscore file: " + fullPath);
                    #endif
                } else {
                    #if USING_LOGGING_DIRECTIVE
                    if (!disableLogging)
                        logMessage("Failed to remove dot-underscore file: " + fullPath);
                    #endif
                }
            } else if (entry->d_type == DT_UNKNOWN) {
                // Verify with stat if type is unknown
                if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISREG(pathStat.st_mode)) {
                    if (remove(fullPath.c_str()) == 0) {
                        #if USING_LOGGING_DIRECTIVE
                        if (!disableLogging)
                            logMessage("Removed dot-underscore file: " + fullPath);
                        #endif
                    } else {
                        #if USING_LOGGING_DIRECTIVE
                        if (!disableLogging)
                            logMessage("Failed to remove dot-underscore file: " + fullPath);
                        #endif
                    }
                }
            }
        }
    
        closedir(dir);
    }
}