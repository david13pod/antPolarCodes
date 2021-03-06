/*
 * Copyright 2020 Johannes Demel
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// #define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
// #include <numpy/arrayobject.h>

#include <cstdint>

#include <polarcode/encoding/butterfly_fip_packed.h>
#include <polarcode/encoding/encoder.h>
#include <polarcode/errordetection/errordetector.h>

namespace py = pybind11;

void bind_encoder(py::module& m)
{
    using namespace PolarCode::Encoding;
    using namespace PolarCode::ErrorDetection;
    py::class_<ButterflyFipPacked>(m, "PolarEncoder")
        .def(py::init<size_t, std::vector<unsigned>>(),
             py::arg("blockLength"),
             py::arg("frozenBitPositions"))
        .def("blockLength", &Encoder::blockLength)
        .def("infoLength", &Encoder::infoLength)
        .def("setSystematic", &Encoder::setSystematic)
        .def("isSystematic", &Encoder::isSystematic)
        .def("frozenBits", &Encoder::frozenBits)
        .def("getErrorDetectionMode", &Encoder::getErrorDetectionMode)
        .def(
            "setErrorDetection",
            [](ButterflyFipPacked& self, unsigned size, std::string type) {
                self.setErrorDetection(PolarCode::ErrorDetection::create(size, type));
            },
            py::arg("size") = 0,
            py::arg("type") = "crc")
        .def("encode_vector",
             [](ButterflyFipPacked& self,
                const py::array_t<uint8_t, py::array::c_style | py::array::forcecast>&
                    array) {
                 py::buffer_info inb = array.request();
                 if (inb.ndim != 1) {
                     throw std::runtime_error("Only ONE-dimensional vectors allowed!");
                 }
                 if ((size_t)inb.size != self.infoLength() / 8) {
                     throw std::runtime_error("Input vector size != infoSize // 8!");
                 }
                 auto result = py::array_t<uint8_t>(self.blockLength() / 8);
                 py::buffer_info resb = result.request();

                 self.encode_vector((void*)inb.ptr, (void*)resb.ptr);
                 return result;
             });
}
