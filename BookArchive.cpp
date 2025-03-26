/**
 * @file    BookArchive.cpp
 * @author  Ashisha Sutradhar
 * @date    2025-03-17
 * @version 1.0.0
 * 
 * @brief   Implementation of the BookArchive class
 *
 * @details This file contains the implementation of the BookArchive class,
 *          which provides functionality for managing a book collection using
 *          an SQLite database. It includes methods for CRUD operations,
 *          database initialization, command processing, and logging.
 */

#include "BookArchive.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cctype>

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

BookArchive::BookArchive(const std::string& db_file, LogLevel log_level)
    : db(nullptr), db_filename(db_file), running(true), current_log_level(log_level) {
    
    // Open log file
    log_file.open("book_archive.log", std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Warning: Could not open log file. Logging disabled." << std::endl;
    }
    
    // Initialize database
    if (!initializeDatabase()) {
        throw std::runtime_error("Failed to initialize database: " + db_filename);
    }
    log(LogLevel::INFO, "********************************************************");
    log(LogLevel::INFO, "Book Archive initialized with database: " + db_filename + " and logging level: " + logLevelToString(log_level));
}

BookArchive::~BookArchive() {
    log(LogLevel::INFO, "Shutting down Book Archive");
    
    // Clean up prepared statements
    cleanupStatements();
    
    // Close database connection
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
    
    // Close log file
    if (log_file.is_open()) {
        log_file.close();
    }
}

bool BookArchive::initializeDatabase() {
    std::lock_guard<std::shared_mutex> lock(db_mutex);
    
    // Open database connection
    int rc = sqlite3_open(db_filename.c_str(), &db);
    if (rc != SQLITE_OK) {
        log(LogLevel::ERROR, "Cannot open database: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return false;
    }
    
    // Enable foreign keys and other pragmas for better performance
    const char* pragmas[] = {
        "PRAGMA foreign_keys = ON;",
        "PRAGMA journal_mode = WAL;",
        "PRAGMA synchronous = NORMAL;",
        "PRAGMA cache_size = 1000;",
        "PRAGMA temp_store = MEMORY;"
    };

    for (const auto& pragma : pragmas) {
        char* errmsg = nullptr;
        rc = sqlite3_exec(db, pragma, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            log(LogLevel::ERROR, "Failed to set pragma: " + std::string(errmsg));
            sqlite3_free(errmsg);
            // Continue despite pragma errors - they're optimizations
        }
    }
    
    // Create books table if it doesn't exist
    const char* createTableSQL = 
        "CREATE TABLE IF NOT EXISTS books ("
        "id INTEGER PRIMARY KEY, "
        "title TEXT NOT NULL, "
        "author TEXT NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
    
    char* errmsg = nullptr;
    rc = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        log(LogLevel::ERROR, "Failed to create table: " + std::string(errmsg));
        sqlite3_free(errmsg);
        return false;
    }
    
    // Create index for faster search
    const char* createIndexSQL =
        "CREATE INDEX IF NOT EXISTS idx_books_title_author ON books(title, author);";
    
    rc = sqlite3_exec(db, createIndexSQL, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        log(LogLevel::ERROR, "Failed to create index: " + std::string(errmsg));
        sqlite3_free(errmsg);
        // Continue despite index creation error
    }
    
    return true;
}

void BookArchive::log(LogLevel level, const std::string& message) {
    // Skip logging if the level is below current_log_level
    if (level < current_log_level) {
        return;
    }
    
    // Skip all non-error logs in release mode
    #ifndef DEBUG_MODE
    if (level != LogLevel::ERROR) {
        return;
    }
    #endif
    
    //exit function if log file is not open
    if (!log_file.is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    log_file << "[" << timestamp.str() << "] [" << logLevelToString(level) << "] "
             << message << std::endl;
    
    // Explicitely calling flush to ensure the error logs are immidiately writen on to the disk.
    if (level == LogLevel::ERROR) {
        log_file.flush();
    }
}

void BookArchive::cleanupStatements() {
    std::lock_guard<std::shared_mutex> lock(stmt_cache_mutex);
    for (auto& stmt_pair : stmt_cache) {
        if (stmt_pair.second) {
            sqlite3_finalize(stmt_pair.second);
            stmt_pair.second = nullptr;
        }
    }
    
    stmt_cache.clear();
}

sqlite3_stmt* BookArchive::getPreparedStatement(const std::string& sql) {
    // First try to find in cache with read lock
    {
        std::shared_lock<std::shared_mutex> cache_lock(stmt_cache_mutex);
        auto it = stmt_cache.find(sql);
        if (it != stmt_cache.end()) {
            return it->second;
        }
    }
    
    // Need to prepare the statement and add to cache
    std::lock_guard<std::shared_mutex> cache_lock(stmt_cache_mutex);
    // Check again after acquiring write lock
    auto it = stmt_cache.find(sql);
    if (it != stmt_cache.end()) {
        return it->second;
    }
    
    // Prepare statement
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        log(LogLevel::ERROR, "Failed to prepare statement: " + 
            std::string(sqlite3_errmsg(db)) + " for SQL: " + sql);
        return nullptr;
    }
    // Add to cache
    stmt_cache[sql] = stmt;
    return stmt;
}

