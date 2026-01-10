#include "network/tcpServer.hpp"

#include <asio/ip/tcp.hpp>

#include <utility>

namespace go::network {

static ConnectionId CONN_ID = 1u;

TcpServer::TcpServer(std::uint16_t port) : m_ioContext(), m_acceptor(m_ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
}

void TcpServer::start() {
	if (m_running.exchange(true)) {
		return;
	}

	m_ioContext.restart();
	m_workGuard.emplace(asio::make_work_guard(m_ioContext));
	doAccept();
	m_ioThread = std::thread([this]() { m_ioContext.run(); });
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

	if (m_workGuard) {
		m_workGuard->reset();
		m_workGuard.reset();
	}
	m_ioContext.stop();

	std::unordered_map<ConnectionId, Connection> connections;
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		connections.swap(m_connections);
	}
	for (auto& [id, conn]: connections) {
		conn.stop();
	}

	if (m_ioThread.joinable()) {
		m_ioThread.join();
	}
}

bool TcpServer::send(ConnectionId connectionId, Message msg) {
	std::lock_guard<std::mutex> lock(m_connectionsMutex);

	if (m_connections.contains(connectionId)) {
		m_connections.at(connectionId).send(msg);
		return true;
	}

	return false;
}

void TcpServer::reject(ConnectionId connectionId) {
	std::lock_guard<std::mutex> lock(m_connectionsMutex);

	if (m_connections.contains(connectionId)) {
		m_connections.at(connectionId).stop();
		m_connections.erase(connectionId);
	}
}

void TcpServer::doAccept() {
	m_acceptor.async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
		if (!m_running) {
			return;
		}
		if (!ec) {
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			const auto connectionId = CONN_ID++;
			if (createConnection(std::move(socket), connectionId)) {
				assert(m_connections.contains(connectionId));
				m_connections.at(connectionId).start();
			}
		}

		if (m_running) {
			doAccept();
		}
	});
}

bool TcpServer::createConnection(asio::ip::tcp::socket socket, ConnectionId connectionId) {
	if (m_connections.contains(connectionId)) {
		return false;
	}

	Connection::Callbacks callbacks;
	callbacks.onConnect = [this](Connection& connection) {
		if (m_callbacks.onConnect) {
			m_callbacks.onConnect(connection.connectionId());
		}
	};
	callbacks.onMessage = [this](Connection& connection, const Message& message) {
		if (m_callbacks.onMessage) {
			m_callbacks.onMessage(connection.connectionId(), message);
		}
	};
	callbacks.onDisconnect = [this](Connection& connection) {
		const auto index = connection.connectionId();
		asio::post(m_ioContext, [this, index] {
			std::lock_guard<std::mutex> lock(m_connectionsMutex);

			if (m_connections.contains(index)) {
				m_connections.at(index).stop();
				m_connections.erase(index);
			}
		});
		if (m_callbacks.onDisconnect) {
			m_callbacks.onDisconnect(index);
		}
	};

	const auto [it, inserted] = m_connections.try_emplace(connectionId, std::move(socket), connectionId, std::move(callbacks));
	return inserted;
}

} // namespace go::network
