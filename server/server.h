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

  Reader(const ChatReader *reader, mutex *mu, condition_variable *notifying,
         std::vector<ChatMessage> *received_messages)
      : name(reader->name()), mu_(mu), reader_(reader), notifying_(notifying),
        received_messages_(received_messages) {
    // NextWrite();
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
    notifying_->notify_one();
  }

  void OnCancel() override {
    Finish(Status::CANCELLED);
    cerr << "System: RPC Cancelled" << endl;
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
  condition_variable *notifying_;
  std::vector<ChatMessage> *received_messages_;
  size_t next_message_{0};
};

class ChatServiceImpl final : public ChatService::CallbackService {
public:
  explicit ChatServiceImpl() {}

  ~ChatServiceImpl() override {
    done_ = true;
    notifying_.notify_all();
    lock_guard<mutex> lock(readers_mu_);
    cout << "System: ChatServiceImpl destroyed" << endl;
  }

  void EndServer() {
    done_ = true;
    notifying_.notify_all();
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
    ChatMessage m;
    m.set_name("System");
    m.set_message(reader->name() + " has joined the chat!");
    mu_.lock();
    received_messages_.push_back(m);
    mu_.unlock();

    Reader *r = new Reader(reader, &mu_, &notifying_, &received_messages_);
    readers_mu_.lock();
    received_readers_.push_back(r);
    readers_mu_.unlock();
    notifying_.notify_one();
    return r;
  }

  void EndChat(const Reader *reader) {
    unique_lock<mutex> lock(readers_mu_);
    auto it = find_if(received_readers_.begin(), received_readers_.end(),
                      [reader](Reader *r) { return r == reader; });
    ChatMessage m;
    if (it != received_readers_.end()) {
      m.set_name("System");
      m.set_message((*it)->name + " has left the chat!");
      received_readers_.erase(it);
    }

    mu_.lock();
    received_messages_.push_back(m);
    mu_.unlock();
  }

  void NotifyReadersThread() {
    while (true) {
      unique_lock<mutex> lock(readers_mu_);
      notifying_.wait(lock);
      if (done_)
        break;

      for (Reader *r : received_readers_) {
        if (r->done) {
          ChatMessage m;
          m.set_name("System");
          m.set_message(r->name + " has left the chat!");

          mu_.lock();
          received_messages_.push_back(m);
          mu_.unlock();
        }
      }

      received_readers_.erase(
          std::remove_if(received_readers_.begin(), received_readers_.end(),
                         [](Reader *r) { return r->done.load(); }),
          received_readers_.end());

      for (Reader *r : received_readers_) {
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
  atomic_bool done_{false};
  friend class Reader;
};
