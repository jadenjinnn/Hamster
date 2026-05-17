#ifndef UIPROTO_HIERARCHY_H
#define UIPROTO_HIERARCHY_H

#include <Hamster.h>
#include <entt/entt.hpp>
#include <memory>
#include <unordered_set>

class Hierarchy {
public:
    Hierarchy(Hamster::EventDispatcher *dispatcher,
              std::shared_ptr<Hamster::Scene> scene);
    ~Hierarchy();

    void Render();

    entt::entity GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(entt::entity e) { m_SelectedEntity = e; }

    void OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
    void RenderEntityRecursive(Hamster::UUID uuid, int depth);
    void RenderSiblingDropGap(Hamster::UUID parent, uint32_t insertIndex);

    Hamster::EventDispatcher *m_Dispatcher;
    Hamster::SubscriptionHandle m_ActiveSceneSub = 0;
    std::shared_ptr<Hamster::Scene> m_Scene;
    entt::entity m_SelectedEntity = entt::null;
    char m_SearchBuffer[128] = {0};
    std::unordered_set<Hamster::UUID> m_Collapsed;
};

#endif // UIPROTO_HIERARCHY_H
