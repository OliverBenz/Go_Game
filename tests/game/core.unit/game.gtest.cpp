#include "core/game.hpp"

#include <gtest/gtest.h>
#include <thread>

namespace tengen::gtest {

// TODO: Verify board state after every place
TEST(Game, BoardUpdate) {
	Game game(9u);
	std::thread gameThread([&] { game.run(); });

	// Setup ?koseki?
	game.pushEvent(PutStoneEvent{Player::Black, {0u, 1u}});
	game.pushEvent(PutStoneEvent{Player::White, {0u, 2u}});
	game.pushEvent(PutStoneEvent{Player::Black, {1u, 0u}});
	game.pushEvent(PutStoneEvent{Player::White, {1u, 3u}});
	game.pushEvent(PutStoneEvent{Player::Black, {2u, 1u}});
	game.pushEvent(PutStoneEvent{Player::White, {2u, 2u}});
	game.pushEvent(PutStoneEvent{Player::Black, {1u, 2u}});

	// White takes
	game.pushEvent(PutStoneEvent{Player::White, {1u, 1u}});

	// Black cannot take (repeating board state)
	game.pushEvent(PutStoneEvent{Player::Black, {1u, 2u}});

	// Black plays somewhere else
	game.pushEvent(PutStoneEvent{Player::White, {5u, 5u}});

	// White plays somewhere else
	game.pushEvent(PutStoneEvent{Player::Black, {5u, 6u}});

	// Black takes back
	game.pushEvent(PutStoneEvent{Player::White, {1u, 2u}});


	game.pushEvent(ShutdownEvent{});
	gameThread.join();
}

} // namespace tengen::gtest