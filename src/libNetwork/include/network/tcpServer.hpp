#pragma once

#include "network/connection.hpp"
#include "network/protocol.hpp"

#include <asio.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace go {
namespace network {

//! Connection manager that runs an async accept loop on a dedicated IO thread.
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
	void doAccept();                                                                //!< Start async accept loop.
	bool createConnection(asio::ip::tcp::socket socket, ConnectionId connectionId); //!< Create and add new connection to map.

private:
	asio::io_context m_ioContext;
	asio::ip::tcp::acceptor m_acceptor;
	std::optional<asio::executor_work_guard<asio::io_context::executor_type>> m_workGuard;

	std::thread m_ioThread;             //!< IO context thread.
	std::atomic<bool> m_running{false}; //!< TCP Server running.

	Callbacks m_callbacks; //!< Callback functions to signal events.

	std::unordered_map<ConnectionId, Connection> m_connections; //!< Active connections.
	std::mutex m_connectionsMutex;                              //!< Handle concurrency.
};

} // namespace network
} // namespace go
