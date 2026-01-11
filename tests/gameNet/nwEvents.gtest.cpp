#include "gameNet/nwEvents.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

TEST(GameNetMessages, ClientToMessage) {
	EXPECT_EQ(gameNet::toMessage(gameNet::ClientPutStone{1u, 2u}), "PUT:1,2");
	EXPECT_EQ(gameNet::toMessage(gameNet::ClientPass{}), "PASS");
	EXPECT_EQ(gameNet::toMessage(gameNet::ClientResign{}), "RESIGN");
	EXPECT_EQ(gameNet::toMessage(gameNet::ClientChat{"hello"}), "CHAT:hello");
}

TEST(GameNetMessages, ClientFromMessageValid) {
	const auto put = gameNet::fromClientMessage("PUT:3,4");
	ASSERT_TRUE(put.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ClientPutStone>(*put));
	const auto putEvent = std::get<gameNet::ClientPutStone>(*put);
	EXPECT_EQ(putEvent.x, 3u);
	EXPECT_EQ(putEvent.y, 4u);

	const auto pass = gameNet::fromClientMessage("PASS");
	ASSERT_TRUE(pass.has_value());
	EXPECT_TRUE(std::holds_alternative<gameNet::ClientPass>(*pass));

	const auto resign = gameNet::fromClientMessage("RESIGN");
	ASSERT_TRUE(resign.has_value());
	EXPECT_TRUE(std::holds_alternative<gameNet::ClientResign>(*resign));

	const auto chat = gameNet::fromClientMessage("CHAT:hello");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ClientChat>(*chat));
	EXPECT_EQ(std::get<gameNet::ClientChat>(*chat).message, "hello");
}

TEST(GameNetMessages, ClientFromMessageInvalid) {
	EXPECT_FALSE(gameNet::fromClientMessage("PUT:1").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage("PUT:1,").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage("PUT:1,a").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage("CHAT").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage("UNKNOWN").has_value());
}

TEST(GameNetMessages, ServerToMessage) {
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerSessionAssign{1u}), "SESSION_ID:1");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerBoardUpdate{gameNet::Seat::Black, 3u, 4u}), "GAME_BOARD:2,3,4");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerPass{gameNet::Seat::White}), "GAME_PASS:4");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerResign{gameNet::Seat::Black}), "GAME_RESIGN:2");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerChat{gameNet::Seat::White, "hi"}), "NEW_CHAT:4,hi");
}

TEST(GameNetMessages, ServerFromMessageValid) {
	const auto session = gameNet::fromServerMessage("SESSION_ID:42");
	ASSERT_TRUE(session.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerSessionAssign>(*session));
	EXPECT_EQ(std::get<gameNet::ServerSessionAssign>(*session).sessionId, 42u);

	const auto board = gameNet::fromServerMessage("GAME_BOARD:2,5,6");
	ASSERT_TRUE(board.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerBoardUpdate>(*board));
	const auto boardEvent = std::get<gameNet::ServerBoardUpdate>(*board);
	EXPECT_EQ(boardEvent.seat, gameNet::Seat::Black);
	EXPECT_EQ(boardEvent.x, 5u);
	EXPECT_EQ(boardEvent.y, 6u);

	const auto pass = gameNet::fromServerMessage("GAME_PASS:4");
	ASSERT_TRUE(pass.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerPass>(*pass));
	EXPECT_EQ(std::get<gameNet::ServerPass>(*pass).seat, gameNet::Seat::White);

	const auto resign = gameNet::fromServerMessage("GAME_RESIGN:2");
	ASSERT_TRUE(resign.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerResign>(*resign));
	EXPECT_EQ(std::get<gameNet::ServerResign>(*resign).seat, gameNet::Seat::Black);

	const auto chat = gameNet::fromServerMessage("NEW_CHAT:2,hello,world");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerChat>(*chat));
	const auto chatEvent = std::get<gameNet::ServerChat>(*chat);
	EXPECT_EQ(chatEvent.seat, gameNet::Seat::Black);
	EXPECT_EQ(chatEvent.message, "hello,world");
}

TEST(GameNetMessages, ServerFromMessageInvalid) {
	EXPECT_FALSE(gameNet::fromServerMessage("SESSION_ID:").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("GAME_BOARD:2,3").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("GAME_BOARD:0,3,4").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("GAME_PASS:0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("GAME_RESIGN:8").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("NEW_CHAT:0,hi").has_value());
}

} // namespace go::gtest
