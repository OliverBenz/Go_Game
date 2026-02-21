#pragma once

#include "network/core/protocol.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace go {
namespace network {
namespace core {

//! Connection manager that runs an async accept loop on a dedicated IO thread.
//! \note    This is a thin wrapper: all heavy lifting is in Connection (async read/write).
//! \example Usage: set callbacks via connect(), then start() once. Call stop() to shut down.
class TcpServer {
public:
	struct Callbacks {
		std::function<void(const ConnectionId&)> onConnect;
		std::function<void(const ConnectionId&, const Message&)> onMessage;
		std::function<void(const ConnectionId&)> onDisconnect;
	};

	TcpServer(std::uint16_t port = DEFAULT_PORT);
	~TcpServer();

	TcpServer(const TcpServer&)            = delete;
	TcpServer& operator=(const TcpServer&) = delete;
	TcpServer(TcpServer&&)                 = delete;
	TcpServer& operator=(TcpServer&&)      = delete;

	void connect(Callbacks callbacks); //!< Connect callback functions to get event signalling. Call before start.
	void start();                      //!< Start accepting clients. Safe to call multiple times (subsequent calls no-op).
	void stop();                       //!< Disconnect clients and stop the server. Safe to call multiple times.

	bool send(ConnectionId connectionId, const Message& msg); //!< Send message to the client with given connectionId. Returns false if not found.
	void reject(ConnectionId connectionId);                   //!< Reject the client with given connectionId (force close connection).

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide asio stuff in public interfaces.
};

} // namespace network
} // namespace go
}
