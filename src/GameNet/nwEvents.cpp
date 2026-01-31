#include "gameNet/nwEvents.hpp"

#include <cassert>
#include <nlohmann/json.hpp>

namespace go::gameNet {

using nlohmann::json;

constexpr bool isValid(ServerAction a) noexcept {
	switch (a) {
	case ServerAction::Place:
	case ServerAction::Pass:
	case ServerAction::Resign:
		return true;
	case ServerAction::Count:
		return false;
	}
	static_assert(static_cast<int>(ServerAction::Count) == 3, "Update isValid(ServerAction) when adding enum values");
	return false;
}
constexpr bool isValid(GameStatus a) noexcept {
	switch (a) {
	case GameStatus::Active:
	case GameStatus::BlackWin:
	case GameStatus::WhiteWin:
	case GameStatus::Draw:
		return true;
	case GameStatus::Count:
		return false;
	}
	static_assert(static_cast<int>(GameStatus::Count) == 4, "Update isValid(GameStatus) when adding enum values");
	return false;
}

static std::string toMessage(const ClientPutStone& e) {
	json j;
	j["type"] = "put";
	j["x"]    = e.c.x;
	j["y"]    = e.c.y;
	return j.dump();
}
static std::string toMessage(const ClientPass&) {
	json j;
	j["type"] = "pass";
	return j.dump();
}
static std::string toMessage(const ClientResign&) {
	json j;
	j["type"] = "resign";
	return j.dump();
}
static std::string toMessage(const ClientChat& e) {
	json j;
	j["type"]    = "chat";
	j["message"] = e.message;
	return j.dump();
}

std::string toMessage(ClientEvent event) {
	return std::visit([&](auto&& ev) { return toMessage(ev); }, event);
}

std::optional<ClientEvent> fromClientMessage(const std::string& message) {
	const auto j = json::parse(message, nullptr, false);
	if (!j.is_object()) {
		return {};
	}
	if (!j.contains("type") || !j["type"].is_string()) {
		return {};
	}
	const auto type = j["type"].get<std::string>();
	if (type == "put") {
		if (!j.contains("x") || !j.contains("y") || !j["x"].is_number_unsigned() || !j["y"].is_number_unsigned()) {
			return {};
		}
		return ClientPutStone{.c = {j["x"].get<unsigned>(), j["y"].get<unsigned>()}};
	}
	if (type == "pass") {
		return ClientPass{};
	}
	if (type == "resign") {
		return ClientResign{};
	}
	if (type == "chat") {
		if (!j.contains("message") || !j["message"].is_string()) {
			return {};
		}
		return ClientChat{.message = j["message"].get<std::string>()};
	}
	return {};
}

static std::string toMessage(const ServerSessionAssign& e) {
	json j;
	j["type"]      = "session";
	j["sessionId"] = e.sessionId;
	return j.dump();
}
static std::string toMessage(const ServerGameConfig& e) {
	json j;
	j["type"]      = "config";
	j["boardSize"] = e.boardSize;
	j["komi"]      = e.komi;
	j["time"]      = e.timeSeconds;
	return j.dump();
}
static std::string toMessage(const ServerDelta& e) {
	json j;
	j["type"]   = "delta";
	j["turn"]   = e.turn;
	j["seat"]   = static_cast<unsigned>(e.seat);
	j["action"] = static_cast<unsigned>(e.action);
	j["next"]   = static_cast<unsigned>(e.next);
	j["status"] = static_cast<unsigned>(e.status);

	// For Place moves we require a coord; otherwise the message is invalid.
	if (e.action == ServerAction::Place) {
		if (!e.coord.has_value()) {
			assert(false && "ServerDelta::Place requires coord");
			return {};
		}
		j["x"] = e.coord->x;
		j["y"] = e.coord->y;
		if (!e.captures.empty()) {
			auto caps = json::array();
			for (const auto& cap: e.captures) {
				caps.push_back({cap.x, cap.y});
			}
			j["captures"] = std::move(caps);
		}
	}

	return j.dump();
}

static std::string toMessage(const ServerChat& e) {
	json j;
	j["type"]    = "chat";
	j["seat"]    = static_cast<unsigned>(e.seat);
	j["message"] = e.message;
	return j.dump();
}
std::string toMessage(ServerEvent event) {
	return std::visit([&](auto&& ev) { return toMessage(ev); }, event);
}

static std::optional<ServerEvent> fromServerDeltaMessage(const json& j) {
	// clang-format off
	if (!j.contains("turn")   || !j["turn"].is_number_unsigned()   ||
		!j.contains("seat")   || !j["seat"].is_number_unsigned()   ||
		!j.contains("action") || !j["action"].is_number_unsigned() ||
		!j.contains("next")   || !j["next"].is_number_unsigned()   ||
		!j.contains("status") || !j["status"].is_number_unsigned())
	{
		return {};
	}
	// clang-format on

	ServerDelta delta{
	        .turn     = 0u,
	        .seat     = Seat::None,
	        .action   = ServerAction::Place,
	        .coord    = std::nullopt,
	        .captures = {},
	        .next     = Seat::None,
	        .status   = GameStatus::Active,
	};

	const auto actionValue = j["action"].get<unsigned>();
	const auto statusValue = j["status"].get<unsigned>();
	if (!isValid(static_cast<ServerAction>(actionValue)) || !isValid(static_cast<GameStatus>(statusValue))) {
		return {};
	}

	delta.turn           = j["turn"].get<unsigned>();
	const auto seatValue = j["seat"].get<unsigned>();
	const auto nextValue = j["next"].get<unsigned>();
	delta.seat           = static_cast<Seat>(seatValue);
	delta.action         = static_cast<ServerAction>(actionValue);
	delta.next           = static_cast<Seat>(nextValue);
	delta.status         = static_cast<GameStatus>(statusValue);

	// Only real player seats are allowed for deltas.
	if (!isPlayer(delta.seat) || !isPlayer(delta.next)) {
		return {};
	}

	if (delta.action == ServerAction::Place) {
		if (!j.contains("x") || !j.contains("y") || !j["x"].is_number_unsigned() || !j["y"].is_number_unsigned()) {
			return {};
		}
		delta.coord = Coord{.x = j["x"].get<unsigned>(), .y = j["y"].get<unsigned>()};
		if (j.contains("captures")) {
			if (!j["captures"].is_array()) {
				return {};
			}
			for (const auto& cap: j["captures"]) {
				if (!cap.is_array() || cap.size() != 2 || !cap[0].is_number_unsigned() || !cap[1].is_number_unsigned()) {
					return {};
				}
				delta.captures.push_back(Coord{.x = cap[0].get<unsigned>(), .y = cap[1].get<unsigned>()});
			}
		}
	} else {
		if (j.contains("x") || j.contains("y") || j.contains("captures")) {
			return {};
		}
	}

	return delta;
}

std::optional<ServerEvent> fromServerMessage(const std::string& message) {
	const auto j = json::parse(message, nullptr, false);
	if (!j.is_object() || !j.contains("type") || !j["type"].is_string()) {
		return {};
	}
	const auto type = j["type"].get<std::string>();
	if (type == "session") {
		if (!j.contains("sessionId") || !j["sessionId"].is_number_unsigned()) {
			return {};
		}
		return ServerSessionAssign{.sessionId = j["sessionId"].get<SessionId>()};
	}
	if (type == "config") {
		if (!j.contains("boardSize") || !j["boardSize"].is_number_unsigned() || !j.contains("komi") || !j["komi"].is_number() || !j.contains("time") ||
		    !j["time"].is_number_unsigned()) {
			return {};
		}
		return ServerGameConfig{.boardSize = j["boardSize"].get<unsigned>(), .komi = j["komi"].get<double>(), .timeSeconds = j["time"].get<unsigned>()};
	}
	if (type == "delta") {
		return fromServerDeltaMessage(j);
	}
	if (type == "chat") {
		if (!j.contains("seat") || !j["seat"].is_number_unsigned() || !j.contains("message") || !j["message"].is_string()) {
			return {};
		}
		const auto seat = static_cast<Seat>(j["seat"].get<unsigned>());
		if (!isPlayer(seat)) {
			return {};
		}
		return ServerChat{.seat = seat, .message = j["message"].get<std::string>()};
	}
	return {};
}

} // namespace go::gameNet
