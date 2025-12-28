#include "core/sgfHandler.hpp"

namespace go {

Coord fromSGF(const std::string& s) {
	return {static_cast<Id>(s[0u] - 'a'), static_cast<Id>(s[1u] - 'a')};
}

std::string toSGF(const Coord c) {
	return {char('a' + c.x), char('a' + c.y)};
}

} // namespace go