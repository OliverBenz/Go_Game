#pragma once

#include "core/types.hpp"

namespace go {

class IGameListener {
public:
	virtual ~IGameListener()                            = default;
	virtual void onGameNotification(Notification event) = 0;
};

} // namespace go