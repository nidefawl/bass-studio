#ifdef HAVE_PYTHON_INTERPRETER
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "pybindings.h"
#include "pymachine.h"


#include "platform.h"
#include "str_util.h"
#include "logging.h"
#include <vector>
namespace py = pybind11;
using namespace py::literals;
const char* moduleName              = "user_pathgen_functions";
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

    template<typename TResult, typename... Args>
    TResult callFunctionAttr(String fnName, Args&&... args) {
        py::module moduleBindings = py::module::import(moduleNameEmbedBindings);
        py::module moduleImpl     = py::module::import(moduleName);// load test.py file from working directory
        py::object result         = moduleImpl.attr(StringAsCStr(fnName))(std::forward<Args>(args)...);
        return result.cast<TResult>();
    }

    void testThings() {
        try {
            std::vector<glm::vec2> vecInput(3);
            vecInput[0] = {0, 1};
            vecInput[1] = {1, 1};
            vecInput[2] = {2, 1};
            callFunctionAttr<void>("pathGen_test", vecInput, 0.0f);
            //	randomizePath();
            log_printf("path size %zu\n", vecInput.size());
        } catch (std::exception& e) {
            log_printf("%s\n", e.what());
            throw std::runtime_error("Python initialization failed");
        }
    }

    double pyEvalExpression(const char* expr, double x, double t) {
        auto locals     = py::dict("x"_a = x, "t"_a = t);
        String fullExpr = StringFormat("import random\nfrom math import *\nval = %s", expr);
        py::exec(StringAsCStr(fullExpr), py::globals(), locals);
        return locals["val"].cast<double>();
    }

    void initPython() {
#if 1
        PyPreConfig preconfig;
        PyPreConfig_InitIsolatedConfig(&preconfig);

        preconfig.utf8_mode = 0;
        // preconfig.dev_mode = 2;

        PyStatus status = Py_PreInitialize(&preconfig);
        if (PyStatus_Exception(status)) {
            log_lf(Log::L_ERROR, "Exception while initializing python\n");
            Py_ExitStatusException(status);
            return;
        }

        PyConfig config;
        PyConfig_InitIsolatedConfig(&config);
        config.isolated = 1;
        config.optimization_level = 2;
        config.install_signal_handlers = 0;
        // config.verbose = 2;
        // config.dev_mode = 1;

        config.configure_c_stdio = 0;
        
        status = Py_InitializeFromConfig(&config);
        if (PyStatus_Exception(status)) {
            log_lf(Log::L_ERROR, "Exception while initializing python\n");
            Py_ExitStatusException(status);
            return;
        }
        PyConfig_Clear(&config);
        wchar_t* empty_argv[1]{pybind11::detail::widen_chars("\0")};
        PySys_SetArgvEx(1, empty_argv, 1);
        PyObject *sys = PyImport_ImportModule("sys");
        PyObject *path = PyObject_GetAttrString(sys, "path");
        std::string resPath = App::Platform::GetResourcePath();
        PyList_Append(path, PyUnicode_FromString(resPath.c_str()));

#endif
        testThings();
    }
    void deinitPython() {
        py::finalize_interpreter();
    }
}// namespace PyMachine

#endif
