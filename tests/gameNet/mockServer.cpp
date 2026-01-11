#include "mockServer.hpp"

namespace go::gtest {

MockServer::MockServer() {
	EXPECT_TRUE(m_network.registerHandler(this));
	m_network.start();
}
MockServer::~MockServer() {
	m_network.stop();
}
void MockServer::onClientConnected(gameNet::SessionId sessionId, gameNet::Seat seat) {
	std::cout << std::format("[Server] Client {} connected to seat: {}\n", sessionId, static_cast<int>(seat));
}

void MockServer::onClientDisconnected(gameNet::SessionId sessionId) {
	std::cout << std::format("[Server] {} disconnected.\n", sessionId);
}

void MockServer::onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) {
	std::visit([&](const auto& e) { handleNetworkEvent(sessionId, e); }, event);
}

void MockServer::handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientPutStone& event) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(gameNet::ServerDelta{
	        .turn     = ++m_turn,
	        .seat     = seat,
	        .action   = gameNet::ServerAction::Place,
	        .coord    = event.c,
	        .captures = {},
	        .next     = nextSeat(seat),
	        .status   = gameNet::GameStatus::Active,
	});
}

void MockServer::handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientPass&) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(gameNet::ServerDelta{
	        .turn     = ++m_turn,
	        .seat     = seat,
	        .action   = gameNet::ServerAction::Pass,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = nextSeat(seat),
	        .status   = gameNet::GameStatus::Active,
	});
}

void MockServer::handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientResign&) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(gameNet::ServerDelta{
	        .turn     = ++m_turn,
	        .seat     = seat,
	        .action   = gameNet::ServerAction::Resign,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = nextSeat(seat),
	        .status   = seat == gameNet::Seat::Black ? gameNet::GameStatus::WhiteWin : gameNet::GameStatus::BlackWin,
	});
}

void MockServer::handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientChat& event) {
	const auto seat = m_network.getSeat(sessionId);
	m_network.broadcast(gameNet::ServerChat{.seat = seat, .message = event.message});
}

gameNet::Seat MockServer::nextSeat(gameNet::Seat seat) const {
	if (seat == gameNet::Seat::Black) {
		return gameNet::Seat::White;
	}
	if (seat == gameNet::Seat::White) {
		return gameNet::Seat::Black;
	}
	return gameNet::Seat::Observer;
}


} // namespace go::gtest
