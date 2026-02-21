#include "network/server.hpp"

#include "SafeQueue.hpp"
#include "network/core/tcpServer.hpp"
#include "serverEvents.hpp"
#include "sessionManager.hpp"

#include <atomic>
#include <thread>

namespace go::network {

class Server::Implementation {
public:
	explicit Implementation(std::uint16_t port);

	void start();
	void stop();
	bool registerHandler(IServerHandler* handler);

	bool send(SessionId sessionId, const ServerEvent& event); //!< Send event to client with given sessionId.
	bool broadcast(const ServerEvent& event);                 //!< Send event to all connected clients.

	Seat getSeat(SessionId sessionId) const; //!< Get the seat connection with a sessionId.

private:
	void serverLoop();                                //!< Server thread: drain queue and act.
	void processEvent(const ServerQueueEvent& event); //!< Server loop calls this. Reads event type and distributes.

	Seat freeSeat() const;

private:
	// Network callbacks (run on libNetwork threads) just enqueue events.
	void onClientConnected(core::ConnectionId connectionId);
	void onClientMessage(core::ConnectionId connectionId, const core::Message& payload);
	void onClientDisconnected(core::ConnectionId connectionId);

private:
	// Processing of server events.
	void processClientMessage(const ServerQueueEvent& event);    //!< Translate payload to network event and handle.
	void processClientConnect(const ServerQueueEvent& event);    //!< Creates session key.
	void processClientDisconnect(const ServerQueueEvent& event); //!< Destroys session key.
	void processShutdown(const ServerQueueEvent& event);         //!< Shutdown server.

private:
	std::atomic<bool> m_isRunning{false};
	std::thread m_serverThread;

	SessionManager m_sessionManager;
	core::TcpServer m_network;

