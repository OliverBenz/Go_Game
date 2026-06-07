#include "network/nwEvents.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <optional>

namespace tengen::gtest {

TEST(GameNetMessages, ClientToMessage) {
	using nlohmann::json;

	EXPECT_EQ(json::parse(network::toMessage(network::ClientPutStone{.c = {1u, 2u}})), json({{"type", "put"}, {"x", 1u}, {"y", 2u}}));
	EXPECT_EQ(json::parse(network::toMessage(network::ClientPass{})), json({{"type", "pass"}}));
	EXPECT_EQ(json::parse(network::toMessage(network::ClientResign{})), json({{"type", "resign"}}));
	EXPECT_EQ(json::parse(network::toMessage(network::ClientChat{"hello"})), json({{"type", "chat"}, {"message", "hello"}}));
}

TEST(GameNetMessages, ClientFromMessageValid) {
	const auto put = network::fromClientMessage(R"({"type":"put","x":3,"y":4})");
	ASSERT_TRUE(put.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ClientPutStone>(*put));
	const auto putEvent = std::get<network::ClientPutStone>(*put);
	EXPECT_EQ(putEvent.c.x, 3u);
	EXPECT_EQ(putEvent.c.y, 4u);

	const auto pass = network::fromClientMessage(R"({"type":"pass"})");
	ASSERT_TRUE(pass.has_value());
	EXPECT_TRUE(std::holds_alternative<network::ClientPass>(*pass));

	const auto resign = network::fromClientMessage(R"({"type":"resign"})");
	ASSERT_TRUE(resign.has_value());
	EXPECT_TRUE(std::holds_alternative<network::ClientResign>(*resign));

	const auto chat = network::fromClientMessage(R"({"type":"chat","message":"hello"})");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ClientChat>(*chat));
	EXPECT_EQ(std::get<network::ClientChat>(*chat).message, "hello");
}

TEST(GameNetMessages, ClientFromMessageInvalid) {
	EXPECT_FALSE(network::fromClientMessage(R"({"type":"put","x":1})").has_value());
	EXPECT_FALSE(network::fromClientMessage(R"({"type":"put","x":"1","y":2})").has_value());
	EXPECT_FALSE(network::fromClientMessage(R"({"type":"chat"})").has_value());
	EXPECT_FALSE(network::fromClientMessage(R"({"type":"unknown"})").has_value());
	EXPECT_FALSE(network::fromClientMessage("not-json").has_value());
}

TEST(GameNetMessages, ServerToMessage) {
	using nlohmann::json;

	EXPECT_EQ(json::parse(network::toMessage(network::ServerSessionAssign{1u})), json({{"type", "session"}, {"sessionId", 1u}}));

	EXPECT_EQ(json::parse(network::toMessage(network::ServerGameConfig{.boardSize = 19u, .komi = 6.5, .timeSeconds = 0u})),
	          json({{"type", "config"}, {"boardSize", 19u}, {"komi", 6.5}, {"time", 0u}}));

	EXPECT_EQ(json::parse(network::toMessage(network::ServerDelta{
	                  .turn     = 42u,
	                  .seat     = network::Seat::Black,
	                  .action   = network::ServerAction::Place,
	                  .coord    = Coord{3u, 4u},
	                  .captures = {Coord{1u, 2u}, Coord{5u, 6u}},
	                  .next     = network::Seat::White,
	                  .status   = network::GameStatus::Active,
	          })),
	          json({{"type", "delta"},
	                {"turn", 42u},
	                {"seat", static_cast<unsigned>(network::Seat::Black)},
	                {"action", static_cast<unsigned>(network::ServerAction::Place)},
	                {"next", static_cast<unsigned>(network::Seat::White)},
	                {"status", static_cast<unsigned>(network::GameStatus::Active)},
	                {"x", 3u},
	                {"y", 4u},
	                {"captures", json::array({json::array({1u, 2u}), json::array({5u, 6u})})}}));

	EXPECT_EQ(json::parse(network::toMessage(network::ServerDelta{
	                  .turn     = 43u,
	                  .seat     = network::Seat::White,
	                  .action   = network::ServerAction::Pass,
	                  .coord    = std::nullopt,
	                  .captures = {},
	                  .next     = network::Seat::Black,
	                  .status   = network::GameStatus::Active,
	          })),
	          json({{"type", "delta"},
	                {"turn", 43u},
	                {"seat", static_cast<unsigned>(network::Seat::White)},
	                {"action", static_cast<unsigned>(network::ServerAction::Pass)},
	                {"next", static_cast<unsigned>(network::Seat::Black)},
	                {"status", static_cast<unsigned>(network::GameStatus::Active)}}));

	EXPECT_EQ(json::parse(network::toMessage(network::ServerDelta{
	                  .turn     = 44u,
	                  .seat     = network::Seat::Black,
	                  .action   = network::ServerAction::Resign,
	                  .coord    = std::nullopt,
	                  .captures = {},
	                  .next     = network::Seat::White,
	                  .status   = network::GameStatus::WhiteWin,
	          })),
	          json({{"type", "delta"},
	                {"turn", 44u},
	                {"seat", static_cast<unsigned>(network::Seat::Black)},
	                {"action", static_cast<unsigned>(network::ServerAction::Resign)},
	                {"next", static_cast<unsigned>(network::Seat::White)},
	                {"status", static_cast<unsigned>(network::GameStatus::WhiteWin)}}));

	EXPECT_EQ(json::parse(network::toMessage(network::ServerChat{Player::White, 0u, "hi"})),
	          json({{"type", "chat"}, {"player", static_cast<unsigned>(Player::White)}, {"messageId", 0}, {"message", "hi"}}));
}

