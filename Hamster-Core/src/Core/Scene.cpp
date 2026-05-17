//
// Created by Jaden on 28/08/2024.
//

#include "HamsterPCH.h"

#include "Scene.h"

#include <algorithm>
#include <box2d/box2d.h>
#include <pybind11/pybind11.h>

#include "Application.h"
#include "Project.h"
#include "Renderer/Renderer.h"
#include "SceneSerialiser.h"
#include "Utils/AssetManager.h"

#include "Scripting/HamsterBehaviour.h"
#include "Scripting/Scripting.h"

namespace {
constexpr float PIXELS_PER_METER = 50.0f;
constexpr int BOX2D_SUB_STEPS = 4;
}

namespace Hamster {
Scene::Scene(EventDispatcher *dispatcher, Application *app)
    : m_Dispatcher(dispatcher), m_App(app) {
  m_ClientLogger = std::make_shared<Logger>(100);

  m_Dispatcher->Subscribe(
      SceneCreated,
      FORWARD_CALLBACK_FUNCTION(Scene::OnSceneCreated, SceneCreatedEvent));

  std::string sceneName = m_Name;
  std::replace(sceneName.begin(), sceneName.end(), ' ', '_');

  m_Path = std::filesystem::path("Scenes") /
           std::filesystem::path(sceneName + "_" + m_UUID.GetUUIDString() +
                                 ".scene");

  m_RenderGroup = m_Registry.group<Sprite, Transform>();
}

UUID Scene::CreateEntity() {
  auto entity = m_Registry.create();

  UUID uuid;

  std::cout << "Created " << uuid.GetUUID() << std::endl;

  m_Registry.emplace<ID>(entity, uuid);
  m_Entities[uuid] = entity;

  AddEntityComponent<Transform>(uuid);
  AddEntityComponent<Name>(uuid);

  // New entities start at the top level. Append to root sibling vector.
  auto &topLevel = m_ChildrenIndex[UUID::GetNil()];
  Hierarchy h;
  h.parent = UUID::GetNil();
  h.siblingIndex = static_cast<uint32_t>(topLevel.size());
  AddEntityComponent<Hierarchy>(uuid, h);
  topLevel.push_back(uuid);

  return uuid;
}

void Scene::CreateEntityWithUUID(UUID uuid) {
  auto entity = m_Registry.create();

  m_Registry.emplace<ID>(entity, uuid);
  m_Entities[uuid] = entity;

  // Default-add Hierarchy at top level. Serialiser may override
  // siblingIndex/parent later when restoring a Hierarchy_ID record.
  auto &topLevel = m_ChildrenIndex[UUID::GetNil()];
  Hierarchy h;
  h.parent = UUID::GetNil();
  h.siblingIndex = static_cast<uint32_t>(topLevel.size());
  AddEntityComponent<Hierarchy>(uuid, h);
  topLevel.push_back(uuid);
}

void Scene::DestroyEntity(UUID entityUUID) {
  if (m_Entities.find(entityUUID) == m_Entities.end()) return;

  // Snapshot descendants in post-order (leaves first) so we never iterate
  // m_ChildrenIndex while mutating it.
  std::vector<UUID> destroyList;
  std::vector<UUID> stack{entityUUID};
  while (!stack.empty()) {
    UUID cur = stack.back();
    stack.pop_back();
    destroyList.push_back(cur);
    auto it = m_ChildrenIndex.find(cur);
    if (it != m_ChildrenIndex.end()) {
      for (UUID c : it->second) stack.push_back(c);
    }
  }
  // destroyList currently has root-first order; reverse for leaves-first.
  std::reverse(destroyList.begin(), destroyList.end());

  // Detach the top-level destroy target from its parent's children list.
  if (EntityHasComponent<Hierarchy>(entityUUID)) {
    UUID p = GetEntityComponent<Hierarchy>(entityUUID).parent;
    auto &siblings = m_ChildrenIndex[p];
    siblings.erase(std::remove(siblings.begin(), siblings.end(), entityUUID),
                   siblings.end());
    for (uint32_t i = 0; i < siblings.size(); i++) {
      GetEntityComponent<Hierarchy>(siblings[i]).siblingIndex = i;
    }
  }

  for (UUID u : destroyList) {
    auto it = m_Entities.find(u);
    if (it == m_Entities.end()) continue;
    m_Registry.destroy(it->second);
    m_Entities.erase(it);
    m_ChildrenIndex.erase(u);
  }
}

bool Scene::SetParent(UUID child, UUID newParent) {
  if (m_Entities.find(child) == m_Entities.end()) return false;
  if (child == newParent) return false;
  if (!UUID::IsNil(newParent) && m_Entities.find(newParent) == m_Entities.end())
    return false;

  // Cycle check: walk newParent's ancestors; if `child` appears, refuse.
  UUID walker = newParent;
  while (!UUID::IsNil(walker)) {
    if (walker == child) return false;
    if (!EntityHasComponent<Hierarchy>(walker)) break;
    walker = GetEntityComponent<Hierarchy>(walker).parent;
  }

  auto &childH = GetEntityComponent<Hierarchy>(child);
  UUID oldParent = childH.parent;
  if (oldParent == newParent) return true; // no-op

  // Remove from old parent's children list.
  auto &oldSiblings = m_ChildrenIndex[oldParent];
  oldSiblings.erase(std::remove(oldSiblings.begin(), oldSiblings.end(), child),
                    oldSiblings.end());
  for (uint32_t i = 0; i < oldSiblings.size(); i++) {
    GetEntityComponent<Hierarchy>(oldSiblings[i]).siblingIndex = i;
  }

  // Append under new parent.
  auto &newSiblings = m_ChildrenIndex[newParent];
  childH.parent = newParent;
  childH.siblingIndex = static_cast<uint32_t>(newSiblings.size());
  newSiblings.push_back(child);
  return true;
}

UUID Scene::GetParent(UUID uuid) {
  if (m_Entities.find(uuid) == m_Entities.end()) return UUID::GetNil();
  if (!EntityHasComponent<Hierarchy>(uuid)) return UUID::GetNil();
  return GetEntityComponent<Hierarchy>(uuid).parent;
}

const std::vector<UUID> &Scene::GetChildren(UUID parent) {
  return m_ChildrenIndex[parent];
}

void Scene::RebuildHierarchyIndex() {
  m_ChildrenIndex.clear();

  // Bucket entities under their parent UUID, recording each child's
  // serialised siblingIndex alongside it for the per-bucket sort.
  std::unordered_map<UUID, std::vector<std::pair<uint32_t, UUID>>> tmp;
  auto view = m_Registry.view<Hierarchy, ID>();
  view.each([&](auto &h, auto &id) {
    tmp[h.parent].emplace_back(h.siblingIndex, id.uuid);
  });

  for (auto &[parent, kids] : tmp) {
    std::sort(kids.begin(), kids.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    auto &out = m_ChildrenIndex[parent];
    out.reserve(kids.size());
    for (uint32_t i = 0; i < kids.size(); i++) {
      out.push_back(kids[i].second);
      // Renumber to contiguous 0..n-1 in case the saved indices had gaps.
      GetEntityComponent<Hierarchy>(kids[i].second).siblingIndex = i;
    }
  }
}

void Scene::ReorderSibling(UUID uuid, uint32_t newIndex) {
  if (m_Entities.find(uuid) == m_Entities.end()) return;
  if (!EntityHasComponent<Hierarchy>(uuid)) return;
  UUID parent = GetEntityComponent<Hierarchy>(uuid).parent;
  auto &siblings = m_ChildrenIndex[parent];
  auto it = std::find(siblings.begin(), siblings.end(), uuid);
  if (it == siblings.end()) return;
  siblings.erase(it);
  if (newIndex > siblings.size()) newIndex = static_cast<uint32_t>(siblings.size());
  siblings.insert(siblings.begin() + newIndex, uuid);
  for (uint32_t i = 0; i < siblings.size(); i++) {
    GetEntityComponent<Hierarchy>(siblings[i]).siblingIndex = i;
  }
}

UUID Scene::CreateEntityRuntime(const std::string &name, const Transform &transform) {
  if (m_IsSimulationPaused) {
    m_ClientLogger->Log(Error, "create_entity can only be called during simulation");
    return UUID::GetNil();
  }

  UUID uuid = CreateEntity();
  GetEntityComponent<Name>(uuid).name = name;
  GetEntityComponent<Transform>(uuid) = transform;
  return uuid;
}

void Scene::QueueDestroyEntity(UUID entityUUID) {
  m_DestroyQueue.push_back(entityUUID);
}

void Scene::FlushDestroyQueue() {
  for (auto &uuid : m_DestroyQueue) {
    if (m_Entities.find(uuid) == m_Entities.end())
      continue;

    if (EntityHasComponent<Rigidbody>(uuid)) {
      auto &rb = GetEntityComponent<Rigidbody>(uuid);
      if (b2Body_IsValid(rb.bodyId)) {
        b2DestroyBody(rb.bodyId);
      }
    }

    m_Registry.destroy(m_Entities[uuid]);
    m_Entities.erase(uuid);
  }
  m_DestroyQueue.clear();
}

void Scene::CreatePendingBodies() {
  for (auto &uuid : m_PendingBodies) {
    if (m_Entities.find(uuid) == m_Entities.end())
      continue;
    if (!EntityHasComponent<Rigidbody>(uuid))
      continue;

    auto &rb = GetEntityComponent<Rigidbody>(uuid);
    auto &transform = GetEntityComponent<Transform>(uuid);
    auto &id = GetEntityComponent<ID>(uuid);

    b2BodyDef bodyDef = b2DefaultBodyDef();

    switch (rb.bodyType) {
    case BodyType::Static:
      bodyDef.type = b2_staticBody;
      break;
    case BodyType::Dynamic:
      bodyDef.type = b2_dynamicBody;
      break;
    case BodyType::Kinematic:
      bodyDef.type = b2_kinematicBody;
      break;
    }

    bodyDef.position = {(transform.position.x + transform.size.x * 0.5f) / PIXELS_PER_METER,
                        (transform.position.y + transform.size.y * 0.5f) / PIXELS_PER_METER};
    bodyDef.rotation = b2MakeRot(glm::radians(transform.rotation));
    bodyDef.gravityScale = rb.gravityScale;
    bodyDef.userData = const_cast<void *>(static_cast<const void *>(&id.uuid));

    rb.bodyId = b2CreateBody(m_PhysicsWorld, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = rb.density;
    shapeDef.friction = rb.friction;
    shapeDef.restitution = rb.restitution;
    shapeDef.enableContactEvents = true;

    glm::vec2 colSize = (rb.colliderSize.x > 0.0f && rb.colliderSize.y > 0.0f)
        ? rb.colliderSize : transform.size;

    float halfW = std::max(std::abs(colSize.x) / PIXELS_PER_METER * 0.5f, 0.05f);
    float halfH = std::max(std::abs(colSize.y) / PIXELS_PER_METER * 0.5f, 0.05f);

    b2Vec2 shapeOffset = {rb.colliderOffset.x / PIXELS_PER_METER,
                          rb.colliderOffset.y / PIXELS_PER_METER};

    if (rb.colliderShape == ColliderShape::Circle) {
      b2Circle circle;
      circle.center = shapeOffset;
      circle.radius = std::max(halfW, halfH);
      b2CreateCircleShape(rb.bodyId, &shapeDef, &circle);
    } else {
      b2Polygon box = b2MakeOffsetBox(halfW, halfH, shapeOffset, 0.0f);
      b2CreatePolygonShape(rb.bodyId, &shapeDef, &box);
    }
  }
  m_PendingBodies.clear();
}

// template<typename T>
// T Scene::GetEntityComponent(UUID uuid) {
//     return m_Registry.get<T>(uuid);
// }
//
// template<typename T, typename... Args>
// void Scene::AddEntityComponent(UUID entityUUID, Args &&... args) {
//     m_Registry.emplace<T>(m_Entities[entityUUID],
//     std::forward<Args>(args)...);
// }

void Scene::OnUpdate() {
  if (!m_IsSimulationPaused) {
    auto currentFrame = static_cast<float>(glfwGetTime());
    m_DeltaTime = currentFrame - m_LastFrame;
    m_LastFrame = currentFrame;

    if (b2World_IsValid(m_PhysicsWorld)) {
      CreatePendingBodies();
      ApplyPendingForces();
      StepPhysics();
      ProcessContactEvents();
      SyncPhysicsToTransforms();
      CacheVelocities();
    }

    auto *assetManager = m_App->GetAssetManager();
    auto animView = m_Registry.view<Animation, Sprite, ID>();
    animView.each([this, assetManager](auto &anim, auto &sprite, auto &id) {
      if (!anim.playing || anim.currentAnimation.empty())
        return;

      auto it = anim.animations.find(anim.currentAnimation);
      if (it == anim.animations.end())
        return;

      auto animData = assetManager->GetAnimation(it->second);
      if (!animData || animData->keyframes.empty())
        return;

      anim.currentTime += m_DeltaTime;

      if (anim.currentTime >= animData->duration) {
        if (anim.runtimeLoop) {
          anim.currentTime = std::fmod(anim.currentTime, animData->duration);
        } else {
          anim.currentTime = animData->duration;
          anim.playing = false;
          anim.completedAnimations.push_back(anim.currentAnimation);

          AnimationCompletedEvent e(id.uuid, anim.currentAnimation);
          m_Dispatcher->Post<AnimationCompletedEvent>(e);
        }
      }

      // Find the last keyframe with time <= currentTime
      const AnimationKeyframe *current = &animData->keyframes[0];
      for (auto &kf : animData->keyframes) {
        if (kf.time <= anim.currentTime)
          current = &kf;
        else
          break;
      }

      try {
        sprite.texture = assetManager->GetTexture(current->textureUUID);
      } catch (const std::out_of_range &) {
        // texture not loaded — keep current sprite
      }
    });

    OnScriptUpdate();
    FlushDestroyQueue();
  }
}

void Scene::InitPhysicsWorld() {
  b2WorldDef worldDef = b2DefaultWorldDef();
  worldDef.gravity = {0.0f, 10.0f}; // +Y is down in screen coords

  m_PhysicsWorld = b2CreateWorld(&worldDef);

  auto view = m_Registry.view<Transform, Rigidbody, ID>();

  view.each([this](auto &transform, auto &rb, auto &id) {
    b2BodyDef bodyDef = b2DefaultBodyDef();

    switch (rb.bodyType) {
    case BodyType::Static:
      bodyDef.type = b2_staticBody;
      break;
    case BodyType::Dynamic:
      bodyDef.type = b2_dynamicBody;
      break;
    case BodyType::Kinematic:
      bodyDef.type = b2_kinematicBody;
      break;
    }

    // transform.position is top-left; Box2D wants center
    bodyDef.position = {(transform.position.x + transform.size.x * 0.5f) / PIXELS_PER_METER,
                        (transform.position.y + transform.size.y * 0.5f) / PIXELS_PER_METER};
    bodyDef.rotation =
        b2MakeRot(glm::radians(transform.rotation));
    bodyDef.gravityScale = rb.gravityScale;

    // Store pointer to entity UUID for contact event resolution
    bodyDef.userData = const_cast<void *>(
        static_cast<const void *>(&id.uuid));

    rb.bodyId = b2CreateBody(m_PhysicsWorld, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = rb.density;
    shapeDef.friction = rb.friction;
    shapeDef.restitution = rb.restitution;
    shapeDef.enableContactEvents = true;

    glm::vec2 colSize = (rb.colliderSize.x > 0.0f && rb.colliderSize.y > 0.0f)
        ? rb.colliderSize : transform.size;

    float halfW = std::max(std::abs(colSize.x) / PIXELS_PER_METER * 0.5f, 0.05f);
    float halfH = std::max(std::abs(colSize.y) / PIXELS_PER_METER * 0.5f, 0.05f);

    b2Vec2 shapeOffset = {rb.colliderOffset.x / PIXELS_PER_METER,
                          rb.colliderOffset.y / PIXELS_PER_METER};

    if (rb.colliderShape == ColliderShape::Circle) {
      b2Circle circle;
      circle.center = shapeOffset;
      circle.radius = std::max(halfW, halfH);
      b2CreateCircleShape(rb.bodyId, &shapeDef, &circle);
    } else {
      b2Polygon box = b2MakeOffsetBox(halfW, halfH, shapeOffset, 0.0f);
      b2CreatePolygonShape(rb.bodyId, &shapeDef, &box);
    }
  });
}

void Scene::DestroyPhysicsWorld() {
  if (b2World_IsValid(m_PhysicsWorld)) {
    // Reset all bodyIds before destroying the world
    auto view = m_Registry.view<Rigidbody>();
    view.each([](auto &rb) { rb.bodyId = b2_nullBodyId; });

    b2DestroyWorld(m_PhysicsWorld);
    m_PhysicsWorld = b2_nullWorldId;
  }
}

void Scene::StepPhysics() {
  b2World_Step(m_PhysicsWorld, m_DeltaTime, BOX2D_SUB_STEPS);
}

void Scene::SyncPhysicsToTransforms() {
  auto view = m_Registry.view<Transform, Rigidbody>();

  view.each([](auto &transform, auto &rb) {
    if (!b2Body_IsValid(rb.bodyId))
      return;

    // Box2D returns center; transform.position is top-left
    b2Vec2 pos = b2Body_GetPosition(rb.bodyId);
    transform.position.x = pos.x * PIXELS_PER_METER - transform.size.x * 0.5f;
    transform.position.y = pos.y * PIXELS_PER_METER - transform.size.y * 0.5f;

    b2Rot rot = b2Body_GetRotation(rb.bodyId);
    transform.rotation = glm::degrees(b2Rot_GetAngle(rot));
  });
}

void Scene::ProcessContactEvents() {
  b2ContactEvents events = b2World_GetContactEvents(m_PhysicsWorld);

  for (int i = 0; i < events.beginCount; i++) {
    b2ContactBeginTouchEvent &evt = events.beginEvents[i];

    b2BodyId bodyA = b2Shape_GetBody(evt.shapeIdA);
    b2BodyId bodyB = b2Shape_GetBody(evt.shapeIdB);

    auto *uuidA = static_cast<UUID *>(b2Body_GetUserData(bodyA));
    auto *uuidB = static_cast<UUID *>(b2Body_GetUserData(bodyB));

    if (uuidA && uuidB) {
      CollisionEvent e(*uuidA, *uuidB);
      m_Dispatcher->Post<CollisionEvent>(e);
    }
  }
}

void Scene::ApplyPendingForces() {
  auto view = m_Registry.view<Rigidbody>();

  view.each([](auto &rb) {
    if (!b2Body_IsValid(rb.bodyId))
      return;

    if (rb.hasPendingVelocity) {
      b2Vec2 vel = {rb.pendingVelocity.x / PIXELS_PER_METER,
                    rb.pendingVelocity.y / PIXELS_PER_METER};
      b2Body_SetLinearVelocity(rb.bodyId, vel);
      rb.hasPendingVelocity = false;
      rb.pendingVelocity = {0.0f, 0.0f};
    }

    if (rb.pendingForce.x != 0.0f || rb.pendingForce.y != 0.0f) {
      b2Vec2 force = {rb.pendingForce.x, rb.pendingForce.y};
      b2Body_ApplyForceToCenter(rb.bodyId, force, true);
      rb.pendingForce = {0.0f, 0.0f};
    }

    if (rb.pendingImpulse.x != 0.0f || rb.pendingImpulse.y != 0.0f) {
      b2Vec2 impulse = {rb.pendingImpulse.x, rb.pendingImpulse.y};
      b2Body_ApplyLinearImpulseToCenter(rb.bodyId, impulse, true);
      rb.pendingImpulse = {0.0f, 0.0f};
    }
  });
}

void Scene::CacheVelocities() {
  auto view = m_Registry.view<Rigidbody>();

  view.each([](auto &rb) {
    if (!b2Body_IsValid(rb.bodyId))
      return;

    b2Vec2 vel = b2Body_GetLinearVelocity(rb.bodyId);
    rb.cachedVelocity = {vel.x * PIXELS_PER_METER, vel.y * PIXELS_PER_METER};
  });
}

void Scene::OnScriptUpdate() {
  auto view = m_Registry.view<Behaviour, ID>();

  bool pythonError = false;

  view.each([this, &pythonError](auto &behaviour, auto &id) mutable {
    for (auto &obj : behaviour.pyObjects) {
      try {
        obj.attr("on_update")(m_DeltaTime);

        obj.attr("reset_input")();

        // Dispatch animation completion callbacks
        if (EntityHasComponent<Animation>(id.uuid)) {
          auto &anim = GetEntityComponent<Animation>(id.uuid);
          for (auto &animName : anim.completedAnimations) {
            if (pybind11::hasattr(obj, "on_animation_complete")) {
              obj.attr("on_animation_complete")(animName);
            }
          }
        }
      } catch (pybind11::error_already_set &e) {
        pythonError = true;

        m_ClientLogger->Log(Error, e.what());

        break;
      }
    }
  });

  // Clear completed animations after dispatching
  auto animClearView = m_Registry.view<Animation>();
  animClearView.each([](auto &anim) {
    anim.completedAnimations.clear();
  });

  if (pythonError) {
    PauseSceneSimulation();
  }
}

void Scene::OnRender(bool renderFlat) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_RenderGroup.sort<Transform>(
      [](const Transform &lhs, const Transform &rhs) {
        return lhs.position.z < rhs.position.z;
      });

  auto *renderer = m_App->GetRenderer();

  if (!renderFlat) {
    m_RenderGroup.each([renderer](auto &sprite, auto &transform) {
      if (sprite.texture != nullptr) {
        renderer->DrawSprite(*sprite.texture, transform.position,
                             transform.size, transform.rotation,
                             sprite.colour);
      }
    });
  } else {
    m_RenderGroup.each([renderer](auto entity, auto &sprite, auto &transform) {
      if (sprite.texture != nullptr) {
        renderer->DrawFlat(
            transform.position, transform.size, transform.rotation,
            Application::IdToColour(entt::to_integral(entity)));
      }
    });
  }
}

void Scene::SaveScene(std::shared_ptr<Scene> scene) {
  std::cout << "Saving scene" << std::endl;

  SceneSerialiser serialiser(scene);

  std::cout << scene->GetPath() << std::endl;

  std::ofstream out(scene->GetPath(), std::ios::binary);

  serialiser.Serialise(out);

  out.close();
}

void Scene::OnSceneCreated(SceneCreatedEvent &e) { SaveScene(e.GetScene()); }

void Scene::SetUUID(const UUID &uuid) {
  m_UUID = uuid;

  std::cout << m_UUID.GetUUID() << std::endl;

  std::string sceneName = m_Name;

  std::replace(sceneName.begin(), sceneName.end(), ' ', '_');

  m_Path = std::filesystem::path("Scenes") /
           std::filesystem::path(sceneName + "_" + m_UUID.GetUUIDString() +
                                 ".scene");
}

void Scene::RunSceneSimulation() {
  if (m_IsSimulationPaused) {
    Project::SaveCurrentProject(m_App->GetAssetManager());

    SaveScene(m_App->GetActiveScene());

    auto scriptReloadView = m_Registry.view<Behaviour>();

    scriptReloadView.each([](auto &behaviour) {
      for (auto const &[uuid, script] : behaviour.scripts) {
        script->ReloadScript();
      }
    });

    m_IsSimulationPaused = false;
    m_LastFrame = static_cast<float>(glfwGetTime());

    // Snapshot original textures for animated entities
    auto animView = m_Registry.view<Animation, Sprite>();
    animView.each([](auto &anim, auto &sprite) {
      anim.originalTexture = sprite.texture;
      anim.currentTime = 0.0f;
      anim.playing = false;
      anim.currentAnimation.clear();

      if (!anim.defaultAnimation.empty()) {
        anim.currentAnimation = anim.defaultAnimation;
        anim.playing = true;
        anim.runtimeLoop = anim.loop;
      }
    });

    InitPhysicsWorld();

    auto view = m_Registry.view<ID, Behaviour>();

    view.each([this](auto &ID, auto &behaviour) {
      behaviour.pyObjects.clear();

      for (auto const &[uuid, script] : behaviour.scripts) {
        for (auto &obj : script->GetPyObjects()) {
          pybind11::object pyObject = obj(
              ID.uuid, m_App->GetActiveScene(),
              m_App);

          behaviour.pyObjects.push_back(pyObject);

          try {
            pyObject.attr("on_create")();
          } catch (pybind11::error_already_set &e) {
            m_ClientLogger->Log(Error, e.what());

            PauseSceneSimulation();
          }
        }
      }
    });

    std::cout << "Simulation unpaused" << std::endl;
  }
}

void Scene::PauseSceneSimulation() {
  if (!m_IsSimulationPaused) {
    DestroyPhysicsWorld();

    // Restore original textures for animated entities
    auto animView = m_Registry.view<Animation, Sprite>();
    animView.each([](auto &anim, auto &sprite) {
      if (anim.originalTexture) {
        sprite.texture = anim.originalTexture;
        anim.originalTexture = nullptr;
      }
      anim.playing = false;
      anim.currentTime = 0.0f;
      anim.currentAnimation.clear();
    });

    m_PendingBodies.clear();
    m_DestroyQueue.clear();

    m_IsSimulationPaused = true;

    std::cout << "Scene paused" << std::endl;
  }
}
} // namespace Hamster
