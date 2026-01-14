#pragma once

#include "core/gameEvent.hpp"

namespace go::gui {

//! Types of signals.
enum GameSignal : uint64_t {
	GS_None         = 0,
	GS_BoardChange  = 1 << 0, //!< Board was modified.
	GS_PlayerChange = 1 << 1, //!< Active player changed.
	GS_StateChange  = 1 << 2, //!< Game state changed. Started or finished.
};

class IGameSignalListener {
public:
	virtual ~IGameSignalListener()              = default;
	virtual void onGameEvent(GameSignal signal) = 0;
};

} // namespace go::gui