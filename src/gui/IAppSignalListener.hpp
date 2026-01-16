#pragma once

#include "core/gameEvent.hpp"

namespace go::gui {

//! Types of signals.
enum AppSignal : uint64_t {
	AS_None         = 0,
	AS_BoardChange  = 1 << 0, //!< Board was modified.
	AS_PlayerChange = 1 << 1, //!< Active player changed.
	AS_StateChange  = 1 << 2, //!< Game state changed. Started or finished.
	AS_NewChat      = 1 << 3, //!< New chat message received.
};

class IAppSignalListener {
public:
	virtual ~IAppSignalListener()             = default;
	virtual void onAppEvent(AppSignal signal) = 0;
};

} // namespace go::gui