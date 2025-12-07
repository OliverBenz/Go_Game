#pragma once

#include "core/SafeQueue.hpp"
#include "core/types.hpp"
#include "core/board.hpp"
#include "core/gameEvent.hpp"
#include "core/moveChecker.hpp"

namespace go {

using EventQueue = SafeQueue<GameEvent>;

//! Core game setup.
class Game {
public:
    //! Setup a game of certain board size without starting the game loop.
    Game(std::size_t boardSize);

    //! Run the main game loop/start handling the event loop.
    void run();

    //! Push an event to the event queue.
    void pushEvent(GameEvent event);

    //! Get board data for rendering.
    const Board& board() const;

    //! Get the current player to make a move.
    Player currentPlayer();

private:
    void switchTurn();

    void handleEvent(const PutStoneEvent& event);
    void handleEvent(const PassEvent& event);
    void handleEvent(const ResignEvent& event);
    void handleEvent(const ShutdownEvent& event);

private:
    bool m_gameActive{false};
    Player m_currentPlayer{Player::Black};

    EventQueue m_eventQueue;
    Board m_board;

    MoveChecker m_moveChecker;
};

}