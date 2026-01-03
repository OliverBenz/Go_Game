#pragma once

#include "network/protocol.hpp"
#include "network/connection.hpp"

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace go {
namespace network {

using SessionId = std::string;  //!< NOTE: Maybe switch to GUID sometime.

//! Connection Manager. 
class TcpServer {
public:
    struct Callbacks {
        std::function<void(const SessionId&)> onConnect;
        std::function<void(const SessionId&, const Message&)> onMessage;
        std::function<void(const SessionId&)> onDisconnect;
    };

    TcpServer(std::uint16_t port, Callbacks callbacks);

    void start();
    void stop();

private:
    void acceptLoop();
    std::shared_ptr<Connection> createConnection(asio::ip::tcp::socket socket);

private:
    asio::io_context m_io;
    asio::ip::tcp::acceptor m_acceptor;

    std::thread m_acceptThread;
    std::atomic<bool> m_running{false};

    Callbacks m_callbacks;

    // exactly 2 clients for now
    std::array<std::shared_ptr<Connection>, MAX_PLAYERS> m_connections;
};

} // namespace network
} // namespace go
