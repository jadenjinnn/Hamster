//
// Created by Jaden on 23/08/2024.
//

#ifndef EDITOR_H
#define EDITOR_H

#include <functional>
#include <glm/glm.hpp>
#include <imgui.h>
#include <memory>

#include "GLFW/glfw3.h"

#include <Hamster.h>
#include <Renderer/FramebufferTexture.h>

#include "Core/Scene.h"
#include "Panels/AssetBrowser.h"
#include "Panels/Console.h"
#include "Panels/FileBrowser.h"
#include "Panels/Hierarchy.h"
#include "Panels/MenuBar.h"
#include "Panels/PropertyEditor.h"
#include "Panels/StartPauseModal.h"

class EditorLayer : public Hamster::Layer {
public:
  EditorLayer(Hamster::Application *app,
              std::shared_ptr<Hamster::Scene> scene);

  void OnAttach() override;

  void OnUpdate() override;

  void OnImGuiUpdate() override;

  void ActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
  bool m_WindowOpen = true;
  bool m_WindowFocused = false;

  Hamster::Application *m_App;
  Hamster::EventDispatcher *m_Dispatcher;
  Hamster::Renderer *m_Renderer;
  std::shared_ptr<Hamster::Scene> m_Scene;

  ImVec2 m_LevelEditorAvailRegion = {0.0f, 0.0f};
  glm::vec2 m_ViewportOffset = {0.0f, 0.0f};
  Hamster::FramebufferTexture m_FramebufferTexture;

  std::unique_ptr<PropertyEditor> m_PropertyEditor;
  std::unique_ptr<Hierarchy> m_Hierarchy;
  std::unique_ptr<FileBrowser> m_FileBrowser;
  std::unique_ptr<AssetBrowser> m_AssetBrowser;
  std::unique_ptr<Console> m_Console;

  std::unique_ptr<StartPauseModal> m_StartPauseModal;
  std::unique_ptr<MenuBar> m_MenuBar;

  void FramebufferSizeChanged(Hamster::FramebufferResizeEvent &e);

  int m_ViewportWidth;
  int m_ViewportHeight;

  float m_TopX = 0.0f;
  float m_TopY = 0.0f;
  float m_MouseDragSpeed = 1.0f;
  float m_MouseWheelZoomSpeed = 0.1f;

  bool xGuizmoHeld = false;
  bool yGuizmoHeld = false;
  bool pickedHeld = false;

  bool entityHeld = false;

  bool topLeftGrabberHeld = false;
  bool topRightGrabberHeld = false;
  bool bottomLeftGrabberHeld = false;
  bool bottomRightGrabberHeld = false;
  bool sceneBackgroundHeld = false;

  float mouseHeldOffsetX = 0.0f;
  float mouseHeldOffsetY = 0.0f;

  float mouseHeldTransformX = 0.0f;
  float mouseHeldTransformY = 0.0f;
};

#endif // EDITOR_H