	IServerHandler* m_handler{nullptr};       //!< The class that will handle server events.
	SafeQueue<ServerQueueEvent> m_eventQueue; //!< Event queue between network threads and server thread.
};

Server::Implementation::Implementation(std::uint16_t port) : m_network{port} {
	// Wire up network callbacks but keep them thin: they only enqueue events.
	core::TcpServer::Callbacks callbacks;
	callbacks.onConnect    = [this](core::ConnectionId connectionId) { return onClientConnected(connectionId); };
	callbacks.onMessage    = [this](core::ConnectionId connectionId, const core::Message& payload) { return onClientMessage(connectionId, payload); };
	callbacks.onDisconnect = [this](core::ConnectionId connectionId) { onClientDisconnected(connectionId); };
	m_network.connect(callbacks);
}

void Server::Implementation::start() {
	if (m_isRunning.exchange(true)) {
		return;
	}

	// Network runs on its own IO thread; serverLoop drains the queue on its own thread.
	m_network.start();
	m_serverThread = std::thread([this] { serverLoop(); });
}

void Server::Implementation::stop() {
	// Wake serverLoop and stop network.
	if (m_isRunning.exchange(false)) {
		m_eventQueue.Push(ServerQueueEvent{.type = ServerQueueEventType::Shutdown});
	}
	m_network.stop();

	try {
		m_eventQueue.Release();
	} catch (...) {
		// Release never throws, but keep intent explicit.
	}

	if (m_serverThread.joinable()) {
		m_serverThread.join();
	}
}

bool Server::Implementation::registerHandler(IServerHandler* handler) {
	if (m_handler) {
		return false;
	}
	m_handler = handler;
	return true;
}

bool Server::Implementation::send(SessionId sessionId, const ServerEvent& event) {
	const auto connectionId = m_sessionManager.getConnectionId(sessionId);
	if (!connectionId) {
		return false;
	}
	const auto message = toMessage(event);
	if (message.empty()) {
		return false;
	}
	return m_network.send(connectionId, message);
}

bool Server::Implementation::broadcast(const ServerEvent& event) {
	const auto message = toMessage(event);
	if (message.empty()) {
		return false;
	}
	bool anySent = false;

	m_sessionManager.forEachSession([&](const SessionContext& context) {
		if (!context.isActive || context.seat == Seat::None) {
			return;
		}
		if (m_network.send(context.connectionId, message)) {
			anySent = true;
		}
	});

	return anySent;
}

Seat Server::Implementation::getSeat(SessionId sessionId) const {
	return m_sessionManager.getSeat(sessionId);
}

void Server::Implementation::onClientConnected(core::ConnectionId connectionId) {
	m_eventQueue.Push(ServerQueueEvent{.type = ServerQueueEventType::ClientConnected, .connectionId = connectionId});
}

void Server::Implementation::onClientMessage(core::ConnectionId connectionId, const core::Message& payload) {
	m_eventQueue.Push(ServerQueueEvent{
	        .type         = ServerQueueEventType::ClientMessage,
	        .connectionId = connectionId,
	        .payload      = payload,
	});
}

void Server::Implementation::onClientDisconnected(core::ConnectionId connectionId) {
	m_eventQueue.Push(ServerQueueEvent{.type = ServerQueueEventType::ClientDisconnected, .connectionId = connectionId});
}

void Server::Implementation::serverLoop() {
	while (m_isRunning) {
		try {
			const auto event = m_eventQueue.Pop();
			processEvent(event);
		} catch (const std::exception&) {
			if (!m_isRunning) {
				break;
			}
		}
	}
}

void Server::Implementation::processEvent(const ServerQueueEvent& event) {
	switch (event.type) {
	case ServerQueueEventType::ClientConnected:
		processClientConnect(event);
		break;
	case ServerQueueEventType::ClientDisconnected:
		processClientDisconnect(event);
		break;
	case ServerQueueEventType::ClientMessage:
		processClientMessage(event);
		break;
	case ServerQueueEventType::Shutdown:
		processShutdown(event);
		break;
	}
}

void Server::Implementation::processClientConnect(const ServerQueueEvent& event) {
	// TODO: Possible to have this connectionId already registered? Yes, reconnect! Not thandled yet
	const auto sessionId = m_sessionManager.add(event.connectionId);
	const auto seat      = freeSeat();

	// Store sessionId & send to client
	m_sessionManager.setSeat(sessionId, seat);
	send(sessionId, ServerSessionAssign{.sessionId = sessionId});

	if (m_handler) {
		m_handler->onClientConnected(sessionId, seat);
	}
}

void Server::Implementation::processClientMessage(const ServerQueueEvent& event) {
	const auto sessionId = m_sessionManager.getSessionId(event.connectionId);
	if (!sessionId) {
		return;
	}

	const auto seat = m_sessionManager.getSeat(sessionId);
	if (!isPlayer(seat)) {
		return; // Non players don't get to do stuff.
	}

	// Server event message contains a network event. Parse and handle.
	const auto networkEvent = network::fromClientMessage(event.payload);
	if (!networkEvent) {
		return;
	}

	// Forward client intent to the game/app layer.
	if (m_handler) {
		m_handler->onNetworkEvent(sessionId, *networkEvent);
	}
}

void Server::Implementation::processClientDisconnect(const ServerQueueEvent& event) {
	const auto sessionId = m_sessionManager.getSessionId(event.connectionId);
	if (!sessionId) {
		return; // Should never happen
	}

	// Remove session
	const auto seat = m_sessionManager.getSeat(sessionId);
	m_sessionManager.setDisconnected(sessionId);

	if (m_handler && isPlayer(seat)) {
		m_handler->onClientDisconnected(sessionId); // Server might want to pause timer.
	}
}

void Server::Implementation::processShutdown(const ServerQueueEvent&) {
	m_isRunning = false;
}

Seat Server::Implementation::freeSeat() const {
	if (!m_sessionManager.getConnectionIdBySeat(Seat::Black)) {
		return Seat::Black;
	}
	if (!m_sessionManager.getConnectionIdBySeat(Seat::White)) {
		return Seat::White;
	}
	return Seat::Observer;
}


Server::Server() : m_pimpl(std::make_unique<Implementation>(core::DEFAULT_PORT)) {
}

Server::Server(std::uint16_t port) : m_pimpl(std::make_unique<Implementation>(port)) {
}

Server::~Server() {
	stop();
}

void Server::start() {
	m_pimpl->start();
}

void Server::stop() {
	m_pimpl->stop();
}

bool Server::registerHandler(IServerHandler* handler) {
	return m_pimpl->registerHandler(handler);
}

bool Server::send(SessionId sessionId, const ServerEvent& event) {
	return m_pimpl->send(sessionId, event);
}

bool Server::broadcast(const ServerEvent& event) {
	return m_pimpl->broadcast(event);
}

Seat Server::getSeat(SessionId sessionId) const {
	return m_pimpl->getSeat(sessionId);
}

} // namespace go::gameNet
