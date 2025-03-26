/**
 * @file    BookArchive.h
 * @author  Ashisha Sutradhar
 * @date    2025-03-17
 * @version 1.0.0
 * 
 * @brief   Header file for the BookArchive class
 *
 * @details Defines the BookArchive class interface, which manages a book
 *          collection using SQLite. The class provides thread-safe database
 *          operations, logging functionality, and a command-line interface
 *          for users to interact with the book collection.
 *
 */

#ifndef BOOK_ARCHIVE_H
#define BOOK_ARCHIVE_H

#include <iostream>
#include <vector>
#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <optional>
#include <memory>
#include <iomanip>

#define VERSION "1.0.0"
#define SQLITE_MAX_RETRIES 5

// Enum for log levels
enum class LogLevel {
    INFO,
    DEBUG,
    ERROR
};

// Simple book structure matching the database schema
struct Book {
    int id;
    std::string title;
    std::string author;
};

class BookArchive {
private:
    sqlite3* db;
    std::string db_filename;
    std::shared_mutex db_mutex;  // Shared mutex for reader/writer pattern
    std::ofstream log_file;
    std::mutex log_mutex;  // Mutex specifically for logging
    std::atomic<bool> running;
    LogLevel current_log_level;
    
    // Prepared statement cache to improve performance
    std::unordered_map<std::string, sqlite3_stmt*> stmt_cache;
    std::shared_mutex stmt_cache_mutex;  // Protect statement cache
    
    // Execute SQL with proper parameter binding (prevents SQL injection)
    bool executeSQLWithParams(const std::string& sql, const std::vector<std::string>& params = {});
    
    // Execute a query and collect results
    std::vector<Book> executeQuery(const std::string& sql, const std::vector<std::string>& params = {});
    
    // Command processing
    void processCommand(const std::string& command);
    
    // Thread-safe logging with different levels
    void log(LogLevel level, const std::string& message);
    
    // Close and cleanup prepared statements
    void cleanupStatements();
    
    // Initialize database and create schema
    bool initializeDatabase();
    
    // Get prepared statement (thread-safe)
    sqlite3_stmt* getPreparedStatement(const std::string& sql);

public:
    BookArchive(const std::string& db_file = "book_archive.db", LogLevel log_level = 
        #ifdef DEBUG_MODE
            LogLevel::DEBUG
        #else
            LogLevel::ERROR
        #endif
    );
    ~BookArchive();
    
    // Non-copyable and non-movable
    BookArchive(const BookArchive&) = delete;
    BookArchive& operator=(const BookArchive&) = delete;
    BookArchive(BookArchive&&) = delete;
    BookArchive& operator=(BookArchive&&) = delete;
    
    // Display all books
    void displayBooks();
    
    // Help and version info
    void help();
    void version();
    
    // CRUD operations
    bool addBook(int id, const std::string& title, const std::string& author);
    bool deleteBook(int id);
    bool updateBook(int id, const std::string& newTitle, const std::string& newAuthor);
    std::vector<Book> searchBook(const std::string& keyword);
    
    // Set log level dynamically
    void setLogLevel(LogLevel level);
    
    // Main command loop
    void run();
};

// LogLevel to string conversion
std::string logLevelToString(LogLevel level);

#endif // BOOK_ARCHIVE_H
