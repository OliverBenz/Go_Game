#pragma once

#include "core/gameEvent.hpp"

namespace go {

class IGameSignalListener {
public:
	virtual ~IGameSignalListener()              = default;
	virtual void onGameEvent(GameSignal signal) = 0;
};

} // namespace go