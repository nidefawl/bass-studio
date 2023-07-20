#include "pymachine.h"
#include "fileio.h"
#include "gui/clipeditor/clipeditor_python_processor.h"
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
const char* moduleNamePathBindings = "path_bindings";
const char* moduleNameNoteBindings = "note_bindings";

static PyObject* pybind11_init_wrapper_path_bindings() {
    auto m = pybind11::module_::create_extension_module(moduleNamePathBindings, nullptr, new PyModuleDef());
    try {
    pybind11::bind_vector<std::vector<glm::vec2>>(m, "VecList");
    pybind11::class_<glm::vec2>(m, "Vec")
            .def_readwrite("x", &glm::vec2::x)
            .def_readwrite("y", &glm::vec2::y)
            .def(pybind11::init<float, float>())
            .def(pybind11::init<>());
        return m.ptr();
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        return nullptr;
    }
}

static PyObject* pybind11_init_wrapper_note_bindings() {
    auto m = pybind11::module_::create_extension_module(moduleNameNoteBindings, nullptr, new PyModuleDef());
    try {
        pybind11::class_<note_t>(m, "note_t")
                .def_readwrite("pitch", &note_t::pitch)
                .def_readwrite("velocity", &note_t::velocity)
                .def_readwrite("time", &note_t::time)
                .def_readwrite("len", &note_t::len)
                .def_readwrite("flags", &note_t::flags)
                .def(pybind11::init<>());
        using DAW::PythonNoteProcessor::python_script_ctxt_t;
        pybind11::class_<python_script_ctxt_t>(m, "python_script_ctxt_t")
                        .def_readonly("notes", &python_script_ctxt_t::notes)
                        .def_readonly("seed", &python_script_ctxt_t::seed)
                        .def_readonly("params", &python_script_ctxt_t::params)
                        .def(pybind11::init<>());
        return m.ptr();
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        return nullptr;
    }
}

extern "C" PyObject* pybind11_init_impl_path_bindings() {
    return pybind11_init_wrapper_path_bindings();
}

pybind11::detail::embedded_module module_path_bindings(moduleNamePathBindings, pybind11_init_wrapper_path_bindings);
pybind11::detail::embedded_module module_note_bindings(moduleNameNoteBindings, pybind11_init_wrapper_note_bindings);

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
        // wchar_t* empty_argv[1]{pybind11::detail::widen_chars("\0")};
        // PySys_SetArgvEx(1, empty_argv, 1);
        PyObject *sys = PyImport_ImportModule("sys");
        PyObject *path = PyObject_GetAttrString(sys, "path");
        auto pathVisualizerPresets = App::Platform::toUserdataPath("presets/Visualizer");
        PyList_Append(path, PyUnicode_FromString(pathVisualizerPresets.c_str()));
        auto pathNoteProcessor = App::Platform::toUserdataPath("presets/NoteProcessor");
        PyList_Append(path, PyUnicode_FromString(pathNoteProcessor.c_str()));
        auto pathCWD = App::Platform::getCurrentWorkingDirectory();
        PyList_Append(path, PyUnicode_FromString(pathCWD.c_str()));
        try {
            py::module::import(moduleNamePathBindings);
            py::module::import(moduleNameNoteBindings);
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_ImportError, e.what());
            return false;
        }
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
#ifdef USE_PYTHON
static bool gPythonInitialized = false;
#endif

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
} // namespace DAW

