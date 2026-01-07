#pragma once

#include <optional>
#include <string>
#include <variant>

namespace go::gameNet {

// Network events
struct NwPutStoneEvent {
	unsigned x;
	unsigned y;
};
struct NwPassEvent {};
struct NwResignEvent {};
struct NwChatEvent {
	std::string message;
};

using NwEvent = std::variant<NwPutStoneEvent, NwPassEvent, NwResignEvent, NwChatEvent>;

//! Network event to message string.
std::string toMessage(NwEvent event);
//! Message string to network event.
std::optional<NwEvent> fromMessage(const std::string& message);

} // namespace go::gameNet
