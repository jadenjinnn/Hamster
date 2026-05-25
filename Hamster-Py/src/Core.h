#include <Core/Application.h>
#include <Core/Scene.h>
#include <memory>
#include <pybind11/pybind11.h>

#include "EntityHandle.h"

void ScenePtrBinding(pybind11::module_ &m) {
  // Returns EntityHandle on hit, None on miss. App* is taken from Scene so
  // handle methods like set_texture work end-to-end.
  pybind11::class_<Hamster::Scene, std::shared_ptr<Hamster::Scene>>(m, "Scene")
      .def("find_entity_by_name",
           [](std::shared_ptr<Hamster::Scene> scene, const std::string &name)
               -> pybind11::object {
             Hamster::UUID uuid = scene->FindEntityByName(name);
             if (Hamster::UUID::IsNil(uuid)) {
               return pybind11::none();
             }
             EntityHandle handle{uuid, scene, scene->GetApp()};
             return pybind11::cast(handle);
           },
           pybind11::arg("name"));
}

void AppInstanceBinding(pybind11::module_ &m) {
  pybind11::class_<Hamster::Application>(m, "Application");
}

void EventDispatcherPtrBinding(pybind11::module_ &m) {
  pybind11::class_<Hamster::EventDispatcher,
                   std::shared_ptr<Hamster::EventDispatcher>>(
      m, "EventDispatcher");
}

