#include "gameNet/nwEvents.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <optional>

namespace go::gtest {

TEST(GameNetMessages, ClientToMessage) {
	using nlohmann::json;

	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ClientPutStone{.c = {1u, 2u}})), json({{"type", "put"}, {"x", 1u}, {"y", 2u}}));
	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ClientPass{})), json({{"type", "pass"}}));
	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ClientResign{})), json({{"type", "resign"}}));
	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ClientChat{"hello"})), json({{"type", "chat"}, {"message", "hello"}}));
}

TEST(GameNetMessages, ClientFromMessageValid) {
	const auto put = gameNet::fromClientMessage(R"({"type":"put","x":3,"y":4})");
	ASSERT_TRUE(put.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ClientPutStone>(*put));
	const auto putEvent = std::get<gameNet::ClientPutStone>(*put);
	EXPECT_EQ(putEvent.c.x, 3u);
	EXPECT_EQ(putEvent.c.y, 4u);

	const auto pass = gameNet::fromClientMessage(R"({"type":"pass"})");
	ASSERT_TRUE(pass.has_value());
	EXPECT_TRUE(std::holds_alternative<gameNet::ClientPass>(*pass));

	const auto resign = gameNet::fromClientMessage(R"({"type":"resign"})");
	ASSERT_TRUE(resign.has_value());
	EXPECT_TRUE(std::holds_alternative<gameNet::ClientResign>(*resign));

	const auto chat = gameNet::fromClientMessage(R"({"type":"chat","message":"hello"})");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ClientChat>(*chat));
	EXPECT_EQ(std::get<gameNet::ClientChat>(*chat).message, "hello");
}

TEST(GameNetMessages, ClientFromMessageInvalid) {
	EXPECT_FALSE(gameNet::fromClientMessage(R"({"type":"put","x":1})").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage(R"({"type":"put","x":"1","y":2})").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage(R"({"type":"chat"})").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage(R"({"type":"unknown"})").has_value());
	EXPECT_FALSE(gameNet::fromClientMessage("not-json").has_value());
}

TEST(GameNetMessages, ServerToMessage) {
	using nlohmann::json;

	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ServerSessionAssign{1u})), json({{"type", "session"}, {"sessionId", 1u}}));

	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ServerDelta{
	                  .turn     = 42u,
	                  .seat     = gameNet::Seat::Black,
	                  .action   = gameNet::ServerAction::Place,
	                  .coord    = gameNet::Coord{3u, 4u},
	                  .captures = {gameNet::Coord{1u, 2u}, gameNet::Coord{5u, 6u}},
	                  .next     = gameNet::Seat::White,
	                  .status   = gameNet::GameStatus::Active,
	          })),
	          json({{"type", "delta"},
	                {"turn", 42u},
	                {"seat", static_cast<unsigned>(gameNet::Seat::Black)},
	                {"action", static_cast<unsigned>(gameNet::ServerAction::Place)},
	                {"next", static_cast<unsigned>(gameNet::Seat::White)},
	                {"status", static_cast<unsigned>(gameNet::GameStatus::Active)},
	                {"x", 3u},
	                {"y", 4u},
	                {"captures", json::array({json::array({1u, 2u}), json::array({5u, 6u})})}}));

	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ServerDelta{
	                  .turn     = 43u,
	                  .seat     = gameNet::Seat::White,
	                  .action   = gameNet::ServerAction::Pass,
	                  .coord    = std::nullopt,
	                  .captures = {},
	                  .next     = gameNet::Seat::Black,
	                  .status   = gameNet::GameStatus::Active,
	          })),
	          json({{"type", "delta"},
	                {"turn", 43u},
	                {"seat", static_cast<unsigned>(gameNet::Seat::White)},
	                {"action", static_cast<unsigned>(gameNet::ServerAction::Pass)},
	                {"next", static_cast<unsigned>(gameNet::Seat::Black)},
	                {"status", static_cast<unsigned>(gameNet::GameStatus::Active)}}));

	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ServerDelta{
	                  .turn     = 44u,
	                  .seat     = gameNet::Seat::Black,
	                  .action   = gameNet::ServerAction::Resign,
	                  .coord    = std::nullopt,
	                  .captures = {},
	                  .next     = gameNet::Seat::White,
	                  .status   = gameNet::GameStatus::WhiteWin,
	          })),
	          json({{"type", "delta"},
	                {"turn", 44u},
	                {"seat", static_cast<unsigned>(gameNet::Seat::Black)},
	                {"action", static_cast<unsigned>(gameNet::ServerAction::Resign)},
	                {"next", static_cast<unsigned>(gameNet::Seat::White)},
	                {"status", static_cast<unsigned>(gameNet::GameStatus::WhiteWin)}}));

	EXPECT_EQ(json::parse(gameNet::toMessage(gameNet::ServerChat{gameNet::Seat::White, "hi"})),
	          json({{"type", "chat"}, {"seat", static_cast<unsigned>(gameNet::Seat::White)}, {"message", "hi"}}));
}

