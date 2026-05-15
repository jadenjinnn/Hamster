#pragma once

#include <GLFW/glfw3.h>

#include "Events/Event.h"

namespace Hamster {
struct WindowData {
  float height, width;
  std::string title;
};

struct WindowProps {
  float height, width;
  std::string title;
  bool borderless;
  bool maximized;

  WindowProps(float height = 500, float width = 500,
              std::string title = "Hamster", bool borderless = false,
              bool maximized = false)
      : height(height), width(width), title(title), borderless(borderless),
        maximized(maximized) {};
};

class Window {
public:
  Window(const WindowProps &props);

  ~Window();

  GLFWwindow *GetGLFWWindowPointer();

  void SetWindowEventDispatcher(EventDispatcher *dispatcher);

  void Update(bool running);

  static void TerminateAllWindows();

private:
  GLFWwindow *m_Window;
  WindowData m_WindowData;
  bool show_another_window = true;
};
} // namespace Hamster
