#pragma once

#include "core/types.hpp"

namespace go {

class IGameListener {
public:
	virtual ~IGameListener()                    = default;
	virtual void onGameEvent(GameSignal signal) = 0;
};

} // namespace go