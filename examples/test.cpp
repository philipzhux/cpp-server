#include "server.hpp"
#include <iostream>

int main() {
  Server s({"127.0.0.1", 18088});
  s.setOnConn([](Context *c) {
    std::cout << "New Client at " << c->getAddress().getAddressString()
              << std::endl;
  });
  s.setOnRecv([](Context *c) {
    std::cout << "Recv message by " << c->getAddress().getAddressString()
              << std::endl;

    std::string msg = c->readString();
    std::cout << msg << std::endl;
    if (!msg.empty() && msg[msg.length() - 1] == '\n') {
      msg.erase(msg.length() - 1);
    }
    c->writeFile(msg);
  });
  s.serve();
}