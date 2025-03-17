<!--
 * @file    readme.md
 * @author  Ashisha Sutradhar
 * @date    2025-03-17
 * @version 1.0.0
 * 
 * @brief   Documentation for the Book Archive application
 *
 * @details This markdown file provides comprehensive documentation for the
 *          Book Archive application, including installation instructions,
 *          usage examples, build options, and troubleshooting information.
 *          It serves as the primary reference for users and contributors.
 *
-->

# Book Archive

A C++ command-line application for managing a personal book collection using SQLite database.


## Overview

Book Archive is a robust command-line tool for cataloging and managing your personal book collection. It provides basic CRUD (Create, Read, Update, Delete) operations for books with title, author, and ID information, all stored in an SQLite database.

Key features:
- Add, update, delete, and search for books
- Persistent storage using SQLite
- Command-line interface
- Configurable logging
- Thread-safe database operations

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
  - [MacOS](#macos)
  - [Linux](#linux)
- [Building from Source](#building-from-source)
- [Usage](#usage)
  - [Command-line Options](#command-line-options)
  - [Application Commands](#application-commands)
- [Example Usage](#example-usage)
- [Development](#development)
  - [Build Types](#build-types)
  - [Clean Build](#clean-build)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## Prerequisites

### MacOS

- g++ (or clang++) with C++17 support
- SQLite3
- make

### Linux

- g++ with C++17 support
- SQLite3 development libraries
- make

## Installation

### MacOS

1. Install the required dependencies using Homebrew:

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install sqlite make gcc
```

2. Clone the repository:

```bash
git clone https://github.com/username/book-archive.git
cd book-archive
```

3. Build and install:

```bash
make
sudo make install  # Optional, installs to /usr/local/bin
```

### Linux

#### Ubuntu/Debian

```bash
# Install dependencies
sudo apt update
sudo apt install g++ make libsqlite3-dev

# Clone repository
git clone https://github.com/username/book-archive.git
cd book-archive

# Build and install
make
sudo make install  # Optional
```

#### Fedora/RHEL/CentOS

```bash
# Install dependencies
sudo dnf install gcc-c++ make sqlite-devel

# Clone repository
git clone https://github.com/username/book-archive.git
cd book-archive

# Build and install
make
sudo make install  # Optional
```

## Building from Source

1. Clone the repository:

```bash
git clone https://github.com/username/book-archive.git
cd book-archive
```

2. Build the application:

```bash
# Release build (optimized)
make

# Debug build (with debug symbols and verbose logging)
make BUILD_TYPE=debug
# or
make debug
```

3. Run the application:

```bash
./book_archive
```

## Usage

### Command-line Options

When starting the application, you can use the following options:

```
./book_archive [options]

Options:
  --db, -d <filename>     Specify database file (default: book_archive.db)
  --log-level, -l <level> Set log level (DEBUG, INFO, ERROR) (default: ERROR in release, DEBUG in debug)
  --help, -h              Display this help message
  --version, -v           Display version information
```

### Application Commands

Once the application is running, you can use these commands:

| Command | Description |
|---------|-------------|
| `add <id> <title>, <author>` | Add a new book |
| `delete <id>` | Delete a book by ID |
| `update <id> <new_title>, <new_author>` | Update a book's information |
| `search <keyword>` | Search books by title or author |
| `display` | Show all books in the database |
| `help` | Show this help menu |
| `version` | Display the tool version |
| `debug` | Toggle debug logging (if compiled with debug mode) |
| `exit` | Quit the program |

## Example Usage

Starting the application:

```bash
# Start with default settings
./book_archive

# Use a custom database file
./book_archive --db mybooks.db

# Set log level to INFO
./book_archive --log-level INFO

# Show version information
./book_archive --version
```

Once inside the application:

```
Book Archive 1.0.0 - Library Management Tool
Type 'help' for available commands, 'exit' to quit.

> add 1 The Great Gatsby, F. Scott Fitzgerald
Book added successfully!

> add 2 To Kill a Mockingbird, Harper Lee
Book added successfully!

> display
Book Archive - All Books:
   ID |                          Title |               Author
------------------------------------------------------------
    1 |                The Great Gatsby |  F. Scott Fitzgerald
    2 |           To Kill a Mockingbird |          Harper Lee

Total: 2 book(s)

> search Gatsby
Search Results for 'Gatsby':
   ID |                          Title |               Author
------------------------------------------------------------
    1 |                The Great Gatsby |  F. Scott Fitzgerald

> update 1 The Great Gatsby, Francis Scott Fitzgerald
Book updated successfully!

> delete 2
Book deleted successfully!

> exit
Exiting Book Archive. Goodbye!
```

## Development

### Build Types

The project supports two build types:

- **Release** (default): Optimized build with minimal logging
  ```bash
  make
  # or
  make BUILD_TYPE=release
  # or
  make release
  ```

- **Debug**: Includes debug symbols and verbose logging
  ```bash
  make BUILD_TYPE=debug
  # or
  make debug
  ```

### Clean Build

To clean compiled objects:

```bash
make clean
```

To clean all generated files (including database and logs):

```bash
make clean-all
```

## Project Structure

- `main.cpp` - Entry point, command-line argument processing, signal handling
- `BookArchive.h` - Class and structure definitions
- `BookArchive.cpp` - Implementation of the BookArchive class
- `Makefile` - Build configuration
- `book_archive.db` - SQLite database file (created on first run)
- `book_archive.log` - Log file (created on first run)

## Troubleshooting

### Common Issues

1. **Compilation error related to SQLite**

   ```
   error: sqlite3.h: No such file or directory
   ```

   **Solution**: Install SQLite development libraries
   - MacOS: `brew install sqlite`
   - Ubuntu/Debian: `sudo apt install libsqlite3-dev`
   - Fedora/RHEL: `sudo dnf install sqlite-devel`

2. **Macro definition conflict**

   If you encounter errors related to the `DEBUG` macro conflicting with `LogLevel::DEBUG`, update the Makefile to use `-DAPP_DEBUG` instead of `-DDEBUG`.

3. **Database locked**

   If you see "database is locked" errors, ensure you don't have another instance of the application running or another program accessing the same database file.

4. **Permission denied when writing to log file**

   Ensure you have write permissions in the directory where the application is running.

### Logs

Application logs are stored in `book_archive.log` in the same directory as the executable. Check this file for detailed error information if you encounter issues.

## Upcomming features:

1. Reading configuration data from a config file
2. Scope to import data from a csv file
3. Multithreaded architecture for better performace in case of multiuser scenario.
4. Improved display function to handle large number of entries.
