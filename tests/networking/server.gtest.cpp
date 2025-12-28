#include "networking/server.hpp"
#include "networking/client.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

// TODO: Verify board state after every place
TEST(Networking, Server) {
    networking::TcpServer server;
    server.start();
    
    networking::TcpClient client1("127.0.0.1");
    networking::TcpClient client2("127.0.0.1");

    client1.connect();
    client2.connect();

    const auto response1 = client1.send_and_receive("hello from client 1");    
    const auto response2 = client2.send_and_receive("hello from client 2");
    
    client1.disconnect();
    client2.disconnect();
}

}