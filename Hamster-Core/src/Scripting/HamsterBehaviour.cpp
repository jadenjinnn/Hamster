#include "HamsterPCH.h"

#include "HamsterBehaviour.h"

#include "Core/Application.h"
#include "Utils/AssetManager.h"

#include "Events/SceneEvents.h"

namespace Hamster {
HamsterBehaviour::HamsterBehaviour(UUID entityUUID,
                                   std::shared_ptr<Scene> scene,
                                   Application *app)
    : m_UUID(entityUUID), m_Scene(std::move(scene)), m_App(app) {
  m_Transform = &m_Scene->GetEntityComponent<Transform>(m_UUID);

  if (m_Scene->EntityHasComponent<Rigidbody>(m_UUID)) {
    m_Rigidbody = &m_Scene->GetEntityComponent<Rigidbody>(m_UUID);
  }

  if (m_Scene->EntityHasComponent<Animation>(m_UUID)) {
    m_Animation = &m_Scene->GetEntityComponent<Animation>(m_UUID);
  }

  m_KeyPressedHandle = app->GetEventDispatcher()->Subscribe(
      KeyPressed, FORWARD_CALLBACK_FUNCTION(HamsterBehaviour::OnKeyPressed,
                                            KeyPressedEvent));

  m_KeyReleasedHandle = app->GetEventDispatcher()->Subscribe(
      KeyReleased, FORWARD_CALLBACK_FUNCTION(HamsterBehaviour::OnKeyReleased,
                                             KeyReleasedEvent));

  m_CollisionHandle = app->GetEventDispatcher()->Subscribe(
      Collision,
      FORWARD_CALLBACK_FUNCTION(HamsterBehaviour::OnCollision, CollisionEvent));
}

HamsterBehaviour::~HamsterBehaviour() {
  if (auto *dispatcher = m_App->GetEventDispatcher().get()) {
    dispatcher->Unsubscribe(KeyPressed, m_KeyPressedHandle);
    dispatcher->Unsubscribe(KeyReleased, m_KeyReleasedHandle);
    dispatcher->Unsubscribe(Collision, m_CollisionHandle);
  }
}

void HamsterBehaviour::OnKeyPressed(KeyPressedEvent &e) {
  m_KeyPressed = e.GetKeyPressed();
}

void HamsterBehaviour::OnKeyReleased(KeyReleasedEvent &e) {
  std::cout << "Key released" << std::endl;

  m_KeyReleased = e.GetKeyReleased();
}

void HamsterBehaviour::Log(LogType type, std::string message) {
  m_Scene->GetClientLogger()->Log(type, message);
}

// void HamsterBehaviour::CrossScriptExecute(std::string &uuid,
//                                           const char *funcName) {
//   auto &behaviour = m_Scene->GetEntityComponent<Behaviour>(UUID(uuid));
//
//   for (auto &obj : behaviour.pyObjects) {
//     if (pybind11::hasattr(obj, funcName)) {
//       obj.attr(funcName)();
//       break;
//     }
//   }
// }

glm::vec2 HamsterBehaviour::GetVelocity() const {
  if (m_Rigidbody) {
    return m_Rigidbody->cachedVelocity;
  }
  return {0.0f, 0.0f};
}

void HamsterBehaviour::SetVelocity(float vx, float vy) {
  if (m_Rigidbody) {
    m_Rigidbody->pendingVelocity = glm::vec2(vx, vy);
    m_Rigidbody->hasPendingVelocity = true;
  }
}

void HamsterBehaviour::SetPosition(float x, float y) {
  // (x, y) is a top-left position, matching transform.position. Box2D bodies
  // are centre-anchored, so convert and queue for the physics step (a Dynamic
  // body's transform is otherwise owned by Box2D and overwritten each frame).
  // No Rigidbody -> write the transform directly.
  if (m_Rigidbody && m_Transform) {
    m_Rigidbody->pendingPosition = glm::vec2(
        x + m_Transform->size.x * 0.5f, y + m_Transform->size.y * 0.5f);
    m_Rigidbody->hasPendingPosition = true;
  } else if (m_Transform) {
    m_Transform->position.x = x;
    m_Transform->position.y = y;
  }
}

void HamsterBehaviour::ApplyForce(float fx, float fy) {
  if (m_Rigidbody) {
    m_Rigidbody->pendingForce += glm::vec2(fx, fy);
  }
}

void HamsterBehaviour::ApplyImpulse(float ix, float iy) {
  if (m_Rigidbody) {
    m_Rigidbody->pendingImpulse += glm::vec2(ix, iy);
  }
}

UUID HamsterBehaviour::CreateEntityRuntime(const std::string &name, const Transform &transform) {
  return m_Scene->CreateEntityRuntime(name, transform);
}

void HamsterBehaviour::DestroyEntityRuntime(UUID uuid) {
  m_Scene->QueueDestroyEntity(uuid);
}

void HamsterBehaviour::Animate(const std::string &name) {
  if (!m_Animation) {
    throw pybind11::value_error("Entity has no Animation component");
  }

  Animate(name, m_Animation->loop);
}

void HamsterBehaviour::Animate(const std::string &name, bool loop) {
  if (!m_Animation) {
    throw pybind11::value_error("Entity has no Animation component");
  }

  auto it = m_Animation->animations.find(name);
  if (it == m_Animation->animations.end()) {
    throw pybind11::value_error("Animation '" + name + "' not found on this entity");
  }

  m_Animation->currentAnimation = name;
  m_Animation->currentTime = 0.0f;
  m_Animation->playing = true;
  m_Animation->runtimeLoop = loop;
}

void HamsterBehaviour::StopAnimation() {
  if (m_Animation) {
    m_Animation->playing = false;
  }
}

bool HamsterBehaviour::IsAnimating() const {
  if (m_Animation) {
    return m_Animation->playing;
  }
  return false;
}

void HamsterBehaviour::AddCollisionEntity(const std::string &uuid) {
  m_CollisionEntities.insert(uuid);

  m_Colliding = true;
}

void HamsterBehaviour::EmptyCollisionEntity() {
  m_CollisionEntities.clear();
  m_Colliding = false;
}

void HamsterBehaviour::OnCollision(CollisionEvent &e) {
  if (e.GetUUIDA().GetUUID() == m_UUID.GetUUID()) {
    AddCollisionEntity(e.GetUUIDB().GetUUIDString());
  } else if (e.GetUUIDB().GetUUID() == m_UUID.GetUUID()) {
    AddCollisionEntity(e.GetUUIDA().GetUUIDString());
  }
}

UUID HamsterBehaviour::GetParent() const {
  return m_Scene->GetParent(m_UUID);
}

std::vector<UUID> HamsterBehaviour::GetChildren() const {
  return m_Scene->GetChildren(m_UUID);
}

void HamsterBehaviour::SetParent(UUID newParent) {
  if (!m_Scene->SetParent(m_UUID, newParent)) {
    throw std::invalid_argument(
        "set_parent failed: cycle, self-parent, or unknown UUID");
  }
}

} // namespace Hamster
