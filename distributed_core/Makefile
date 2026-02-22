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
OBJ_DIR = build/obj
# Default target
all: $(LIB_TARGET) $(SHARED_LIB_TARGET)
# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BUILD_DIR)/$(LIB_TARGET) $(BUILD_DIR)/$(SHARED_LIB_TARGET)
	rm -f $(BIN_DIR)/test_server
	@echo "Cleaned build artifacts"
# Dependencies
$(OBJ_DIR)/metadata_manager.o: $(INCLUDE_DIR)/metadata_manager.h
