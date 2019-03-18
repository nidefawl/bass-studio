#pragma once
#if USE_GLAD_GL_HEADERS
//gl functions for actual compilation
#include <glad/glad.h>
#else
//gl defines just for the IDE
#include "glcorearb.h"
#endif
