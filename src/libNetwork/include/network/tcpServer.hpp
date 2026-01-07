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

//! Connection Manager. Accepts and manages client connections.
class TcpServer {
public:
	struct Callbacks {
		std::function<void(const ConnectionId&)> onConnect;
		std::function<void(const ConnectionId&, const Message&)> onMessage;
		std::function<void(const ConnectionId&)> onDisconnect;
	};

	TcpServer(std::uint16_t port = DEFAULT_PORT);

	void start();                      //!< Start accepting clients.
	void connect(Callbacks callbacks); //!< Connect callback functions to get event signalling.
	void stop();                       //!< Disconnect clients and stop the server.

	bool send(ConnectionId connectionId, Message msg); //!< Send message to the client with given connectionId.
	void reject(ConnectionId connectionId);            //!< Reject the client with given connectionId.

private:
	void acceptLoop();                                                              //!< Wait and create two client connections.
	bool createConnection(asio::ip::tcp::socket socket, ConnectionId connectionId); //!< Create and add new connection to map.

private:
	asio::io_context m_ioContext;
	asio::ip::tcp::acceptor m_acceptor;

	std::thread m_acceptThread;         //!< Waits for two client connections.
	std::atomic<bool> m_running{false}; //!< TCP Server running.

	Callbacks m_callbacks; //!< Callback functions to signal events.

	std::unordered_map<ConnectionId, Connection> m_connections; //!< Active connections.
	std::mutex m_connectionsMutex;                              //!< Handle concurrency.
};

} // namespace network
} // namespace go
