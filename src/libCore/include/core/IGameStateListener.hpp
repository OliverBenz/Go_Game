#pragma once

#include "core/gameEvent.hpp"

namespace go {

class IGameStateListener {
public:
	virtual ~IGameStateListener()                    = default;
	virtual void onGameDelta(const GameDelta& delta) = 0;
};

} // namespace go
