#include "core/moveChecker.hpp"
#include "model/board.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

// Liberties of single stones at all board positions
TEST(MoveChecker, ComputeConnectedLiberties_Single) {
	Board board(9u);
	// In corner
	EXPECT_EQ(computeGroupLiberties(board, {0u, 0u}, Player::Black), 2u);
	EXPECT_EQ(computeGroupLiberties(board, {8u, 8u}, Player::Black), 2u);
	EXPECT_EQ(computeGroupLiberties(board, {0u, 8u}, Player::Black), 2u);
	EXPECT_EQ(computeGroupLiberties(board, {8u, 0u}, Player::Black), 2u);
	// At border
	for (unsigned i = 1; i != 8; ++i) {
		EXPECT_EQ(computeGroupLiberties(board, {i, 0u}, Player::Black), 3u);
		EXPECT_EQ(computeGroupLiberties(board, {i, 8u}, Player::Black), 3u);
	}
	for (unsigned j = 1; j != 8; ++j) {
		EXPECT_EQ(computeGroupLiberties(board, {0u, j}, Player::Black), 3u);
		EXPECT_EQ(computeGroupLiberties(board, {8u, j}, Player::Black), 3u);
	}
	// No borders
	for (unsigned i = 1; i != 8; ++i) {
		for (unsigned j = 1; j != 8; ++j) {
			EXPECT_EQ(computeGroupLiberties(board, {i, j}, Player::Black), 4u);
		}
	}
}

// Liberties of groups not touching borders
TEST(MoveChecker, ComputeConnectedLiberties_Center) {
	{
		Board board(9u);

		board.place({4u, 3u}, Board::Stone::Black);
		board.place({4u, 4u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {4u, 3u}, Player::Black), 6u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 4u}, Player::Black), 6u);
	}
	{
		Board board(9u);

		board.place({4u, 3u}, Board::Stone::Black);
		board.place({4u, 4u}, Board::Stone::Black);
		board.place({4u, 5u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {4u, 3u}, Player::Black), 8u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 4u}, Player::Black), 8u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 5u}, Player::Black), 8u);
	}
	{
		Board board(9u);

		board.place({4u, 3u}, Board::Stone::Black);
		board.place({4u, 4u}, Board::Stone::Black);
		board.place({5u, 4u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {4u, 3u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 4u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {5u, 4u}, Player::Black), 7u);
	}
	{
		Board board(9u);

		board.place({4u, 3u}, Board::Stone::Black);
		board.place({4u, 4u}, Board::Stone::Black);
		board.place({4u, 5u}, Board::Stone::Black);
		board.place({5u, 5u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {4u, 3u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 4u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 5u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {5u, 5u}, Player::Black), 9u);
	}
	{
		Board board(9u);

		board.place({4u, 3u}, Board::Stone::Black);
		board.place({4u, 4u}, Board::Stone::Black);
		board.place({4u, 5u}, Board::Stone::Black);

		board.place({5u, 3u}, Board::Stone::Black);
		board.place({5u, 5u}, Board::Stone::Black);

		board.place({6u, 4u}, Board::Stone::Black);
		board.place({6u, 5u}, Board::Stone::Black);

		board.place({7u, 4u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {4u, 3u}, Player::Black), 13u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 4u}, Player::Black), 13u);
		EXPECT_EQ(computeGroupLiberties(board, {4u, 5u}, Player::Black), 13u);

		EXPECT_EQ(computeGroupLiberties(board, {5u, 3u}, Player::Black), 13u);
		EXPECT_EQ(computeGroupLiberties(board, {5u, 5u}, Player::Black), 13u);

		EXPECT_EQ(computeGroupLiberties(board, {6u, 4u}, Player::Black), 13u);
		EXPECT_EQ(computeGroupLiberties(board, {6u, 5u}, Player::Black), 13u);

		EXPECT_EQ(computeGroupLiberties(board, {7u, 4u}, Player::Black), 13u);
	}
}

