#pragma once

#include "engine/types.hpp"

#include <optional>
#include <string>

namespace tengen::engine {

//! Backend-neutral engine session for exactly one running game.
//! The local rules engine stays authoritative; implementations only mirror accepted moves.
class IEngineSession {
public:
	virtual ~IEngineSession() = default;

	virtual bool newGame(const GameConfig& config) = 0;

	//! Record a move only after the local rules engine accepted it.
	virtual bool recordMove(const Move& move) = 0;

	//! Request the next move for the given side without mutating local game state.
	virtual std::optional<Decision> requestMove(Player player) = 0;

	virtual SessionState state() const  = 0;
	virtual std::string lastError() const = 0;

	virtual void shutdown() = 0;
};

} // namespace tengen::engine
