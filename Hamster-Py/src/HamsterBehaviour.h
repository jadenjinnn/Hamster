#pragma once

#include <Core/UUID.h>
#include <Scripting/HamsterBehaviour.h>
#include "EntityHandle.h"

class PyHamsterBehaviour : Hamster::HamsterBehaviour {
public:
  using Hamster::HamsterBehaviour::HamsterBehaviour;

  void OnCreate() override {
    PYBIND11_OVERRIDE(void, Hamster::HamsterBehaviour, OnCreate, );
  }

  void OnUpdate(float deltaTime) override {
    PYBIND11_OVERRIDE(void, Hamster::HamsterBehaviour, OnUpdate, deltaTime);
  }
};

void HamsterBehaviourBinding(pybind11::module_ m) {
  pybind11::class_<Hamster::HamsterBehaviour, PyHamsterBehaviour>(
      m, "HamsterBehaviour")
      .def(pybind11::init<Hamster::UUID, std::shared_ptr<Hamster::Scene>,
                          Hamster::Application *>())
      .def("on_create", &Hamster::HamsterBehaviour::OnCreate)
      .def("on_update", &Hamster::HamsterBehaviour::OnUpdate)
      .def("reset_input", &Hamster::HamsterBehaviour::ResetInput)
      // .def_property("transform", &);
      .def_property("transform", &Hamster::HamsterBehaviour::GetTransform,
                    &Hamster::HamsterBehaviour::SetTransform)
      .def_property_readonly("key_pressed",
                             &Hamster::HamsterBehaviour::GetKeyPressed)
      .def_property_readonly("key_released",
                             &Hamster::HamsterBehaviour::GetKeyReleased)
      .def_property_readonly("colliding",
                             &Hamster::HamsterBehaviour::IsColliding)
      .def_property_readonly("collision_entities",
                             &Hamster::HamsterBehaviour::GetCollisionEntites)
      .def("reset_collision_entities",
           &Hamster::HamsterBehaviour::EmptyCollisionEntity)
      .def("log", [](Hamster::HamsterBehaviour &self, pybind11::object msg, Hamster::LogType type) {
          self.Log(type, pybind11::str(msg).cast<std::string>());
      }, pybind11::arg("msg"), pybind11::arg("type") = Hamster::LogType::Info)
      .def_property_readonly("velocity",
                             &Hamster::HamsterBehaviour::GetVelocity)
      .def("apply_force", &Hamster::HamsterBehaviour::ApplyForce)
      .def("apply_impulse", &Hamster::HamsterBehaviour::ApplyImpulse)
      .def("set_velocity", &Hamster::HamsterBehaviour::SetVelocity)
      .def("create_entity", [](Hamster::HamsterBehaviour &self,
                                const std::string &name,
                                const Hamster::Transform &transform) {
          Hamster::UUID uuid = self.CreateEntityRuntime(name, transform);
          return EntityHandle{uuid, self.GetScene()};
      })
      .def("destroy_entity", &Hamster::HamsterBehaviour::DestroyEntityRuntime);
}
