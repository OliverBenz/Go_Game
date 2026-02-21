#pragma once

#include "network/core/protocol.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace go {
namespace network {

//! Minimal synchronous TCP client.
//! \note    This is intentionally blocking I/O to keep the client logic simple.
//!          On any network failure, send/read return false/empty and the client is considered disconnected.
//! \example Usage: connect() once, then send()/read() from a single thread.
class TcpClient {
public:
	TcpClient();
	~TcpClient();

	TcpClient(const TcpClient&)            = delete;
	TcpClient& operator=(const TcpClient&) = delete;
	TcpClient(TcpClient&&)                 = delete;
	TcpClient& operator=(TcpClient&&)      = delete;

	//! Connect to host:port. Returns false on failure or if already connected.
	bool connect(std::string host, std::uint16_t port = DEFAULT_PORT);
	bool isConnected() const;
	void disconnect();

	bool send(const Message& message); //!< Send a message with a size prefix header. Returns false on failure.
	Message read();                    //!< Read a full message. Returns empty string if disconnected or on error.

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide asio stuff in public interfaces.
};

} // namespace network
} // namespace go
