

#include <grpc/grpc.h>
#include <grpcpp/alarm.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "chatservice.grpc.pb.h"
#include "chatservice.pb.h"
using namespace std;
using namespace grpc;
using namespace grpc::experimental;
using namespace chat;

class Reader : public grpc::ClientReadReactor<ChatMessage> {
public:
  Reader(ChatService::Stub *stub, const ChatReader reader) : reader_(reader) {
    stub->async()->ReadChat(&context_, &reader_, this);
    StartRead(&message_);
    StartCall();
  }

  void OnReadDone(bool ok) override {
    if (ok) {
      std::cout << message_.name() << ": " << message_.message() << std::endl;
      StartRead(&message_);
    }
  }

  void OnDone(const Status &s) override {
    unique_lock<mutex> l(mu_);
    status_ = s;
    done_ = true;
    cv_.notify_one();
  }

  void EndChat() {
    unique_lock<mutex> l(mu_);
    status_ = Status::OK;
    done_ = true;
    cv_.notify_one();
  }

  Status Await() {
    unique_lock<mutex> l(mu_);
    cv_.wait(l, [this] { return done_; });
    return std::move(status_);
  }

private:
  ClientContext context_;
  mutex mu_;
  condition_variable cv_;
  Status status_;
  bool done_ = false;
  const ChatReader reader_;
  ChatMessage message_;
};

class ChatServiceClient {
public:
  ChatServiceClient(string user_name, std::shared_ptr<Channel> channel)
      : stub_(ChatService::NewStub(channel)), user_name_(user_name) {}

  void Send(string message) {
    ChatMessage chat_message;
    chat_message.set_message(message);
    chat_message.set_name(user_name_);
    ClientContext context;
    Response res;
    Status status = stub_->Send(&context, chat_message, &res);
    if (status.ok()) {
      cout << "System: Message sent successfully: " << res.result() << endl;
    } else {
      cout << "System: Failed to send message." << endl;
    }
  }

  void ReadChat() {
    ChatReader reader;
    reader.set_name(user_name_);
    if (reader_ == nullptr) {
      reader_ = new Reader(stub_.get(), reader);
    }
    Status status = reader_->Await();
    cout << "Chat ended status=" << status.error_message() << endl;
  }

  void EndChat() {
    if (reader_ != nullptr) {
      reader_->EndChat();
    }
  }

private:
  std::unique_ptr<ChatService::Stub> stub_;
  string user_name_;
  Reader *reader_;
};

void threadFunc(ChatServiceClient &client) {
  string message;
  while (true) {
    getline(std::cin, message);
    if (message == "/quit") {
      client.EndChat();
      break;
    }
    client.Send(message);
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " USER_NAME" << endl;
    return 1;
  }

  ChatServiceClient chatter(
      argv[1], grpc::CreateChannel("localhost:9090",
                                   grpc::InsecureChannelCredentials()));

  thread t(threadFunc, std::ref(chatter));
  t.detach();
  chatter.ReadChat();
  return 0;
}
