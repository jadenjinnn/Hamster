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

void UIAnchorBinding(py::module_ &m) {
  py::enum_<Hamster::UIAnchor>(m, "UIAnchor")
      .value("TopLeft",      Hamster::UIAnchor::TopLeft)
      .value("TopCentre",    Hamster::UIAnchor::TopCentre)
      .value("TopRight",     Hamster::UIAnchor::TopRight)
      .value("MiddleLeft",   Hamster::UIAnchor::MiddleLeft)
      .value("Centre",       Hamster::UIAnchor::Centre)
      .value("MiddleRight",  Hamster::UIAnchor::MiddleRight)
      .value("BottomLeft",   Hamster::UIAnchor::BottomLeft)
      .value("BottomCentre", Hamster::UIAnchor::BottomCentre)
      .value("BottomRight",  Hamster::UIAnchor::BottomRight);
}

void UITextAlignBinding(py::module_ &m) {
  py::enum_<Hamster::UITextAlign>(m, "UITextAlign")
      .value("Left",   Hamster::UITextAlign::Left)
      .value("Centre", Hamster::UITextAlign::Centre)
      .value("Right",  Hamster::UITextAlign::Right);
}

void UIButtonBinding(py::module_ &m) {
  py::class_<Hamster::UIButton>(m, "UIButton")
      .def(py::init<>())
      .def_readwrite("anchor", &Hamster::UIButton::anchor)
      .def_readwrite("offset", &Hamster::UIButton::offset)
      .def_readwrite("size", &Hamster::UIButton::size)
      .def_readwrite("auto_size", &Hamster::UIButton::autoSize)
      .def_readwrite("padding", &Hamster::UIButton::padding)
      .def_readwrite("bg_colour", &Hamster::UIButton::bgColour)
      .def_readwrite("label", &Hamster::UIButton::label)
      .def_readwrite("text_colour", &Hamster::UIButton::textColour)
      .def_readwrite("font_size", &Hamster::UIButton::fontSize)
      .def_readwrite("text_align", &Hamster::UIButton::textAlign)
      .def_readwrite("bold", &Hamster::UIButton::bold);
}

void UITextBinding(py::module_ &m) {
  py::class_<Hamster::UIText>(m, "UIText")
      .def(py::init<>())
      .def_readwrite("anchor", &Hamster::UIText::anchor)
      .def_readwrite("offset", &Hamster::UIText::offset)
      .def_readwrite("text", &Hamster::UIText::text)
      .def_readwrite("text_colour", &Hamster::UIText::textColour)
      .def_readwrite("font_size", &Hamster::UIText::fontSize)
      .def_readwrite("wrap_width", &Hamster::UIText::wrapWidth)
      .def_readwrite("bold", &Hamster::UIText::bold);
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