bool BookArchive::executeSQLWithParams(const std::string& sql, 
                                        const std::vector<std::string>& params) {
    log(LogLevel::DEBUG, "Executing SQL: " + sql + " with " + 
        std::to_string(params.size()) + " parameters");
    
    sqlite3_stmt* stmt = getPreparedStatement(sql);
    if (!stmt) {
        return false;
    }

    
    // Reset statement before binding parameters
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            log(LogLevel::ERROR, "Failed to bind parameter " + std::to_string(i + 1) + 
                ": " + std::string(sqlite3_errmsg(db)));
            return false;
        }
    }
    
    // Execute with retry for SQLITE_BUSY errors
    int retries = 0;
    int rc;

    {
        std::lock_guard<std::shared_mutex> lock(db_mutex);
        while ((rc = sqlite3_step(stmt)) == SQLITE_BUSY && retries < SQLITE_MAX_RETRIES) {
            log(LogLevel::DEBUG, "Database busy, retrying... (" + 
                std::to_string(retries + 1) + "/" + std::to_string(SQLITE_MAX_RETRIES) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * (1 << retries)));
            retries++;
        }
    }
    
    if (rc != SQLITE_DONE) {
        log(LogLevel::ERROR, "Failed to execute SQL: " + std::string(sqlite3_errmsg(db)));
        return false;
    }
    return true;
}

std::vector<Book> BookArchive::executeQuery(const std::string& sql, 
                                             const std::vector<std::string>& params) {
    log(LogLevel::DEBUG, "Executing query: " + sql);
    
    std::vector<Book> results;
    
    sqlite3_stmt* stmt = getPreparedStatement(sql);
    if (!stmt) {
        return results;
    }
    
    // Reset statement and clear bindings
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            log(LogLevel::ERROR, "Failed to bind parameter " + std::to_string(i + 1) + 
                ": " + std::string(sqlite3_errmsg(db)));
            return results;
        }
    }
    
    // Execute with retry for SQLITE_BUSY errors
    int retries = 0;
    int rc;
    
    while (true) {
        {
            //shared lock for all reads
            std::shared_lock<std::shared_mutex> lock(db_mutex);
            rc = sqlite3_step(stmt);
        }
        
        if (rc == SQLITE_ROW) {
            Book book;
            book.id = sqlite3_column_int(stmt, 0);
            
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            
            book.title = title ? title : "";
            book.author = author ? author : "";
            
            results.push_back(book);
        } else if (rc == SQLITE_BUSY && retries < SQLITE_MAX_RETRIES) {
            log(LogLevel::DEBUG, "Database busy, retrying... (" + 
                std::to_string(retries + 1) + "/" + std::to_string(SQLITE_MAX_RETRIES) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * (1 << retries)));
            retries++;
            continue;
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            log(LogLevel::ERROR, "Failed to execute query: " + std::string(sqlite3_errmsg(db)));
            break;
        }
    }
    
    if (!results.empty()) {
        log(LogLevel::DEBUG, "Query returned " + std::to_string(results.size()) + " results");
    } else {
        log(LogLevel::DEBUG, "Query returned no results");
    }
    
    return results;
}

bool BookArchive::addBook(int id, const std::string& title, const std::string& author) {
    log(LogLevel::INFO, "Adding book: ID=" + std::to_string(id) + ", Title='" + title + "', Author='" + author + "'");
    
    const std::string sql = "INSERT INTO books (id, title, author) VALUES (?, ?, ?);";
    std::vector<std::string> params = {
        std::to_string(id),
        title,
        author
    };
    if (!executeSQLWithParams(sql, params)) {
        log(LogLevel::ERROR, "Failed to add book");
        std::cout << "Error: Failed to add the book. Check logs for details." << std::endl;
        return false;
    }
    
    std::cout << "Book added successfully!" << std::endl;
    return true;
}

