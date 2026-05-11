//
// Created by Jaden on 28/08/2024.
//

#include "HamsterPCH.h"

#include "Scene.h"

#include <box2d/box2d.h>
#include <pybind11/pybind11.h>

#include "Application.h"
#include "Project.h"
#include "Renderer/Renderer.h"
#include "SceneSerialiser.h"

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

  return uuid;
}

void Scene::CreateEntityWithUUID(UUID uuid) {
  auto entity = m_Registry.create();

  m_Registry.emplace<ID>(entity, uuid);
  m_Entities[uuid] = entity;
}

void Scene::DestroyEntity(UUID entityUUID) {
  m_Registry.destroy(m_Entities[entityUUID]);
  m_Entities.erase(entityUUID);
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
      ApplyPendingForces();
      StepPhysics();
      ProcessContactEvents();
      SyncPhysicsToTransforms();
      CacheVelocities();
    }

    OnScriptUpdate();
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

    float halfW = std::max(std::abs(transform.size.x) / PIXELS_PER_METER * 0.5f, 0.05f);
    float halfH = std::max(std::abs(transform.size.y) / PIXELS_PER_METER * 0.5f, 0.05f);

    if (rb.colliderShape == ColliderShape::Circle) {
      b2Circle circle;
      circle.center = {0.0f, 0.0f};
      circle.radius = std::max(halfW, halfH);
      b2CreateCircleShape(rb.bodyId, &shapeDef, &circle);
    } else {
      b2Polygon box = b2MakeBox(halfW, halfH);
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
  auto view = m_Registry.view<Behaviour>();

  bool pythonError = false;

  view.each([this, &pythonError](auto &behaviour) mutable {
    for (auto &obj : behaviour.pyObjects) {
      try {
        obj.attr("on_update")(m_DeltaTime);

        obj.attr("reset_input")();
      } catch (pybind11::error_already_set &e) {
        pythonError = true;

        m_ClientLogger->Log(Error, e.what());

        break;
      }
    }
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

    m_IsSimulationPaused = true;

    std::cout << "Scene paused" << std::endl;
  }
}
} // namespace Hamster
