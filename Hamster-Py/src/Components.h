#include <Core/Components.h>
#include <glm/glm.hpp>
#include <pybind11/pybind11.h>

namespace py = pybind11;

void TransformBinding(py::module_ &m) {
  py::class_<Hamster::Transform>(m, "Transform")
      .def(py::init<glm::vec3, float, glm::vec2>())
      .def_readwrite("position", &Hamster::Transform::position)
      .def_readwrite("rotation", &Hamster::Transform::rotation)
      .def_readwrite("size", &Hamster::Transform::size)
      .def("__repr__", [](const Hamster::Transform &t) {
          return "Transform(pos=(" + std::to_string(t.position.x) + ", " +
                 std::to_string(t.position.y) + ", " + std::to_string(t.position.z) +
                 "), rot=" + std::to_string(t.rotation) +
                 ", size=(" + std::to_string(t.size.x) + ", " + std::to_string(t.size.y) + "))";
      });
}

void BodyTypeBinding(py::module_ &m) {
  py::enum_<Hamster::BodyType>(m, "BodyType")
      .value("Static", Hamster::BodyType::Static)
      .value("Dynamic", Hamster::BodyType::Dynamic)
      .value("Kinematic", Hamster::BodyType::Kinematic);
}

void ColliderShapeBinding(py::module_ &m) {
  py::enum_<Hamster::ColliderShape>(m, "ColliderShape")
      .value("Box", Hamster::ColliderShape::Box)
      .value("Circle", Hamster::ColliderShape::Circle);
}

void SpriteBinding(py::module_ &m) {
  py::class_<Hamster::Sprite>(m, "Sprite")
      .def(py::init<glm::vec3>())
      .def_readwrite("colour", &Hamster::Sprite::colour);
}

void RigidbodyBinding(py::module_ &m) {
  py::class_<Hamster::Rigidbody>(m, "Rigidbody")
      .def(py::init<>())
      .def(py::init([](Hamster::BodyType bodyType, Hamster::ColliderShape shape,
                        float density, float friction, float restitution,
                        float gravityScale) {
          Hamster::Rigidbody rb;
          rb.bodyType = bodyType;
          rb.colliderShape = shape;
          rb.density = density;
          rb.friction = friction;
          rb.restitution = restitution;
          rb.gravityScale = gravityScale;
          return rb;
      }),
      py::arg("body_type") = Hamster::BodyType::Static,
      py::arg("collider_shape") = Hamster::ColliderShape::Box,
      py::arg("density") = 1.0f,
      py::arg("friction") = 0.3f,
      py::arg("restitution") = 0.0f,
      py::arg("gravity_scale") = 1.0f)
      .def_readwrite("body_type", &Hamster::Rigidbody::bodyType)
      .def_readwrite("collider_shape", &Hamster::Rigidbody::colliderShape)
      .def_readwrite("density", &Hamster::Rigidbody::density)
      .def_readwrite("friction", &Hamster::Rigidbody::friction)
      .def_readwrite("restitution", &Hamster::Rigidbody::restitution)
      .def_readwrite("gravity_scale", &Hamster::Rigidbody::gravityScale);
}
