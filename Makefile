##
# @file    Makefile
# @author  Ashisha Sutradhar
# @date    2025-03-17
# @version 1.0.0
# 
# @brief   Build configuration for the Book Archive application
#
# @details This Makefile provides the build system for the Book Archive
#          application. It supports multiple build types (debug/release),
#          dependency tracking, installation, and clean operations.
#          The build system uses g++ with C++17 standard and requires
#          SQLite3 and pthread libraries.
#
# @usage   make [target]
#          Targets:
#          - all (default): Build the application
#          - clean: Remove object files and binary
#          - clean-all: Remove all generated files including db and logs
#          - install: Install the application to /usr/local/bin
#          - debug: Build with debug symbols and additional logging
#          - release: Build optimized version
#          - run: Build and run the application
#
##


# Compiler and linker settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -pthread
LDFLAGS = -lsqlite3 -pthread

# Build type configuration
ifeq ($(BUILD_TYPE),debug)
    CXXFLAGS += -g -O0 -DDEBUG_MODE
else
    CXXFLAGS += -O2 -DNDEBUG
endif

# Source files and build targets
TARGET = book_archive
SRCS = BookArchive.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

# Default target
all: $(TARGET)

# Link rule
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compilation and dependency generation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Clean rules
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

clean-all: clean
	rm -f *.db *.log

# Install target
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Run targets
run: $(TARGET)
	./$(TARGET)

# Debug and release targets for convenience
debug:
	$(MAKE) BUILD_TYPE=debug

release:
	$(MAKE) BUILD_TYPE=release

# Include dependency files
-include $(DEPS)

.PHONY: all clean clean-all install run debug release