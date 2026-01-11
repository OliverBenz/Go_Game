#include "gameNet/nwEvents.hpp"

#include <cassert>
#include <nlohmann/json.hpp>

namespace go::gameNet {

using nlohmann::json;

static std::string toMessage(const ClientPutStone& e) {
	json j;
	j["type"] = "put";
	j["x"]    = e.x;
	j["y"]    = e.y;
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
		return ClientPutStone{.x = j["x"].get<unsigned>(), .y = j["y"].get<unsigned>()};
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
static std::string toMessage(const ServerDelta& e) {
	json j;
	j["type"]   = "delta";
	j["turn"]   = e.turn;
	j["seat"]   = static_cast<unsigned>(e.seat);
	j["action"] = static_cast<unsigned>(e.action);
	j["next"]   = static_cast<unsigned>(e.next);
	j["status"] = static_cast<unsigned>(e.status);

	if (e.action == ServerAction::Place) {
		if (!e.x || !e.y) {
			assert(false && "ServerDelta::Place requires x and y");
			return {};
		}
		j["x"] = *e.x;
		j["y"] = *e.y;
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
	if (!j.contains("turn") || !j["turn"].is_number_unsigned()) {
		return {};
	}
	if (!j.contains("seat") || !j["seat"].is_number_unsigned()) {
		return {};
	}
	if (!j.contains("action") || !j["action"].is_number_unsigned()) {
		return {};
	}
	if (!j.contains("next") || !j["next"].is_number_unsigned()) {
		return {};
	}
	if (!j.contains("status") || !j["status"].is_number_unsigned()) {
		return {};
	}

	ServerDelta delta{
	        .turn     = 0u,
	        .seat     = Seat::None,
	        .action   = ServerAction::Place,
	        .x        = std::nullopt,
	        .y        = std::nullopt,
	        .captures = {},
	        .next     = Seat::None,
	        .status   = GameStatus::Active,
	};

	const auto actionValue = j["action"].get<unsigned>();
	const auto statusValue = j["status"].get<unsigned>();
	if (actionValue > static_cast<unsigned>(ServerAction::Resign)) {
		return {};
	}
	if (statusValue > static_cast<unsigned>(GameStatus::Draw)) {
		return {};
	}

	delta.turn           = j["turn"].get<unsigned>();
	const auto seatValue = j["seat"].get<unsigned>();
	const auto nextValue = j["next"].get<unsigned>();
	delta.seat           = static_cast<Seat>(seatValue);
	delta.action         = static_cast<ServerAction>(actionValue);
	delta.next           = static_cast<Seat>(nextValue);
	delta.status         = static_cast<GameStatus>(statusValue);

	if (!isPlayer(delta.seat) || !isPlayer(delta.next)) {
		return {};
	}

	if (delta.action == ServerAction::Place) {
		if (!j.contains("x") || !j.contains("y") || !j["x"].is_number_unsigned() || !j["y"].is_number_unsigned()) {
			return {};
		}
		delta.x = j["x"].get<unsigned>();
		delta.y = j["y"].get<unsigned>();
		if (j.contains("captures")) {
			if (!j["captures"].is_array()) {
				return {};
			}
			for (const auto& cap: j["captures"]) {
				if (!cap.is_array() || cap.size() != 2 || !cap[0].is_number_unsigned() || !cap[1].is_number_unsigned()) {
					return {};
				}
				delta.captures.push_back(CaptureCoord{.x = cap[0].get<unsigned>(), .y = cap[1].get<unsigned>()});
			}
		}
	} else {
		if (j.contains("x") || j.contains("y") || j.contains("captures")) {
			return {};
		}
		delta.x = std::nullopt;
		delta.y = std::nullopt;
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
