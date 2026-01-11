#include "gameNet/nwEvents.hpp"

#include <format>
#include <string_view>

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

static std::string toMessage(const ServerSessionAssign& e) {
	return std::format("{}{}", SERVER_SESSION, e.sessionId);
}
static std::string toMessage(const ServerDelta& e) {
	std::string captures;
	for (std::size_t i = 0; i < e.captures.size(); ++i) {
		if (i != 0) {
			captures.push_back(';');
		}
		captures += std::format("{}|{}", e.captures[i].x, e.captures[i].y);
	}

	const auto action = [&]() -> std::string_view {
		switch (e.action) {
		case ServerAction::Place:
			return "PLACE";
		case ServerAction::Pass:
			return "PASS";
		case ServerAction::Resign:
			return "RESIGN";
		}
		return "PLACE";
	}();
	const auto status = [&]() -> std::string_view {
		switch (e.status) {
		case GameStatus::Active:
			return "ACTIVE";
		case GameStatus::BlackWin:
			return "BLACK_WIN";
		case GameStatus::WhiteWin:
			return "WHITE_WIN";
		case GameStatus::Draw:
			return "DRAW";
		}
		return "ACTIVE";
	}();

	const auto x = e.x ? std::to_string(*e.x) : std::string{};
	const auto y = e.y ? std::to_string(*e.y) : std::string{};

	return std::format("{}turn={},seat={},action={},x={},y={},captures={},next={},status={}", SERVER_DELTA, e.turn, static_cast<unsigned>(e.seat), action, x, y,
	                   captures, static_cast<unsigned>(e.next), status);
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
			const auto payload   = message.substr(SERVER_SESSION.size());
			const auto sessionId = static_cast<SessionId>(std::stoul(payload));
			return ServerSessionAssign{.sessionId = sessionId};
		} catch (const std::exception&) { return {}; }
	}

	if (message.rfind(SERVER_DELTA, 0) == 0) {
		const auto payload = message.substr(SERVER_DELTA.size());
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

		std::size_t start = 0;
		while (start <= payload.size()) {
			const auto end   = payload.find(',', start);
			const auto token = payload.substr(start, end == std::string::npos ? std::string::npos : end - start);
			if (!token.empty()) {
				const auto eq = token.find('=');
				if (eq != std::string::npos) {
					const auto key   = token.substr(0, eq);
					const auto value = token.substr(eq + 1);

					try {
						if (key == "turn") {
							delta.turn = static_cast<unsigned>(std::stoul(value));
							hasTurn    = true;
						} else if (key == "seat") {
							const auto seatValue = static_cast<unsigned>(std::stoul(value));
							delta.seat           = static_cast<Seat>(seatValue);
							hasSeat              = true;
						} else if (key == "action") {
							if (value == "PLACE") {
								action = ServerAction::Place;
							} else if (value == "PASS") {
								action = ServerAction::Pass;
							} else if (value == "RESIGN") {
								action = ServerAction::Resign;
							}
						} else if (key == "x") {
							if (!value.empty()) {
								delta.x = static_cast<unsigned>(std::stoul(value));
							}
						} else if (key == "y") {
							if (!value.empty()) {
								delta.y = static_cast<unsigned>(std::stoul(value));
							}
						} else if (key == "captures") {
							if (!value.empty()) {
								std::size_t capStart = 0;
								while (capStart <= value.size()) {
									const auto capEnd   = value.find(';', capStart);
									const auto capToken = value.substr(capStart, capEnd == std::string::npos ? std::string::npos : capEnd - capStart);
									if (!capToken.empty()) {
										const auto sep = capToken.find('|');
										if (sep == std::string::npos) {
											return {};
										}
										const auto cx = static_cast<unsigned>(std::stoul(capToken.substr(0, sep)));
										const auto cy = static_cast<unsigned>(std::stoul(capToken.substr(sep + 1)));
										delta.captures.push_back(CaptureCoord{.x = cx, .y = cy});
									}
									if (capEnd == std::string::npos) {
										break;
									}
									capStart = capEnd + 1;
								}
							}
						} else if (key == "next") {
							const auto seatValue = static_cast<unsigned>(std::stoul(value));
							delta.next           = static_cast<Seat>(seatValue);
							hasNext              = true;
						} else if (key == "status") {
							if (value == "ACTIVE") {
								status = GameStatus::Active;
							} else if (value == "BLACK_WIN") {
								status = GameStatus::BlackWin;
							} else if (value == "WHITE_WIN") {
								status = GameStatus::WhiteWin;
							} else if (value == "DRAW") {
								status = GameStatus::Draw;
							}
						}
					} catch (const std::exception&) { return {}; }
				}
			}
			if (end == std::string::npos) {
				break;
			}
			start = end + 1;
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
			delta.x = std::nullopt;
			delta.y = std::nullopt;
		}

		return delta;
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
