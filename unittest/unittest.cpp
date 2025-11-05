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

  ChatServiceClient *client = new ChatServiceClient(
      "user", CreateChannel("localhost:9090", InsecureChannelCredentials()));
  thread t([client]() {
    client->Send("Hello, World 4");
    client->ReadChat();
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  client->EndChat();
  t.join();
  auto last_message = client->GetLastMessage();
  CHECK(last_message.message() == "Hello, World 4");
  CHECK(last_message.name() == "user");

  auto received_messages = service.GetReceivedMessages();
  REQUIRE(received_messages.size() == 1);
  CHECK(received_messages[0].message() == "Hello, World 4");
  CHECK(received_messages[0].name() == "user");
}
