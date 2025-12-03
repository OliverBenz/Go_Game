#include "sgfHandler.hpp"

namespace go::core {

Coord fromSGF(const std::string& s) {
    return {
        static_cast<Id>(s[0] - 'a'),
        static_cast<Id>(s[1] - 'a')
    };
}

std::string toSGF(const Coord c) {
    return {
        char('a' + c.x),
        char('a' + c.y)
    };
}

}