TEST(GameNetMessages, ServerFromMessageValid) {
	const auto session = gameNet::fromServerMessage(R"({"type":"session","sessionId":42})");
	ASSERT_TRUE(session.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerSessionAssign>(*session));
	EXPECT_EQ(std::get<gameNet::ServerSessionAssign>(*session).sessionId, 42u);

	const auto delta = gameNet::fromServerMessage(R"({"type":"delta","turn":7,"seat":2,"action":0,"x":1,"y":2,"captures":[[3,4],[5,6]],"next":4,"status":0})");
	ASSERT_TRUE(delta.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerDelta>(*delta));
	const auto deltaEvent = std::get<gameNet::ServerDelta>(*delta);
	EXPECT_EQ(deltaEvent.turn, 7u);
	EXPECT_EQ(deltaEvent.seat, gameNet::Seat::Black);
	EXPECT_EQ(deltaEvent.action, gameNet::ServerAction::Place);
	ASSERT_TRUE(deltaEvent.coord.has_value());
	EXPECT_EQ(deltaEvent.coord->x, 1u);
	EXPECT_EQ(deltaEvent.coord->y, 2u);
	ASSERT_EQ(deltaEvent.captures.size(), 2u);
	EXPECT_EQ(deltaEvent.captures[0].x, 3u);
	EXPECT_EQ(deltaEvent.captures[0].y, 4u);
	EXPECT_EQ(deltaEvent.captures[1].x, 5u);
	EXPECT_EQ(deltaEvent.captures[1].y, 6u);
	EXPECT_EQ(deltaEvent.next, gameNet::Seat::White);
	EXPECT_EQ(deltaEvent.status, gameNet::GameStatus::Active);

	const auto passDelta = gameNet::fromServerMessage(R"({"type":"delta","turn":8,"seat":4,"action":1,"next":2,"status":0})");
	ASSERT_TRUE(passDelta.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerDelta>(*passDelta));
	const auto passEvent = std::get<gameNet::ServerDelta>(*passDelta);
	EXPECT_EQ(passEvent.action, gameNet::ServerAction::Pass);
	EXPECT_FALSE(passEvent.coord.has_value());
	EXPECT_TRUE(passEvent.captures.empty());

	const auto chat = gameNet::fromServerMessage(R"({"type":"chat","seat":2,"message":"hello,world"})");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<gameNet::ServerChat>(*chat));
	const auto chatEvent = std::get<gameNet::ServerChat>(*chat);
	EXPECT_EQ(chatEvent.seat, gameNet::Seat::Black);
	EXPECT_EQ(chatEvent.message, "hello,world");
}

TEST(GameNetMessages, ServerFromMessageInvalid) {
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"session"})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":"2","next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":99,"next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":2,"captures":"bad","next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":2,"captures":[[1]],"next":4,"status":0})").has_value());
	EXPECT_FALSE(
	        gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":2,"captures":[[1,"a"]],"next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"x":1,"next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"captures":[[1,1]],"next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"next":4,"status":99})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":0,"action":1,"next":4,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"next":0,"status":0})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage(R"({"type":"chat","seat":0,"message":"hi"})").has_value());
	EXPECT_FALSE(gameNet::fromServerMessage("not-json").has_value());
}

TEST(GameNetMessages, ServerDeltaOmitsEmptyFields) {
	using nlohmann::json;

	const auto message = gameNet::toMessage(gameNet::ServerDelta{
	        .turn     = 9u,
	        .seat     = gameNet::Seat::Black,
	        .action   = gameNet::ServerAction::Pass,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = gameNet::Seat::White,
	        .status   = gameNet::GameStatus::Active,
	});
	const auto j       = json::parse(message);
	EXPECT_FALSE(j.contains("x"));
	EXPECT_FALSE(j.contains("y"));
	EXPECT_FALSE(j.contains("captures"));
}

#if GTEST_HAS_DEATH_TEST
TEST(GameNetMessages, ServerDeltaMissingXYSerialization) {
	const auto build = [] {
		gameNet::toMessage(gameNet::ServerDelta{
		        .turn     = 1u,
		        .seat     = gameNet::Seat::Black,
		        .action   = gameNet::ServerAction::Place,
		        .coord    = std::nullopt,
		        .captures = {},
		        .next     = gameNet::Seat::White,
		        .status   = gameNet::GameStatus::Active,
		});
	};
	EXPECT_DEATH(build(), "ServerDelta::Place requires coord");
}
#endif

} // namespace go::gtest
