# FUSED - Distributed File System in C
# Makefile for building the filesystem

CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 -I./include $(shell pkg-config fuse --cflags)
LDFLAGS = -lfuse -lpthread $(shell pkg-config fuse --libs)
# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Target binary
TARGET = $(BIN_DIR)/fused

# Source and object files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))


# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Build the main executable
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Install the filesystem binary
install: $(TARGET)
	@echo "Installing $(TARGET) to /usr/local/bin/..."
	@install -m 755 $(TARGET) /usr/local/bin/fused_fs
	@echo "Installation complete"

# Uninstall
uninstall:
	@echo "Removing /usr/local/bin/fused_fs..."
	@rm -f /usr/local/bin/fused_fs
	@echo "Uninstall complete"

# Show help
help:
	@echo "FUSED Filesystem - Makefile targets:"
	@echo "  make all       - Build the filesystem (default)"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make install   - Install to /usr/local/bin"
	@echo "  make uninstall - Remove from /usr/local/bin"
	@echo "  make help      - Show this help message"
