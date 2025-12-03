#pragma once

#include "boardLogic.hpp"
#include <variant>

namespace go::core {

enum class Player {
    Black = FieldValue::Black,
    White = FieldValue::White
};


using EventQueue = SafeQueue<GameEvent>;

//! Core game setup.
class Game {
public:
    Game(){}

    void pushEvent(Event event) {
        m_eventQueue.Push(event);
    }

    void run() {
        while(m_gameActive) {
            const auto event = m_eventQueue.Pop();
            std::visit([&](auto&& ev) {
                handleEvent(ev);
            }, event);
        }
    }

private:
    void handleEvent(const PutStoneEvent& event) {
        m_board.setAt(event.c, static_cast<FieldValue>(event.player));
        m_turnBlack = !m_turnBlack;
    }
    void handleEvent(const PassEvent& event) {
        m_turnBlack = !m_turnBlack;
    }
    void handleEvent(const ResignEvent& event) {
        m_gameActive = false;
    }
    void handleEvent(const NetworkMoveEvent& event) {
        // TODO: 
    }
    void handleEvent(const ShutdownEvent& event) {
        m_gameActive = false;
    }

private:
    bool m_gameActive{true};
    bool m_turnBlack{true};

    EventQueue m_eventQueue;
    Board m_board;
};

}