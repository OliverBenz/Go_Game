#pragma once

#include <asio.hpp>

#include <cstdint>
#include <string>
#include <string_view>

#include "network/protocol.hpp"

namespace go {
namespace network {

// Minimal synchronous TCP client that sends a length-prefixed packet and waits for the server to acknowledge it.
class TcpClient {
public:
	TcpClient();
	~TcpClient();

	void connect(std::string host, std::uint16_t port = DEFAULT_PORT);
	void disconnect();

	bool send(NwEvent event);
	std::string read();

	bool isConnected() const;

private:
	bool send(std::string_view payload);
	std::string ping(std::string_view payload); //!< Send payload and receive ack.

	BasicMessageHeader read_header();
	std::string read_payload(std::uint32_t expected_bytes);

private:
	asio::io_context m_ioContext{};
	asio::ip::tcp::resolver m_resolver;
	asio::ip::tcp::socket m_socket;

	bool m_isConnected{false};
};

} // namespace network
} // namespace go