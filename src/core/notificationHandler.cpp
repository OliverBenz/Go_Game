#include "core/notificationHandler.hpp"

#include <algorithm>

namespace go {

void NotificationHandler::addListener(IGameListener* listener) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listeners.push_back(listener);
}

void NotificationHandler::remListener(IGameListener* listener) {
    m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
}

void NotificationHandler::signalBoardChange() {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for(const auto& listener : m_listeners) {
        // TODO: How to ensure these listener side functions are not heavy.
        //       Else they might block the game thread.
        listener->onBoardChange();
    }
}

}