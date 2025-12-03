#pragma once

#include "core/SafeQueue.hpp"
#include "core/types.hpp"
#include "core/board.hpp"
#include "core/gameEvent.hpp"

namespace go {

using EventQueue = SafeQueue<GameEvent>;

//! Core game setup.
class Game {
public:
    Game() = default;

    void setup(std::size_t size);
    void run();

    void pushEvent(GameEvent event);

private:
    void handleEvent(const PutStoneEvent& event);
    void handleEvent(const PassEvent& event);
    void handleEvent(const ResignEvent& event);
    void handleEvent(const NetworkMoveEvent& event);
    void handleEvent(const ShutdownEvent& event);

private:
    bool m_gameActive{false};
    bool m_turnBlack{true};

    EventQueue m_eventQueue;
    Board m_board{19u};
};

}