#pragma once

#include "network/connection.hpp"
#include "network/protocol.hpp"

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace go {
namespace network {

//! Connection Manager.
class TcpServer {
public:
	struct Callbacks {
		std::function<void(const SessionId&)> onConnect;
		std::function<void(const SessionId&, const Message&)> onMessage;
		std::function<void(const SessionId&)> onDisconnect;
	};

	TcpServer(std::uint16_t port = DEFAULT_PORT);

	void start();                      //!< Start accepting clients.
	void connect(Callbacks callbacks); //!< Connect callback functions to get event signalling.
	void stop();                       //!< Disconnect clients and stop the server.

private:
	void acceptLoop(); //!< Wait and create two client connections.
	std::shared_ptr<Connection> createConnection(asio::ip::tcp::socket socket, std::size_t clientIndex);

private:
	asio::io_context m_ioContext;
	asio::ip::tcp::acceptor m_acceptor;

	std::thread m_acceptThread;         //!< Waits for two client connections.
	std::atomic<bool> m_running{false}; //!< TCP Server running.

	Callbacks m_callbacks; //!< Callback functions to signal events.

	std::array<std::shared_ptr<Connection>, MAX_PLAYERS> m_connections; //!< Active connections.
	std::mutex m_connectionsMutex;                                      //!< Handle concurrency.
};

} // namespace network
} // namespace go
