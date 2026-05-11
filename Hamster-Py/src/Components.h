#include <Core/Components.h>
#include <glm/glm.hpp>
#include <pybind11/pybind11.h>

namespace py = pybind11;

void TransformBinding(py::module_ &m) {
  py::class_<Hamster::Transform>(m, "Transform")
      .def(py::init<glm::vec3, float, glm::vec3>())
      .def_readwrite("position", &Hamster::Transform::position)
      .def_readwrite("rotation", &Hamster::Transform::rotation)
      .def_readwrite("size", &Hamster::Transform::size);
}
