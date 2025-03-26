/**
 * @file    main.cpp
 * @author  Ashisha Sutradhar
 * @date    2025-03-17
 * @version 1.0.0
 * 
 * @brief   Entry point for the Book Archive application
 *
 * @details This file contains the main function and command-line argument
 *          processing for the Book Archive application. It handles program
 *          initialization, signal handling for graceful shutdown, and
 *          command-line option parsing.
 *
 */

#include "BookArchive.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <csignal>
#include <cstring>

// Global pointer for signal handling
BookArchive* g_archive = nullptr;

// Signal handler for clean shutdown
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived termination signal. Shutting down gracefully..." << std::endl;
        if (g_archive) {
            g_archive->~BookArchive();
            g_archive = nullptr;
        }
        exit(0);
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --db, -d <filename>     Specify database file (default: book_archive.db)" << std::endl;
    std::cout << "  --log-level, -l <level> Set log level (DEBUG, INFO, ERROR) (default: ERROR in release, DEBUG in debug)" << std::endl;
    std::cout << "  --help, -h              Display this help message" << std::endl;
    std::cout << "  --version, -v           Display version information" << std::endl;
}

LogLevel parseLogLevel(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "ERROR") return LogLevel::ERROR;
    
    std::cerr << "Warning: Unknown log level '" << level << "'. Using default." << std::endl;
    
    #ifdef DEBUG_MODE
    return LogLevel::DEBUG;
    #else
    return LogLevel::ERROR;
    #endif
}

int main(int argc, char** argv) {
    std::string db_file = "book_archive.db";
    LogLevel log_level = 
        #ifdef DEBUG_MODE
            LogLevel::DEBUG;
        #else
            LogLevel::ERROR;
        #endif
    
    // Process command-line arguments if present
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--db" || arg == "-d") {
                if (i + 1 < argc) {
                    db_file = argv[++i];
                } else {
                    std::cerr << "Error: Missing database filename after " << arg << std::endl;
                    return 1;
                }
            } else if (arg == "--log-level" || arg == "-l") {
                if (i + 1 < argc) {
                    log_level = parseLogLevel(argv[++i]);
                } else {
                    std::cerr << "Error: Missing log level after " << arg << std::endl;
                    return 1;
                }
            } else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            } else if (arg == "--version" || arg == "-v") {
                std::cout << "Book Archive Version: " << VERSION << std::endl;
                std::cout << "Build date: " << __DATE__ << " " << __TIME__ << std::endl;
                std::cout << "SQLite version: " << sqlite3_libversion() << std::endl;
                #ifdef DEBUG_MODE
                std::cout << "Build type: Debug" << std::endl;
                #else
                std::cout << "Build type: Release" << std::endl;
                #endif
                return 0;
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        }
    }
    
    // Set up signal handlers for clean shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Create the BookArchive object on the heap so we can use it in signal handler
        g_archive = new BookArchive(db_file, log_level);
        g_archive->run();
        
        // Clean shutdown
        delete g_archive;
        g_archive = nullptr;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        
        // Clean up if needed
        if (g_archive) {
            delete g_archive;
            g_archive = nullptr;
        }
        
        return 1;
    }
}
