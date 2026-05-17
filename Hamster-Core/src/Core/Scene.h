//
// Created by Jaden on 28/08/2024.
//

#ifndef SCENE_H
#define SCENE_H

#include <box2d/box2d.h>
#include <entt/entt.hpp>

#include <boost/container_hash/hash.hpp>

#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include <memory>

#include "Components.h"

#include "Events/ApplicationEvents.h"
#include "Events/WindowEvents.h"
#include "Log.h"

namespace Hamster {
class Application;
class SceneCreatedEvent;

class Scene {
public:
  Scene(EventDispatcher *dispatcher, Application *app);

  UUID CreateEntity();
  UUID CreateEntityRuntime(const std::string &name, const Transform &transform);

  void CreateEntityWithUUID(UUID uuid);

  void DestroyEntity(UUID entityUUID);
  void QueueDestroyEntity(UUID entityUUID);
  void FlushDestroyQueue();
  void CreatePendingBodies();

  // Hierarchy. parent == UUID::GetNil() means top-level.
  // SetParent returns false on self-parent, cycle, or unknown UUIDs.
  bool SetParent(UUID child, UUID newParent);
  UUID GetParent(UUID uuid);
  const std::vector<UUID> &GetChildren(UUID parent);
  const std::vector<UUID> &GetTopLevelEntities() { return m_ChildrenIndex[UUID::GetNil()]; }
  // Move `uuid` to a new position among its siblings (clamped to valid range).
  void ReorderSibling(UUID uuid, uint32_t newIndex);

  // Reconstruct m_ChildrenIndex from the Hierarchy components on every entity.
  // Called by SceneSerialiser after all entities have been deserialised so
  // parent UUIDs referencing entities appearing later in the file resolve.
  void RebuildHierarchyIndex();

  entt::entity &GetEntity(UUID entityUUID) { return m_Entities[entityUUID]; }

  UUID GetEntityUUID(entt::entity entity) {
    return m_Registry.get<ID>(entity).uuid;
  }

  entt::registry &GetRegistry() { return m_Registry; }

  // template <typename T>
  // T GetEntityComponent(UUID uuid);
  //
  // template <typename T, typename... Args>
  // void AddEntityComponent(UUID entityUUID, Args&&... args);

  template <typename T> T &GetEntityComponent(UUID uuid) {
    return m_Registry.get<T>(m_Entities[uuid]);
  }

  template <typename T, typename... Args>
  void AddEntityComponent(UUID entityUUID, Args &&...args) {
    m_Registry.emplace<T>(m_Entities[entityUUID], std::forward<Args>(args)...);
  }

  template <typename T> bool EntityHasComponent(UUID entityUUID) {
    return m_Registry.all_of<T>(m_Entities[entityUUID]);
  }

  void OnUpdate();
  void OnScriptUpdate();

  void InitPhysicsWorld();
  void DestroyPhysicsWorld();
  void StepPhysics();
  void SyncPhysicsToTransforms();
  void ProcessContactEvents();
  void ApplyPendingForces();
  void CacheVelocities();

  void OnRender(bool renderFlat);

  bool IsSceneRunning() const { return m_IsRunning; }
  bool IsSceneSimulationPaused() const { return m_IsSimulationPaused; }

  void RunScene() { m_IsRunning = true; }

  void RunSceneSimulation();

  void PauseScene() { m_IsRunning = false; }

  void PauseSceneSimulation();

  // Snapshot restore is deferred to here (called from Application::Run at
  // the top of each frame) so it never runs while the registry is being
  // iterated. PauseSceneSimulation can be called mid-iteration (e.g. from
  // a script's on_create throwing inside the pyObject instantiation loop);
  // clearing the registry there would invalidate the outer view iterator.
  // Doing the clear+deserialise between frames sidesteps that.
  void ProcessPendingRestore();

  UUID GetUUID() const { return m_UUID; }

  void SetUUID(const UUID &uuid);

  uint32_t GetEntityCount() const {
    return static_cast<uint32_t>(m_Entities.size());
  }

  const std::unordered_map<UUID, entt::entity> &GetEntityMap() {
    return m_Entities;
  }

  static void SaveScene(std::shared_ptr<Scene> scene);

  std::string &GetName() { return m_Name; }

  std::filesystem::path &GetPath() { return m_Path; }

  void OnSceneCreated(SceneCreatedEvent &e);

  std::shared_ptr<Logger> GetClientLogger() { return m_ClientLogger; }

  std::vector<UUID> &GetPendingBodies() { return m_PendingBodies; }

private:
  bool m_IsRunning = false;
  bool m_IsSimulationPaused = true;

  entt::registry m_Registry;
  std::unordered_map<UUID, entt::entity> m_Entities;

  entt::basic_group<
      entt::owned_t<
          entt::basic_sigh_mixin<
              entt::basic_storage<Hamster::Sprite, entt::entity,
                                  std::allocator<Hamster::Sprite>, void>,
              entt::basic_registry<entt::entity, std::allocator<entt::entity>>>,
          entt::basic_sigh_mixin<
              entt::basic_storage<Hamster::Transform, entt::entity,
                                  std::allocator<Hamster::Transform>, void>,
              entt::basic_registry<entt::entity,
                                   std::allocator<entt::entity>>>>,
      entt::get_t<>, entt::exclude_t<>>
      m_RenderGroup;

  UUID m_UUID;
  std::string m_Name = "Untitled Scene";
  std::filesystem::path m_Path;

  float m_DeltaTime = 0.0f;
  float m_LastFrame = 0.0f;

  b2WorldId m_PhysicsWorld = b2_nullWorldId;

  std::vector<UUID> m_DestroyQueue;
  std::vector<UUID> m_PendingBodies;

  // In-memory snapshot of the entire scene captured at RunSceneSimulation
  // start, restored on PauseSceneSimulation end. Implements Unity-style
  // non-destructive play: runtime-spawned entities, physics-moved transforms,
  // and script-mutated components all revert on stop. Empty when no
  // simulation is active. Holds the raw bytes a SceneSerialiser stringstream
  // round-trip produces — same format as on-disk .scene files.
  std::string m_PlaySnapshot;

  // Set true by PauseSceneSimulation when a snapshot is waiting to be
  // restored; cleared by ProcessPendingRestore at the top of the next
  // frame. Defers registry mutation so PauseSceneSimulation is safe to
  // call mid-iteration (e.g. from on_create exception handlers).
  bool m_PendingRestore = false;

  // Reverse-index for the hierarchy tree. m_ChildrenIndex[parent] holds the
  // children of `parent` in sibling order. Top-level entities live under
  // UUID::GetNil(). Kept in sync with Hierarchy components by Scene's
  // create/destroy/SetParent/Reorder paths — no external mutators.
  std::unordered_map<UUID, std::vector<UUID>> m_ChildrenIndex;

  std::shared_ptr<Logger> m_ClientLogger;

  EventDispatcher *m_Dispatcher;
  Application *m_App;
};
} // namespace Hamster

#endif // SCENE_H
