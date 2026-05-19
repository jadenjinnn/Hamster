#pragma once

#include <memory>
#include <pybind11/pybind11.h>

#include <Core/Application.h>
#include <Core/Components.h>
#include <Core/Scene.h>
#include <Core/UUID.h>
#include <Renderer/Texture.h>
#include <Utils/AssetManager.h>

namespace py = pybind11;

struct EntityHandle {
  Hamster::UUID uuid;
  std::shared_ptr<Hamster::Scene> scene;
  Hamster::Application *app = nullptr;

  void AddComponent(py::object component) {
    if (Hamster::UUID::IsNil(uuid)) {
      return;
    }

    if (py::isinstance<Hamster::Sprite>(component)) {
      if (scene->EntityHasComponent<Hamster::Sprite>(uuid)) {
        return;
      }
      scene->AddEntityComponent<Hamster::Sprite>(uuid, component.cast<Hamster::Sprite>());
    } else if (py::isinstance<Hamster::Rigidbody>(component)) {
      if (scene->EntityHasComponent<Hamster::Rigidbody>(uuid)) {
        return;
      }
      scene->AddEntityComponent<Hamster::Rigidbody>(uuid, component.cast<Hamster::Rigidbody>());
      scene->GetPendingBodies().push_back(uuid);
    } else {
      throw py::type_error("Unknown component type");
    }
  }

  Hamster::UUID GetParent() const { return scene->GetParent(uuid); }
  std::vector<Hamster::UUID> GetChildren() const { return scene->GetChildren(uuid); }
  void SetParent(Hamster::UUID newParent) {
    if (!scene->SetParent(uuid, newParent)) {
      throw py::value_error(
          "set_parent failed: cycle, self-parent, or unknown UUID");
    }
  }

  // Transform get/set — mirrors `self.transform` on HamsterBehaviour but
  // reaches into any entity by UUID rather than the script's own entity.
  // Returns by value (pybind owns its own Transform); writing back persists
  // via the scene's registry.
  Hamster::Transform GetTransform() const {
    if (Hamster::UUID::IsNil(uuid)) {
      throw py::value_error("transform: nil entity handle");
    }
    return scene->GetEntityComponent<Hamster::Transform>(uuid);
  }

  void SetTransform(const Hamster::Transform &t) {
    if (Hamster::UUID::IsNil(uuid)) return;
    auto &dst = scene->GetEntityComponent<Hamster::Transform>(uuid);
    dst.position = t.position;
    dst.rotation = t.rotation;
    dst.size = t.size;
  }

  // Rigidbody pending-velocity write — same deferred-write pattern Scene
  // uses (Box2D mutations are queued and flushed inside Scene::OnUpdate).
  void SetVelocity(float vx, float vy) {
    if (Hamster::UUID::IsNil(uuid)) return;
    if (!scene->EntityHasComponent<Hamster::Rigidbody>(uuid)) {
      throw py::value_error("set_velocity: entity has no Rigidbody");
    }
    auto &rb = scene->GetEntityComponent<Hamster::Rigidbody>(uuid);
    rb.pendingVelocity = glm::vec2(vx, vy);
    rb.hasPendingVelocity = true;
  }

  void ApplyImpulse(float ix, float iy) {
    if (Hamster::UUID::IsNil(uuid)) return;
    if (!scene->EntityHasComponent<Hamster::Rigidbody>(uuid)) {
      throw py::value_error("apply_impulse: entity has no Rigidbody");
    }
    auto &rb = scene->GetEntityComponent<Hamster::Rigidbody>(uuid);
    rb.pendingImpulse += glm::vec2(ix, iy);
  }

  // Live mutation of a UIButton's label text. Refuses (ValueError) if the
  // entity doesn't have a UIButton component — game scripts will want a
  // descriptive failure rather than a silent no-op.
  void SetLabel(const std::string &text) {
    if (Hamster::UUID::IsNil(uuid)) return;
    if (!scene->EntityHasComponent<Hamster::UIButton>(uuid)) {
      throw py::value_error("set_label: entity has no UIButton component");
    }
    scene->GetEntityComponent<Hamster::UIButton>(uuid).label = text;
  }

  // Live mutation of a UIText's text content. Same semantics as set_label
  // but for the UIText component.
  void SetText(const std::string &text) {
    if (Hamster::UUID::IsNil(uuid)) return;
    if (!scene->EntityHasComponent<Hamster::UIText>(uuid)) {
      throw py::value_error("set_text: entity has no UIText component");
    }
    scene->GetEntityComponent<Hamster::UIText>(uuid).text = text;
  }

  // Look the texture up by name in AssetManager and assign it to this
  // entity's Sprite. Entity must already have a Sprite component (call
  // add_component(Sprite(...)) first). Added so the batching benchmark
  // script can spawn textured sprites at runtime without going through
  // the editor.
  void SetTexture(const std::string &name) {
    if (Hamster::UUID::IsNil(uuid)) return;
    if (!scene->EntityHasComponent<Hamster::Sprite>(uuid)) {
      throw py::value_error(
          "set_texture: entity has no Sprite component yet");
    }
    if (!app) {
      throw py::value_error("set_texture: no AssetManager available");
    }
    auto *am = app->GetAssetManager();
    for (auto const &[texUUID, tex] : am->GetTextureMap()) {
      if (tex && tex->GetName() == name) {
        scene->GetEntityComponent<Hamster::Sprite>(uuid).texture = tex;
        return;
      }
    }
    throw py::value_error("set_texture: no texture named '" + name + "'");
  }
};

void EntityHandleBinding(py::module_ &m) {
  py::class_<EntityHandle>(m, "EntityHandle")
      .def_readonly("uuid", &EntityHandle::uuid)
      .def("add_component", &EntityHandle::AddComponent)
      .def("set_texture", &EntityHandle::SetTexture)
      .def("set_label", &EntityHandle::SetLabel)
      .def("set_text", &EntityHandle::SetText)
      .def_property("transform", &EntityHandle::GetTransform,
                    &EntityHandle::SetTransform)
      .def("set_velocity", &EntityHandle::SetVelocity)
      .def("apply_impulse", &EntityHandle::ApplyImpulse)
      .def_property_readonly("parent", &EntityHandle::GetParent)
      .def_property_readonly("children", &EntityHandle::GetChildren)
      .def("set_parent", &EntityHandle::SetParent);
}
