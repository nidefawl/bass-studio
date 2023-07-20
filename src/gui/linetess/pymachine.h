#pragma once
#ifdef HAVE_PYTHON_INTERPRETER

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
