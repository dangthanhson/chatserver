
#include "server.h"

void RunServer(const std::string &server_address) {
  ServerBuilder builder;
  ChatServiceImpl service;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  cout << "Server listening on " << server_address << endl;
  std::unique_ptr<Server> server(builder.BuildAndStart());

  thread notify_thread(&ChatServiceImpl::NotifyReadersThread, &service);
  notify_thread.detach();
  server->Wait();
}

int main(int argc, char **argv) {
  std::string server_address = "0.0.0.0:9090";
  RunServer(server_address);

  return 0;
}
