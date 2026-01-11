#include "gameNet/nwEvents.hpp"

#include <format>
#include <string_view>

namespace go::gameNet {

static constexpr std::string_view CLIENT_PUT    = "PUT:";
static constexpr std::string_view CLIENT_PASS   = "PASS";
static constexpr std::string_view CLIENT_RESIGN = "RESIGN";
static constexpr std::string_view CLIENT_CHAT   = "CHAT:";

static constexpr std::string_view SERVER_SESSION = "SESSION_ID:";
static constexpr std::string_view SERVER_BOARD   = "GAME_BOARD:";
static constexpr std::string_view SERVER_PASS    = "GAME_PASS:";
static constexpr std::string_view SERVER_RESIGN  = "GAME_RESIGN:";
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

static std::string toMessage(const ServerSessionAssign& e) {
	return std::format("{}{}", SERVER_SESSION, e.sessionId);
}
static std::string toMessage(const ServerBoardUpdate& e) {
	return std::format("{}{},{},{}", SERVER_BOARD, static_cast<unsigned>(e.seat), e.x, e.y);
}
static std::string toMessage(const ServerPass& e) {
	return std::format("{}{}", SERVER_PASS, static_cast<unsigned>(e.seat));
}
static std::string toMessage(const ServerResign& e) {
	return std::format("{}{}", SERVER_RESIGN, static_cast<unsigned>(e.seat));
}
static std::string toMessage(const ServerChat& e) {
	return std::format("{}{},{}", SERVER_CHAT, static_cast<unsigned>(e.seat), e.message);
}
std::string toMessage(ServerEvent event) {
	return std::visit([&](auto&& ev) { return toMessage(ev); }, event);
}

std::optional<ServerEvent> fromServerMessage(const std::string& message) {
	if (message.rfind(SERVER_SESSION, 0) == 0) {
		try {
			const auto payload = message.substr(SERVER_SESSION.size());
			const auto sessionId = static_cast<SessionId>(std::stoul(payload));
			return ServerSessionAssign{.sessionId = sessionId};
		} catch (const std::exception&) {
			return {};
		}
	}

	if (message.rfind(SERVER_BOARD, 0) == 0) {
		const auto payload = message.substr(SERVER_BOARD.size());
		const auto firstComma = payload.find(',');
		if (firstComma == std::string::npos) {
			return {};
		}
		const auto secondComma = payload.find(',', firstComma + 1);
		if (secondComma == std::string::npos) {
			return {};
		}

		try {
			const auto seatValue = static_cast<unsigned>(std::stoul(payload.substr(0, firstComma)));
			const auto x = static_cast<unsigned>(std::stoul(payload.substr(firstComma + 1, secondComma - firstComma - 1)));
			const auto y = static_cast<unsigned>(std::stoul(payload.substr(secondComma + 1)));
			const auto seat = static_cast<Seat>(seatValue);
			if (!isPlayer(seat)) {
				return {};
			}
			return ServerBoardUpdate{.seat = seat, .x = x, .y = y};
		} catch (const std::exception&) {
			return {};
		}
	}

	if (message.rfind(SERVER_PASS, 0) == 0) {
		try {
			const auto payload = message.substr(SERVER_PASS.size());
			const auto seatValue = static_cast<unsigned>(std::stoul(payload));
			const auto seat = static_cast<Seat>(seatValue);
			if (!isPlayer(seat)) {
				return {};
			}
			return ServerPass{.seat = seat};
		} catch (const std::exception&) {
			return {};
		}
	}

	if (message.rfind(SERVER_RESIGN, 0) == 0) {
		try {
			const auto payload = message.substr(SERVER_RESIGN.size());
			const auto seatValue = static_cast<unsigned>(std::stoul(payload));
			const auto seat = static_cast<Seat>(seatValue);
			if (!isPlayer(seat)) {
				return {};
			}
			return ServerResign{.seat = seat};
		} catch (const std::exception&) {
			return {};
		}
	}

	if (message.rfind(SERVER_CHAT, 0) == 0) {
		const auto payload = message.substr(SERVER_CHAT.size());
		const auto commaPos = payload.find(',');
		if (commaPos == std::string::npos) {
			return {};
		}
		try {
			const auto seatValue = static_cast<unsigned>(std::stoul(payload.substr(0, commaPos)));
			const auto seat = static_cast<Seat>(seatValue);
			if (!isPlayer(seat)) {
				return {};
			}
			const auto chat = payload.substr(commaPos + 1);
			return ServerChat{.seat = seat, .message = chat};
		} catch (const std::exception&) {
			return {};
		}
	}

	return {};
}

} // namespace go::gameNet
