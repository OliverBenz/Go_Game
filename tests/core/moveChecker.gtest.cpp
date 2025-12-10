#include "core/moveChecker.hpp"
#include "core/board.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

// Liberties of single stones at all board positions
TEST(MoveChecker, ComputeConnectedLiberties_Single) {
	Board board(9u);
	MoveChecker checker(board);
	// In corner
	EXPECT_EQ(checker.computeGroupLiberties({0u, 0u}, Player::Black), 2u);
	EXPECT_EQ(checker.computeGroupLiberties({8u, 8u}, Player::Black), 2u);
	EXPECT_EQ(checker.computeGroupLiberties({0u, 8u}, Player::Black), 2u);
	EXPECT_EQ(checker.computeGroupLiberties({8u, 0u}, Player::Black), 2u);
	// At border
	for (Id i = 1; i != 8; ++i) {
		EXPECT_EQ(checker.computeGroupLiberties({i, 0u}, Player::Black), 3u);
		EXPECT_EQ(checker.computeGroupLiberties({i, 8u}, Player::Black), 3u);
	}
	for (Id j = 1; j != 8; ++j) {
		EXPECT_EQ(checker.computeGroupLiberties({0u, j}, Player::Black), 3u);
		EXPECT_EQ(checker.computeGroupLiberties({8u, j}, Player::Black), 3u);
	}
	// No borders
	for (Id i = 1; i != 8; ++i) {
		for (Id j = 1; j != 8; ++j) {
			EXPECT_EQ(checker.computeGroupLiberties({i, j}, Player::Black), 4u);
		}
	}
}

// Liberties of groups not touching borders
TEST(MoveChecker, ComputeConnectedLiberties_Center) {
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({4u, 3u}, Board::FieldValue::Black);
		board.setAt({4u, 4u}, Board::FieldValue::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(checker.computeGroupLiberties({4u, 3u}, Player::Black), 6u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 4u}, Player::Black), 6u);
	}
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({4u, 3u}, Board::FieldValue::Black);
		board.setAt({4u, 4u}, Board::FieldValue::Black);
		board.setAt({4u, 5u}, Board::FieldValue::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(checker.computeGroupLiberties({4u, 3u}, Player::Black), 8u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 4u}, Player::Black), 8u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 5u}, Player::Black), 8u);
	}
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({4u, 3u}, Board::FieldValue::Black);
		board.setAt({4u, 4u}, Board::FieldValue::Black);
		board.setAt({5u, 4u}, Board::FieldValue::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(checker.computeGroupLiberties({4u, 3u}, Player::Black), 7u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 4u}, Player::Black), 7u);
		EXPECT_EQ(checker.computeGroupLiberties({5u, 4u}, Player::Black), 7u);
	}
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({4u, 3u}, Board::FieldValue::Black);
		board.setAt({4u, 4u}, Board::FieldValue::Black);
		board.setAt({4u, 5u}, Board::FieldValue::Black);
		board.setAt({5u, 5u}, Board::FieldValue::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(checker.computeGroupLiberties({4u, 3u}, Player::Black), 9u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 4u}, Player::Black), 9u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 5u}, Player::Black), 9u);
		EXPECT_EQ(checker.computeGroupLiberties({5u, 5u}, Player::Black), 9u);
	}
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({4u, 3u}, Board::FieldValue::Black);
		board.setAt({4u, 4u}, Board::FieldValue::Black);
		board.setAt({4u, 5u}, Board::FieldValue::Black);

		board.setAt({5u, 3u}, Board::FieldValue::Black);
		board.setAt({5u, 5u}, Board::FieldValue::Black);

		board.setAt({6u, 4u}, Board::FieldValue::Black);
		board.setAt({6u, 5u}, Board::FieldValue::Black);

		board.setAt({7u, 4u}, Board::FieldValue::Black);

		// Check liberties for each stone to check that full chain is found.
		EXPECT_EQ(checker.computeGroupLiberties({4u, 3u}, Player::Black), 12u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 4u}, Player::Black), 12u);
		EXPECT_EQ(checker.computeGroupLiberties({4u, 5u}, Player::Black), 12u);

		EXPECT_EQ(checker.computeGroupLiberties({5u, 3u}, Player::Black), 12u);
		EXPECT_EQ(checker.computeGroupLiberties({5u, 5u}, Player::Black), 12u);

		EXPECT_EQ(checker.computeGroupLiberties({6u, 4u}, Player::Black), 12u);
		EXPECT_EQ(checker.computeGroupLiberties({6u, 5u}, Player::Black), 12u);

		EXPECT_EQ(checker.computeGroupLiberties({7u, 4u}, Player::Black), 12u);
	}
}

