#include "network/tcpClient.hpp"
#include "network/protocol.hpp"

#include <asio.hpp>
#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <optional>
#include <utility>

namespace go {
namespace network {

class TcpClient::Implementation {
public:
	Implementation();

public:
	bool connect(std::string host, std::uint16_t port);
	void disconnect();
	bool isConnected() const;

	bool send(const Message& message);
	Message read();

private:
	std::optional<BasicMessageHeader> read_header();
	std::optional<Message> read_payload(std::uint32_t expected_bytes);

	asio::io_context m_ioContext{};
	asio::ip::tcp::resolver m_resolver;
	asio::ip::tcp::socket m_socket;

	bool m_isConnected{false};
};

TcpClient::Implementation::Implementation() : m_resolver(m_ioContext), m_socket(m_ioContext) {
}

bool TcpClient::Implementation::connect(std::string host, std::uint16_t port) {
	if (m_isConnected) {
		return false;
	}

	// Use error_code overloads to avoid exceptions. Catch-all is a last-resort safety net.
	try {
		asio::error_code ec;
		const auto endpoints = m_resolver.resolve(host, std::to_string(port), ec);
		if (ec) {
			return false;
		}
		asio::connect(m_socket, endpoints, ec);
		if (ec) {
			return false;
		}
	} catch (...) { return false; }

	m_isConnected = true;
	return true;
}

void TcpClient::Implementation::disconnect() {
	asio::error_code ec;
	m_socket.cancel(ec);
	m_socket.shutdown(asio::socket_base::shutdown_both, ec);
	m_socket.close(ec);
	m_isConnected = false;
}

bool TcpClient::Implementation::isConnected() const {
	return m_isConnected;
}

bool TcpClient::Implementation::send(const Message& message) {
	if (!m_isConnected) {
		return false;
	}

	if (message.size() > MAX_PAYLOAD_BYTES) {
		return false;
	}

	BasicMessageHeader header{};
	header.payload_size = to_network_u32(static_cast<std::uint32_t>(message.size()));

	std::array<asio::const_buffer, 2> buffers = {asio::buffer(&header, sizeof(header)), asio::buffer(message.data(), message.size())};

	// For fixed-size packets, emit exactly FIXED_PACKET_PAYLOAD_BYTES here and drop the header.
	try {
		asio::error_code ec;
		asio::write(m_socket, buffers, ec);
		if (ec) {
			m_isConnected = false;
			return false;
		}
	} catch (...) {
		m_isConnected = false;
		return false;
	}
	return true;
}

Message TcpClient::Implementation::read() {
	try {
		const auto header = read_header();
		if (!header) {
			return {};
		}
		const auto payload_size = from_network_u32(header->payload_size);

		if (payload_size > MAX_PAYLOAD_BYTES) {
			m_isConnected = false;
			return {};
		}

		// For fixed-size packets, swap this for a straight read of FIXED_PACKET_PAYLOAD_BYTES bytes.
		const auto payload = read_payload(payload_size);
		if (!payload) {
			return {};
		}
		return *payload;
	} catch (...) {
		m_isConnected = false;
		return {};
	}
}

std::optional<BasicMessageHeader> TcpClient::Implementation::read_header() {
	BasicMessageHeader header{};
	asio::error_code ec;
	asio::read(m_socket, asio::buffer(&header, sizeof(header)), ec);
	if (ec) {
		m_isConnected = false;
		return std::nullopt;
	}
	return header;
}

std::optional<Message> TcpClient::Implementation::read_payload(std::uint32_t expected_bytes) {
	if (expected_bytes == 0) {
		return Message{};
	}

	Message payload;
	try {
		payload.assign(expected_bytes, '\0');
	} catch (...) {
		m_isConnected = false;
		return std::nullopt;
	}
	asio::error_code ec;
	asio::read(m_socket, asio::buffer(payload.data(), payload.size()), ec);
	if (ec) {
		m_isConnected = false;
		return std::nullopt;
	}
	return payload;
}


TcpClient::TcpClient() : m_pimpl(std::make_unique<Implementation>()) {
}

TcpClient::~TcpClient() {
	disconnect();
}

bool TcpClient::connect(std::string host, std::uint16_t port) {
	return m_pimpl->connect(host, port);
}

void TcpClient::disconnect() {
	m_pimpl->disconnect();
}

bool TcpClient::isConnected() const {
	return m_pimpl->isConnected();
}

bool TcpClient::send(const Message& message) {
	return m_pimpl->send(message);
}

Message TcpClient::read() {
	return m_pimpl->read();
}

} // namespace network
} // namespace go
