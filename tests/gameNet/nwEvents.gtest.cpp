#include "gameNet/nwEvents.hpp"

#include <gtest/gtest.h>

#include <optional>

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
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerDelta{
	                  .turn     = 42u,
	                  .seat     = gameNet::Seat::Black,
	                  .action   = gameNet::ServerAction::Place,
	                  .x        = 3u,
	                  .y        = 4u,
	                  .captures = {gameNet::CaptureCoord{1u, 2u}, gameNet::CaptureCoord{5u, 6u}},
	                  .next     = gameNet::Seat::White,
	                  .status   = gameNet::GameStatus::Active,
	          }),
	          "DELTA:turn=42,seat=2,action=0,x=3,y=4,captures=1|2;5|6,next=4,status=0");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerDelta{
	                  .turn     = 43u,
	                  .seat     = gameNet::Seat::White,
	                  .action   = gameNet::ServerAction::Pass,
	                  .x        = std::nullopt,
	                  .y        = std::nullopt,
	                  .captures = {},
	                  .next     = gameNet::Seat::Black,
	                  .status   = gameNet::GameStatus::Active,
	          }),
	          "DELTA:turn=43,seat=4,action=1,next=2,status=0");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerDelta{
	                  .turn     = 44u,
	                  .seat     = gameNet::Seat::Black,
	                  .action   = gameNet::ServerAction::Resign,
	                  .x        = std::nullopt,
	                  .y        = std::nullopt,
	                  .captures = {},
	                  .next     = gameNet::Seat::White,
	                  .status   = gameNet::GameStatus::WhiteWin,
	          }),
	          "DELTA:turn=44,seat=2,action=2,next=4,status=2");
	EXPECT_EQ(gameNet::toMessage(gameNet::ServerChat{gameNet::Seat::White, "hi"}), "NEW_CHAT:4,hi");
}

TEST(GameNetMessages, ServerFromMessageValid) {
	const auto session = gameNet::fromServerMessage("SESSION_ID:42");
	ASSERT_TRUE(session.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerSessionAssign>(*session));
	EXPECT_EQ(std::get<gameNet::ServerSessionAssign>(*session).sessionId, 42u);

	const auto delta = gameNet::fromServerMessage("DELTA:turn=7,seat=2,action=0,x=1,y=2,captures=3|4;5|6,next=4,status=0");
	ASSERT_TRUE(delta.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerDelta>(*delta));
	const auto deltaEvent = std::get<gameNet::ServerDelta>(*delta);
	EXPECT_EQ(deltaEvent.turn, 7u);
	EXPECT_EQ(deltaEvent.seat, gameNet::Seat::Black);
	EXPECT_EQ(deltaEvent.action, gameNet::ServerAction::Place);
	EXPECT_EQ(deltaEvent.x, 1u);
	EXPECT_EQ(deltaEvent.y, 2u);
	ASSERT_EQ(deltaEvent.captures.size(), 2u);
	EXPECT_EQ(deltaEvent.captures[0].x, 3u);
	EXPECT_EQ(deltaEvent.captures[0].y, 4u);
	EXPECT_EQ(deltaEvent.captures[1].x, 5u);
	EXPECT_EQ(deltaEvent.captures[1].y, 6u);
	EXPECT_EQ(deltaEvent.next, gameNet::Seat::White);
	EXPECT_EQ(deltaEvent.status, gameNet::GameStatus::Active);

	const auto passDelta = gameNet::fromServerMessage("DELTA:turn=8,seat=4,action=1,next=2,status=0");
	ASSERT_TRUE(passDelta.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerDelta>(*passDelta));
	const auto passEvent = std::get<gameNet::ServerDelta>(*passDelta);
	EXPECT_EQ(passEvent.action, gameNet::ServerAction::Pass);
	EXPECT_FALSE(passEvent.x.has_value());
	EXPECT_FALSE(passEvent.y.has_value());
	EXPECT_TRUE(passEvent.captures.empty());

	const auto chat = gameNet::fromServerMessage("NEW_CHAT:2,hello,world");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerChat>(*chat));
	const auto chatEvent = std::get<gameNet::ServerChat>(*chat);
	EXPECT_EQ(chatEvent.seat, gameNet::Seat::Black);
	EXPECT_EQ(chatEvent.message, "hello,world");
}

TEST(GameNetMessages, ServerFromMessageInvalid) {
	EXPECT_FALSE(gameNet::fromServerMessage("SESSION_ID:").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=0,x=,y=,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=0,action=1,x=,y=,captures=,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=0,x=1,y=,captures=,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=0,x=1,y=2,captures=3|,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=0,x=1,y=2,captures=,next=0,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=1,x=,y=,captures=,next=4,status=99").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=1,x=1,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=1,captures=1|1,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=1,next=4,status=0,foo=1").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,turn=2,seat=2,action=1,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,,seat=2,action=1,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("DELTA:turn=1,seat=2,action=0,x=1,y=1,captures=1|2;;3|4,next=4,status=0").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("NEW_CHAT:0,hi").has_value());
}

TEST(GameNetMessages, ServerDeltaOmitsEmptyFields) {
	const auto message = gameNet::toMessage(gameNet::ServerDelta{
	        .turn     = 9u,
	        .seat     = gameNet::Seat::Black,
	        .action   = gameNet::ServerAction::Pass,
	        .x        = std::nullopt,
	        .y        = std::nullopt,
	        .captures = {},
	        .next     = gameNet::Seat::White,
	        .status   = gameNet::GameStatus::Active,
	});
	EXPECT_EQ(message.find("x="), std::string::npos);
	EXPECT_EQ(message.find("y="), std::string::npos);
	EXPECT_EQ(message.find("captures="), std::string::npos);
}

#if GTEST_HAS_DEATH_TEST
TEST(GameNetMessages, ServerDeltaMissingXYSerialization) {
	const auto build = [] {
		gameNet::toMessage(gameNet::ServerDelta{
		        .turn     = 1u,
		        .seat     = gameNet::Seat::Black,
		        .action   = gameNet::ServerAction::Place,
		        .x        = std::nullopt,
		        .y        = std::nullopt,
		        .captures = {},
		        .next     = gameNet::Seat::White,
		        .status   = gameNet::GameStatus::Active,
		});
	};
	EXPECT_DEATH(build(), "ServerDelta::Place requires x and y");
}
#endif

} // namespace go::gtest
