#include "pymachine.h"
#ifdef USE_PYTHON
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "platform.h"
#include "str_util.h"
#include "logging.h"
#include <vector>

#ifdef _WIN32
#include "str_win32.h"
#endif

namespace py = pybind11;
using namespace py::literals;
const char* moduleNameEmbedBindings = "path_bindings";

static void pybind11_init_path_bindings(pybind11::module& m) {
    pybind11::bind_vector<std::vector<glm::vec2>>(m, "VecList");
    pybind11::class_<glm::vec2>(m, "Vec")
            .def_readwrite("x", &glm::vec2::x)
            .def_readwrite("y", &glm::vec2::y)
            .def(pybind11::init<float, float>())
            .def(pybind11::init<>());
}
static PyObject* pybind11_init_wrapper_path_bindings() {
    auto m = pybind11::module_::create_extension_module("path_bindings", nullptr, new PyModuleDef());
    try {
        pybind11_init_path_bindings(m);
        return m.ptr();
    } catch (pybind11::error_already_set& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        return nullptr;
    }
}
extern "C" PyObject* pybind11_init_impl_path_bindings() {
    return pybind11_init_wrapper_path_bindings();
}
pybind11::detail::embedded_module module_path_bindings("path_bindings", pybind11_init_impl_path_bindings);

namespace PyMachine {

    double pyEvalExpression(const char* expr, double x, double t) {
        auto locals     = py::dict("x"_a = x, "t"_a = t);
        String fullExpr = StringFormat("import random\nfrom math import *\nval = %s", expr);
        py::exec(StringAsCStr(fullExpr), py::globals(), locals);
        return locals["val"].cast<double>();
    }

    bool initPython() {
#if 1
        PyPreConfig preconfig;
        PyPreConfig_InitIsolatedConfig(&preconfig);

        preconfig.utf8_mode = 0;
        // preconfig.dev_mode = 2;

        PyStatus status = Py_PreInitialize(&preconfig);
        if (PyStatus_Exception(status)) {
            log_lf(Log::L_ERROR, "Py_PreInitialize: Exception. Failed to initialize python interpreter\n");
            Py_ExitStatusException(status);
            return false;
        }

        PyConfig config;
        PyConfig_InitIsolatedConfig(&config);
        config.isolated = 1;
        config.optimization_level = 2;
        config.install_signal_handlers = 0;
        // config.verbose = 2;
        // config.dev_mode = 1;
        // config.pathconfig_warnings = 1;

        config.configure_c_stdio = 0;

        status = Py_InitializeFromConfig(&config);
        if (PyStatus_Exception(status)) {
            log_lf(Log::L_ERROR, "Py_InitializeFromConfig: Exception. Failed to initialize python interpreter\n");
            Py_ExitStatusException(status);
            return false;
        }
        PyConfig_Clear(&config);
        wchar_t* empty_argv[1]{pybind11::detail::widen_chars("\0")};
        PySys_SetArgvEx(1, empty_argv, 1);
        PyObject *sys = PyImport_ImportModule("sys");
        PyObject *path = PyObject_GetAttrString(sys, "path");
        auto pathVisualizerPresets = App::Platform::toUserdataPath("presets/Visualizer");
        PyList_Append(path, PyUnicode_FromString(pathVisualizerPresets.c_str()));
        return true;
#else
        return false;
#endif
    }

    void deinitPython() {
        py::finalize_interpreter();
    }
}// namespace PyMachine

#endif

namespace DAW {
static bool gPythonInitialized = false;

void InitPythonInterpreter() {
#ifdef USE_PYTHON
    if (!gPythonInitialized) {
        gPythonInitialized = PyMachine::initPython();
    }
#endif
}

void DeinitPythonInterpreter() {
#ifdef USE_PYTHON
    if (gPythonInitialized) {
        gPythonInitialized = false;
        PyMachine::deinitPython();
    }
#endif
}

bool IsPythonInitialized() {
#ifdef USE_PYTHON
    return gPythonInitialized;
#else
    return false;
#endif
}

}
