#pragma once

#include "core/position.hpp"

#include <filesystem>

namespace tengen {

//! Read a linear sgf file into a gamePosition.
bool getGameStateFromSgf(std::filesystem::path sgfPath, GamePosition& position);

//! Write a game position into a sgf file.
bool saveGameAsSgf(std::filesystem::path sgfPath, const GamePosition& position);

} // namespace tengen
