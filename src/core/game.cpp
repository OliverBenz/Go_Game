#include "core/game.hpp"

namespace go {

Game::Game(const std::size_t boardSize)
    : m_gameActive{true}, m_board{boardSize}, m_moveChecker{m_board}
{}

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

void Game::handleEvent(const PutStoneEvent& event) {
    if (m_moveChecker.isValidMove(m_currentPlayer, event.c)) {
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