#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

#include <string>

namespace go {
namespace network {
namespace core {

using ConnectionId = std::uint32_t; //!< Identifies a connection on network layer.
using Message      = std::string;   //!< Message type.

inline constexpr std::size_t MAX_PLAYERS    = 2;
inline constexpr std::uint16_t DEFAULT_PORT = 12345;

//! Maximum payload we are willing to read.
//! \note Replace or raise this when switching to larger variable-length frames.
inline constexpr std::uint32_t MAX_PAYLOAD_BYTES = 4 * 1024;

//! For variable-sized packets we prefix with payload_size bytes.
//! \note For fixed-sized packets we would omit this header and always read
struct BasicMessageHeader {
	std::uint32_t payload_size{};
};

// Local helpers to move integers to/from network byte order without relying on platform headers; safe enough for this starter implementation.
// This is intentionally tiny and self-contained so we can swap the wire format later without touching the client/server logic.
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

} // namespace core
} // namespace network
} // namespace go
