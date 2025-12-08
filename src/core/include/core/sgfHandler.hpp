#pragma once

#include "core/types.hpp"

#include <string>

namespace go {

//! Convert sgf code to core game coordinate.
Coord fromSGF(const std::string& s);

//! Convert core game coordinate to sgf code.
std::string toSGF(Coord c);


} // namespace go