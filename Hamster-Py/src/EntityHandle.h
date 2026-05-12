#pragma once

#include <memory>
#include <pybind11/pybind11.h>

#include <Core/Components.h>
#include <Core/Scene.h>
#include <Core/UUID.h>

namespace py = pybind11;

struct EntityHandle {
  Hamster::UUID uuid;
  std::shared_ptr<Hamster::Scene> scene;

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
};

void EntityHandleBinding(py::module_ &m) {
  py::class_<EntityHandle>(m, "EntityHandle")
      .def_readonly("uuid", &EntityHandle::uuid)
      .def("add_component", &EntityHandle::AddComponent);
}
