#include "gameNet/nwEvents.hpp"

#include <cassert>
#include <charconv>
#include <format>
#include <string_view>
#include <vector>

namespace go::gameNet {

static constexpr std::string_view CLIENT_PUT    = "PUT:";
static constexpr std::string_view CLIENT_PASS   = "PASS";
static constexpr std::string_view CLIENT_RESIGN = "RESIGN";
static constexpr std::string_view CLIENT_CHAT   = "CHAT:";

static constexpr std::string_view SERVER_SESSION = "SESSION_ID:";
static constexpr std::string_view SERVER_DELTA   = "DELTA:";
static constexpr std::string_view SERVER_CHAT    = "NEW_CHAT:";

static std::string toMessage(const ClientPutStone& e) {
	return std::format("{}{},{}", CLIENT_PUT, e.x, e.y);
}
static std::string toMessage(const ClientPass&) {
	return std::string{CLIENT_PASS};
}
static std::string toMessage(const ClientResign&) {
	return std::string{CLIENT_RESIGN};
}
static std::string toMessage(const ClientChat& e) {
	return std::format("{}{}", CLIENT_CHAT, e.message);
}

std::string toMessage(ClientEvent event) {
	return std::visit([&](auto&& ev) { return toMessage(ev); }, event);
}

std::optional<ClientEvent> fromClientMessage(const std::string& message) {
	if (message.rfind(CLIENT_PUT, 0) == 0) {
		// Expect "PUT:x,y"
		const auto payload = message.substr(CLIENT_PUT.size());
		auto commaPos      = payload.find(',');
		if (commaPos == std::string::npos) {
			return {};
		}

		try {
			const auto x = static_cast<unsigned>(std::stoul(payload.substr(0, commaPos)));
			const auto y = static_cast<unsigned>(std::stoul(payload.substr(commaPos + 1)));

			return ClientPutStone{.x = x, .y = y};
		} catch (const std::exception&) {}

		return {};
	}

	if (message.rfind(CLIENT_CHAT, 0) == 0) {
		return ClientChat{.message = message.substr(CLIENT_CHAT.size())};
	}

	if (message == CLIENT_PASS) {
		return ClientPass{};
	}

	if (message == CLIENT_RESIGN) {
		return ClientResign{};
	}

	// Invalid
	return {};
}

static std::string encodeCaptures(const std::vector<CaptureCoord>& caps) {
	std::string out;
	for (std::size_t i = 0; i < caps.size(); ++i) {
		if (i)
			out.push_back(';');
		out += std::to_string(caps[i].x);
		out.push_back('|');
		out += std::to_string(caps[i].y);
	}
	return out;
}

static std::string toMessage(const ServerSessionAssign& e) {
	return std::format("{}{}", SERVER_SESSION, e.sessionId);
}
static std::string toMessage(const ServerDelta& e) {
	std::string payload;
	payload.reserve(96);
	payload += std::format("turn={},seat={},action={}", e.turn, static_cast<unsigned>(e.seat), static_cast<unsigned>(e.action));

	if (e.action == ServerAction::Place) {
		if (!e.x || !e.y) {
			assert(false && "ServerDelta::Place requires x and y");
			return {};
		}
		payload += std::format(",x={},y={}", *e.x, *e.y);
		if (!e.captures.empty()) {
			payload += ",captures=";
			payload += encodeCaptures(e.captures);
		}
	}
	payload += std::format(",next={},status={}", static_cast<unsigned>(e.next), static_cast<unsigned>(e.status));

	payload.insert(0, SERVER_DELTA);
	return payload;
}

static std::string toMessage(const ServerChat& e) {
	return std::format("{}{},{}", SERVER_CHAT, static_cast<unsigned>(e.seat), e.message);
}
std::string toMessage(ServerEvent event) {
	return std::visit([&](auto&& ev) { return toMessage(ev); }, event);
}

static std::optional<ServerEvent> fromServerDeltaMessage(const std::string& payload) {
	auto parseUnsigned = [](std::string_view value, unsigned& out) -> bool {
		if (value.empty()) {
			return false;
		}
		const auto* begin    = value.data();
		const auto* end      = value.data() + value.size();
		unsigned parsed      = 0;
		const auto [ptr, ec] = std::from_chars(begin, end, parsed);
		if (ec != std::errc() || ptr != end) {
			return false;
		}
		out = parsed;
		return true;
	};

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

	std::optional<ServerAction> action;
	std::optional<GameStatus> status;
	bool hasSeat = false;
	bool hasNext = false;
	bool hasTurn = false;
	bool hasAction = false;
	bool hasStatus = false;
	bool hasX = false;
	bool hasY = false;
	bool hasCaptures = false;

	std::vector<std::string_view> tokens;
	tokens.reserve(12);
	std::size_t start = 0;
	while (start <= payload.size()) {
		const auto end   = payload.find(',', start);
		const auto token = std::string_view(payload).substr(start, end == std::string::npos ? std::string_view::npos : end - start);
		if (token.empty()) {
			return {};
		}
		tokens.push_back(token);
		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
	}

	for (const auto token: tokens) {
		const auto eq = token.find('=');
		if (eq == std::string_view::npos) {
			return {};
		}
		const auto key   = token.substr(0, eq);
		const auto value = token.substr(eq + 1);
		if (value.empty()) {
			return {};
		}

		if (key == "turn") {
			if (hasTurn) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			delta.turn = parsed;
			hasTurn    = true;
		} else if (key == "seat") {
			if (hasSeat) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			delta.seat = static_cast<Seat>(parsed);
			hasSeat    = true;
		} else if (key == "action") {
			if (hasAction) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			if (parsed > static_cast<unsigned>(ServerAction::Resign)) {
				return {};
			}
			action = static_cast<ServerAction>(parsed);
			hasAction = true;
		} else if (key == "x") {
			if (hasX) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			delta.x = parsed;
			hasX = true;
		} else if (key == "y") {
			if (hasY) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			delta.y = parsed;
			hasY = true;
		} else if (key == "captures") {
			if (hasCaptures) {
				return {};
			}
			hasCaptures = true;
			std::size_t capStart = 0;
				while (capStart <= value.size()) {
					const auto capEnd   = value.find(';', capStart);
					const auto capToken = value.substr(capStart, capEnd == std::string_view::npos ? std::string_view::npos : capEnd - capStart);
					if (capToken.empty()) {
						return {};
					}
					const auto sep = capToken.find('|');
					if (sep == std::string_view::npos) {
						return {};
					}
					unsigned cx = 0;
					unsigned cy = 0;
					if (!parseUnsigned(capToken.substr(0, sep), cx) || !parseUnsigned(capToken.substr(sep + 1), cy)) {
						return {};
					}
					delta.captures.push_back(CaptureCoord{.x = cx, .y = cy});
					if (capEnd == std::string_view::npos) {
						break;
					}
				capStart = capEnd + 1;
			}
		} else if (key == "next") {
			if (hasNext) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			delta.next = static_cast<Seat>(parsed);
			hasNext    = true;
		} else if (key == "status") {
			if (hasStatus) {
				return {};
			}
			unsigned parsed = 0;
			if (!parseUnsigned(value, parsed)) {
				return {};
			}
			if (parsed > static_cast<unsigned>(GameStatus::Draw)) {
				return {};
			}
			status = static_cast<GameStatus>(parsed);
			hasStatus = true;
		} else {
			return {};
		}
	}

	if (!hasTurn || !hasSeat || !hasNext || !action || !status) {
		return {};
	}
	if (!isPlayer(delta.seat) || !isPlayer(delta.next)) {
		return {};
	}
	delta.action = *action;
	delta.status = *status;
	if (delta.action == ServerAction::Place) {
		if (!delta.x || !delta.y) {
			return {};
		}
	} else {
		if (delta.x || delta.y || hasCaptures) {
			return {};
		}
		delta.x = std::nullopt;
		delta.y = std::nullopt;
	}

	return delta;
}

std::optional<ServerEvent> fromServerMessage(const std::string& message) {
	if (message.rfind(SERVER_SESSION, 0) == 0) {
		try {
			const auto payload   = message.substr(SERVER_SESSION.size());
			const auto sessionId = static_cast<SessionId>(std::stoul(payload));
			return ServerSessionAssign{.sessionId = sessionId};
		} catch (const std::exception&) { return {}; }
	}

	if (message.rfind(SERVER_DELTA, 0) == 0) {
		const auto payload = message.substr(SERVER_DELTA.size());
		return fromServerDeltaMessage(payload);
	}

	if (message.rfind(SERVER_CHAT, 0) == 0) {
		const auto payload  = message.substr(SERVER_CHAT.size());
		const auto commaPos = payload.find(',');
		if (commaPos == std::string::npos) {
			return {};
		}
		try {
			const auto seatValue = static_cast<unsigned>(std::stoul(payload.substr(0, commaPos)));
			const auto seat      = static_cast<Seat>(seatValue);
			if (!isPlayer(seat)) {
				return {};
			}
			const auto chat = payload.substr(commaPos + 1);
			return ServerChat{.seat = seat, .message = chat};
		} catch (const std::exception&) { return {}; }
	}

	return {};
}

} // namespace go::gameNet
