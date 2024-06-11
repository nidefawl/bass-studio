#pragma once
#if HAVE_PYTHON_INTERPRETER

namespace PyMachine {

    bool initPython();
    void deinitPython();
    double pyEvalExpression(const char* expr, double x, double t);

}// namespace PyMachine

#define USE_PYTHON
#endif

namespace DAW {
    void InitPythonInterpreter();
    void DeinitPythonInterpreter();
    bool IsPythonInitialized();
} // namespace DAW
