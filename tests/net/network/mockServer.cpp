#include "mockServer.hpp"

#include <format>

namespace go::gtest {

MockServer::MockServer() {
	EXPECT_TRUE(m_network.registerHandler(this));
	m_network.start();
}
MockServer::~MockServer() {
	m_network.stop();
}
void MockServer::onClientConnected(network::SessionId sessionId, network::Seat seat) {
	std::cout << std::format("[Server] Client {} connected to seat: {}\n", sessionId, static_cast<int>(seat));
	m_network.send(sessionId, network::ServerGameConfig{.boardSize = 9u, .komi = 6.5, .timeSeconds = 0u});
}

void MockServer::onClientDisconnected(network::SessionId sessionId) {
	std::cout << std::format("[Server] {} disconnected.\n", sessionId);
}

void MockServer::onNetworkEvent(network::SessionId sessionId, const network::ClientEvent& event) {
	std::visit([&](const auto& e) { handleNetworkEvent(sessionId, e); }, event);
}

void MockServer::handleNetworkEvent(network::SessionId sessionId, const network::ClientPutStone& event) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(network::ServerDelta{
	        .turn     = ++m_turn,
	        .seat     = seat,
	        .action   = network::ServerAction::Place,
	        .coord    = event.c,
	        .captures = {},
	        .next     = nextSeat(seat),
	        .status   = network::GameStatus::Active,
	});
}

void MockServer::handleNetworkEvent(network::SessionId sessionId, const network::ClientPass&) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(network::ServerDelta{
	        .turn     = ++m_turn,
	        .seat     = seat,
	        .action   = network::ServerAction::Pass,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = nextSeat(seat),
	        .status   = network::GameStatus::Active,
	});
}

void MockServer::handleNetworkEvent(network::SessionId sessionId, const network::ClientResign&) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(network::ServerDelta{
	        .turn     = ++m_turn,
	        .seat     = seat,
	        .action   = network::ServerAction::Resign,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = nextSeat(seat),
	        .status   = seat == network::Seat::Black ? network::GameStatus::WhiteWin : network::GameStatus::BlackWin,
	});
}

void MockServer::handleNetworkEvent(network::SessionId sessionId, const network::ClientChat& event) {
	static unsigned messageId = 0u;

	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(network::ServerChat{seat == network::Seat::Black ? Player::Black : Player::White, messageId++, event.message});
}

network::Seat MockServer::nextSeat(network::Seat seat) const {
	if (seat == network::Seat::Black) {
		return network::Seat::White;
	}
	if (seat == network::Seat::White) {
		return network::Seat::Black;
	}
	return network::Seat::Observer;
}


} // namespace go::gtest
