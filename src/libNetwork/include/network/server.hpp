#pragma once

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "network/protocol.hpp"

namespace go {
namespace network {

//! Blocking TCP server that accepts up to two clients (Black/White) and acknowledges any length-prefixed message it receives.
class TcpServer {
public: // Callback handlers. Intentionally don't return data. Keep lightweight.
	using ConnectHandler    = std::function<void(std::size_t /*clientIndex*/, const asio::ip::tcp::endpoint&)>; //!< Inform host when a new client connects.
	using MessageHandler    = std::function<void(std::size_t /*clientIndex*/, const std::string&)>;             //!< Inform host on client message
	using DisconnectHandler = std::function<void(std::size_t /*clientIndex*/)>;                                 //!< Inform host when a client disconnects.

public:
	explicit TcpServer(std::uint16_t port = DEFAULT_PORT);
	~TcpServer();

	//! Start listening for connections on the configured port.
	void start();

	//! Stop listening and close client sessions.
	void stop();

	//! Register lifecycle hooks. These must remain cheap; heavy work should be posted to higher layers.
	void setOnConnect(ConnectHandler handler);
	void setOnMessage(MessageHandler handler);
	void setOnDisconnect(DisconnectHandler handler);

	//! Send a payload to a specific connected client. No framing beyond length prefix is applied.
	void sendToClient(std::size_t client_index, std::string_view payload);

private:
	//! Waiting for our two clients to connect. Runs in accept thread.
	void accept_loop();

	//! Runs on each client thread. Handle incoming messages. Sends ack.
	void handle_client(std::shared_ptr<asio::ip::tcp::socket> socket, std::size_t client_index);

	BasicMessageHeader read_header(asio::ip::tcp::socket& socket);
	std::string read_payload(asio::ip::tcp::socket& socket, std::uint32_t expected_bytes);

	//! Send acknowledgment to the client in response to receiving a message.
	void send_ack(asio::ip::tcp::socket& socket, std::string_view message);

	std::shared_ptr<asio::ip::tcp::socket> getClientSocket(std::size_t client_index);

private:
	asio::io_context m_ioContext{};
	asio::ip::tcp::acceptor m_acceptor;

	std::atomic<bool> m_isRunning{false};

	std::thread m_acceptThread;                           //!< Thread for listening for client connections.
	std::array<std::thread, MAX_PLAYERS> m_clientThreads; //!< Each client gets their thread. (Dont do this for observers; only players.)

	std::array<std::shared_ptr<asio::ip::tcp::socket>, MAX_PLAYERS> m_clientSockets{}; //!< Keep sockets alive for outbound sends.

	ConnectHandler m_onConnect;
	MessageHandler m_onMessage;
	DisconnectHandler m_onDisconnect;
};

} // namespace network
} // namespace go
