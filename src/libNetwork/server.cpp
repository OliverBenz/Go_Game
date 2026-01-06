#include "network/server.hpp"

#include <asio/ip/tcp.hpp>

#include <utility>

namespace go::network {

TcpServer::TcpServer(std::uint16_t port) : m_ioContext(), m_acceptor(m_ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
}

void TcpServer::start() {
	if (m_running.exchange(true)) {
		return;
	}

	m_acceptThread = std::thread([this]() { acceptLoop(); });
}

void TcpServer::connect(Callbacks callbacks) {
	m_callbacks = std::move(callbacks);
}

void TcpServer::stop() {
	if (!m_running.exchange(false)) {
		return;
	}

	asio::error_code ec;
	m_acceptor.cancel(ec);
	m_acceptor.close(ec);
	m_ioContext.stop();

	std::array<std::shared_ptr<Connection>, MAX_PLAYERS> connections;
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		connections = m_connections;
	}

	for (auto& connection: connections) {
		if (connection) {
			connection->stop();
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		for (auto& connection: m_connections) {
			connection.reset();
		}
	}

	if (m_acceptThread.joinable()) {
		m_acceptThread.join();
	}
}

bool TcpServer::send(SessionId sessionId, Message msg) {
	// TODO: Should be handled more clean.
	std::lock_guard<std::mutex> lock(m_connectionsMutex);
	for (const auto& connection: m_connections) {
		if (connection && connection->sessionId() == sessionId) {
			connection->send(msg);
			return true;
		}
	}
	return false;
}

void TcpServer::reject(SessionId sessionId) {
	std::lock_guard<std::mutex> lock(m_connectionsMutex);
	auto it = std::ranges::find_if(m_connections, [&](const auto& conn) { return conn && conn->sessionId() == sessionId; });

	if (it != m_connections.end()) {
		(*it)->stop();
		it->reset(); // remove from array
	}
}

void TcpServer::acceptLoop() {
	while (m_running) {
		asio::ip::tcp::socket socket(m_ioContext);
		asio::error_code ec;
		m_acceptor.accept(socket, ec);

		if (!m_running) {
			break;
		}

		if (ec) {
			if (ec == asio::error::operation_aborted) {
				break;
			}
			continue;
		}

		std::size_t slot = MAX_PLAYERS;
		{
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			for (std::size_t i = 0; i < m_connections.size(); ++i) {
				if (!m_connections[i]) {
					slot = i;
					break;
				}
			}
		}

		if (slot == MAX_PLAYERS) {
			asio::error_code closeEc;
			socket.shutdown(asio::socket_base::shutdown_both, closeEc);
			socket.close(closeEc);
			continue;
		}

		auto connection = createConnection(std::move(socket), slot);
		{
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			m_connections[slot] = connection;
		}

		connection->start();
	}
}

std::shared_ptr<Connection> TcpServer::createConnection(asio::ip::tcp::socket socket, std::size_t clientIndex) {
	Connection::Callbacks callbacks;
	callbacks.onConnect = [this](Connection& connection) {
		if (m_callbacks.onConnect) {
			m_callbacks.onConnect(connection.sessionId());
		}
	};
	callbacks.onMessage = [this](Connection& connection, const Message& message) {
		if (m_callbacks.onMessage) {
			m_callbacks.onMessage(connection.sessionId(), message);
		}
	};
	callbacks.onDisconnect = [this](Connection& connection) {
		const auto index = connection.clientIndex();
		const auto sessionId = connection.sessionId();
		asio::post(m_ioContext, [this, index, sessionId] {
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			if (index < m_connections.size()) {
				if (m_connections[index] && m_connections[index]->sessionId() == sessionId) {
					m_connections[index].reset();
				}
			}
		});
		if (m_callbacks.onDisconnect) {
			m_callbacks.onDisconnect(sessionId);
		}
	};

	return std::make_shared<Connection>(std::move(socket), clientIndex, std::move(callbacks));
}

} // namespace go::network
