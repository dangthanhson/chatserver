#pragma once
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

#include "proto/chatservice.grpc.pb.h"
#include "proto/chatservice.pb.h"

using namespace std;
using namespace grpc;
using namespace grpc::experimental;
using namespace chat;

class Reader : public grpc::ServerWriteReactor<ChatMessage> {
public:
  atomic<bool> done{false};
  string name;

  Reader(mutex *mu, const ChatReader *reader,
         std::vector<ChatMessage> *received_messages)
      : name(reader->name()), mu_(mu), reader_(reader),
        received_messages_(received_messages), next_message_(0) {
    NextWrite();
  }

  ~Reader() override {
    cout << "System: Reader for " << name << " destroyed" << endl;
  }

  void OnWriteDone(bool ok) override {
    if (!ok) {
      Finish(Status(grpc::StatusCode::UNKNOWN, "Unexpected Failure"));
      return;
    }
    NextWrite();
  }

  void OnDone() override {
    cout << "System: RPC Completed" << endl;
    done = true;
    // delete this;
  }

  void OnCancel() override {
    Finish(Status::CANCELLED);
    cerr << "System: RPC Cancelled" << endl;
    done = true;
  }

  void NextWrite() {
    lock_guard<mutex> lock(*mu_);
    if (next_message_ < received_messages_->size()) {
      StartWrite(&received_messages_->at(next_message_++));
    }
  }

  void EndChat() { Finish(Status::OK); }

private:
  mutex *mu_;
  const ChatReader *reader_;
  std::vector<ChatMessage> *received_messages_;
  size_t next_message_;
};

class ChatServiceImpl final : public ChatService::CallbackService {
public:
  explicit ChatServiceImpl() {}

  ~ChatServiceImpl() override {
    cout << "System: ChatServiceImpl destroyed" << endl;
  }

  ServerUnaryReactor *Send(CallbackServerContext *context,
                           const ChatMessage *message,
                           Response *response) override {
    mu_.lock();
    received_messages_.push_back(*message);
    mu_.unlock();

    cout << "System: Received message from " << message->name() << ": "
         << message->message() << endl;

    notifying_.notify_one();
    response->set_result("OK");
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
        if (r->done)
          continue;
        cout << "System: Notifying reader " << r->name << endl;
        r->NextWrite();
      }
    }
  }

  // for testing purposes
  std::vector<ChatMessage> GetReceivedMessages() {
    lock_guard<mutex> lock(mu_);
    return received_messages_;
  }

private:
  mutex mu_;
  mutex readers_mu_;
  condition_variable notifying_{};
  std::vector<ChatMessage> received_messages_;
  std::vector<Reader *> received_readers_;
};
