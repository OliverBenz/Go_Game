#include "core/game.hpp"
#include "core/moveChecker.hpp"

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
	// TODO: Other threads access this. We ensure game thread does not update board during a read.
	return m_position.board;
}

Player Game::currentPlayer() const {
	return m_position.currentPlayer;
}

bool Game::isActive() const {
	return m_gameActive;
}

void Game::handleEvent(const PutStoneEvent& event) {
	assert(m_hasher);

	if (event.player != m_position.currentPlayer) {
		return;
	}

	Position next{m_position.board.size()};
	if (isNextPositionLegal(m_position, m_position.currentPlayer, event.c, *m_hasher, m_seenHashes, next)) {
		m_consecutivePasses = 0;

		m_position = std::move(next);
		m_seenHashes.insert(m_position.hash);

		m_eventHub.signal(GS_BoardChange);
		m_eventHub.signal(GS_PlayerChange);
	}
}

void Game::handleEvent(const PassEvent& event) {
	assert(m_hasher);

	if (event.player != m_position.currentPlayer) {
		return;
	}

	++m_consecutivePasses;
	if (m_consecutivePasses == 2) {
		m_gameActive = false;
		m_eventHub.signal(GS_StateChange);
		return;
	}

	Position next = m_position;
	next.pass(*m_hasher);

	if (m_seenHashes.contains(next.hash)) {
		return;
	}

	m_position = std::move(next);
	m_seenHashes.insert(m_position.hash);

	m_eventHub.signal(GS_PlayerChange);
}

void Game::handleEvent(const ResignEvent&) {
	m_gameActive = false;

	m_eventHub.signal(GS_StateChange);
}

void Game::handleEvent(const ShutdownEvent&) {
	m_gameActive = false;
}

void Game::subscribeEvents(IGameListener* listener, uint64_t signalMask) {
	m_eventHub.subscribe(listener, signalMask);
}

void Game::unsubscribeEvents(IGameListener* listener) {
	m_eventHub.unsubscribe(listener);
}

} // namespace go
