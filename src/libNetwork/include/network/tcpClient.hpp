#pragma once

#include "network/protocol.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace go {
namespace network {

//! Minimal synchronous TCP client.
class TcpClient {
public:
	TcpClient();
	~TcpClient();

	void connect(std::string host, std::uint16_t port = DEFAULT_PORT);
	bool isConnected() const;
	void disconnect();

	bool send(const Message& message);
	Message read();

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide asio stuff in public interfaces.
};

} // namespace network
} // namespace go
