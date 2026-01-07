#include "network/client.hpp"
#include "network/protocol.hpp"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <stdexcept>
#include <utility>

namespace go {
namespace network {

TcpClient::TcpClient() : m_resolver(m_ioContext), m_socket(m_ioContext) {
}

TcpClient::~TcpClient() {
	disconnect();
}

void TcpClient::connect(std::string host, std::uint16_t port) {
	if (m_isConnected) {
		return;
	}

	const auto endpoints = m_resolver.resolve(host, std::to_string(port));
	asio::connect(m_socket, endpoints);
	m_isConnected = true;
}

void TcpClient::disconnect() {
	if (!m_isConnected) {
		return;
	}

	asio::error_code ec;
	m_socket.shutdown(asio::socket_base::shutdown_both, ec);
	m_socket.close(ec);
	m_isConnected = false;
}
bool TcpClient::isConnected() const {
	return m_isConnected;
}

bool TcpClient::send(const Message& message) {
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
	asio::write(m_socket, buffers);
	return true;
}

Message TcpClient::read() {
	const auto header       = read_header();
	const auto payload_size = from_network_u32(header.payload_size);

	if (payload_size > MAX_PAYLOAD_BYTES) {
		throw std::runtime_error("Networking: incoming payload too large");
	}

	// For fixed-size packets, swap this for a straight read of FIXED_PACKET_PAYLOAD_BYTES bytes.
	auto payload = read_payload(payload_size);

	return payload;
}

BasicMessageHeader TcpClient::read_header() {
	BasicMessageHeader header{};
	asio::read(m_socket, asio::buffer(&header, sizeof(header)));
	return header;
}

Message TcpClient::read_payload(std::uint32_t expected_bytes) {
	if (expected_bytes == 0) {
		return {};
	}

	Message payload(expected_bytes, '\0');
	asio::read(m_socket, asio::buffer(payload.data(), payload.size()));
	return payload;
}

} // namespace network
} // namespace go
