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

void Game::handleEvent(const PutStoneEvent& event) {
    m_board.setAt(event.c, static_cast<Board::FieldValue>(event.player));
    m_turnBlack = !m_turnBlack;
}

void Game::handleEvent(const PassEvent& event) {
    m_turnBlack = !m_turnBlack;
}

void Game::handleEvent(const ResignEvent& event) {
    m_gameActive = false;
}

void Game::handleEvent(const NetworkMoveEvent& event) {
    // TODO: 
}

void Game::handleEvent(const ShutdownEvent& event) {
    m_gameActive = false;
}

}