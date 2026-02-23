# Distributed Core Makefile
# Builds the Metadata Cluster control plane

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -g -I./include -fPIC -D_GNU_SOURCE
CXXFLAGS = $(CFLAGS) -std=c++11
LDFLAGS = -pthread

# Targets
TARGET = distributed_core
LIB_TARGET = lib$(TARGET).a
SHARED_LIB_TARGET = lib$(TARGET).so

# Directory structure
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
OBJ_DIR = build/obj

# Source files
SOURCES = $(SRC_DIR)/paxos.c $(SRC_DIR)/metadata_manager.c $(SRC_DIR)/network_engine.c $(SRC_DIR)/storage_interface.c
HEADERS = $(INCLUDE_DIR)/paxos.h $(INCLUDE_DIR)/metadata_manager.h $(INCLUDE_DIR)/network_engine.h $(INCLUDE_DIR)/storage_interface.h

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Build directories
# Use environment variable if set (for Docker), otherwise use parent dirs
ifeq ($(IN_DOCKER),1)
    BUILD_DIR = ./build
    BIN_DIR = ./bin
else
    BUILD_DIR = ../build
    BIN_DIR = ../bin
endif

# Default target
all: $(LIB_TARGET) $(SHARED_LIB_TARGET)

# Static library
$(LIB_TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $(BUILD_DIR)/$(LIB_TARGET) $(OBJECTS)
	@echo "Built static library: $(BUILD_DIR)/$(LIB_TARGET)"

# Shared library
$(SHARED_LIB_TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -shared -o $(BUILD_DIR)/$(SHARED_LIB_TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "Built shared library: $(BUILD_DIR)/$(SHARED_LIB_TARGET)"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/%.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Test binary (optional)
test_server: $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/test_server $(TEST_DIR)/test_server.c $(OBJECTS) $(LDFLAGS)
	@echo "Built test server: $(BIN_DIR)/test_server"

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BUILD_DIR)/$(LIB_TARGET) $(BUILD_DIR)/$(SHARED_LIB_TARGET)
	rm -f $(BIN_DIR)/test_server
	@echo "Cleaned build artifacts"

# Install headers (for integration with FUSE layer)
install: $(LIB_TARGET) $(SHARED_LIB_TARGET)
	@mkdir -p ../include/distributed_core
	cp $(HEADERS) ../include/distributed_core/
	@echo "Installed headers to ../include/distributed_core/"

# Run static analysis
check:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SOURCES)

# Format code
format:
	clang-format -i $(SOURCES) $(HEADERS)

# Dependencies
$(OBJ_DIR)/paxos.o: $(INCLUDE_DIR)/paxos.h
$(OBJ_DIR)/metadata_manager.o: $(INCLUDE_DIR)/metadata_manager.h
$(OBJ_DIR)/network_engine.o: $(INCLUDE_DIR)/network_engine.h
$(OBJ_DIR)/storage_interface.o: $(INCLUDE_DIR)/storage_interface.h

.PHONY: all clean install check format test_server
