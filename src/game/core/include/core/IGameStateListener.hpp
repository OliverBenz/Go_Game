#pragma once

#include "core/gameEvent.hpp"

namespace tengen {

class IGameStateListener {
public:
	virtual ~IGameStateListener()                    = default;
	virtual void onGameDelta(const GameDelta& delta) = 0;
};

} // namespace tengen
