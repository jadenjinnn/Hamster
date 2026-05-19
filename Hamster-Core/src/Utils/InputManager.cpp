//
// Created by jaden on 02/08/24.
//

#include "HamsterPCH.h"

#include "Core/Base.h"
#include "Events/Event.h"
#include "Events/InputEvents.h"
#include "InputManager.h"

namespace Hamster {
InputManager::InputManager(GLFWwindow *window) : m_Window(window) {
  m_EventDispatcher =
      static_cast<EventDispatcher *>(glfwGetWindowUserPointer(window));

  AttachCallbacks(window);
}

void InputManager::AttachCallbacks(GLFWwindow *window) {
  glfwSetMouseButtonCallback(window, [](GLFWwindow *windowGLFW, int button,
                                         int action, int mods) {
    auto *dispatcher =
        static_cast<EventDispatcher *>(glfwGetWindowUserPointer(windowGLFW));
    if (!dispatcher) return;

    double xpos, ypos;
    glfwGetCursorPos(windowGLFW, &xpos, &ypos);

    if (action == GLFW_PRESS) {
      MouseButtonClickedEvent e(static_cast<MouseButtons>(button), xpos, ypos);
      dispatcher->Post<MouseButtonClickedEvent>(e);
    }
  });

  glfwSetKeyCallback(window, [](GLFWwindow *windowGLFW, int key, int scancode,
                                 int action, int mods) {
    auto *dispatcher =
        static_cast<EventDispatcher *>(glfwGetWindowUserPointer(windowGLFW));
    if (!dispatcher) return;

    if (action == GLFW_PRESS) {
      KeyPressedEvent e(static_cast<KeyCodes>(key));
      dispatcher->Post<KeyPressedEvent>(e);
    }

    if (action == GLFW_RELEASE) {
      KeyReleasedEvent e(static_cast<KeyCodes>(key));
      dispatcher->Post<KeyReleasedEvent>(e);
    }
  });
}
} // namespace Hamster
