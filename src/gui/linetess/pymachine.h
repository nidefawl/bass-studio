#pragma once
#ifdef HAVE_PYTHON_INTERPRETER
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "math/vec.h"
#include "str_util.h"

namespace PyMachine {

    void initPython();
    void deinitPython();
    double pyEvalExpression(const char* expr, double x, double t);

}// namespace PyMachine

#endif
