#pragma once

#include "model/coordinate.hpp"
#include "model/player.hpp"

#include <cstdint>

namespace tengen {

//! Interface to allow storing different board size instantiations.
class IZobristHash {
public:
	virtual ~IZobristHash()                       = default;
	virtual uint64_t stone(Coord c, Player color) = 0;
	virtual uint64_t togglePlayer()               = 0;
};

} // namespace tengen
