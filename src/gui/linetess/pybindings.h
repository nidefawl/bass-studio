#pragma once
#ifdef HAVE_PYTHON_INTERPRETER
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
extern pybind11::detail::embedded_module module_path_bindings;
#endif
