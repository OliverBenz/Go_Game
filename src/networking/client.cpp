#include "networking/client.hpp"
#include "Logging.hpp"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <stdexcept>
#include <utility>

namespace go {
namespace networking {

TcpClient::TcpClient(std::string host, std::uint16_t port)
    : m_resolver(m_ioContext), m_socket(m_ioContext), m_host(std::move(host)), m_port(port) {
}

TcpClient::~TcpClient() {
	disconnect();
}

void TcpClient::connect() {
	if (m_isConnected) {
		return;
	}

	const auto endpoints = m_resolver.resolve(m_host, std::to_string(m_port));
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

void TcpClient::send(std::string_view payload) {
	if (!m_isConnected) {
		connect();
	}

	if (payload.size() > MAX_PAYLOAD_BYTES) {
		throw std::runtime_error("Networking: payload too large for demo client");
	}

	BasicMessageHeader header{};
	header.payload_size = to_network_u32(static_cast<std::uint32_t>(payload.size()));

	std::array<asio::const_buffer, 2> buffers = {asio::buffer(&header, sizeof(header)), asio::buffer(payload.data(), payload.size())};

	// For fixed-size packets, emit exactly FIXED_PACKET_PAYLOAD_BYTES here and drop the header.
	asio::write(m_socket, buffers);
}

std::string TcpClient::read() {
	const auto header       = read_header();
	const auto payload_size = from_network_u32(header.payload_size);

	if (payload_size > MAX_PAYLOAD_BYTES) {
		throw std::runtime_error("Networking: incoming payload too large");
	}

	// For fixed-size packets, swap this for a straight read of FIXED_PACKET_PAYLOAD_BYTES bytes.
	auto payload = read_payload(payload_size);

	auto logger = Logger();
	logger.Log(Logging::LogLevel::Debug, "[Network] Client received payload: " + payload);

	return payload;
}

std::string TcpClient::send_and_receive(std::string_view payload) {
	send(payload);
	return read();
}

BasicMessageHeader TcpClient::read_header() {
	BasicMessageHeader header{};
	asio::read(m_socket, asio::buffer(&header, sizeof(header)));
	return header;
}

std::string TcpClient::read_payload(std::uint32_t expected_bytes) {
	if (expected_bytes == 0) {
		return {};
	}

	std::string payload(expected_bytes, '\0');
	asio::read(m_socket, asio::buffer(payload.data(), payload.size()));
	return payload;
}

} // namespace networking
} // namespace go
