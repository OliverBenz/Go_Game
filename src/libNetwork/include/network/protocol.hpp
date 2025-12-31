#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <variant>

namespace go {
namespace network {

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


inline constexpr std::size_t MAX_PLAYERS    = 2;
inline constexpr std::uint16_t DEFAULT_PORT = 12345;

// Maximum payload we are willing to read in this initial prototype.
// Replace or raise this when switching to larger variable-length frames.
inline constexpr std::uint32_t MAX_PAYLOAD_BYTES = 4 * 1024;

// If we later commit to a fixed-size protocol, this constant is where we would
// define the wire size and drop the header entirely.
inline constexpr std::size_t FIXED_PACKET_PAYLOAD_BYTES = 64;

struct BasicMessageHeader {
	// For variable-sized packets we prefix with payload_size bytes.
	// For fixed-sized packets we would omit this header and always read
	// FIXED_PACKET_PAYLOAD_BYTES worth of data.
	std::uint32_t payload_size{};
};

// Local helpers to move integers to/from network byte order without relying on
// platform headers; safe enough for this starter implementation.
constexpr std::uint32_t byteswap_u32(std::uint32_t value) {
	return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) | ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
}

constexpr std::uint32_t to_network_u32(std::uint32_t value) {
	if constexpr (std::endian::native == std::endian::big) {
		return value;
	}
	return byteswap_u32(value);
}

constexpr std::uint32_t from_network_u32(std::uint32_t value) {
	return to_network_u32(value);
}

} // namespace network
} // namespace go
