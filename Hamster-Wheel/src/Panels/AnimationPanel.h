#ifndef UIPROTO_ANIMATION_PANEL_H
#define UIPROTO_ANIMATION_PANEL_H

#include <Hamster.h>
#include <Core/UUID.h>
#include <Core/Components.h>
#include <memory>
#include <string>
#include <vector>

namespace Hamster {
class AssetManager;
}

class AnimationPanel {
public:
    AnimationPanel(Hamster::EventDispatcher *dispatcher,
                   std::shared_ptr<Hamster::Scene> scene,
                   Hamster::AssetManager *assetManager);

    void Render();
    void OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
    void UpdatePreview();

    Hamster::EventDispatcher *m_Dispatcher;
    std::shared_ptr<Hamster::Scene> m_Scene;
    Hamster::AssetManager *m_AssetManager;

    Hamster::UUID m_CurrentAnimUUID = Hamster::UUID::GetNil();
    std::string m_AnimName;
    char m_NameBuffer[128] = {0};
    std::vector<Hamster::AnimationKeyframe> m_Keyframes;
    float m_Duration = 0.0f;
    int m_SelectedKeyframe = -1;
    bool m_DraggingKeyframe = false;
    bool m_Playing = false;
    float m_PlaybackTime = 0.0f;
    float m_LastFrameTime = 0.0f;
    bool m_Dirty = false;
};

#endif // UIPROTO_ANIMATION_PANEL_H
