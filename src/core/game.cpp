#include "core/game.hpp"

#include "zobristHash.hpp"

namespace go {

Game::Game(const std::size_t boardSize) : m_gameActive{true}, m_position{boardSize} {
	switch (m_position.board.size()) {
	case 9u:
		m_hasher = std::make_unique<ZobristHash<9u>>();
		break;
	case 13u:
		m_hasher = std::make_unique<ZobristHash<13u>>();
		break;
	case 19u:
		m_hasher = std::make_unique<ZobristHash<19u>>();
		break;
	default:
		assert(false);
		break;
	}
	m_seenHashes.insert(m_position.hash);
}

void Game::pushEvent(GameEvent event) {
	m_eventQueue.Push(event);
}

void Game::run() {
	while (m_gameActive) {
		const auto event = m_eventQueue.Pop();
		std::visit([&](auto&& ev) { handleEvent(ev); }, event);
	}
}

const Board& Game::board() const {
	return m_position.board;
}

void Game::handleEvent(const PutStoneEvent& event) {
	assert(m_hasher);

	Position next{m_position.board.size()};
	if (isNextPositionLegal(m_position, m_position.currentPlayer, event.c, *m_hasher, m_seenHashes, next)) {
		m_position = std::move(next);
		m_seenHashes.insert(m_position.hash);
		m_notificationHandler.signalBoardChange();
	}
}

void Game::handleEvent(const PassEvent& event) {
	assert(m_hasher);

	Position next = m_position;
	next.pass(*m_hasher);

	if (m_seenHashes.contains(next.hash))
		return;

	m_position = std::move(next);
	m_seenHashes.insert(m_position.hash);
	m_notificationHandler.signalBoardChange();
}

void Game::handleEvent(const ResignEvent& event) {
	m_gameActive = false;
}

void Game::handleEvent(const ShutdownEvent& event) {
	m_gameActive = false;
}

void Game::addNotificationListener(IGameListener* listener) {
	m_notificationHandler.addListener(listener);
}

void Game::removeNotificationListener(IGameListener* listener) {
	m_notificationHandler.remListener(listener);
}

} // namespace go
