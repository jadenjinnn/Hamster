#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/functional.h>

void UUIDBinding(pybind11::module_ &m) {
    pybind11::class_<Hamster::UUID>(m, "UUID")
            .def(pybind11::init<>())
            .def("GetUUIDString", &Hamster::UUID::GetUUIDString)
            .def("GetUUID", &Hamster::UUID::GetUUID)
            .def_static("nil", &Hamster::UUID::GetNil)
            .def_static("is_nil", &Hamster::UUID::IsNil)
            .def(pybind11::self == pybind11::self)
            .def(pybind11::self != pybind11::self)
            .def("__hash__", [](const Hamster::UUID &u) {
                return std::hash<Hamster::UUID>()(u);
            })
            .def("__repr__", [](Hamster::UUID &u) {
                return "UUID(" + u.GetUUIDString() + ")";
            });
}
