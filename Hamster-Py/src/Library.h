//-------------------------------------------------------------------------

// Type definitions from libraries for python

//-------------------------------------------------------------------------

#include <pybind11/pybind11.h>

namespace py = pybind11;

//-------------------------------------------------------------------------

// glm

//-------------------------------------------------------------------------

#include <pybind11/operators.h>

#include <glm/glm.hpp>

void Vec2Binding(py::module_ &m) {

  py::class_<glm::vec2>(m, "vec2")

      .def(py::init<float, float>())

      .def_readwrite("x", &glm::vec2::x)
      .def_readwrite("y", &glm::vec2::y)

      .def(py::self + py::self)

      .def(py::self * py::self)

      .def(py::self - py::self)

      .def(py::self * float())
      .def("__repr__", [](const glm::vec2 &v) {
          return "vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
      });

  /*.def("__add__",

          [](const glm::vec2& a, consrt glm::vec2& b) {

                  return a + b;

          })



  .def("__sub__",

          [](const glm::vec2& a, const glm::vec2& b) {

                  return a - b;

          })



  .def("__mul__",

          [](const glm::vec2& a, const glm::vec2& b) {

                  return a * b;

          });*/
}

void Vec4Binding(py::module_ &m) {
  py::class_<glm::vec4>(m, "vec4")
      .def(py::init<float, float, float, float>())
      .def_readwrite("x", &glm::vec4::x)
      .def_readwrite("y", &glm::vec4::y)
      .def_readwrite("z", &glm::vec4::z)
      .def_readwrite("w", &glm::vec4::w)
      .def_readwrite("r", &glm::vec4::r)
      .def_readwrite("g", &glm::vec4::g)
      .def_readwrite("b", &glm::vec4::b)
      .def_readwrite("a", &glm::vec4::a)
      .def("__repr__", [](const glm::vec4 &v) {
          return "vec4(" + std::to_string(v.x) + ", " + std::to_string(v.y) +
                 ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
      });
}

void Vec3Binding(py::module_ &m) {

  py::class_<glm::vec3>(m, "vec3")

      .def(py::init<float, float, float>())

      .def_readwrite("x", &glm::vec3::x)
      .def_readwrite("y", &glm::vec3::y)
      .def_readwrite("z", &glm::vec3::z)

      .def("__add__",

           [](const glm::vec3 &a, const glm::vec3 &b) { return a + b; })

      .def("__sub__",

           [](const glm::vec3 &a, const glm::vec3 &b) { return a - b; })

      .def("__mul__",

           [](const glm::vec3 &a, const glm::vec3 &b) { return a * b; })
      .def("__repr__", [](const glm::vec3 &v) {
          return "vec3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
      });
}
