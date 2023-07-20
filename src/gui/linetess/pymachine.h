#pragma once
#ifdef HAVE_PYTHON_INTERPRETER
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "math/vec.h"
#include "str_util.h"
#include "note.h"

extern pybind11::detail::embedded_module module_path_bindings;

namespace PyMachine {

    bool initPython();
    void deinitPython();
    double pyEvalExpression(const char* expr, double x, double t);

}// namespace PyMachine

namespace DAW {
    void InitPythonInterpreter();
    void DeinitPythonInterpreter();
    bool IsPythonInitialized();
} // namespace DAW

#define USE_PYTHON
#endif
