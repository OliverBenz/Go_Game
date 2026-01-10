#pragma once

#include "network/protocol.hpp"

#include <asio.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace go {
namespace network {

//! Minimal synchronous TCP client.
class TcpClient {
public:
	TcpClient();
	~TcpClient();

	void connect(std::string host, std::uint16_t port = DEFAULT_PORT);
	void disconnect();

	bool send(const Message& message);
	Message read();

	bool isConnected() const;

private:
	BasicMessageHeader read_header();
	Message read_payload(std::uint32_t expected_bytes);

private:
	asio::io_context m_ioContext{};
	asio::ip::tcp::resolver m_resolver;
	asio::ip::tcp::socket m_socket;

	bool m_isConnected{false};
};

} // namespace network
} // namespace go
