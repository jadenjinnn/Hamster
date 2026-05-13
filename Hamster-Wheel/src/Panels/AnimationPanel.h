#ifndef ANIMATIONPANEL_H
#define ANIMATIONPANEL_H

#include <Gui/Panel.h>
#include <Core/UUID.h>
#include <Renderer/Texture.h>

namespace Hamster { class AssetManager; }

class AnimationPanel : public Hamster::Panel {
public:
  AnimationPanel(Hamster::EventDispatcher *dispatcher,
                 std::shared_ptr<Hamster::Scene> scene,
                 Hamster::AssetManager *assetManager);

  void Render() override;

private:
  void RenderToolbar();
  void RenderTimeline();
  void RenderKeyframeList();
  void UpdatePreview();

  Hamster::AssetManager *m_AssetManager;

  Hamster::UUID m_CurrentAnimUUID = Hamster::UUID::GetNil();
  std::string m_AnimName;
  std::vector<Hamster::AnimationKeyframe> m_Keyframes;
  float m_Duration = 0.0f;

  bool m_Playing = false;
  float m_PlaybackTime = 0.0f;
  float m_LastFrameTime = 0.0f;

  int m_SelectedKeyframe = -1;
  bool m_DraggingKeyframe = false;

  char m_NameBuffer[128] = {};
  bool m_Dirty = false;
};

#endif // ANIMATIONPANEL_H
