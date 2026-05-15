#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <Core/Components.h>
#include <Core/Scene.h>
#include <Renderer/Texture.h>

class ColliderEditor {
public:
  void Open(Hamster::UUID entityUUID, std::shared_ptr<Hamster::Scene> scene);
  void Close();
  void Render();

  bool IsOpen() const { return m_Open; }

private:
  bool m_Open = false;

  Hamster::UUID m_EntityUUID = Hamster::UUID::GetNil();
  std::shared_ptr<Hamster::Scene> m_Scene;

  // Drag state
  bool m_DraggingBody = false;
  int m_DraggingHandle = -1; // -1 = none, 0-7 = handle index
  glm::vec2 m_DragStart = {0.0f, 0.0f};
};
