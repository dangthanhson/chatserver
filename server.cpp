
#define GRPC_CALLBACK_API_NONEXPERIMENTAL
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdio.h>
#include <thread>

#include "chatservice.grpc.pb.h"
#include "chatservice.pb.h"

using namespace std;
using namespace grpc;
using namespace grpc::experimental;
using namespace chat;

class Reader : public grpc::ServerWriteReactor<ChatMessage> {
public:
  atomic<bool> done{false};

  Reader(mutex *mu, const ChatReader *reader,
         std::vector<ChatMessage> *received_messages)
      : mu_(mu), reader_(reader), received_messages_(received_messages),
        next_message_(0) {
    NextWrite();
  }

  void OnWriteDone(bool ok) override {
    if (!ok) {
      Finish(Status(grpc::StatusCode::UNKNOWN, "Unexpected Failure"));
      return;
    }
    NextWrite();
  }

  void OnDone() override {
    cout << "RPC Completed" << endl;
    done = true;
  }

  void OnCancel() override {
    cerr << "RPC Cancelled" << endl;
    done = true;
  }

  void EndChat() { Finish(Status::OK); }

  void NextWrite() {
    lock_guard<mutex> lock(*mu_);
    cout << "NextWrite called, next_message_: " << next_message_ << endl;
    if (next_message_ < received_messages_->size()) {
      message_.CopyFrom(received_messages_->at(next_message_));
      next_message_++;
      StartWrite(&message_);
    }
  }

private:
  mutex *mu_;
  const ChatReader *reader_;
  std::vector<ChatMessage> *received_messages_;
  size_t next_message_;
  ChatMessage message_;
};

class ChatServiceImpl final : public ChatService::CallbackService {
public:
  explicit ChatServiceImpl() {}

  ServerUnaryReactor *Send(CallbackServerContext *context,
                           const ChatMessage *message,
                           Response *response) override {
    mu_.lock();
    received_messages_.push_back(*message);
    mu_.unlock();

    cout << "Received message from " << message->name() << ": "
         << message->message() << endl;

    notifying_.notify_one();
    response->set_result("Message received");
    auto *reactor = context->DefaultReactor();
    reactor->Finish(Status::OK);
    return reactor;
  }

  grpc::ServerWriteReactor<ChatMessage> *
  ReadChat(CallbackServerContext *context, const ChatReader *reader) override {
    Reader *r = new Reader(&mu_, reader, &received_messages_);
    unique_lock<mutex> lock(readers_mu_);
    received_readers_.push_back(r);
    return r;
  }

  void NotifyReadersThread() {
    while (true) {
      unique_lock<mutex> lock(readers_mu_);
      notifying_.wait(lock);

      for (Reader *r : received_readers_) {
        cout << "Notifying reader" << endl;
        if (r->done)
          continue;
        r->NextWrite();
      }
    }
  }

private:
  mutex mu_;
  mutex readers_mu_;
  condition_variable notifying_{};
  std::vector<ChatMessage> received_messages_;
  std::vector<Reader *> received_readers_;
};

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
