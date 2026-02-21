#pragma once

#include "core/gameEvent.hpp"

namespace tengen {

class IGameSignalListener {
public:
	virtual ~IGameSignalListener()              = default;
	virtual void onGameEvent(GameSignal signal) = 0;
};

} // namespace tengen