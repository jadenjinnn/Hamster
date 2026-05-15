#include "HamsterPCH.h"

#include <glad/glad.h>

#include "Application.h"
#include "Window.h"

#ifdef _WIN32
// Borderless-with-snap setup: re-add WS_THICKFRAME (Aero Snap requires it) and
// intercept WM_NCCALCSIZE so the window stays visually borderless.
#include <GLFW/glfw3native.h>
#include <Windows.h>
#include <windowsx.h>
#include <Commctrl.h>

static LRESULT CALLBACK BorderlessSubclassProc(HWND hwnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam,
                                               UINT_PTR, DWORD_PTR) {
  switch (msg) {
  case WM_NCCALCSIZE: {
    if (wParam == TRUE) {
      // Strip the non-client area so the window paints to its full extent.
      // When maximised, Windows oversizes the window beyond the monitor and
      // DWM paints a 1px accent line at the top — clamp the client rect to
      // the monitor's work area to avoid both.
      auto *p = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
      WINDOWPLACEMENT wp{sizeof(wp)};
      if (GetWindowPlacement(hwnd, &wp) && wp.showCmd == SW_SHOWMAXIMIZED) {
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
        if (mon) {
          MONITORINFO mi{sizeof(mi)};
          if (GetMonitorInfoW(mon, &mi)) {
            p->rgrc[0] = mi.rcWork;
          }
        }
      }
      return 0;
    }
    break;
  }
  case WM_NCHITTEST: {
    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    RECT rc;
    GetWindowRect(hwnd, &rc);
    const int border = 6;
    bool left = pt.x < rc.left + border;
    bool right = pt.x >= rc.right - border;
    bool top = pt.y < rc.top + border;
    bool bottom = pt.y >= rc.bottom - border;
    if (top && left)
      return HTTOPLEFT;
    if (top && right)
      return HTTOPRIGHT;
    if (bottom && left)
      return HTBOTTOMLEFT;
    if (bottom && right)
      return HTBOTTOMRIGHT;
    if (left)
      return HTLEFT;
    if (right)
      return HTRIGHT;
    if (top)
      return HTTOP;
    if (bottom)
      return HTBOTTOM;
    // Body and title bar — let ImGui see clicks; our title-bar drag
    // forwards WM_NCLBUTTONDOWN itself when the user clicks the title area.
    return HTCLIENT;
  }
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void EnableBorderlessSnap(GLFWwindow *gw) {
  HWND hwnd = glfwGetWin32Window(gw);
  LONG style = GetWindowLongW(hwnd, GWL_STYLE);
  // Re-add the styles GLFW stripped when GLFW_DECORATED was false.
  SetWindowLongW(hwnd, GWL_STYLE,
                 style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX |
                     WS_SYSMENU);
  SetWindowSubclass(hwnd, BorderlessSubclassProc, 1, 0);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}
#endif

static uint8_t s_NumWindows = 0;

namespace Hamster {
Window::Window(const WindowProps &props) {
  if (glfwInit()) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (props.borderless) {
      glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    }
  } else {
    std::cout << "glfw failed to initialise" << std::endl;
  }

  /*glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);*/

  GLFWwindow *window = glfwCreateWindow(props.width, props.height,
                                        props.title.c_str(), NULL, NULL);

  if (window == NULL) {
    std::cout << "Window creation failed" << std::endl;
  } else {
    s_NumWindows += 1;
  }

  m_Window = window;

#ifdef _WIN32
  if (window != nullptr && props.borderless) {
    EnableBorderlessSnap(window);
  }
#endif

  if (window != nullptr && props.maximized) {
    glfwMaximizeWindow(window);
  }

  glfwMakeContextCurrent(window);
  // glViewport(0, 0, 800, 600);

  m_WindowData.height = props.height;
  m_WindowData.width = props.width;
  m_WindowData.title = props.title;
}

Window::~Window() {
  // s_NumWindows -= 1;
  glfwDestroyWindow(m_Window);

  glfwTerminate();

  // if (s_NumWindows == 0) {
  //   std::cout << "here" << std::endl;

  //   glfwTerminate();
  // } else {
  //   glfwDestroyWindow(m_Window);
  // }
}

GLFWwindow *Window::GetGLFWWindowPointer() { return m_Window; }

void Window::SetWindowEventDispatcher(EventDispatcher *dispatcher) {
  glfwSetWindowUserPointer(m_Window, dispatcher);
}

void Window::Update(bool running) {
  glfwPollEvents();

  // if (show_another_window)
  // {
  //   ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer
  //   to our bool variable (the window will have a closing button that will
  //   clear the bool when clicked) ImGui::Text("Hello from another window!");
  //   if (ImGui::Button("Close Me"))
  //     show_another_window = false;
  //   ImGui::End();
  // }

  // ImGui::ShowDemoWindow();
  //
  // ImGui::End();
  // ImGui::Render();
  //
  //
  //
  // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  // if (m_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  // {
  //   GLFWwindow* backup_current_context = glfwGetCurrentContext();
  //   ImGui::UpdatePlatformWindows();
  //   ImGui::RenderPlatformWindowsDefault();
  //   glfwMakeContextCurrent(backup_current_context);
  // }

  glfwSwapBuffers(m_Window);
}

void Window::TerminateAllWindows() { glfwTerminate(); }
} // namespace Hamster