bool BookArchive::deleteBook(int id) {
    log(LogLevel::INFO, "Deleting book with ID: " + std::to_string(id));
    
    const std::string sql = "DELETE FROM books WHERE id = ?;";
    std::vector<std::string> params = {std::to_string(id)};
    
    if (!executeSQLWithParams(sql, params)) {
        log(LogLevel::ERROR, "Failed to delete book");
        std::cout << "Error: Failed to delete the book. Check logs for details." << std::endl;
        return false;
    }
    
    std::cout << "Book deleted successfully!" << std::endl;
    return true;
}

bool BookArchive::updateBook(int id, const std::string& newTitle, const std::string& newAuthor) {
    log(LogLevel::INFO, "Updating book: ID=" + std::to_string(id) + 
        ", New Title='" + newTitle + "', New Author='" + newAuthor + "'");
    
    const std::string sql = "UPDATE books SET title = ?, author = ? WHERE id = ?;";
    std::vector<std::string> params = {
        newTitle,
        newAuthor,
        std::to_string(id)
    };
    
    if (!executeSQLWithParams(sql, params)) {
        log(LogLevel::ERROR, "Failed to update book");
        std::cout << "Error: Failed to update the book. Check logs for details." << std::endl;
        return false;
    }
    
    std::cout << "Book updated successfully!" << std::endl;
    return true;
}

std::vector<Book> BookArchive::searchBook(const std::string& keyword) {
    log(LogLevel::INFO, "Searching for books with keyword: '" + keyword + "'");
    
    const std::string sql = 
        "SELECT * FROM books WHERE title LIKE ? OR author LIKE ? ORDER BY id;";
    
    std::string searchPattern = "%" + keyword + "%";
    std::vector<std::string> params = {searchPattern, searchPattern};
    
    std::vector<Book> results = executeQuery(sql, params);
    
    if (results.empty()) {
        std::cout << "No books found matching '" << keyword << "'." << std::endl;
    } else {
        std::cout << "Search Results for '" << keyword << "':" << std::endl;
        std::cout << std::setw(5) << "ID" << " | " 
                  << std::setw(30) << "Title" << " | " 
                  << std::setw(20) << "Author" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (const auto& book : results) {
            std::cout << std::setw(5) << book.id << " | " 
                      << std::setw(30) << (book.title.length() > 30 ? book.title.substr(0, 27) + "..." : book.title) << " | " 
                      << std::setw(20) << (book.author.length() > 20 ? book.author.substr(0, 17) + "..." : book.author) 
                      << std::endl;
        }
    }
    
    return results;
}

void BookArchive::displayBooks() {
    log(LogLevel::INFO, "Displaying all books");
    
    const std::string sql = "SELECT * FROM books ORDER BY id;";
    std::vector<Book> results = executeQuery(sql);
    
    if (results.empty()) {
        std::cout << "No books found in the database." << std::endl;
    } else {
        std::cout << "Book Archive - All Books:" << std::endl;
        std::cout << std::setw(5) << "ID" << " | " 
                  << std::setw(30) << "Title" << " | " 
                  << std::setw(20) << "Author" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (const auto& book : results) {
            std::cout << std::setw(5) << book.id << " | " 
                      << std::setw(30) << (book.title.length() > 30 ? book.title.substr(0, 27) + "..." : book.title) << " | " 
                      << std::setw(20) << (book.author.length() > 20 ? book.author.substr(0, 17) + "..." : book.author) 
                      << std::endl;
        }
        
        std::cout << "\nTotal: " << results.size() << " book(s)" << std::endl;
    }
}

void BookArchive::help() {
    std::cout << "\nBook Archive " << VERSION << " - Command List\n" << std::endl;
    std::cout << "  add <id> <title>, <author>              - Add a new book" << std::endl;
    std::cout << "  delete <id>                             - Delete a book by ID" << std::endl;
    std::cout << "  update <id> <new_title>, <new_author>   - Update a book's information based on ID" << std::endl;
    std::cout << "  search <keyword>                        - Search books by title or author" << std::endl;
    std::cout << "  display                                 - Show all books in the database" << std::endl;
    std::cout << "  help                                    - Show this help menu" << std::endl;
    std::cout << "  version                                 - Display the tool version" << std::endl;
    std::cout << "  debug                                   - Toggle debug logging (if compiled with DEBUG_MODE)" << std::endl;
    std::cout << "  exit                                    - Quit the program\n" << std::endl;
}

void BookArchive::version() {
    std::cout << "Book Archive Version: " << VERSION << std::endl;
    std::cout << "Build date: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "SQLite version: " << sqlite3_libversion() << std::endl;
    
    #ifdef DEBUG_MODE
    std::cout << "Build type: Debug" << std::endl;
    #else
    std::cout << "Build type: Release" << std::endl;
    #endif
}

