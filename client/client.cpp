

#include "client.h"

void UserInputThread(ChatServiceClient &client) {
  string message;
  while (true) {
    getline(cin, message);
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
      argv[1], CreateChannel("localhost:9090", InsecureChannelCredentials()));

  thread t(UserInputThread, std::ref(chatter));
  t.detach();
  chatter.ReadChat();
  return 0;
}
