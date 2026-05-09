//
// Created by Jaden on 28/08/2024.
//

#include "HamsterPCH.h"

#include "Scene.h"

#include <pybind11/pybind11.h>

#include "Application.h"
#include "Physics/Physics.h"
#include "Project.h"
#include "Renderer/Renderer.h"
#include "SceneSerialiser.h"

#include "Scripting/HamsterBehaviour.h"
#include "Scripting/Scripting.h"

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
  OnPhysicsDetect();

  if (!m_IsSimulationPaused) {
    auto currentFrame = static_cast<float>(glfwGetTime());
    m_DeltaTime = currentFrame - m_LastFrame;
    m_LastFrame = currentFrame;

    OnScriptUpdate();
    OnPhysicsResolve();
  }
}

void Scene::OnPhysicsDetect() {
  auto collisionDetect = m_Registry.view<Transform, Rigidbody, ID>();

  collisionDetect.each([this, collisionDetect](auto entityA, auto &transformA,
                                               auto &rbA, auto &idA) mutable {
    collisionDetect.each([this, entityA, &transformA, &idA](
                             auto entityB, auto &transformB, auto &rbB,
                             auto &idB) mutable {
      if (entityA != entityB) {
        if (Physics::IsColliding(transformA, transformB)) {
          CollisionEvent e(idA.uuid, idB.uuid);

          m_Dispatcher->Post<CollisionEvent>(e);
        }
      }
    });
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

void Scene::OnPhysicsResolve() {
  auto physicsUpdate = m_Registry.view<Transform, Rigidbody>();

  physicsUpdate.each(
      [physicsUpdate](auto entityA, auto &transformA, auto &rbA) mutable {
        physicsUpdate.each([entityA, &transformA, &rbA](auto entityB,
                                                      auto &transformB,
                                                      auto &rbB) mutable {
          if (entityA != entityB) {
            Physics::ResolveCollision(transformA, rbA, transformB, rbB);
          }
        });
      });
}

void Scene::OnRender(bool renderFlat) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (m_IsRunning) {
    m_RenderGroup.sort<Transform>(
        [](const Transform &lhs, const Transform &rhs) {
          return lhs.position.z < rhs.position.z;
        });

    if (!renderFlat) {
      m_RenderGroup.each([](auto &sprite, auto &transform) {
        if (sprite.texture != nullptr) {
          Renderer::DrawSprite(*sprite.texture, transform.position,
                               transform.size, transform.rotation,
                               sprite.colour);
        }
      });
    } else {
      // render all sprites as a unique flat colour which can be used to
      // identify to their id

      // std::cout << "hi" << std::endl;

      m_RenderGroup.each([](auto entity, auto &sprite, auto &transform) {
        if (sprite.texture != nullptr) {
          Renderer::DrawFlat(
              transform.position, transform.size, transform.rotation,
              Application::IdToColour(entt::to_integral(entity)));
        }
      });
    }
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
    Project::SaveCurrentProject();

    SaveScene(m_App->GetActiveScene());

    auto scriptReloadView = m_Registry.view<Behaviour>();

    scriptReloadView.each([](auto &behaviour) {
      for (auto const &[uuid, script] : behaviour.scripts) {
        script->ReloadScript();
      }
    });

    m_IsSimulationPaused = false;

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
    m_IsSimulationPaused = true;

    std::cout << "Scene paused" << std::endl;
  }
}
} // namespace Hamster
