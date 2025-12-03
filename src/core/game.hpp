#pragma once

#include "SafeQueue.hpp"
#include "types.hpp"
#include "board.hpp"
#include "gameEvent.hpp"

namespace go::core {

using EventQueue = SafeQueue<GameEvent>;

//! Core game setup.
class Game {
public:
    Game() = default;

    //! Setup a game of certain board size without starting the game loop.
    void setup(std::size_t size);

    //! Run the main game loop/start handling the event loop.
    void run();

    //! Push an event to the event queue.
    void pushEvent(GameEvent event);

    //! Get board data for rendering.
    const Board& board() const;

    //! Get the current player to make a move.
    Player currentPlayer();

private:
    void handleEvent(const PutStoneEvent& event);
    void handleEvent(const PassEvent& event);
    void handleEvent(const ResignEvent& event);
    void handleEvent(const NetworkMoveEvent& event);
    void handleEvent(const ShutdownEvent& event);

private:
    bool m_gameActive{false};
    Player m_currentPlayer{Player::Black};

    EventQueue m_eventQueue;
    Board m_board{19u};
};

}