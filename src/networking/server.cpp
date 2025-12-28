#include "networking/server.hpp"
#include "Logging.hpp"

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <stdexcept>
#include <utility>

namespace go {
namespace networking {

TcpServer::TcpServer(std::uint16_t port, std::size_t max_clients)
    : m_acceptor(m_ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)), m_maxClients(max_clients) {
}

TcpServer::~TcpServer() {
	stop();
}

void TcpServer::start() {
	if (m_isRunning.exchange(true)) {
		return;
	}

	m_acceptThread = std::thread([this]() { accept_loop(); });
}

void TcpServer::stop() {
	if (!m_isRunning.exchange(false)) {
		return;
	}

	asio::error_code ec;
	m_acceptor.cancel(ec);
	m_acceptor.close(ec);
	m_ioContext.stop();

	if (m_acceptThread.joinable()) {
		m_acceptThread.join();
	}

	for (auto& thread: m_clientThreads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

void TcpServer::accept_loop() {
	std::size_t connected_clients = 0;

	while (m_isRunning && connected_clients < m_maxClients) {
		asio::ip::tcp::socket socket(m_ioContext);
		asio::error_code ec;
		m_acceptor.accept(socket, ec);

		if (!m_isRunning) {
			break;
		}

		if (ec) {
			if (ec == asio::error::operation_aborted) {
				break;
			}

			auto logger = Logger();
			logger.Log(Logging::LogLevel::Error, "[Network] Accept failed - " + ec.message());

			continue;
		}

		// TODO: Verify this pattern
		auto socket_ptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));

		m_clientThreads.emplace_back([this, socket_ptr, connected_clients]() { handle_client(socket_ptr, connected_clients); });
		++connected_clients;

		auto logger = Logger();
		logger.Log(Logging::LogLevel::Info, std::format("[Network] Client {} connected from '{}'.", connected_clients,
		                                                socket_ptr->remote_endpoint().address().to_string()));
	}

	auto logger = Logger();
	logger.Log(Logging::LogLevel::Debug, "[Network] Accept loop finished. Both clients connected.");
}

void TcpServer::handle_client(std::shared_ptr<asio::ip::tcp::socket> socket, std::size_t client_index) {
	try {
		while (m_isRunning) {
			const auto header       = read_header(*socket);
			const auto payload_size = from_network_u32(header.payload_size);

			if (payload_size > MAX_PAYLOAD_BYTES) {
				throw std::runtime_error("[Network] payload too large for demo server");
			}

			// In a fixed-size protocol, skip the header and always read FIXED_PACKET_PAYLOAD_BYTES here.
			const auto payload = read_payload(*socket, payload_size);

			auto logger = Logger();
			logger.Log(Logging::LogLevel::Info, std::format("[Network] Client {} sent '{}'.", client_index + 1, payload));

			send_ack(*socket, "SUCCESS");
		}
	} catch (const std::exception& ex) {
		if (m_isRunning) {
			auto logger = Logger();
			logger.Log(Logging::LogLevel::Error, std::format("[Network] Client '{}' session ended: '{}'", (client_index + 1), ex.what()));
		}
	}
}

BasicMessageHeader TcpServer::read_header(asio::ip::tcp::socket& socket) {
	BasicMessageHeader header{};
	asio::read(socket, asio::buffer(&header, sizeof(header)));
	return header;
}

std::string TcpServer::read_payload(asio::ip::tcp::socket& socket, std::uint32_t expected_bytes) {
	if (expected_bytes == 0) {
		return {};
	}

	std::string payload(expected_bytes, '\0');
	asio::read(socket, asio::buffer(payload.data(), payload.size()));
	return payload;
}

void TcpServer::send_ack(asio::ip::tcp::socket& socket, std::string_view message) {
	BasicMessageHeader header{};
	header.payload_size = to_network_u32(static_cast<std::uint32_t>(message.size()));

	// TODO: Verify not just single write?
	std::array<asio::const_buffer, 2> buffers = {asio::buffer(&header, sizeof(header)), asio::buffer(message.data(), message.size())};
	asio::write(socket, buffers);
}

} // namespace networking
} // namespace go