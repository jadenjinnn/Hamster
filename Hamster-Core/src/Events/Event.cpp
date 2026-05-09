#include "HamsterPCH.h"

#include "Event.h"

namespace Hamster {
    SubscriptionHandle EventDispatcher::Subscribe(EventType e, std::function<void(Event &)> fn) {
        auto handle = m_NextHandle++;
        m_Observers[e].emplace_back(handle, std::move(fn));
        return handle;
    }

    void EventDispatcher::Unsubscribe(EventType e, SubscriptionHandle handle) {
        auto it = m_Observers.find(e);
        if (it == m_Observers.end()) return;

        auto &vec = it->second;
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                           [handle](const auto &pair) { return pair.first == handle; }),
            vec.end());
    }
} // namespace Hamster
