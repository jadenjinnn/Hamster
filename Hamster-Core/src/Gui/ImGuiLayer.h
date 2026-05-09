//
// Created by Jaden on 23/08/2024.
//

#ifndef IMGUILAYER_H
#define IMGUILAYER_H

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include "Core/Layer.h"

struct GLFWwindow;

namespace Hamster {

class ImGuiLayer : public Layer {
public:
  ImGuiLayer() = default;
  ~ImGuiLayer() override = default;

  void SetWindow(GLFWwindow *window) { m_Window = window; }

  void Begin();
  void End();

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate() override;

private:
  GLFWwindow *m_Window = nullptr;
};

} // namespace Hamster

#endif // IMGUILAYER_H
