#pragma once
#include <FMU.h>
#include <complex>
#include <stdexcept>

namespace BlockTypeSupports::BasicFMUSupport {

// ---------- FromPyObject ----------
template<typename T>
T FromPyObject(PyObject* obj);

// specialization for double
template<>
inline double FromPyObject<double>(PyObject* obj) {
    if (PyFloat_Check(obj)) return PyFloat_AsDouble(obj);
    if (PyLong_Check(obj))  return static_cast<double>(PyLong_AsLong(obj));
    throw std::runtime_error("FromPyObject<double>: object is not a number");
}

// specialization for complex<double>
template<>
inline std::complex<double> FromPyObject<std::complex<double>>(PyObject* obj) {
    if (PyComplex_Check(obj))
        return { PyComplex_RealAsDouble(obj), PyComplex_ImagAsDouble(obj) };
    if (PyFloat_Check(obj) || PyLong_Check(obj))
        return { PyFloat_AsDouble(obj), 0.0 };
    throw std::runtime_error("FromPyObject<complex<double>>: invalid object");
}


// ---------- ToPyObject ----------
template<typename T>
PyObject* ToPyObject(const T& v);

// specialization for double
template<>
inline PyObject* ToPyObject<double>(const double& v) {
    return PyFloat_FromDouble(v);
}

// specialization for complex<double>
template<>
inline PyObject* ToPyObject<std::complex<double>>(const std::complex<double>& v) {
    return PyComplex_FromDoubles(v.real(), v.imag());
}

} // namespace
