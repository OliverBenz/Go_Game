#include "gameNet/client.hpp"

#include <cstring>
#include <sstream>

namespace go::gameNet {

namespace {
constexpr const char* SESSION_PREFIX = "SESSION:";
}

Client::Client() = default;

Client::~Client() {
	disconnect();
}

bool Client::registerHandler(IClientHandler* handler) {
	if (m_handler) {
		return false;
	}
	m_handler = handler;
	return true;
}

void Client::connect(const std::string& host, std::uint16_t port) {
	if (m_client.isConnected()) {
		return;
	}

	m_client.connect(host, port);
	startReadLoop();
}

void Client::disconnect() {
	m_client.disconnect();
	stopReadLoop();
}

bool Client::send(const NwEvent& event) {
	return m_client.send(toMessage(event));
}

bool Client::isConnected() const {
	return m_client.isConnected();
}

SessionId Client::sessionId() const {
	return m_sessionId;
}

void Client::startReadLoop() {
	if (m_running.exchange(true)) {
		return;
	}

	m_readThread = std::thread([this] { readLoop(); });
}

void Client::stopReadLoop() {
	if (!m_running.exchange(false)) {
		return;
	}

	if (m_readThread.joinable() && m_readThread.get_id() != std::this_thread::get_id()) {
		m_readThread.join();
	} else if (m_readThread.joinable()) {
		m_readThread.detach();
	}
}

void Client::readLoop() {
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

void Client::handleIncoming(const network::Message& message) {
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

} // namespace go::gameNet
