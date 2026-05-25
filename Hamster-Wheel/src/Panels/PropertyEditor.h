#ifndef UIPROTO_PROPERTY_EDITOR_H
#define UIPROTO_PROPERTY_EDITOR_H

#include <Hamster.h>
#include <Core/UUID.h>
#include <Core/Components.h>
#include <memory>

namespace Hamster {
class AssetManager;
}

class ColliderEditor;

class PropertyEditor {
public:
    PropertyEditor(Hamster::EventDispatcher *dispatcher,
                   std::shared_ptr<Hamster::Scene> scene,
                   Hamster::AssetManager *assetManager,
                   ColliderEditor *colliderEditor);
    ~PropertyEditor();

    void Render();
    void SetSelectedEntity(Hamster::UUID uuid);

    void OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
    Hamster::EventDispatcher *m_Dispatcher;
    Hamster::SubscriptionHandle m_ActiveSceneSub = 0;
    std::shared_ptr<Hamster::Scene> m_Scene;
    Hamster::AssetManager *m_AssetManager;
    ColliderEditor *m_ColliderEditor;

    Hamster::UUID m_SelectedEntity = Hamster::UUID::GetNil();
    Hamster::Name      *m_Name      = nullptr;
    Hamster::Transform *m_Transform = nullptr;
    Hamster::Sprite    *m_Sprite    = nullptr;
    Hamster::Rigidbody *m_Rigidbody = nullptr;
    Hamster::Animation *m_Animation = nullptr;
    Hamster::Behaviour *m_Behaviour = nullptr;
    Hamster::UIButton  *m_UIButton  = nullptr;
    Hamster::UIText    *m_UIText    = nullptr;
};

#endif // UIPROTO_PROPERTY_EDITOR_H
