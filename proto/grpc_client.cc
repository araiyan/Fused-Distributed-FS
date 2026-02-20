#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "filesystem.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::ByteBuffer;
using grpc::Status;

using fused::CreateRequest;
using fused::CreateResponse;
using fused::MkdirRequest;
using fused::MkdirResponse;
using fused::WriteResponse;
using fused::WriteRequest;
using fused::GetResponse;
using fused::GetRequest;
using fused::ReadDirectoryRequest;
using fused::ReadDirectoryResponse;
using fused::FileEntry;

using namespace std;

class GrpcClient {
 public:
  GrpcClient(std::shared_ptr<Channel> channel)
      : stub_(fused::FileSystemService::NewStub(channel)) {
  }

  Status Create(const string parent_path, const string name, const int mode) {
    CreateRequest req;
    req.set_pathname(parent_path);
    req.set_filename(name);
    req.set_mode(mode);

    CreateResponse resp;

    ClientContext context;

    Status status = stub_->Create(&context, req, &resp);
    cout << resp.error_message() << endl;
    return status;

  }
  Status Mkdir(const string parent_path, const string name, const int mode) {
    MkdirRequest req;
    req.set_pathname(parent_path);
    req.set_dirname(name);
    req.set_mode(mode);

    MkdirResponse resp;

    ClientContext context;

    Status status = stub_->Mkdir(&context, req, &resp);
    cout << resp.error_message() << endl;
    return status;

  }

  Status Write(const string path, const string data, const int offset) {
    WriteRequest req;
    req.set_pathname(path);
    req.set_data(data);
    req.set_offset(offset);

    WriteResponse resp;

    ClientContext context;

    Status status = stub_->Write(&context, req, &resp);
    cout << "Number of bytes written: " << resp.bytes_written() << endl;
    cout << resp.error_message() << endl;
    return status;

  }

  Status Get(const string path, const int offset, const int size) {
    GetRequest req;
    req.set_pathname(path);
    req.set_offset(offset);
    req.set_size(size);

    GetResponse resp;

    ClientContext context;

    Status status = stub_->Get(&context, req, &resp);
    cout << "The following was read: " << resp.data() << endl;
    cout << resp.error_message() << endl;
    return status;

  }

  Status ReadDirectory(const string path) {
    ReadDirectoryRequest req;
    req.set_pathname(path);

    ReadDirectoryResponse resp;

    ClientContext context;

    Status status = stub_->ReadDirectory(&context, req, &resp);
    int num_entries = resp.entries_size();
    cout << "Found " << num_entries << " items." << endl;
    for (int i = 0; i < num_entries; i++){
      cout << "Filename: " << resp.entries(i).name() << endl;
    }
        
    cout << resp.error_message() << endl;
    return status;

  }
  std::unique_ptr<fused::FileSystemService::Stub> stub_;
};

int main(int argc, char** argv) {
  absl::InitializeLog();
  // Expect only arg: --db_path=path/to/route_guide_db.json.
  GrpcClient client(
      grpc::CreateChannel("localhost:50051",
                          grpc::InsecureChannelCredentials()));

  const string filename = "test.txt";
  const string dirname = "/mnt/fused";
  std::cout << "-------------- Creating file --------------" << std::endl;
  Status status = client.Create(dirname,filename, 755);
  std::cout << status.error_details();

  std::cout << "-------------- Creating directory --------------" << std::endl;
  status = client.Mkdir(dirname, "testdir", 755);
  std::cout << status.error_details();

  const string pathname = "/mnt/fused/test.txt";
  std::cout << "-------------- Writing to a file --------------" << std::endl;
  status = client.Write(pathname, "this message should be written to a file", 0);
  std::cout << status.error_details();

  std::cout << "-------------- Reading a file --------------" << std::endl;
  status = client.Get(pathname, 0, 250);
  std::cout << status.error_details();

  std::cout << "-------------- Listing directory --------------" << std::endl;
  status = client.ReadDirectory(dirname);
  std::cout << status.error_details();
  return 0;
}
