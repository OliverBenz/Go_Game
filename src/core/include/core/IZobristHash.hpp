#pragma once

#include <cstdint>
#include "core/types.hpp"

namespace go {

//! Interface to allow storing different board size instantiations.
class IZobristHash {
public:
	virtual ~IZobristHash() = default;
	virtual uint64_t stone(Coord c, Player color) = 0;
	virtual uint64_t togglePlayer()               = 0;
};

} // namespace go
