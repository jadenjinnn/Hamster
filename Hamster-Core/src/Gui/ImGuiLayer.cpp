//
// Created by Jaden on 23/08/2024.
//

#include "HamsterPCH.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Core/Application.h"
#include "Renderer/Renderer.h"

#include "ImGuiLayer.h"

namespace Hamster {
  void ImGuiLayer::OnAttach() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable
    // Multi-Viewport / Platform Windows

    // Color scheme applied by HamsterTheme::Apply() in HamsterWheelApp.cpp

    // ImGuiStyle &style = ImGui::GetStyle();
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    // style.WindowRounding = 0.0f;
    // style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);

    ImGui_ImplOpenGL3_Init("#version 400 core");
  }

  void ImGuiLayer::OnDetach() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void ImGuiLayer::Begin() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    // Renderer::Clear();

    ImGui::NewFrame();
    // ImGuizmo::BeginFrame();
  }

  void ImGuiLayer::End() {
    ImGui::Render();

    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow *backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }
  }

  void ImGuiLayer::OnUpdate() {
  }
} // namespace Hamster