// Liberties of groups touching borders and corners
TEST(MoveChecker, ComputeConnectedLiberties_Borders) {
	{
		Board board(9u);

		board.place({0u, 0u}, Board::Stone::Black);
		board.place({0u, 1u}, Board::Stone::Black);
		board.place({0u, 2u}, Board::Stone::Black);

		board.place({1u, 1u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {0u, 0u}, Player::Black), 4u);
		EXPECT_EQ(computeGroupLiberties(board, {0u, 1u}, Player::Black), 4u);
		EXPECT_EQ(computeGroupLiberties(board, {0u, 2u}, Player::Black), 4u);

		EXPECT_EQ(computeGroupLiberties(board, {1u, 1u}, Player::Black), 4u);
	}
	{
		Board board(9u);

		board.place({0u, 0u}, Board::Stone::Black);
		board.place({0u, 1u}, Board::Stone::Black);
		board.place({0u, 2u}, Board::Stone::Black);

		board.place({1u, 1u}, Board::Stone::Black);

		board.place({2u, 0u}, Board::Stone::Black);
		board.place({2u, 1u}, Board::Stone::Black);
		board.place({2u, 2u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {0u, 0u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {0u, 1u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {0u, 2u}, Player::Black), 7u);

		EXPECT_EQ(computeGroupLiberties(board, {1u, 1u}, Player::Black), 7u);

		EXPECT_EQ(computeGroupLiberties(board, {2u, 0u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {2u, 1u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {2u, 2u}, Player::Black), 7u);
	}
	{
		Board board(9u);

		board.place({0u, 0u}, Board::Stone::Black);
		board.place({0u, 1u}, Board::Stone::Black);
		board.place({0u, 2u}, Board::Stone::Black);

		board.place({1u, 0u}, Board::Stone::Black);
		board.place({1u, 2u}, Board::Stone::Black);

		board.place({2u, 0u}, Board::Stone::Black);
		board.place({2u, 1u}, Board::Stone::Black);
		board.place({2u, 2u}, Board::Stone::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(computeGroupLiberties(board, {0u, 0u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {0u, 1u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {0u, 2u}, Player::Black), 7u);

		EXPECT_EQ(computeGroupLiberties(board, {1u, 0u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {1u, 2u}, Player::Black), 7u);

		EXPECT_EQ(computeGroupLiberties(board, {2u, 0u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {2u, 1u}, Player::Black), 7u);
		EXPECT_EQ(computeGroupLiberties(board, {2u, 2u}, Player::Black), 7u);
	}
	{
		Board board(9u);

		board.place({0u, 0u}, Board::Stone::Black);
		board.place({1u, 0u}, Board::Stone::Black);
		board.place({2u, 0u}, Board::Stone::Black);
		board.place({3u, 0u}, Board::Stone::Black);

		board.place({0u, 1u}, Board::Stone::Black);
		board.place({3u, 1u}, Board::Stone::Black);

		board.place({0u, 2u}, Board::Stone::Black);
		board.place({1u, 2u}, Board::Stone::Black);
		board.place({2u, 2u}, Board::Stone::Black);
		board.place({3u, 2u}, Board::Stone::Black);

		EXPECT_EQ(computeGroupLiberties(board, {0u, 0u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {1u, 0u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {2u, 0u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {3u, 0u}, Player::Black), 9u);

		EXPECT_EQ(computeGroupLiberties(board, {0u, 1u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {3u, 1u}, Player::Black), 9u);

		EXPECT_EQ(computeGroupLiberties(board, {0u, 2u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {1u, 2u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {2u, 2u}, Player::Black), 9u);
		EXPECT_EQ(computeGroupLiberties(board, {3u, 2u}, Player::Black), 9u);
	}
}

// TODO: Write tests to check we find the whole group and liberty count
TEST(MoveChecker, FindGroup) {
}

TEST(MoveChecker, Suicide) {
	{
		Board board(9u);

		board.place({0u, 1u}, Board::Stone::Black);
		board.place({1u, 0u}, Board::Stone::Black);
		board.place({1u, 2u}, Board::Stone::Black);

		// Legal move
		EXPECT_TRUE(isValidMove(board, Player::Black, {1u, 1u}));
		EXPECT_EQ(computeGroupLiberties(board, {1u, 1u}, Player::White), 1u);
	}

	{
		Board board(9u);

		board.place({0u, 1u}, Board::Stone::Black);
		board.place({1u, 0u}, Board::Stone::Black);
		board.place({1u, 2u}, Board::Stone::Black);
		board.place({2u, 1u}, Board::Stone::Black);

		// Suicide -> invalid move
		EXPECT_FALSE(isValidMove(board, Player::White, {1u, 1u}));
		EXPECT_EQ(computeGroupLiberties(board, {1u, 1u}, Player::White), 0u);
	}
	{
		Board board(9u);

		board.place({0u, 1u}, Board::Stone::Black);
		board.place({0u, 2u}, Board::Stone::Black);
		board.place({0u, 3u}, Board::Stone::Black);

		board.place({1u, 0u}, Board::Stone::Black);
		board.place({1u, 1u}, Board::Stone::White);
		board.place({1u, 2u}, Board::Stone::White);
		board.place({1u, 3u}, Board::Stone::Black);

		board.place({2u, 0u}, Board::Stone::Black);
		board.place({2u, 1u}, Board::Stone::White);
		board.place({2u, 2u}, Board::Stone::Black);
		board.place({2u, 3u}, Board::Stone::Black);

		board.place({3u, 0u}, Board::Stone::Black);
		board.place({3u, 2u}, Board::Stone::Black);

		board.place({4u, 1u}, Board::Stone::Black);

		// Suicide -> invalid move
		EXPECT_FALSE(isValidMove(board, Player::White, {3u, 1u}));
		EXPECT_EQ(computeGroupLiberties(board, {3u, 1u}, Player::White), 0u);

		// Now add white stones which would allow the same move to be a capture
		// Surround the rightmost black stone.
		board.place({4u, 0u}, Board::Stone::White);
		board.place({4u, 2u}, Board::Stone::White);
		board.place({5u, 1u}, Board::Stone::White);

		// Now we capture -> Move valid
		EXPECT_TRUE(isValidMove(board, Player::White, {3u, 1u}));
		EXPECT_EQ(computeGroupLiberties(board, {3u, 1u}, Player::White), 0u);
	}

	{
		Board board(9u);

		board.place({0u, 1u}, Board::Stone::Black);

		board.place({1u, 0u}, Board::Stone::Black);
		board.place({1u, 2u}, Board::Stone::Black);

		board.place({2u, 0u}, Board::Stone::White);
		board.place({2u, 1u}, Board::Stone::Black);
		board.place({2u, 2u}, Board::Stone::White);

		board.place({3u, 1u}, Board::Stone::White);

		// Captures -> valid move
		EXPECT_TRUE(isValidMove(board, Player::White, {1u, 1u}));
		EXPECT_EQ(computeGroupLiberties(board, {1u, 1u}, Player::White), 0u);
	}
}

TEST(MoveChecker, Kill) {
	{
		Board board(9u);

		board.place({0u, 0u}, Board::Stone::White);
		board.place({0u, 1u}, Board::Stone::Black);
		board.place({0u, 2u}, Board::Stone::White);

		board.place({1u, 0u}, Board::Stone::Black);
		board.place({1u, 2u}, Board::Stone::Black);
		board.place({1u, 3u}, Board::Stone::White);

		board.place({2u, 0u}, Board::Stone::White);
		board.place({2u, 1u}, Board::Stone::Black);
		board.place({2u, 2u}, Board::Stone::White);

		board.place({3u, 1u}, Board::Stone::White);

		EXPECT_TRUE(isValidMove(board, Player::White, {1u, 1u}));
	}
}

// TEST(MoveChecker, SuperkoRejectsRepeatedState) {
// 	Position pos{9u};
// 	ZobristHash<9u> hasher;
// 	std::unordered_set<uint64_t> history;
// 	history.insert(pos.hash); // Empty board, black to move.

// 	Position next{pos.board.size()};
// 	ASSERT_TRUE(isNextPositionLegal(pos, Player::Black, {4u, 4u}, hasher, history, next));
// 	history.insert(next.hash);

// 	Position rejected{pos.board.size()};
// 	EXPECT_FALSE(isNextPositionLegal(pos, Player::Black, {4u, 4u}, hasher, history, rejected));
// }

// TEST(MoveChecker, SuperkoAllowsNewHashes) {
// 	Board board(9u);
// 	board.place({0u, 1u}, Board::Stone::White);
// 	board.place({1u, 0u}, Board::Stone::White);
// 	board.place({1u, 2u}, Board::Stone::White);

// 	board.place({2u, 1u}, Board::Stone::Black);

// 	Position pos{9u};
// 	pos.board = board;
// 	pos.currentPlayer = Player::White;

// 	ZobristHash<9u> hasher;
// 	uint64_t h = 0;
// 	for (Id x = 0; x < board.size(); ++x)
// 		for (Id y = 0; y < board.size(); ++y) {
// 			const Coord c{x, y};
// 			const auto v = board.getAt(c);
// 			if (v != Board::Stone::Empty)
// 				h ^= hasher.stone(c, v == Board::Stone::White ? Player::White : Player::Black);
// 		}
// 	h ^= hasher.togglePlayer(); // White to move
// 	pos.hash = h;

// 	std::unordered_set<uint64_t> history{pos.hash};

// 	Position next{pos.board.size()};
// 	EXPECT_TRUE(isNextPositionLegal(pos, Player::White, {2u, 0u}, hasher, history, next));
// 	EXPECT_NE(next.hash, pos.hash);
// 	history.insert(next.hash);
// 	EXPECT_FALSE(history.contains(pos.hash)); // Ensure we did not accidentally mutate history inside the call.
// }

} // namespace go::gtest