TEST(GameNetMessages, ServerFromMessageValid) {
	const auto session = network::fromServerMessage(R"({"type":"session","sessionId":42})");
	ASSERT_TRUE(session.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ServerSessionAssign>(*session));
	EXPECT_EQ(std::get<network::ServerSessionAssign>(*session).sessionId, 42u);

	const auto delta = network::fromServerMessage(R"({"type":"delta","turn":7,"seat":2,"action":0,"x":1,"y":2,"captures":[[3,4],[5,6]],"next":4,"status":0})");
	ASSERT_TRUE(delta.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ServerDelta>(*delta));
	const auto deltaEvent = std::get<network::ServerDelta>(*delta);
	EXPECT_EQ(deltaEvent.turn, 7u);
	EXPECT_EQ(deltaEvent.seat, network::Seat::Black);
	EXPECT_EQ(deltaEvent.action, network::ServerAction::Place);
	ASSERT_TRUE(deltaEvent.coord.has_value());
	EXPECT_EQ(deltaEvent.coord->x, 1u);
	EXPECT_EQ(deltaEvent.coord->y, 2u);
	ASSERT_EQ(deltaEvent.captures.size(), 2u);
	EXPECT_EQ(deltaEvent.captures[0].x, 3u);
	EXPECT_EQ(deltaEvent.captures[0].y, 4u);
	EXPECT_EQ(deltaEvent.captures[1].x, 5u);
	EXPECT_EQ(deltaEvent.captures[1].y, 6u);
	EXPECT_EQ(deltaEvent.next, network::Seat::White);
	EXPECT_EQ(deltaEvent.status, network::GameStatus::Active);

	const auto passDelta = network::fromServerMessage(R"({"type":"delta","turn":8,"seat":4,"action":1,"next":2,"status":0})");
	ASSERT_TRUE(passDelta.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ServerDelta>(*passDelta));
	const auto passEvent = std::get<network::ServerDelta>(*passDelta);
	EXPECT_EQ(passEvent.action, network::ServerAction::Pass);
	EXPECT_FALSE(passEvent.coord.has_value());
	EXPECT_TRUE(passEvent.captures.empty());

	const auto chat = network::fromServerMessage(R"({"type":"chat","player":2,"messageId":0,"message":"hello,world"})");
	ASSERT_TRUE(chat.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ServerChat>(*chat));
	const auto chatEvent = std::get<network::ServerChat>(*chat);
	EXPECT_EQ(chatEvent.player, Player::White);
	EXPECT_EQ(chatEvent.messageId, 0u);
	EXPECT_EQ(chatEvent.message, "hello,world");

	const auto config = network::fromServerMessage(R"({"type":"config","boardSize":13,"komi":6.5,"time":300})");
	ASSERT_TRUE(config.has_value());
	ASSERT_TRUE(std::holds_alternative<network::ServerGameConfig>(*config));
	const auto configEvent = std::get<network::ServerGameConfig>(*config);
	EXPECT_EQ(configEvent.boardSize, 13u);
	EXPECT_DOUBLE_EQ(configEvent.komi, 6.5);
	EXPECT_EQ(configEvent.timeSeconds, 300u);
}

TEST(GameNetMessages, ServerFromMessageInvalid) {
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"session"})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"config","boardSize":9,"komi":"bad","time":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"config","boardSize":9,"komi":6.5})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"config","komi":6.5,"time":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":"2","next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":99,"next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":2,"captures":"bad","next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":2,"captures":[[1]],"next":4,"status":0})").has_value());
	EXPECT_FALSE(
	        network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":0,"x":1,"y":2,"captures":[[1,"a"]],"next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"x":1,"next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"captures":[[1,1]],"next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"next":4,"status":99})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":0,"action":1,"next":4,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"delta","turn":1,"seat":2,"action":1,"next":0,"status":0})").has_value());
	EXPECT_FALSE(network::fromServerMessage(R"({"type":"chat","seat":0,"message":"hi"})").has_value());
	EXPECT_FALSE(network::fromServerMessage("not-json").has_value());
}

TEST(GameNetMessages, ServerDeltaOmitsEmptyFields) {
	using nlohmann::json;

	const auto message = network::toMessage(network::ServerDelta{
	        .turn     = 9u,
	        .seat     = network::Seat::Black,
	        .action   = network::ServerAction::Pass,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = network::Seat::White,
	        .status   = network::GameStatus::Active,
	});
	const auto j       = json::parse(message);
	EXPECT_FALSE(j.contains("x"));
	EXPECT_FALSE(j.contains("y"));
	EXPECT_FALSE(j.contains("captures"));
}

#if GTEST_HAS_DEATH_TEST
TEST(GameNetMessages, ServerDeltaMissingXYSerialization) {
	const auto build = [] {
		network::toMessage(network::ServerDelta{
		        .turn     = 1u,
		        .seat     = network::Seat::Black,
		        .action   = network::ServerAction::Place,
		        .coord    = std::nullopt,
		        .captures = {},
		        .next     = network::Seat::White,
		        .status   = network::GameStatus::Active,
		});
	};
	EXPECT_DEATH(build(), "ServerDelta::Place requires coord");
}
#endif

} // namespace tengen::gtest
