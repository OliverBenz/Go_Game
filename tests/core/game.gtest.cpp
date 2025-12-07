#include "core/game.hpp"

#include <gtest/gtest.h>
#include <thread>

namespace go::gtest {

// TODO: Verify board state after every place 
TEST(Game, BoardUpdate) {
    Game game(9u);
    std::thread gameThread([&]{
        game.run();
    });

    // Setup ?koseki?
    game.pushEvent(PutStoneEvent{0u, 1u});
    game.pushEvent(PutStoneEvent{0u, 2u});
    game.pushEvent(PutStoneEvent{1u, 0u});
    game.pushEvent(PutStoneEvent{1u, 3u});
    game.pushEvent(PutStoneEvent{2u, 1u});
    game.pushEvent(PutStoneEvent{2u, 2u});
    game.pushEvent(PutStoneEvent{1u, 2u});

    // White takes 
    game.pushEvent(PutStoneEvent{1u, 1u});

    // Black cannot take (repeating board state)
    game.pushEvent(PutStoneEvent{1u, 2u});

    // Black plays somewhere else
    game.pushEvent(PutStoneEvent{5u, 5u});

    // White plays somewhere else
    game.pushEvent(PutStoneEvent{5u, 6u});

    // Black takes back
    game.pushEvent(PutStoneEvent{1u, 2u});


    game.pushEvent(ShutdownEvent{});
    gameThread.join();
}

}