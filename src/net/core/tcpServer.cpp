#include "network/tcpServer.hpp"
#include "connection.hpp"

#include <asio.hpp>
#include <asio/ip/tcp.hpp>

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>

namespace go::network {

static ConnectionId CONN_ID = 1u;

class TcpServer::Implementation {
public:
	Implementation(std::uint16_t port);

	void start();
	void connect(Callbacks callbacks);
	void stop();

	bool send(ConnectionId connectionId, const Message& msg);
	void reject(ConnectionId connectionId);

private:
	void doAccept();                                                                //!< Start async accept loop.
	bool createConnection(asio::ip::tcp::socket socket, ConnectionId connectionId); //!< Create and add new connection to map.

private:
	asio::io_context m_ioContext{};
	asio::ip::tcp::acceptor m_acceptor;
	std::optional<asio::executor_work_guard<asio::io_context::executor_type>> m_workGuard;
	bool m_acceptorReady{false};

	std::thread m_ioThread;             //!< IO context thread.
	std::atomic<bool> m_running{false}; //!< TCP Server running.

	Callbacks m_callbacks; //!< Callback functions to signal events.

	std::unordered_map<ConnectionId, std::shared_ptr<Connection>> m_connections; //!< Active connections.
	std::mutex m_connectionsMutex;                                               //!< Handle concurrency.
};


TcpServer::Implementation::Implementation(std::uint16_t port) : m_acceptor(m_ioContext) {
	// Do a manual open/bind/listen so we can stay in error_code land and avoid throws.
	asio::error_code ec;
	m_acceptor.open(asio::ip::tcp::v4(), ec);
	if (ec) {
		return;
	}
	m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
	if (ec) {
		return;
	}
	m_acceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port), ec);
	if (ec) {
		return;
	}
	m_acceptor.listen(asio::socket_base::max_listen_connections, ec);
	if (ec) {
		return;
	}
	m_acceptorReady = true;
}

void TcpServer::Implementation::start() {
	if (m_running.exchange(true)) {
		return;
	}
	// If acceptor init failed during construction, don't start a dead server.
	if (!m_acceptorReady) {
		m_running = false;
		return;
	}

	m_ioContext.restart();
	m_workGuard.emplace(asio::make_work_guard(m_ioContext));
	doAccept();
	m_ioThread = std::thread([this]() { m_ioContext.run(); });
}

void TcpServer::Implementation::connect(Callbacks callbacks) {
	m_callbacks = std::move(callbacks);
}

void TcpServer::Implementation::stop() {
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

	std::unordered_map<ConnectionId, std::shared_ptr<Connection>> connections;
	{
		std::lock_guard<std::mutex> lock(m_connectionsMutex);
		connections.swap(m_connections);
	}
	for (auto& [id, conn]: connections) {
		conn->stop();
	}

	if (m_ioThread.joinable()) {
		m_ioThread.join();
	}
}

bool TcpServer::Implementation::send(ConnectionId connectionId, const Message& msg) {
	std::lock_guard<std::mutex> lock(m_connectionsMutex);

	if (m_connections.contains(connectionId)) {
		m_connections.at(connectionId)->send(msg);
		return true;
	}

	return false;
}

void TcpServer::Implementation::reject(ConnectionId connectionId) {
	std::lock_guard<std::mutex> lock(m_connectionsMutex);

	if (m_connections.contains(connectionId)) {
		m_connections.at(connectionId)->stop();
		m_connections.erase(connectionId);
	}
}

void TcpServer::Implementation::doAccept() {
	m_acceptor.async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
		if (!m_running) {
			return;
		}
		if (!ec) {
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			const auto connectionId = CONN_ID++;
			if (createConnection(std::move(socket), connectionId)) {
				assert(m_connections.contains(connectionId));
				m_connections.at(connectionId)->start();
			}
		}

		if (m_running) {
			doAccept();
		}
	});
}

bool TcpServer::Implementation::createConnection(asio::ip::tcp::socket socket, ConnectionId connectionId) {
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
		{
			std::lock_guard<std::mutex> lock(m_connectionsMutex);
			if (m_connections.contains(index)) {
				m_connections.erase(index);
			}
		}
		if (m_callbacks.onDisconnect) {
			m_callbacks.onDisconnect(index);
		}
	};

	try {
		auto connection           = std::make_shared<Connection>(std::move(socket), connectionId, std::move(callbacks));
		const auto [it, inserted] = m_connections.try_emplace(connectionId, std::move(connection));
		return inserted;
	} catch (...) { return false; }
}


TcpServer::TcpServer(std::uint16_t port) : m_pimpl(std::make_unique<Implementation>(port)) {
}

TcpServer::~TcpServer() {
	stop();
}

void TcpServer::start() {
	m_pimpl->start();
}

void TcpServer::connect(Callbacks callbacks) {
	m_pimpl->connect(callbacks);
}

void TcpServer::stop() {
	m_pimpl->stop();
}

bool TcpServer::send(ConnectionId connectionId, const Message& msg) {
	return m_pimpl->send(connectionId, msg);
}

void TcpServer::reject(ConnectionId connectionId) {
	m_pimpl->reject(connectionId);
}


} // namespace go::network