void BookArchive::setLogLevel(LogLevel level) {
    current_log_level = level;
    log(LogLevel::INFO, "Log level set to: " + logLevelToString(level));
}

void BookArchive::processCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string action;
    iss >> action;
    
    try {
        if (action == "add") {
            int id;
            std::string title, author, idStr;
            
            // Parse ID first
            iss >> idStr;
            if (idStr.empty()) {
                throw std::runtime_error("Missing book ID");
            }
            
            try {
                id = std::stoi(idStr);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid book ID: " + idStr);
            }
            
            // Parse title and author
            std::string remainder;
            std::getline(iss, remainder);
            
            size_t commaPos = remainder.find(',');
            if (commaPos == std::string::npos) {
                throw std::runtime_error("Invalid format. Use: add <id> <title>, <author>");
            }
            
            title = remainder.substr(0, commaPos);
            author = remainder.substr(commaPos + 1);
            
            // Trim whitespace before and after
            title.erase(0, title.find_first_not_of(" \t"));
            title.erase(title.find_last_not_of(" \t") + 1);
            author.erase(0, author.find_first_not_of(" \t"));
            author.erase(author.find_last_not_of(" \t") + 1);
            
            if (title.empty() || author.empty()) {
                throw std::runtime_error("Title and author cannot be empty");
            }

            addBook(id, title, author);
        } else if (action == "delete") {
            int id;
            std::string idStr;
            iss >> idStr;
            
            if (idStr.empty()) {
                throw std::runtime_error("Missing book ID");
            }
            
            try {
                id = std::stoi(idStr);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid book ID: " + idStr);
            }
            
            deleteBook(id);
        } else if (action == "update") {
            int id;
            std::string newTitle, newAuthor, idStr;
            
            // Parse ID first
            iss >> idStr;
            if (idStr.empty()) {
                throw std::runtime_error("Missing book ID");
            }
            
            try {
                id = std::stoi(idStr);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid book ID: " + idStr);
            }
            
            // Parse title and author
            std::string remainder;
            std::getline(iss, remainder);
            
            size_t commaPos = remainder.find(',');
            if (commaPos == std::string::npos) {
                throw std::runtime_error("Invalid format. Use: update <id> <new_title>, <new_author>");
            }
            
            newTitle = remainder.substr(0, commaPos);
            newAuthor = remainder.substr(commaPos + 1);
            
            // Trim whitespace
            newTitle.erase(0, newTitle.find_first_not_of(" \t"));
            newTitle.erase(newTitle.find_last_not_of(" \t") + 1);
            newAuthor.erase(0, newAuthor.find_first_not_of(" \t"));
            newAuthor.erase(newAuthor.find_last_not_of(" \t") + 1);
            
            if (newTitle.empty() || newAuthor.empty()) {
                throw std::runtime_error("Title and author cannot be empty");
            }
            
            updateBook(id, newTitle, newAuthor);
        } else if (action == "search") {
            std::string keyword;
            std::getline(iss >> std::ws, keyword);
            
            if (keyword.empty()) {
                throw std::runtime_error("Missing search keyword");
            }
            
            searchBook(keyword);
        } else if (action == "display") {
            displayBooks();
        } else if (action == "help") {
            help();
        } else if (action == "version") {
            version();
        } else if (action == "debug") {
            #ifdef DEBUG_MODE
            if (current_log_level == LogLevel::DEBUG) {
                setLogLevel(LogLevel::INFO);
                std::cout << "Logging level switched to INFO. Showing all logs." << std::endl;
            } else {
                setLogLevel(LogLevel::DEBUG);
                std::cout << "Logging level switched to DEBUG. Recording all DEBUG and ERROER logs only." << std::endl;
            }
            #else
            std::cout << "Logging level switching is not available in release build. Log level set to ERROR" << std::endl;
            #endif
        } else if (action == "exit") {
            running = false;
            std::cout << "Exiting Book Archive. Goodbye!" << std::endl;
        } else {
            std::cout << "Invalid command. Type 'help' for a list of commands." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        log(LogLevel::ERROR, "Command error: " + std::string(e.what()) + " (Command: " + command + ")");
    }
}

void BookArchive::run() {
    std::cout << "Book Archive " << VERSION << " - Library Management Tool" << std::endl;
    std::cout << "Type 'help' for available commands, 'exit' to quit." << std::endl;
    
    std::string command;
    while (running) {
        std::cout << "\n> ";
        std::getline(std::cin, command);
        
        if (!command.empty()) {
            processCommand(command);
        }
    }
}
