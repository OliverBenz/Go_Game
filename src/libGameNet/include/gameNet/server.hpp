#pragma once

#include "gameNet/SafeQueue.hpp"
#include "gameNet/nwEvents.hpp"
#include "gameNet/sessionManager.hpp"
#include "network/tcpServer.hpp"

#include <thread>

namespace go::gameNet {

struct ServerEvent;

class IServerHandler {
public:
	virtual ~IServerHandler()                                              = default;
	virtual void onClientConnected(SessionId sessionId, Seat seat)         = 0;
	virtual void onClientDisconnected(SessionId sessionId)                 = 0;
	virtual void onNetworkEvent(SessionId sessionId, const NwEvent& event) = 0;
};

class Server {
public:
	explicit Server(std::uint16_t port = network::DEFAULT_PORT);
	~Server();

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

} // namespace go::gameNet
