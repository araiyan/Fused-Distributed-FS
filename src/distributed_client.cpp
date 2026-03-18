/**
 * @file distributed_client.cpp
 * @brief Simple CLI client for testing distributed filesystem
 */

#include <grpcpp/grpcpp.h>
#include "filesystem.grpc.pb.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <cstring>
#include <chrono>
#include <vector>

using fused::CreateRequest;
using fused::CreateResponse;
using fused::FileEntry;
using fused::FileSystemService;
using fused::GetRequest;
using fused::GetResponse;
using fused::MkdirRequest;
using fused::MkdirResponse;
using fused::RemoveRequest;
using fused::RemoveResponse;
using fused::ReadDirectoryRequest;
using fused::ReadDirectoryResponse;
using fused::RmdirRequest;
using fused::RmdirResponse;
using fused::UploadChunk;
using fused::UploadResponse;
using fused::WriteRequest;
using fused::WriteResponse;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Status;

class DistributedFileSystemClient {
private:
    std::unique_ptr<FileSystemService::Stub> stub_;

    static void set_deadline(ClientContext& context) {
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(20));
    }

public:
    DistributedFileSystemClient(std::shared_ptr<Channel> channel)
        : stub_(FileSystemService::NewStub(channel)) {}

    int Mkdir(const std::string& parent_path, const std::string& dirname, uint32_t mode = 0755) {
        MkdirRequest request;
        request.set_pathname(parent_path);
        request.set_dirname(dirname);
        request.set_mode(mode);

        MkdirResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->Mkdir(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Mkdir failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "✓ Directory created: " << parent_path << "/" << dirname << std::endl;
        return 0;
    }

    int Create(const std::string& parent_path, const std::string& filename, uint32_t mode = 0644) {
        CreateRequest request;
        request.set_pathname(parent_path);
        request.set_filename(filename);
        request.set_mode(mode);

        CreateResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->Create(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Create failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "✓ File created: " << parent_path << "/" << filename << std::endl;
        return 0;
    }

    int RemoveFile(const std::string& path) {
        RemoveRequest request;
        request.set_pathname(path);

        RemoveResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->Remove(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Remove failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "✓ File removed: " << path << std::endl;
        return 0;
    }

    int RemoveDirectory(const std::string& path) {
        RmdirRequest request;
        request.set_pathname(path);

        RmdirResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->Rmdir(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Rmdir failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "✓ Directory removed: " << path << std::endl;
        return 0;
    }

    int Write(const std::string& path, const std::string& data, uint64_t offset = 0) {
        WriteRequest request;
        request.set_pathname(path);
        request.set_data(data);
        request.set_offset(offset);

        WriteResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->Write(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Write failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "✓ Wrote " << response.bytes_written() << " bytes to " << path << std::endl;
        return 0;
    }

    int Read(const std::string& path, std::string& data_out, uint64_t offset = 0, uint64_t size = 0) {
        GetRequest request;
        request.set_pathname(path);
        request.set_offset(offset);
        request.set_size(size);

        GetResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->Get(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Read failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        data_out = response.data();
        std::cout << "✓ Read " << response.bytes_read() << " bytes from " << path << std::endl;
        return 0;
    }

    int UploadFile(const std::string& local_path,
                   const std::string& remote_parent,
                   const std::string& remote_filename,
                   size_t chunk_size = 65536,
                   uint32_t mode = 0644) {
        if (chunk_size == 0) {
            std::cerr << "Upload failed: chunk_size must be > 0" << std::endl;
            return -1;
        }

        std::ifstream input(local_path, std::ios::binary);
        if (!input.is_open()) {
            std::cerr << "Upload failed: cannot open local file: " << local_path << std::endl;
            return -1;
        }

        UploadResponse response;
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::minutes(30));

        std::unique_ptr<ClientWriter<UploadChunk>> writer(stub_->Upload(&context, &response));
        if (!writer) {
            std::cerr << "Upload failed: stream could not be created" << std::endl;
            return -1;
        }

        std::vector<char> buffer(chunk_size);
        uint64_t offset = 0;
        bool sent_data = false;

        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize n = input.gcount();
            if (n <= 0) {
                break;
            }

            UploadChunk chunk;
            chunk.set_parent_path(remote_parent);
            chunk.set_filename(remote_filename);
            chunk.set_offset(static_cast<int64_t>(offset));
            chunk.set_data(buffer.data(), static_cast<size_t>(n));
            chunk.set_mode(mode);
            chunk.set_is_first(!sent_data);
            chunk.set_is_last(false);

            if (!writer->Write(chunk)) {
                writer->WritesDone();
                Status finish_status = writer->Finish();
                std::cerr << "Upload failed while sending data: "
                          << finish_status.error_message() << std::endl;
                return -1;
            }

            offset += static_cast<uint64_t>(n);
            sent_data = true;
        }

        if (input.bad()) {
            std::cerr << "Upload failed: error reading local file" << std::endl;
            return -1;
        }

        UploadChunk final_chunk;
        final_chunk.set_parent_path(remote_parent);
        final_chunk.set_filename(remote_filename);
        final_chunk.set_mode(mode);
        final_chunk.set_offset(static_cast<int64_t>(offset));
        final_chunk.set_is_first(!sent_data);
        final_chunk.set_is_last(true);

        if (!writer->Write(final_chunk)) {
            writer->WritesDone();
            Status finish_status = writer->Finish();
            std::cerr << "Upload failed while sending final marker: "
                      << finish_status.error_message() << std::endl;
            return -1;
        }

        writer->WritesDone();
        Status status = writer->Finish();

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "Upload failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "✓ Uploaded " << response.bytes_uploaded()
                  << " bytes to " << response.file_path() << std::endl;
        return 0;
    }

    int ListDirectory(const std::string& path) {
        ReadDirectoryRequest request;
        request.set_pathname(path);

        ReadDirectoryResponse response;
        ClientContext context;
        set_deadline(context);

        Status status = stub_->ReadDirectory(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
            return -1;
        }

        if (response.status_code() != 0) {
            std::cerr << "ReadDirectory failed: " << response.error_message() << std::endl;
            return response.status_code();
        }

        std::cout << "Directory listing for " << path << ":" << std::endl;
        for (const auto& entry : response.entries()) {
            std::cout << "  " << (entry.is_directory() ? "[DIR] " : "[FILE]") 
                     << entry.name() << " (" << entry.size() << " bytes)" << std::endl;
        }
        
        return 0;
    }
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <server_address> <command> [args...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  mkdir <parent_path> <dirname>              - Create directory" << std::endl;
    std::cout << "  create <parent_path> <filename>            - Create file" << std::endl;
    std::cout << "  rm <file_path>                             - Delete file" << std::endl;
    std::cout << "  rmdir <directory_path>                     - Delete empty directory" << std::endl;
    std::cout << "  write <file_path> <text> [offset]          - Write text to file" << std::endl;
    std::cout << "  upload <local_file> <parent_path> <filename> [chunk_size]"
              << " - Upload local file" << std::endl;
    std::cout << "  read <file_path> [offset] [size]           - Read file contents" << std::endl;
    std::cout << "  ls <directory_path>                        - List directory" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << prog << " localhost:60051 mkdir / videos" << std::endl;
    std::cout << "  " << prog << " localhost:60051 create /videos test.txt" << std::endl;
    std::cout << "  " << prog << " localhost:60051 rm /videos/test.txt" << std::endl;
    std::cout << "  " << prog << " localhost:60051 rmdir /videos" << std::endl;
    std::cout << "  " << prog << " localhost:60051 write /videos/test.txt \"Hello World\"" << std::endl;
    std::cout << "  " << prog << " localhost:60051 upload ./clip.mp4 /videos clip.mp4 65536" << std::endl;
    std::cout << "  " << prog << " localhost:60051 read /videos/test.txt" << std::endl;
    std::cout << "  " << prog << " localhost:60051 ls /videos" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    std::string server_address = argv[1];
    std::string command = argv[2];

    // Create client
    DistributedFileSystemClient client(
        grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials())
    );

    if (command == "mkdir") {
        if (argc < 5) {
            std::cerr << "Usage: mkdir <parent_path> <dirname>" << std::endl;
            return 1;
        }
        return client.Mkdir(argv[3], argv[4]);
    }
    else if (command == "create") {
        if (argc < 5) {
            std::cerr << "Usage: create <parent_path> <filename>" << std::endl;
            return 1;
        }
        return client.Create(argv[3], argv[4]);
    }
    else if (command == "rm") {
        if (argc < 4) {
            std::cerr << "Usage: rm <file_path>" << std::endl;
            return 1;
        }
        return client.RemoveFile(argv[3]);
    }
    else if (command == "rmdir") {
        if (argc < 4) {
            std::cerr << "Usage: rmdir <directory_path>" << std::endl;
            return 1;
        }
        return client.RemoveDirectory(argv[3]);
    }
    else if (command == "write") {
        if (argc < 5) {
            std::cerr << "Usage: write <file_path> <text>" << std::endl;
            return 1;
        }
        std::string path = argv[3];
        std::string text = argv[4];
        uint64_t offset = 0;
        if (argc >= 6) {
            offset = strtoull(argv[5], nullptr, 10);
        }
        return client.Write(path, text, offset);
    }
    else if (command == "upload") {
        if (argc < 6) {
            std::cerr << "Usage: upload <local_file> <parent_path> <filename> [chunk_size]" << std::endl;
            return 1;
        }

        std::string local_file = argv[3];
        std::string parent_path = argv[4];
        std::string filename = argv[5];
        size_t chunk_size = 65536;
        if (argc >= 7) {
            chunk_size = static_cast<size_t>(strtoull(argv[6], nullptr, 10));
        }

        return client.UploadFile(local_file, parent_path, filename, chunk_size);
    }
    else if (command == "read") {
        if (argc < 4) {
            std::cerr << "Usage: read <file_path>" << std::endl;
            return 1;
        }
        std::string path = argv[3];
        uint64_t offset = 0;
        uint64_t size = 0;
        if (argc >= 5) {
            offset = strtoull(argv[4], nullptr, 10);
        }
        if (argc >= 6) {
            size = strtoull(argv[5], nullptr, 10);
        }
        std::string data;
        int result = client.Read(path, data, offset, size);
        if (result >= 0) {
            std::cout << "Content:" << std::endl;
            std::cout << data << std::endl;
        }
        return result;
    }
    else if (command == "ls") {
        if (argc < 4) {
            std::cerr << "Usage: ls <directory_path>" << std::endl;
            return 1;
        }
        return client.ListDirectory(argv[3]);
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
