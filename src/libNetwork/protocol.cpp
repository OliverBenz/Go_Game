#include "network/protocol.hpp"

#include <format>
#include <string_view>

namespace go::network {

static constexpr std::string_view MSG_PUT    = "PUT:";
static constexpr std::string_view MSG_CHAT   = "CHAT:";
static constexpr std::string_view MSG_PASS   = "PASS";
static constexpr std::string_view MSG_RESIGN = "RESIGN";

static std::string toMessage(const NwPutStoneEvent& e) {
	return std::format("{}{},{}", MSG_PUT, e.x, e.y);
}
static std::string toMessage(const NwPassEvent&) {
	return std::string{MSG_PASS};
}
static std::string toMessage(const NwResignEvent&) {
	return std::string{MSG_RESIGN};
}
static std::string toMessage(const NwChatEvent& e) {
	return std::format("{}{}", MSG_CHAT, e.message);
}

std::string toMessage(NwEvent event) {
	return std::visit([&](auto&& ev) { return toMessage(ev); }, event);
}

std::optional<NwEvent> fromMessage(const std::string& message) {
	if (message.rfind(MSG_PUT, 0) == 0) {
		// Expect "PUT:x,y"
		const auto payload = message.substr(MSG_PUT.size());
		auto commaPos      = payload.find(',');
		if (commaPos == std::string::npos) {
			return {};
		}

		try {
			const auto x = static_cast<unsigned>(std::stoul(payload.substr(0, commaPos)));
			const auto y = static_cast<unsigned>(std::stoul(payload.substr(commaPos + 1)));

			return NwPutStoneEvent{.x = x, .y = y};
		} catch (const std::exception&) {}

		return {};
	}

	if (message.rfind(MSG_CHAT, 0) == 0) {
		return NwChatEvent{.message = message.substr(MSG_CHAT.size())};
	}

	if (message == MSG_PASS) {
		return NwPassEvent{};
	}

	if (message == MSG_RESIGN) {
		return NwResignEvent{};
	}

	// Invalid
	return {};
}

} // namespace go::network
