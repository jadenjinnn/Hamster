#pragma once

#include <pybind11/pybind11.h>

void UUIDBinding(pybind11::module_ &m) {
    pybind11::class_<Hamster::UUID>(m, "UUID")
            .def(pybind11::init<>())
            .def("GetUUIDString", &Hamster::UUID::GetUUIDString)
            .def("GetUUID", &Hamster::UUID::GetUUID)
            .def("__repr__", [](Hamster::UUID &u) {
                return "UUID(" + u.GetUUIDString() + ")";
            });
}
