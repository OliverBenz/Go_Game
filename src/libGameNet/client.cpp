#include "gameNet/client.hpp"

#include "network/tcpClient.hpp"

#include <atomic>
#include <cstring>
#include <sstream>
#include <thread>

namespace go::gameNet {

namespace {
// TODO: Move to protocol.
constexpr const char* SESSION_PREFIX = "SESSION:";
} // namespace

class Client::Implementation {
public:
	Implementation() = default;

	bool registerHandler(IClientHandler* handler);

	void connect(const std::string& host, std::uint16_t port);
	void disconnect();
	bool isConnected() const;

	bool send(const NwEvent& event);

	SessionId sessionId() const;

private:
	void startReadLoop(); //!< Starts background read thread for blocking reads.
	void stopReadLoop();
	void readLoop();
	void handleIncoming(const network::Message& message);

private:
	network::TcpClient m_client;
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

void Client::Implementation::connect(const std::string& host, std::uint16_t port) {
	if (m_client.isConnected()) {
		return;
	}

	m_client.connect(host, port);
	startReadLoop();
}

void Client::Implementation::disconnect() {
	m_client.disconnect();
	stopReadLoop();
}
bool Client::Implementation::isConnected() const {
	return m_client.isConnected();
}

bool Client::Implementation::send(const NwEvent& event) {
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

		try {
			const auto message = m_client.read();
			handleIncoming(message);
		} catch (...) { break; }
	}

	if (m_handler) {
		m_handler->onDisconnected();
	}
}

void Client::Implementation::handleIncoming(const network::Message& message) {
	if (message.rfind(SESSION_PREFIX, 0) == 0) {
		std::istringstream stream(message.substr(std::strlen(SESSION_PREFIX)));
		SessionId sessionId{};
		if (stream >> sessionId) {
			m_sessionId = sessionId;
			if (m_handler) {
				m_handler->onSessionAssigned(sessionId);
			}
		}
		return;
	}

	const auto event = fromMessage(message);
	if (!event) {
		return;
	}

	if (m_handler) {
		m_handler->onNetworkEvent(*event);
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

void Client::connect(const std::string& host) {
	m_pimpl->connect(host, network::DEFAULT_PORT);
}

void Client::connect(const std::string& host, std::uint16_t port) {
	m_pimpl->connect(host, port);
}

void Client::disconnect() {
	m_pimpl->disconnect();
}

bool Client::isConnected() const {
	return m_pimpl->isConnected();
}

bool Client::send(const NwEvent& event) {
	return m_pimpl->send(event);
}

SessionId Client::sessionId() const {
	return m_pimpl->sessionId();
}

} // namespace go::gameNet
