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

  void ImGuiLayer::AttachDefaultColourScheme() {
    ImGuiStyle *style = &ImGui::GetStyle();
    ImVec4 *colors = style->Colors;

    ImVec4 reallyBlack =
        ImVec4(0.0156862745f, 0.03921568627f, 0.05098039215f, 1.0f);
    ImVec4 black = ImVec4(0.09803921568, 0.09803921568, 0.09803921568, 1.0f);
    ImVec4 grey = ImVec4(0.11372549019f, 0.13333333333f, 0.14509803921f, 1.0f);
    ImVec4 white = ImVec4(0.9725490196f, 0.98823529411f, 1.0f, 1.0f);

    colors[ImGuiCol_Text] = white;
    colors[ImGuiCol_TextDisabled] =
        ImVec4(0.63137254902f, 0.65882352941f, 0.67450980392f, 1.00f);
    colors[ImGuiCol_WindowBg] = grey;
    colors[ImGuiCol_ChildBg] = grey;
    colors[ImGuiCol_PopupBg] = grey;
    colors[ImGuiCol_Border] = black;
    colors[ImGuiCol_BorderShadow] = black;
    colors[ImGuiCol_FrameBg] = black;
    colors[ImGuiCol_FrameBgHovered] = black;
    colors[ImGuiCol_FrameBgActive] = black;
    colors[ImGuiCol_TitleBg] = black;
    colors[ImGuiCol_TitleBgActive] = black;
    colors[ImGuiCol_TitleBgCollapsed] = black;
    colors[ImGuiCol_MenuBarBg] = black;
    colors[ImGuiCol_ScrollbarBg] = white;
    colors[ImGuiCol_ScrollbarGrab] = white;
    colors[ImGuiCol_ScrollbarGrabHovered] = white;
    colors[ImGuiCol_ScrollbarGrabActive] = white;
    colors[ImGuiCol_CheckMark] = black;
    colors[ImGuiCol_SliderGrab] = black;
    colors[ImGuiCol_SliderGrabActive] = black;
    colors[ImGuiCol_Button] = reallyBlack;
    colors[ImGuiCol_ButtonHovered] = reallyBlack;
    colors[ImGuiCol_ButtonActive] = black;
    colors[ImGuiCol_Header] = black;
    colors[ImGuiCol_HeaderHovered] = black;
    colors[ImGuiCol_HeaderActive] = black;
    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered] = black;
    colors[ImGuiCol_SeparatorActive] = black;
    colors[ImGuiCol_ResizeGrip] = black;
    colors[ImGuiCol_ResizeGripHovered] = black;
    colors[ImGuiCol_ResizeGripActive] = black;
    colors[ImGuiCol_Tab] = grey;
    colors[ImGuiCol_TabActive] = grey;
    colors[ImGuiCol_TabUnfocused] = grey;
    colors[ImGuiCol_TabUnfocusedActive] = grey;
    colors[ImGuiCol_TabSelectedOverline] = grey;
    colors[ImGuiCol_TabHovered] = grey;
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = grey;
    colors[ImGuiCol_NavWindowingHighlight] = grey;
    colors[ImGuiCol_NavWindowingDimBg] = grey;
    colors[ImGuiCol_ModalWindowDimBg] = grey;

    style->WindowPadding = ImVec2(8.0f, 16.0f);
    style->WindowBorderSize = 8.0f;
  }
} // namespace Hamster
