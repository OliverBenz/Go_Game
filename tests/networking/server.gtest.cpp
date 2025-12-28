#include "networking/server.hpp"
#include "networking/client.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

// TODO: Verify board state after every place
TEST(Networking, Server) {
    networking::TcpServer server;
    server.start();
    
    networking::TcpClient client("127.0.0.1");
    client.connect();
    const auto response = client.send_and_receive("hello server");
    client.disconnect();

    std::cout << response;
}

}