// Liberties of groups touching borders and corners
TEST(MoveChecker, ComputeConnectedLiberties_Borders) {
}

TEST(MoveChecker, Suicide) {
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({0u, 1u}, Board::FieldValue::Black);
		board.setAt({1u, 0u}, Board::FieldValue::Black);
		board.setAt({1u, 2u}, Board::FieldValue::Black);

		// Legal move
		EXPECT_TRUE(checker.isValidMove(Player::Black, {1u, 1u}));
		EXPECT_EQ(checker.computeGroupLiberties({1u, 1u}, Player::Black), 1u);
	}

	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({0u, 1u}, Board::FieldValue::Black);
		board.setAt({1u, 0u}, Board::FieldValue::Black);
		board.setAt({1u, 2u}, Board::FieldValue::Black);
		board.setAt({2u, 1u}, Board::FieldValue::Black);

		// Suicide -> invalid move
		EXPECT_FALSE(checker.isValidMove(Player::Black, {1u, 1u}));
		EXPECT_EQ(checker.computeGroupLiberties({1u, 1u}, Player::Black), 0u);
	}
	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({0u, 1u}, Board::FieldValue::Black);
		board.setAt({0u, 2u}, Board::FieldValue::Black);
		board.setAt({0u, 3u}, Board::FieldValue::Black);

		board.setAt({1u, 0u}, Board::FieldValue::Black);
		board.setAt({1u, 1u}, Board::FieldValue::White);
		board.setAt({1u, 2u}, Board::FieldValue::White);
		board.setAt({1u, 3u}, Board::FieldValue::Black);

		board.setAt({2u, 0u}, Board::FieldValue::Black);
		board.setAt({2u, 1u}, Board::FieldValue::White);
		board.setAt({2u, 2u}, Board::FieldValue::Black);
		board.setAt({2u, 3u}, Board::FieldValue::Black);

		board.setAt({3u, 0u}, Board::FieldValue::Black);
		board.setAt({3u, 2u}, Board::FieldValue::Black);

		board.setAt({4u, 1u}, Board::FieldValue::Black);

		// Suicide -> invalid move
		EXPECT_FALSE(checker.isValidMove(Player::White, {3u, 1u}));
		EXPECT_EQ(checker.computeGroupLiberties({1u, 1u}, Player::Black), 0u);

		// Now add white stones which would allow the same move to be a capture
		// Surround the rightmost black stone.
		board.setAt({4u, 0u}, Board::FieldValue::White);
		board.setAt({4u, 2u}, Board::FieldValue::White);
		board.setAt({5u, 1u}, Board::FieldValue::White);

		// Now we capture -> Move valid
		EXPECT_TRUE(checker.isValidMove(Player::White, {3u, 1u}));
		EXPECT_EQ(checker.computeGroupLiberties({1u, 1u}, Player::Black), 0u);
	}

	{
		Board board(9u);
		MoveChecker checker(board);

		board.setAt({0u, 1u}, Board::FieldValue::Black);

		board.setAt({1u, 0u}, Board::FieldValue::Black);
		board.setAt({1u, 2u}, Board::FieldValue::Black);

		board.setAt({2u, 0u}, Board::FieldValue::White);
		board.setAt({2u, 1u}, Board::FieldValue::Black);
		board.setAt({2u, 2u}, Board::FieldValue::White);

		board.setAt({3u, 1u}, Board::FieldValue::White);

		// Captures -> valid move
		EXPECT_TRUE(checker.isValidMove(Player::White, {1u, 1u}));
		EXPECT_EQ(checker.computeGroupLiberties({1u, 1u}, Player::Black), 0u);
	}
}


} // namespace go::gtest