//
// Created by Jaden on 28/08/2024.
//

#include "HamsterPCH.h"

#include "Scene.h"

#include <algorithm>
#include <box2d/box2d.h>
#include <pybind11/pybind11.h>
#include <sstream>

#include "Application.h"
#include "Project.h"
#include "Renderer/FontAtlas.h"
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

  m_ButtonClickedHandle = m_Dispatcher->Subscribe(
      ButtonClicked,
      FORWARD_CALLBACK_FUNCTION(Scene::OnButtonClicked, ButtonClickedEvent));

  std::string sceneName = m_Name;
  std::replace(sceneName.begin(), sceneName.end(), ' ', '_');

  m_Path = std::filesystem::path("Scenes") /
           std::filesystem::path(sceneName + "_" + m_UUID.GetUUIDString() +
                                 ".scene");

  m_RenderGroup = m_Registry.group<Sprite, Transform>();
}

Scene::~Scene() {
  if (m_Dispatcher && m_ButtonClickedHandle != 0) {
    m_Dispatcher->Unsubscribe(ButtonClicked, m_ButtonClickedHandle);
  }
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

void Scene::OnButtonClicked(ButtonClickedEvent &e) {
  m_ClickedButtonsThisFrame.push_back(e.GetEntityId());
}

UUID Scene::FindEntityByName(const std::string &name) {
  auto view = m_Registry.view<Name, ID>();
  for (auto entity : view) {
    auto &n = view.get<Name>(entity);
    if (n.name == name) {
      return view.get<ID>(entity).uuid;
    }
  }
  return UUID::GetNil();
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

    if (rb.hasPendingPosition) {
      b2Body_SetTransform(rb.bodyId,
                          {rb.pendingPosition.x / PIXELS_PER_METER,
                           rb.pendingPosition.y / PIXELS_PER_METER},
                          b2Body_GetRotation(rb.bodyId));
      rb.hasPendingPosition = false;
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

        // Dispatch UI button clicks. Every behaviour with on_button_clicked
        // sees every click this frame; the script filters by uuid.
        if (pybind11::hasattr(obj, "on_button_clicked")) {
          for (auto &btnUUID : m_ClickedButtonsThisFrame) {
            obj.attr("on_button_clicked")(btnUUID);
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

  // Drain the per-frame click queue regardless of script errors so a
  // stale click doesn't carry into the next simulation tick.
  m_ClickedButtonsThisFrame.clear();

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

  // Rebuild the spatial index every frame from the just-sorted view.
  // Consumers: renderer (viewport cull below) + EditorLayer (picking).
  // Cheap rebuild keeps the diff small; switch to incremental if profiling
  // demands it (see spec future-work).
  RebuildSpatialIndex();

  auto *assetManager = m_App->GetAssetManager();

  // Resolve a sprite to (texture, uvRect) via AssetManager when possible —
  // sub-sprite UUID → parent texture + UV region; texture UUID → whole
  // texture. Pre-spritesheet scenes whose Sprite.assetUUID is nil fall
  // back to the cached `sprite.texture` pointer with full UV (the scene
  // deserialiser sets assetUUID from texture on load to keep this branch
  // rare).
  auto resolve = [&](const Sprite &sprite)
      -> std::pair<Texture *, glm::vec4> {
    if (!UUID::IsNil(sprite.assetUUID) && assetManager) {
      SpriteSource src = assetManager->ResolveSpriteSource(sprite.assetUUID);
      return {src.texture, src.uvRect};
    }
    return {sprite.texture.get(), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
  };

  if (!renderFlat) {
    // Viewport-rect cull: submit only sprites whose AABB intersects the
    // camera rect. At zoom=1 with the camera covering N sprites of 5000,
    // SubmitSprite runs ~N times instead of 5000.
    AABB viewport = renderer->GetViewportWorldAABB();
    std::vector<UUID> visible = m_SpatialIndex.QueryRect(viewport);

    // QueryRect returns UUIDs in quadtree order, and the batch draws in
    // submission order with no depth test — so sort by sprite z here to get
    // painter's-algorithm layering (lower z behind, higher z in front).
    // Without this, layering follows quadtree order and the z value is
    // effectively ignored (bug 0015).
    std::sort(visible.begin(), visible.end(),
              [&](const UUID &a, const UUID &b) {
                auto ia = m_Entities.find(a), ib = m_Entities.find(b);
                float za = (ia != m_Entities.end() &&
                            m_Registry.all_of<Transform>(ia->second))
                               ? m_Registry.get<Transform>(ia->second).position.z
                               : 0.0f;
                float zb = (ib != m_Entities.end() &&
                            m_Registry.all_of<Transform>(ib->second))
                               ? m_Registry.get<Transform>(ib->second).position.z
                               : 0.0f;
                return za < zb;
              });

    renderer->BeginSpriteBatch();
    for (const UUID &uuid : visible) {
      auto it = m_Entities.find(uuid);
      if (it == m_Entities.end()) continue;
      entt::entity e = it->second;
      if (!m_Registry.all_of<Sprite, Transform>(e)) continue;
      const auto &sprite = m_Registry.get<Sprite>(e);
      const auto &transform = m_Registry.get<Transform>(e);
      auto [tex, uvRect] = resolve(sprite);
      if (tex != nullptr) {
        renderer->SubmitSprite(*tex, transform.position, transform.size,
                               transform.rotation, sprite.colour,
                               transform.position.z, uvRect);
      }
    }
    renderer->EndSpriteBatch();
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

void Scene::OnRenderUI(float panelW, float panelH) {
  if (panelW <= 0.0f || panelH <= 0.0f) return;

  auto *renderer = m_App->GetRenderer();
  const FontAtlas *atlas = renderer->GetFontAtlas();
  const FontAtlas *boldAtlas = renderer->GetFontAtlasBold();
  auto pickAtlas = [&](bool bold) {
    return (bold && boldAtlas && boldAtlas->IsValid()) ? boldAtlas : atlas;
  };

  renderer->BeginUIPass(panelW, panelH);

  // Buttons. Backgrounds first, then flush rects, THEN labels — bold text
  // forces a mid-pass text flush, so rects must already be drawn or the label
  // (flushed early) gets painted over by the opaque rect.
  auto buttonView = m_Registry.view<UIButton>();
  buttonView.each([renderer, panelW, panelH](auto &btn) {
    UIRect r = renderer->ResolveUIButton(btn, panelW, panelH);
    renderer->SubmitUIRect(r, btn.bgColour);
  });
  renderer->FlushUIRect();

  buttonView.each([renderer, &pickAtlas, panelW, panelH](auto &btn) {
    const FontAtlas *la = pickAtlas(btn.bold);
    if (btn.label.empty() || !la || !la->IsValid()) return;
    UIRect r = renderer->ResolveUIButton(btn, panelW, panelH);
    float labelW = la->MeasureWidth(btn.label, btn.fontSize);
    float labelX = 0.0f;
    switch (btn.textAlign) {
      case UITextAlign::Left:
        labelX = r.x + btn.padding;
        break;
      case UITextAlign::Centre:
        labelX = r.x + (r.w - labelW) * 0.5f;
        break;
      case UITextAlign::Right:
        labelX = r.x + r.w - labelW - btn.padding;
        break;
    }
    float labelY = r.y + (r.h - btn.fontSize) * 0.5f;
    renderer->SubmitUIText(btn.label, {labelX, labelY},
                           btn.fontSize, btn.textColour, 0.0f, btn.bold);
  });

  // UIText — anchored, optional wrap.
  auto textView = m_Registry.view<UIText>();
  textView.each([renderer, &pickAtlas, panelW, panelH](auto &txt) {
    const FontAtlas *ta = pickAtlas(txt.bold);
    if (txt.text.empty() || !ta || !ta->IsValid()) return;
    // Pivot the bounding box by the text's measured width × fontSize so the
    // chosen anchor lines up with the matching corner of the rendered text.
    glm::vec2 size = {ta->MeasureWidth(txt.text, txt.fontSize),
                      txt.fontSize};
    glm::vec2 tl = ResolveAnchoredTopLeft(txt.anchor, txt.offset, size,
                                          panelW, panelH);
    renderer->SubmitUIText(txt.text, tl, txt.fontSize, txt.textColour,
                           txt.wrapWidth, txt.bold);
  });

  renderer->EndUIPass();
}

void Scene::RebuildSpatialIndex() {
  // Build (UUID, tight-AABB) pairs over every entity with Sprite+Transform.
  // Rotated sprites: AABB encloses all 4 rotated corners. Cost: 4 sin/cos
  // per rotated sprite per frame — dwarfed by the vertex generation in
  // SubmitSprite, which runs only for the visible subset after this.
  std::vector<std::pair<UUID, AABB>> entries;
  entries.reserve(m_Entities.size());

  auto view = m_Registry.view<ID, Sprite, Transform>();
  view.each([&entries](auto &id, auto &sprite, auto &transform) {
    (void)sprite; // sprite component existence gates index membership
    const float hw = transform.size.x * 0.5f;
    const float hh = transform.size.y * 0.5f;
    const float cx = transform.position.x + hw;
    const float cy = transform.position.y + hh;
    AABB box;
    if (transform.rotation == 0.0f) {
      box.min = {transform.position.x, transform.position.y};
      box.max = {transform.position.x + transform.size.x,
                 transform.position.y + transform.size.y};
    } else {
      const float rad = glm::radians(transform.rotation);
      const float c = std::cos(rad);
      const float s = std::sin(rad);
      glm::vec2 corners[4] = {
          {-hw * c - -hh * s + cx, -hw * s + -hh * c + cy},
          { hw * c - -hh * s + cx,  hw * s + -hh * c + cy},
          {-hw * c -  hh * s + cx, -hw * s +  hh * c + cy},
          { hw * c -  hh * s + cx,  hw * s +  hh * c + cy},
      };
      box.min = corners[0];
      box.max = corners[0];
      for (int i = 1; i < 4; ++i) {
        box.min.x = std::min(box.min.x, corners[i].x);
        box.min.y = std::min(box.min.y, corners[i].y);
        box.max.x = std::max(box.max.x, corners[i].x);
        box.max.y = std::max(box.max.y, corners[i].y);
      }
    }
    entries.emplace_back(id.uuid, box);
  });

  m_SpatialIndex.Rebuild(entries);
}

void Scene::SaveScene(std::shared_ptr<Scene> scene) {
  // Pass the AssetManager so sprite asset references serialise correctly —
  // this matches the play-snapshot serialiser. The AssetManager-less ctor was
  // the odd one out and could drop sprite asset data (bug 0016 hardening).
  AssetManager *am =
      scene->GetApp() ? scene->GetApp()->GetAssetManager() : nullptr;
  SceneSerialiser serialiser(scene, am);
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
    // Pre-flight: refuse to start if any Behaviour points at a missing
    // script. Each broken reference is logged with its entity name so the
    // user can find and fix it (re-attach or remove).
    bool anyMissing = false;
    auto check = m_Registry.view<Behaviour, Name>();
    check.each([this, &anyMissing](auto &beh, auto &name) {
      for (auto const &[uuid, script] : beh.scripts) {
        if (script) continue;
        anyMissing = true;
        std::string cached = "(unknown)";
        auto it = beh.cachedNames.find(uuid);
        if (it != beh.cachedNames.end() && !it->second.empty()) {
          cached = it->second;
        }
        std::stringstream ss;
        ss << "Entity '" << name.name << "' references missing script '"
           << cached << "' — did you rename or delete it?";
        m_ClientLogger->Log(Error, ss.str());
      }
    });
    if (anyMissing) return;

    // Capture a snapshot of the pre-play scene state so PauseSceneSimulation
    // can revert any runtime mutations (script-set components, physics-moved
    // transforms, runtime-created entities). Same binary format as the
    // on-disk .scene file; SceneSerialiser is already stream-based so a
    // stringstream round-trip works without any factoring. Snapshot fires
    // AFTER the missing-script check above so a refused play doesn't leave
    // a stale snapshot behind.
    {
      std::stringstream snap(std::ios::in | std::ios::out | std::ios::binary);
      SceneSerialiser snapWriter(m_App->GetActiveScene(),
                                 m_App->GetAssetManager());
      snapWriter.Serialise(snap);
      m_PlaySnapshot = snap.str();
    }

    Project::SaveCurrentProject(m_App->GetAssetManager());

    SaveScene(m_App->GetActiveScene());

    auto scriptReloadView = m_Registry.view<Behaviour>();

    scriptReloadView.each([](auto &behaviour) {
      for (auto const &[uuid, script] : behaviour.scripts) {
        if (!script) continue; // missing script (Phase 6 UI handles this)
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
        if (!script) continue; // missing script (Phase 6 UI handles this)
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

    // Mark the snapshot restore as pending — ProcessPendingRestore at the
    // top of the next frame will do the registry clear+deserialise. Doing
    // it inline here would be unsafe: PauseSceneSimulation can be called
    // mid-iteration (e.g. when on_create throws inside RunSceneSimulation's
    // pyObject instantiation view.each), and clearing the registry while
    // the outer iterator is still alive crashes with a use-after-free on
    // the next bucket-node dereference.
    if (!m_PlaySnapshot.empty()) {
      m_PendingRestore = true;
    }

    m_IsSimulationPaused = true;

    std::cout << "Scene paused" << std::endl;
  }
}

void Scene::ProcessPendingRestore() {
  if (!m_PendingRestore) return;
  m_PendingRestore = false;

  // Order matters: pyObjects hold pybind11::object refs that share lifetime
  // with the Python interpreter — release them before destroying the
  // owning Behaviour components by clearing the registry.
  auto behView = m_Registry.view<Behaviour>();
  behView.each([](auto &beh) { beh.pyObjects.clear(); });

  m_Registry.clear();
  m_Entities.clear();
  m_ChildrenIndex.clear();

  // Drop the spatial index — its entries hold UUIDs whose entt::entity
  // mapping in m_Entities has just been wiped. A stale QueryPoint hit
  // would dereference GetEntityComponent<Transform> through operator[]
  // (which would insert a phantom entt::null entry) and then crash inside
  // registry.get<Transform>(null). The next OnRender rebuilds the index
  // from the restored entity set.
  m_SpatialIndex.Rebuild({});

  try {
    std::stringstream in(m_PlaySnapshot,
                         std::ios::in | std::ios::out | std::ios::binary);
    SceneSerialiser restore(m_App->GetActiveScene(),
                            m_App->GetAssetManager());
    restore.Deserialise(in);
    m_PlaySnapshot.clear();

    // Panels cache raw component pointers across frames; the registry
    // clear+deserialise invalidated them. Reposting ActiveSceneChangedEvent
    // runs each panel's reset path synchronously so the next render finds
    // a clean slate.
    ActiveSceneChangedEvent e(m_App->GetActiveScene());
    m_Dispatcher->Post<ActiveSceneChangedEvent>(e);
  } catch (std::exception &e) {
    m_ClientLogger->Log(
        Error, std::string("Snapshot restore failed: ") + e.what());
    // Keep the snapshot alive so a future fix attempt could retry. The
    // registry is currently empty — user will see a blank scene; better
    // than partial restore writing garbage to disk on next save.
  }
}
} // namespace Hamster
