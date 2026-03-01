# FUSED - Distributed File System in C
# Makefile for building the filesystem

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 -I./include $(shell pkg-config fuse --cflags)
CXXFLAGS = -std=c++14 -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 -I./include
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

# Clean protobuf generated files
clean-proto:
	@echo "Cleaning protobuf generated files..."
	@rm -f $(PROTO_DIR)/*.pb.h $(PROTO_DIR)/*.pb.cc
	@echo "Protobuf clean complete"

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

# ============================================================================
# Testing Infrastructure
# ============================================================================

# Testing targets
TEST_DIR = tests
TEST_BIN = $(BIN_DIR)/unit_tests
TEST_LDFLAGS = $(LDFLAGS) -lcunit

$(TEST_BIN): directories $(BUILD_DIR)/unit_tests.o $(BUILD_DIR)/fused_ops.o
	@echo "Linking unit tests..."
	@$(CC) $(BUILD_DIR)/unit_tests.o $(BUILD_DIR)/fused_ops.o -o $@ $(TEST_LDFLAGS)

$(BUILD_DIR)/unit_tests.o: $(TEST_DIR)/unit_tests.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

test-unit: $(TEST_BIN)
	@$(TEST_BIN)

test-functional:
	@chmod +x $(TEST_DIR)/functional_test.sh
	@$(TEST_DIR)/functional_test.sh

test: test-unit test-functional

# ============================================================================
# RPC SERVER
# ============================================================================

PROTO_DIR = proto
PROTO_SRC = $(PROTO_DIR)/filesystem.proto
PROTO_GEN = $(PROTO_DIR)/filesystem.pb.cc $(PROTO_DIR)/filesystem.grpc.pb.cc

# RPC server binary
RPC_SERVER = $(BIN_DIR)/fused_rpc_server

# Generate protobuf code
proto: $(PROTO_GEN)

$(PROTO_GEN): $(PROTO_SRC)
	@echo "Generating protobuf code..."
	@mkdir -p $(PROTO_DIR)
	@protoc -I$(PROTO_DIR) --cpp_out=$(PROTO_DIR) $(PROTO_SRC)
	@protoc -I$(PROTO_DIR) --grpc_out=$(PROTO_DIR) \
		--plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $(PROTO_SRC)

# Build RPC server
rpc-server: directories proto $(BUILD_DIR)/fused_ops.o
	@echo "Building RPC server..."
	$(CXX) -std=c++14 -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 -I./include -Iproto \
		src/fused_rpc_server.cpp \
		proto/filesystem.pb.cc \
		proto/filesystem.grpc.pb.cc \
		build/fused_ops.o \
		-o bin/fused_rpc_server \
		-lgrpc++ -lgrpc -lprotobuf -lpthread -lgrpc++_reflection -lfuse
	@echo "RPC server built: $(RPC_SERVER)"

# ============================================================================
# TCP ADAPTER (Paxos's Protocol Bridge)
# ============================================================================

TCP_ADAPTER = $(BIN_DIR)/storage_tcp_adapter

tcp-adapter: directories proto
	@echo "Building TCP adapter..."
	$(CXX) -std=c++14 -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 -I./include -I$(PROTO_DIR) \
		src/storage_tcp_adapter.cpp \
		$(PROTO_DIR)/filesystem.pb.cc \
		$(PROTO_DIR)/filesystem.grpc.pb.cc \
		-o $(TCP_ADAPTER) \
		-lgrpc++ -lgrpc -lprotobuf -lpthread
	@echo "TCP adapter built: $(TCP_ADAPTER)"

# ============================================================================
# DISTRIBUTED FRONTEND COORDINATOR
# ============================================================================

DIST_CORE_DIR = distributed_core
DIST_CORE_LIB = $(BUILD_DIR)/libdistributed_core.a

# Build distributed core library first
$(DIST_CORE_LIB):
	@echo "Building distributed core library..."
	@cd $(DIST_CORE_DIR) && $(MAKE) all
	@mkdir -p $(BUILD_DIR)
	@cp $(DIST_CORE_DIR)/build/libdistributed_core.a $(BUILD_DIR)/
	@echo "Distributed core library ready"

DIST_FRONTEND = $(BIN_DIR)/distributed_frontend
DIST_CLIENT = $(BIN_DIR)/distributed_client

distributed-frontend: directories proto $(DIST_CORE_LIB)
	@echo "Building distributed frontend coordinator..."
	$(CXX) -std=c++14 -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 \
		-I./include -I$(PROTO_DIR) -I$(DIST_CORE_DIR)/include \
		src/distributed_frontend.cpp \
		$(PROTO_DIR)/filesystem.pb.cc \
		$(PROTO_DIR)/filesystem.grpc.pb.cc \
		-o $(DIST_FRONTEND) \
		$(DIST_CORE_LIB) \
		-lgrpc++ -lgrpc -lprotobuf -lpthread -lgrpc++_reflection -luuid
	@echo "Distributed frontend built: $(DIST_FRONTEND)"

distributed-client: directories proto
	@echo "Building distributed client..."
	$(CXX) -std=c++14 -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64 \
		-I./include -I$(PROTO_DIR) \
		src/distributed_client.cpp \
		$(PROTO_DIR)/filesystem.pb.cc \
		$(PROTO_DIR)/filesystem.grpc.pb.cc \
		-o $(DIST_CLIENT) \
		-lgrpc++ -lgrpc -lprotobuf -lpthread
	@echo "Distributed client built: $(DIST_CLIENT)"

distributed: distributed-frontend distributed-client
	@echo "All distributed components built successfully"

.PHONY: test test-unit test-functional tcp-adapter rpc-server proto clean install uninstall help distributed distributed-frontend distributed-client