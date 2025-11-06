#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "client/client.h"
#include "server/server.h"
#include <chrono>

TEST_CASE("Server::CreateServer") {
  ServerBuilder builder;
  ChatServiceImpl service;
  builder.AddListeningPort("0.0.0.0:9090", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
}

TEST_CASE("Server::CreateClient") {
  ChatServiceClient chatter(
      "user", CreateChannel("localhost:9090", InsecureChannelCredentials()));
}

TEST_CASE("Server::ClientServerIntegration") {
  ServerBuilder builder;
  ChatServiceImpl service;
  builder.AddListeningPort("0.0.0.0:9090", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());

  ChatServiceClient chatter(
      "user", CreateChannel("localhost:9090", InsecureChannelCredentials()));
  chatter.Send("Hello, World!");

  auto received_messages = service.GetReceivedMessages();
  REQUIRE(received_messages.size() == 1);
  CHECK(received_messages[0].message() == "Hello, World!");
  CHECK(received_messages[0].name() == "user");
}

TEST_CASE("Server::ClientServerIntegration_MultipleMessages") {
  ServerBuilder builder;
  ChatServiceImpl service;
  builder.AddListeningPort("0.0.0.0:9090", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());

  ChatServiceClient chatter(
      "user", CreateChannel("localhost:9090", InsecureChannelCredentials()));
  chatter.Send("Hello, World 1");
  chatter.Send("Hello, World 2");
  chatter.Send("Hello, World 3");

  auto received_messages = service.GetReceivedMessages();
  REQUIRE(received_messages.size() == 3);
  CHECK(received_messages[0].message() == "Hello, World 1");
  CHECK(received_messages[0].name() == "user");
  CHECK(received_messages[1].message() == "Hello, World 2");
  CHECK(received_messages[1].name() == "user");
  CHECK(received_messages[2].message() == "Hello, World 3");
  CHECK(received_messages[2].name() == "user");
}

void readMessages(ChatServiceClient *client_ptr) {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  client_ptr->Send("Hello, World 4");
  client_ptr->ReadChat();
  cout << "Exiting readMessages thread" << endl;
}

TEST_CASE("Server::ClientServerIntegration_ReadMessage") {
  ServerBuilder builder;
  ChatServiceImpl service;
  builder.AddListeningPort("0.0.0.0:9090", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  unique_ptr<Server> server(builder.BuildAndStart());
  thread notify_thread(&ChatServiceImpl::NotifyReadersThread, &service);

  shared_ptr<ChatServiceClient> client = make_shared<ChatServiceClient>(
      "user", CreateChannel("localhost:9090", InsecureChannelCredentials()));
  thread t([client]() { client->ReadChat(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  client->Send("Hello, World 4");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  client->EndChat();
  t.join();
  auto last_message = client->GetLastMessage();
  CHECK(last_message.message() == "Hello, World 4");
  CHECK(last_message.name() == "user");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto received_messages = service.GetReceivedMessages();
  REQUIRE(received_messages.size() == 3);
  CHECK(received_messages[0].message() == "user has joined the chat!");
  CHECK(received_messages[0].name() == "System");
  CHECK(received_messages[1].message() == "Hello, World 4");
  CHECK(received_messages[1].name() == "user");
  CHECK(received_messages[2].message() == "user has left the chat!");
  CHECK(received_messages[2].name() == "System");

  service.EndServer();
  notify_thread.join();
}

TEST_CASE("Server::ClientServerIntegration_MultipleClientsReadMessage") {
  ServerBuilder builder;
  ChatServiceImpl service;
  builder.AddListeningPort("0.0.0.0:9090", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  unique_ptr<Server> server(builder.BuildAndStart());
  thread notify_thread(&ChatServiceImpl::NotifyReadersThread, &service);

  shared_ptr<ChatServiceClient> client1 = make_shared<ChatServiceClient>(
      "user1", CreateChannel("localhost:9090", InsecureChannelCredentials()));
  thread t1([client1]() { client1->ReadChat(); });

  shared_ptr<ChatServiceClient> client2 = make_shared<ChatServiceClient>(
      "user2", CreateChannel("localhost:9090", InsecureChannelCredentials()));
  thread t2([client2]() { client2->ReadChat(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  client1->Send("Hello from user1");
  client2->Send("Hello from user2");

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  client1->EndChat();
  t1.join();
  auto last_message1 = client1->GetLastMessage();
  CHECK(last_message1.message() == "Hello from user2");
  CHECK(last_message1.name() == "user2");

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  client2->EndChat();
  t2.join();
  auto last_message2 = client2->GetLastMessage();
  CHECK(last_message2.message() == "user1 has left the chat!");
  CHECK(last_message2.name() == "System");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto received_messages = service.GetReceivedMessages();
  REQUIRE(received_messages.size() == 6);

  service.EndServer();
  notify_thread.join();
}
