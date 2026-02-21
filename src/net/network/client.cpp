#include "network/client.hpp"

#include "network/core/tcpClient.hpp"

#include <atomic>
#include <cstring>
#include <thread>

namespace go::network {

class Client::Implementation {
public:
	Implementation() = default;

	bool registerHandler(IClientHandler* handler);

	bool connect(const std::string& host, std::uint16_t port);
	void disconnect();
	bool isConnected() const;

	bool send(const ClientEvent& event);

	SessionId sessionId() const;

private:
	void startReadLoop(); //!< Starts background read thread for blocking reads.
	void stopReadLoop();
	void readLoop();

private:
	void handleNetworkEvent(const ServerSessionAssign& event);
	void handleNetworkEvent(const ServerGameConfig& event);
	void handleNetworkEvent(const ServerDelta& event);
	void handleNetworkEvent(const ServerChat& event);

private:
	core::TcpClient m_client;
	std::atomic<bool> m_running{false}; //!< Read thread running.
	std::thread m_readThread;

	IClientHandler* m_handler{nullptr};
	SessionId m_sessionId{0};
};

bool Client::Implementation::registerHandler(IClientHandler* handler) {
	if (m_handler) {
		return false;
	}
	m_handler = handler;
	return true;
}

bool Client::Implementation::connect(const std::string& host, std::uint16_t port) {
	if (m_client.isConnected()) {
		return false;
	}

	// Start the blocking read loop only after a successful connect.
	if (m_client.connect(host, port)) {
		startReadLoop();
		return true;
	}
	return false;
}

void Client::Implementation::disconnect() {
	m_client.disconnect();
	stopReadLoop();
}
bool Client::Implementation::isConnected() const {
	return m_client.isConnected();
}

bool Client::Implementation::send(const ClientEvent& event) {
	return m_client.send(toMessage(event));
}

SessionId Client::Implementation::sessionId() const {
	return m_sessionId;
}

void Client::Implementation::startReadLoop() {
	if (m_running.exchange(true)) {
		return;
	}

	m_readThread = std::thread([this] { readLoop(); });
}

void Client::Implementation::stopReadLoop() {
	if (!m_running.exchange(false)) {
		return;
	}

	if (m_readThread.joinable() && m_readThread.get_id() != std::this_thread::get_id()) {
		m_readThread.join();
	} else if (m_readThread.joinable()) {
		m_readThread.detach();
	}
}

void Client::Implementation::readLoop() {
	while (m_running) {
		if (!m_client.isConnected()) {
			break;
		}

		// TcpClient::read is blocking; this loop lives on its own thread.
		try {
			const auto message = m_client.read();
			const auto event   = fromServerMessage(message);
			if (!event) {
				continue;
			}
			std::visit([&](const auto& e) { handleNetworkEvent(e); }, *event);
		} catch (...) { break; }
	}

	if (m_handler) {
		m_handler->onDisconnected();
	}
}

void Client::Implementation::handleNetworkEvent(const ServerSessionAssign& event) {
	if (m_sessionId != 0u) {
		// TODO: Update to handle reconnect
	}
	m_sessionId = event.sessionId;
}

void Client::Implementation::handleNetworkEvent(const ServerGameConfig& event) {
	if (m_handler) {
		m_handler->onGameConfig(event);
	}
}

void Client::Implementation::handleNetworkEvent(const ServerDelta& event) {
	if (m_handler) {
		m_handler->onGameUpdate(event);
	}
}

void Client::Implementation::handleNetworkEvent(const ServerChat& event) {
	if (m_handler) {
		m_handler->onChatMessage(event);
	}
}


Client::Client() : m_pimpl(std::make_unique<Implementation>()) {
}

Client::~Client() {
	disconnect();
}

bool Client::registerHandler(IClientHandler* handler) {
	return m_pimpl->registerHandler(handler);
}

bool Client::connect(const std::string& host) {
	return m_pimpl->connect(host, core::DEFAULT_PORT);
}

bool Client::connect(const std::string& host, std::uint16_t port) {
	return m_pimpl->connect(host, port);
}

void Client::disconnect() {
	m_pimpl->disconnect();
}

bool Client::isConnected() const {
	return m_pimpl->isConnected();
}

bool Client::send(const ClientEvent& event) {
	return m_pimpl->send(event);
}

SessionId Client::sessionId() const {
	return m_pimpl->sessionId();
}

} // namespace go::gameNet
