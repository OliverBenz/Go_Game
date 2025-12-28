#pragma once

#include <asio.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "networking/protocol.hpp"

namespace go {
namespace networking {

// Blocking TCP server that accepts up to two clients (Black/White) and
// acknowledges any length-prefixed message it receives.
class TcpServer {
public:
	explicit TcpServer(std::uint16_t port = DEFAULT_PORT, std::size_t max_clients = MAX_PLAYERS);
	~TcpServer();

	//! Start listening for connections on the configured port.
	void start();

	//! Stop listening and close client sessions.
	void stop();

private:
	//! Waiting for our two clients to connect.
	void accept_loop();

	void handle_client(std::shared_ptr<asio::ip::tcp::socket> socket, std::size_t client_index);
	BasicMessageHeader read_header(asio::ip::tcp::socket& socket);
	std::string read_payload(asio::ip::tcp::socket& socket, std::uint32_t expected_bytes);
	//! Send acknowledgment to the client in response to receiving a message.
	void send_ack(asio::ip::tcp::socket& socket, std::string_view message);

private:
	asio::io_context m_ioContext{};
	asio::ip::tcp::acceptor m_acceptor;

	const std::size_t m_maxClients;
	std::atomic<bool> m_isRunning{false};

	std::thread m_acceptThread;
	std::vector<std::thread> m_clientThreads;
};

} // namespace networking
} // namespace go
