#include "core/game.hpp"

namespace go {

void Game::setup(const std::size_t size) {
    m_board = Board(size);
    m_gameActive = true;
}

void Game::pushEvent(GameEvent event) {
    m_eventQueue.Push(event);
}

void Game::run() {
    while(m_gameActive) {
        const auto event = m_eventQueue.Pop();
        std::visit([&](auto&& ev) {
            handleEvent(ev);
        }, event);
    }
}

const Board& Game::board() const {
    return m_board;
}

Player Game::currentPlayer() {
    return m_currentPlayer;
}

void Game::switchTurn() {
    m_currentPlayer = m_currentPlayer == Player::Black ? Player::White : Player::Black;
}

bool Game::isValidMove(Player player, Coord c) {
    // Check valid coord for board size
    if(c.x >= m_board.size() || c.y >= m_board.size()){
        return false;
    }
    
    // Check coord free
    if(m_board.getAt(c) != Board::FieldValue::None) {
        return false;
    }
    
    // TODO: Check no board state repeat
}

void Game::handleEvent(const PutStoneEvent& event) {
    if (isValidMove(m_currentPlayer, event.c)) {
        m_board.setAt(event.c, static_cast<Board::FieldValue>(m_currentPlayer));
        switchTurn();
    }
}

void Game::handleEvent(const PassEvent& event) {
    switchTurn();
}

void Game::handleEvent(const ResignEvent& event) {
    m_gameActive = false;
}

void Game::handleEvent(const ShutdownEvent& event) {
    m_gameActive = false;
}

}