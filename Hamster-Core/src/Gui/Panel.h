//
// Created by Jaden on 25/08/2024.
//

#ifndef PANEL_H
#define PANEL_H
#include "Core/Scene.h"
#include "Events/Event.h"

namespace Hamster {
class Panel {
public:
  Panel(EventDispatcher *dispatcher, std::shared_ptr<Scene> scene,
        bool defaultOpen = true)
      : m_Dispatcher(dispatcher), m_WindowOpen(defaultOpen),
        m_Scene(std::move(scene)) {
    m_ActiveSceneSub = m_Dispatcher->Subscribe(
        ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(Panel::OnActiveSceneChanged,
                                  ActiveSceneChangedEvent));
  };

  Panel(EventDispatcher *dispatcher, bool defaultOpen)
      : m_Dispatcher(dispatcher), m_WindowOpen(defaultOpen) {
    m_ActiveSceneSub = m_Dispatcher->Subscribe(
        ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(Panel::OnActiveSceneChanged,
                                  ActiveSceneChangedEvent));
  };

  explicit Panel(EventDispatcher *dispatcher) : m_Dispatcher(dispatcher) {
    m_ActiveSceneSub = m_Dispatcher->Subscribe(
        ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(Panel::OnActiveSceneChanged,
                                  ActiveSceneChangedEvent));
  };

  virtual ~Panel() {
    if (m_Dispatcher) {
      m_Dispatcher->Unsubscribe(ActiveSceneChanged, m_ActiveSceneSub);
    }
  };

  virtual void Render() = 0;

  [[nodiscard]] bool IsPanelOpen() const { return m_WindowOpen; }

  void OpenPanel() { m_WindowOpen = true; }
  void ClosePanel() { m_WindowOpen = false; }

protected:
  EventDispatcher *m_Dispatcher = nullptr;
  SubscriptionHandle m_ActiveSceneSub = 0;
  bool m_WindowOpen = true;
  std::shared_ptr<Scene> m_Scene = nullptr;

  void OnActiveSceneChanged(ActiveSceneChangedEvent &e) {
    m_Scene = std::move(e.GetActiveScene());
  };
};
} // namespace Hamster

#endif // PANEL_H
