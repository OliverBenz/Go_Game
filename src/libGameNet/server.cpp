#include "gameNet/server.hpp"

#include "SafeQueue.hpp"
#include "network/tcpServer.hpp"
#include "serverEvents.hpp"
#include "sessionManager.hpp"

#include <atomic>
#include <thread>

namespace go::gameNet {

class Server::Implementation {
public:
	explicit Implementation(std::uint16_t port);

	void start();
	void stop();
	bool registerHandler(IServerHandler* handler);

	bool send(SessionId sessionId, const NwEvent& event); //!< Send event to client with given sessionId.
	bool broadcast(const NwEvent& event);                 //!< Send event to all connected clients.

	Seat getSeat(SessionId sessionId) const; //!< Get the seat connection with a sessionId.

private:
	void serverLoop();                           //!< Server thread: drain queue and act.
	void processEvent(const ServerEvent& event); //!< Server loop calls this. Reads event type and distributes.

	Seat freeSeat() const;

private:
	// Network callbacks (run on libNetwork threads) just enqueue events.
	void onClientConnected(network::ConnectionId connectionId);
	void onClientMessage(network::ConnectionId connectionId, const network::Message& payload);
	void onClientDisconnected(network::ConnectionId connectionId);

private:
	// Processing of server events.
	void processClientMessage(const ServerEvent& event);    //!< Translate payload to network event and handle.
	void processClientConnect(const ServerEvent& event);    //!< Creates session key.
	void processClientDisconnect(const ServerEvent& event); //!< Destroys session key.
	void processShutdown(const ServerEvent& event);         //!< Shutdown server.

private:
	std::atomic<bool> m_isRunning{false};
	std::thread m_serverThread;

	SessionManager m_sessionManager;
	network::TcpServer m_network;

	IServerHandler* m_handler{nullptr};  //!< The class that will handle server events.
	SafeQueue<ServerEvent> m_eventQueue; //!< Event queue between network threads and server thread.
};

Server::Implementation::Implementation(std::uint16_t port) : m_network{port} {
	// Wire up network callbacks but keep them thin: they only enqueue events.
	network::TcpServer::Callbacks callbacks;
	callbacks.onConnect    = [this](network::ConnectionId connectionId) { return onClientConnected(connectionId); };
	callbacks.onMessage    = [this](network::ConnectionId connectionId, const network::Message& payload) { return onClientMessage(connectionId, payload); };
	callbacks.onDisconnect = [this](network::ConnectionId connectionId) { onClientDisconnected(connectionId); };
	m_network.connect(callbacks);
}

void Server::Implementation::start() {
	if (m_isRunning.exchange(true)) {
		return;
	}

	m_network.start();
	m_serverThread = std::thread([this] { serverLoop(); });
}

void Server::Implementation::stop() {
	// Wake serverLoop and stop network.
	if (m_isRunning.exchange(false)) {
		m_eventQueue.Push(ServerEvent{.type = ServerEventType::Shutdown});
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

bool Server::Implementation::send(SessionId sessionId, const NwEvent& event) {
	const auto connectionId = m_sessionManager.getConnectionId(sessionId);
	if (!connectionId) {
		return false;
	}
	return m_network.send(connectionId, toMessage(event));
}

bool Server::Implementation::broadcast(const NwEvent& event) {
	const auto message = toMessage(event);
	bool anySent       = false;

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

void Server::Implementation::onClientConnected(network::ConnectionId connectionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientConnected, .connectionId = connectionId});
}

void Server::Implementation::onClientMessage(network::ConnectionId connectionId, const network::Message& payload) {
	m_eventQueue.Push(ServerEvent{
	        .type         = ServerEventType::ClientMessage,
	        .connectionId = connectionId,
	        .payload      = payload,
	});
}

void Server::Implementation::onClientDisconnected(network::ConnectionId connectionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientDisconnected, .connectionId = connectionId});
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

void Server::Implementation::processEvent(const ServerEvent& event) {
	switch (event.type) {
	case ServerEventType::ClientConnected:
		processClientConnect(event);
		break;
	case ServerEventType::ClientDisconnected:
		processClientDisconnect(event);
		break;
	case ServerEventType::ClientMessage:
		processClientMessage(event);
		break;
	case ServerEventType::Shutdown:
		processShutdown(event);
		break;
	}
}

void Server::Implementation::processClientConnect(const ServerEvent& event) {
	// TODO: Possible to have this connectionId already registered?
	const auto sessionId = m_sessionManager.add(event.connectionId);
	const auto seat      = freeSeat();

	// If we have a player free
	m_sessionManager.setSeat(sessionId, seat);
	m_network.send(event.connectionId, "SESSION:" + std::to_string(sessionId)); // TODO: Move to protocol

	if (m_handler && isPlayer(seat)) {
		m_handler->onClientConnected(sessionId, seat);
	}
}

void Server::Implementation::processClientMessage(const ServerEvent& event) {
	const auto sessionId = m_sessionManager.getSessionId(event.connectionId);
	if (!sessionId) {
		return;
	}

	const auto seat = m_sessionManager.getSeat(sessionId);
	if (!isPlayer(seat)) {
		return; // Non players don't get to do stuff.
	}

	// Server event message contains a network event. Parse and handle.
	const auto networkEvent = gameNet::fromMessage(event.payload);
	if (!networkEvent) {
		return;
	}

	if (m_handler) {
		m_handler->onNetworkEvent(sessionId, *networkEvent);
	}
}

void Server::Implementation::processClientDisconnect(const ServerEvent& event) {
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

void Server::Implementation::processShutdown(const ServerEvent&) {
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


Server::Server() : m_pimpl(std::make_unique<Implementation>(network::DEFAULT_PORT)) {
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

bool Server::send(SessionId sessionId, const NwEvent& event) {
	return m_pimpl->send(sessionId, event);
}

bool Server::broadcast(const NwEvent& event) {
	return m_pimpl->broadcast(event);
}

Seat Server::getSeat(SessionId sessionId) const {
	return m_pimpl->getSeat(sessionId);
}

} // namespace go::gameNet