namespace DAW::PythonNoteProcessor {

std::vector<python_note_processor_t> pyNoteProcessorList;
std::vector<python_note_processor_t> GetNoteProcessors() { return pyNoteProcessorList; }

#ifndef USE_PYTHON

void Init() {
}

std::vector<note_t> RunPythonNoteProcessor(const String& processorName, python_script_ctxt_t& ctxt) {
    return {};
}

#else


void TryLoadPythonNoteProcessor(const char* path) {
    try {
        namespace py = pybind11;
        String pathIn = path;
        String name;
        SplitPath(pathIn, nullptr, &name, nullptr);
        py::module moduleImpl = py::module::import(StringAsCStr(name));
        moduleImpl.reload();
        auto moduleDict = moduleImpl.attr("__dict__");
        auto pyListRegisteredProcessors = moduleImpl.attr("export_processors");
        for (auto& pyNoteProcessorObj : pyListRegisteredProcessors) {
            python_note_processor_t pyNoteProcessor;
            pyNoteProcessor.descriptiveName = py::str(pyNoteProcessorObj.attr("getName")());
            // create class name from module name :: class name
            pyNoteProcessor.processorName = StringFormat("%s::%s", StringAsCStr(name), StringAsCStr(py::str(pyNoteProcessorObj.attr("__class__").attr("__name__")).cast<String>()));
            auto retValGetParameters = pyNoteProcessorObj.attr("getParameters")();
            String pyStrRetValGetParameters = py::str(retValGetParameters);
            auto pyParams = retValGetParameters.cast<std::vector<pybind11::tuple>>();
            for (auto& pyParam : pyParams) {
                python_func_param_t param;
                param.name = pyParam[0].cast<String>();
                param.type = pyParam[1].cast<int>() == 0 ? python_func_param_t::param_type_int : python_func_param_t::param_type_float;
                param.rangeMin = pyParam[2].cast<float>();
                param.rangeMax = pyParam[3].cast<float>();
                param.defValue = pyParam[4].cast<float>();
                pyNoteProcessor.params.push_back(std::move(param));
            }
            log_lf(Log::L_DEBUG, "Registered new midi note processor from python script:\n");
            log_lf(Log::L_DEBUG, "  name: %s\n", StringAsCStr(pyNoteProcessor.descriptiveName));
            for (auto& param : pyNoteProcessor.params) {
                const char* typeName = param.type == python_func_param_t::param_type_int ? "int" : "float";
                log_lf(Log::L_DEBUG, "  param: %s (%s), min %f, max %f, default %f\n", StringAsCStr(param.name), typeName, param.rangeMin, param.rangeMax, param.defValue);
            }
            pyNoteProcessorList.push_back(std::move(pyNoteProcessor));
        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "Python script failed: %s\n", e.what());
    }
}

void Init() {
    pyNoteProcessorList.clear();
    if (!IsPythonInitialized()) {
        InitPythonInterpreter();
    }
    if (IsPythonInitialized()) {
        auto pathNoteProcessor = App::Platform::toUserdataPath("presets/NoteProcessor");
        String strExt = "py";
        std::vector<FileFound> filesFound;
        findFilesWithExt(pathNoteProcessor, strExt, true, filesFound);
        for (auto& fileFound : filesFound) {
            TryLoadPythonNoteProcessor(fileFound.path.c_str());
        }
    }
}

std::vector<note_t> RunPythonNoteProcessor(const String& fqClassName, python_script_ctxt_t& ctxt) {
    if (!DAW::IsPythonInitialized()) {
        DAW::InitPythonInterpreter();
    }
    if (!DAW::IsPythonInitialized()) {
        throw std::runtime_error("Python interpreter not initialized");
    }
    // split class name at "::"
    auto idxOf = fqClassName.find("::");
    String moduleName, processorName;
    if (idxOf != String::npos) {
        moduleName = fqClassName.substr(0, idxOf);
        processorName = fqClassName.substr(idxOf + 2);
    } else {
        throw std::runtime_error("Invalid class name");
    }
    py::module moduleImpl = py::module::import(StringAsCStr(moduleName));
    moduleImpl.reload();
    auto moduleDict = moduleImpl.attr("__dict__");
    auto pyNoteProcessorListObj = moduleImpl.attr("export_processors").cast<py::list>();
    // auto selectedProcessor = pyNoteProcessorListObj[0];
    // find processor with name processorName
    py::object selectedProcessor;
    for (auto& pyNoteProcessorObj : pyNoteProcessorListObj) {
        auto name = py::str(pyNoteProcessorObj.attr("__class__").attr("__name__")).cast<String>();
        if (name == processorName) {
            auto retVal = pyNoteProcessorObj.attr("process")(ctxt);
            return retVal.cast<std::vector<note_t>>();
            break;
        }
    }
    log_lf(Log::L_ERROR, "Python script failed: processor %s not found\n", StringAsCStr(processorName));
    return {};
}

#endif

}// namespace DAW::PythonNoteProcessor
