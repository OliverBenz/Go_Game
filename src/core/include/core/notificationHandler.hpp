#pragma once

#include "core/IGameListener.hpp"

#include <mutex>
#include <vector>

namespace go {

class NotificationHandler {
public:
	void addListener(IGameListener* listener);
	void remListener(IGameListener* listener);

	//! Calls onBoardChange for every listener.
	void signalBoardChange();

private:
	std::mutex m_listenerMutex;
	std::vector<IGameListener*> m_listeners;
};

} // namespace go