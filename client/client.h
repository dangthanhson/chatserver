#pragma once
#define GRPC_CALLBACK_API_NONEXPERIMENTAL
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "proto/chatservice.grpc.pb.h"
#include "proto/chatservice.pb.h"

using namespace std;
using namespace grpc;
using namespace grpc::experimental;
using namespace chat;

class ReadChatStub : public grpc::ClientReadReactor<ChatMessage> {
public:
  ReadChatStub(ChatService::Stub *stub, const ChatReader reader)
      : reader_(reader) {
    stub->async()->ReadChat(&context_, &reader_, this);
    StartRead(&message_);
    StartCall();
  }

  ~ReadChatStub() override {
    context_.TryCancel();
    cout << "System: ReadChatStub destroyed" << endl;
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

  void EndRead() {
    unique_lock<mutex> l(mu_);
    status_ = Status::OK;
    done_ = true;
    cv_.notify_one();
  }

  Status Await() {
    unique_lock<mutex> l(mu_);
    cv_.wait(l);
    return status_;
  }

private:
  ClientContext context_;
  mutex mu_;
  condition_variable cv_;
  Status status_;
  bool done_ = false;
  const ChatReader reader_;
  ChatMessage message_;

  friend class ChatServiceClient;
};

class ChatServiceClient {
public:
  ChatServiceClient(string user_name, std::shared_ptr<Channel> channel)
      : stub_(ChatService::NewStub(channel)), user_name_(user_name) {}

  ~ChatServiceClient() {
    EndChat();
    cout << "System: ChatServiceClient destroyed" << endl;
  }

  void Send(string message) {
    ChatMessage chat_message;
    chat_message.set_message(message);
    chat_message.set_name(user_name_);
    ClientContext context;
    Response res;
    Status status = stub_->Send(&context, chat_message, &res);
    cout << "System: Message sent: "
         << (status.ok() ? "OK" : status.error_message()) << endl;
  }

  void ReadChat() {
    ChatReader reader;
    reader.set_name(user_name_);
    if (!reader_) {
      reader_ = make_unique<ReadChatStub>(stub_.get(), reader);
    }
    Status status = reader_->Await();
    cout << "System: Chat ended status: "
         << (status.ok() ? "OK" : status.error_message()) << endl;

    last_message_.CopyFrom(reader_->message_);
    reader_.reset();
  }

  void EndChat() {
    if (reader_) {
      reader_->EndRead();
    }
  }

  // for testing purposes
  ChatMessage GetLastMessage() { return last_message_; }

private:
  unique_ptr<ChatService::Stub> stub_;
  string user_name_;
  unique_ptr<ReadChatStub> reader_;
  ChatMessage last_message_;
};
