#include "gameNet/server.hpp"

#include "serverEvents.hpp"

namespace go::gameNet {

Server::Server(std::uint16_t port) : m_network{port} {
	// Wire up network callbacks but keep them thin: they only enqueue events.
	network::TcpServer::Callbacks callbacks;
	callbacks.onConnect    = [this](network::ConnectionId connectionId) { return onClientConnected(connectionId); };
	callbacks.onMessage    = [this](network::ConnectionId connectionId, const network::Message& payload) { return onClientMessage(connectionId, payload); };
	callbacks.onDisconnect = [this](network::ConnectionId connectionId) { onClientDisconnected(connectionId); };
	m_network.connect(callbacks);
}

Server::~Server() {
	stop();
}

void Server::start() {
	if (m_isRunning.exchange(true)) {
		return;
	}

	m_network.start();
	serverLoop();
}

void Server::stop() {
	if (!m_isRunning.exchange(false)) {
		return;
	}

	// Wake serverLoop and stop network.
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::Shutdown});
	m_network.stop();

	try {
		m_eventQueue.Release();
	} catch (...) {
		// Release never throws, but keep intent explicit.
	}
}

bool Server::registerHandler(IServerHandler* handler) {
    if (m_handler) {
        return false;
    }
    m_handler = handler;
    return true;
}

bool Server::send(SessionId sessionId, const NwEvent& event) {
	const auto connectionId = m_sessionManager.getConnectionId(sessionId);
	if (!connectionId) {
		return false;
	}
	return m_network.send(connectionId, toMessage(event));
}

bool Server::broadcast(const NwEvent& event) {
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
Seat Server::getSeat(SessionId sessionId) const {
	return m_sessionManager.getSeat(sessionId);
}

void Server::onClientConnected(network::ConnectionId connectionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientConnected, .connectionId = connectionId});
}

void Server::onClientMessage(network::ConnectionId connectionId, const network::Message& payload) {
	m_eventQueue.Push(ServerEvent{
	        .type         = ServerEventType::ClientMessage,
	        .connectionId = connectionId,
	        .payload      = payload,
	});
}

void Server::onClientDisconnected(network::ConnectionId connectionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientDisconnected, .connectionId = connectionId});
}


void Server::serverLoop() {
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


void Server::processEvent(const ServerEvent& event) {
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

void Server::processClientConnect(const ServerEvent& event) {
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

void Server::processClientMessage(const ServerEvent& event) {
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

void Server::processClientDisconnect(const ServerEvent& event) {
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

void Server::processShutdown(const ServerEvent&) {
	m_isRunning = false;
}

Seat Server::freeSeat() const {
	if (!m_sessionManager.getConnectionIdBySeat(Seat::Black)) {
		return Seat::Black;
	}
	if (!m_sessionManager.getConnectionIdBySeat(Seat::White)) {
		return Seat::White;
	}
	return Seat::Observer;
}

} // namespace go::gameNet
