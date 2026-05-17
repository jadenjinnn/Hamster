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
      .def_property_readonly("parent", &EntityHandle::GetParent)
      .def_property_readonly("children", &EntityHandle::GetChildren)
      .def("set_parent", &EntityHandle::SetParent);
}
