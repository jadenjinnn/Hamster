#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hamster {
enum EventType {
  // Window Events
  WindowClose,
  WindowResize,
  FramebufferResize,

  // GUI Events
  LevelEditorViewportSizeChanged,

  // Input Events
  KeyPressed,
  KeyReleased,
  MouseButtonClicked,

  // Application Events
  ActiveSceneChanged,
  ProjectOpened,
  SceneCreated,
  SceneSimulationStarted,
  SceneSimulationPaused,

  // Game Events
  Collision,
  AnimationCompleted,
  ButtonClicked
};

class Event {
public:
  [[nodiscard]] virtual EventType GetEventType() const = 0;

  [[nodiscard]] virtual std::string GetEventName() const = 0;

  [[nodiscard]] bool IsHandled() const { return m_Handled; }
  void Handled() { m_Handled = true; }

private:
  bool m_Handled = false;
};

using SubscriptionHandle = uint64_t;

class EventDispatcher {
public:
  SubscriptionHandle Subscribe(EventType e, std::function<void(Event &)> fn);

  void Unsubscribe(EventType e, SubscriptionHandle handle);

  template <typename T> void Post(Event &e) {
    auto it = m_Observers.find(e.GetEventType());
    if (it == m_Observers.end()) {
      return;
    }

    for (auto &[handle, observer] : it->second) {
      observer(static_cast<T &>(e));
    }
  }

private:
  uint64_t m_NextHandle = 1;
  std::unordered_map<EventType,
                     std::vector<std::pair<SubscriptionHandle,
                                           std::function<void(Event &)>>>>
      m_Observers;
};
} // namespace Hamster
