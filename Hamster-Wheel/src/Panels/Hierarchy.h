#ifndef UIPROTO_HIERARCHY_H
#define UIPROTO_HIERARCHY_H

#include <Hamster.h>
#include <entt/entt.hpp>
#include <memory>

class Hierarchy {
public:
    Hierarchy(Hamster::EventDispatcher *dispatcher,
              std::shared_ptr<Hamster::Scene> scene);

    void Render();

    entt::entity GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(entt::entity e) { m_SelectedEntity = e; }

    void OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
    Hamster::EventDispatcher *m_Dispatcher;
    std::shared_ptr<Hamster::Scene> m_Scene;
    entt::entity m_SelectedEntity = entt::null;
    char m_SearchBuffer[128] = {0};
};

#endif // UIPROTO_HIERARCHY_H
