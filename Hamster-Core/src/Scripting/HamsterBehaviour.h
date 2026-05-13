#pragma once

#include "Core/Components.h"
#include "Core/Scene.h"
#include "Events/Event.h"
#include "Events/InputEvents.h"
#include "Events/SceneEvents.h"
#include "Utils/InputManager.h"

namespace Hamster {
class Scene;
struct Transform;
// struct ID;
struct Behaviour;
class Application;

class HamsterBehaviour {
public:
  HamsterBehaviour(UUID entityUUID, std::shared_ptr<Scene> scene,
                   Application *app);

  ~HamsterBehaviour();

  virtual void OnCreate() {}

  virtual void OnUpdate(float deltaTime) {}

  const Transform &GetTransform() { return *m_Transform; }

  void SetTransform(const Transform &transform) {
    m_Transform->position = transform.position;
    m_Transform->rotation = transform.rotation;
    m_Transform->size = transform.size;
  }

  const KeyCodes &GetKeyPressed() { return m_KeyPressed; }
  const KeyCodes &GetKeyReleased() { return m_KeyReleased; }

  void OnKeyPressed(KeyPressedEvent &e);

  void OnKeyReleased(KeyReleasedEvent &e);

  void ResetInput() {
    m_KeyPressed = NOT_PRESSED;
    m_KeyReleased = NOT_PRESSED;
  }

  void OnCollision(CollisionEvent &e);

  void AddCollisionEntity(const std::string &uuid);

  void EmptyCollisionEntity();

  [[nodiscard]] bool IsColliding() const { return m_Colliding; }
  [[nodiscard]] std::set<std::string> GetCollisionEntites() const {
    return m_CollisionEntities;
  }

  void Log(LogType type, std::string message);

  glm::vec2 GetVelocity() const;
  void SetVelocity(float vx, float vy);
  void ApplyForce(float fx, float fy);
  void ApplyImpulse(float ix, float iy);

  void Animate(const std::string &name);
  void Animate(const std::string &name, bool loop);
  void StopAnimation();
  bool IsAnimating() const;

  UUID CreateEntityRuntime(const std::string &name, const Transform &transform);
  void DestroyEntityRuntime(UUID uuid);

  std::shared_ptr<Scene> GetScene() const { return m_Scene; }

private:
  UUID m_UUID;
  std::shared_ptr<Scene> m_Scene;
  Transform t;
  Application *m_App;

  Transform *m_Transform = nullptr;
  Rigidbody *m_Rigidbody = nullptr;
  KeyCodes m_KeyPressed = NOT_PRESSED;
  KeyCodes m_KeyReleased = NOT_PRESSED;

  bool m_Colliding = false;
  std::set<std::string> m_CollisionEntities;

  Animation *m_Animation = nullptr;

  SubscriptionHandle m_KeyPressedHandle = 0;
  SubscriptionHandle m_KeyReleasedHandle = 0;
  SubscriptionHandle m_CollisionHandle = 0;
};
} // namespace Hamster
