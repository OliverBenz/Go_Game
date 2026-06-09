#pragma once

#include "model/coordinate.hpp"
#include "model/player.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace tengen::engine {

enum class Difficulty {
	Easy,
	Medium,
	Hard,
	Custom
};

enum class MoveKind {
	Place,
	Pass,
	Resign
};

enum class SessionState {
	Idle,
	Ready,
	Thinking,
	Error,
	Closed
};

struct SearchLimits {
	std::optional<unsigned> maxVisits{};
	std::optional<double> maxTimeSeconds{};
};

//! Engine-neutral configuration for one game.
struct GameConfig {
	std::size_t boardSize{9u};
	double komi{6.5};
	std::string rules{"chinese"};
	Difficulty difficulty{Difficulty::Medium};
	SearchLimits limits{};
};

//! Move that was accepted by the local rules engine or suggested by the bot backend.
struct Move {
	MoveKind kind{MoveKind::Place};
	Player player{Player::Black};
	std::optional<Coord> coord{};
};

//! Optional metadata returned by an engine search.
struct Decision {
	Move move{};
	std::optional<unsigned> visits{};
	std::optional<double> winrate{};
	std::string rawPayload{};
};

} // namespace tengen::engine
