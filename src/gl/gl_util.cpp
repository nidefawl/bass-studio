#include "gl/gl_util.hpp"
#include "glheaders.h"
#include <GLFW/glfw3.h>
#include <vector>
#include "assert_dbg.h"
#include "gl_tess2d.hpp"
#include "gl_attr.hpp"
#include "gl_vbo.hpp"
#include "gl_framebuffer.hpp"
#include "logging.hpp"
#include "seq_util.hpp"
#include "str_util.hpp"

void debugCB(GLenum source,
             GLenum type,
             GLuint id,
             GLenum severity,
             GLsizei length,
             const GLchar* message,
             const void* userParam) {
    if (!strstr(message, "Buffer detailed info") && !strstr(message, "state performance warning")) {
        log_printf("%s\n", message);
        if (strstr(message, "error")) {
        }
    }
}

void enableGlDebugCallback() {

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    if (glDebugMessageCallback)
        glDebugMessageCallback(debugCB, nullptr);
    GLuint unusedIds = 0;
    if (glDebugMessageControl)
        glDebugMessageControl(GL_DONT_CARE,
                              GL_DONT_CARE,
                              GL_DONT_CARE,
                              0,
                              &unusedIds,
                              true);
}
#ifndef NDEBUG
const char* getGlErrorString(int error_code) {
    switch (error_code) {
        case GL_NO_ERROR:
            return "No error";
        case GL_INVALID_ENUM:
            return "Invalid enum";
        case GL_INVALID_VALUE:
            return "Invalid value";
        case GL_INVALID_OPERATION:
            return "Invalid operation";
        case GL_STACK_OVERFLOW:
            return "Stack overflow";
        case GL_STACK_UNDERFLOW:
            return "Stack underflow";
        case GL_OUT_OF_MEMORY:
            return "Out of memory";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "Invalid framebuffer operation";
        default:
            return "Unknown error enum";
    }
}
#else
#endif // NDEBUG
bool checkGLError(const char* s) {
#ifndef NDEBUG
    auto i = isGLContextPresent() ? glGetError() : 0;
    if (i != 0) {
        log_lf(Log::L_ERROR, "%s: %s\n", s, getGlErrorString(i));
        dbgassert(0);
        return true;
    }
#endif
    return false;
}
int getStatus(GLuint obj, GLenum type) {
    GLint n = 0;
    if (type == GL_LINK_STATUS) {
        glGetProgramiv(obj, type, &n);
    } else {
        glGetShaderiv(obj, type, &n);
    }
    return n;
}
String getLog(int logtype, GLuint obj) {
    GLint maxLength = 0;
    if (logtype == 0) {
        glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &maxLength);
    } else {

        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &maxLength);
    }
    checkGLError("glGetProgramiv");
    if (maxLength <= 0) {
        return "";
    }
    // The maxLength includes the NULL character
    std::vector<char> infoLog(maxLength);
    if (logtype == 0) {
        glGetShaderInfoLog(obj, maxLength, &maxLength, &infoLog[0]);
        checkGLError("glGetShaderInfoLog");
    } else {
        glGetProgramInfoLog(obj, maxLength, &maxLength, &infoLog[0]);
        checkGLError("glGetProgramInfoLog");
    }
    String s;
    if (!infoLog.empty()) s = infoLog.data();
    return s;
}

int compileShader(int type, const String& src) {
    int iShader = glCreateShader(type);
    if (iShader == 0) {
        log_lf(Log::L_ERROR, "glCreateShader failed\n");
        checkGLError("glCreateShader");
        return 0;
    }
    checkGLError("glCreateShader");
    const auto* szSrc = (const GLchar*) StringAsCStr(src);
    glShaderSource(iShader, 1, &szSrc, nullptr);
    checkGLError("glShaderSourceARB");
    glCompileShader(iShader);
    checkGLError("glCompileShader");
    String log = getLog(0, iShader);
    checkGLError("getLog");
    if (getStatus(iShader, GL_COMPILE_STATUS) != 1) {
        glDeleteShader(iShader);
        checkGLError("getStatus");
        log_lf(Log::L_ERROR, "Compile error: %s\n", StringAsCStr(log));
        return 0;
    }
    if (!log.empty()) {
        log_lf(Log::L_WARN, "Compile log: %s\n", StringAsCStr(log));
    }
    return iShader;
}

/*static*/ void tess2d::uploadVBO(tess2d& tess, DrawVBO& vbo) {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    tess.store(vertices, indices);
    if (vbo.vboVertId == 0) {
        vbo.genBuffers();
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * vertices.size()), vertices.data(), GL_STREAM_DRAW);
    checkGLError("upload vertex data");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(uint32_t) * indices.size()), indices.data(), GL_STREAM_DRAW);
    checkGLError("upload index data");
    vbo.nIndices = static_cast<int32_t>(indices.size());
}
void bindVertexAttributes(std::vector<VertexAttr>& attrs, int32_t fixedStride) {
    int32_t vertStrideBytes = fixedStride;
    if (!vertStrideBytes) {
        for (auto & attr : attrs) {
            vertStrideBytes += (int32_t)(attr.elements * sizeof(float));
        }
    }

    for (int i = (int)attrs.size(); i < 6; i++) {
        glDisableVertexAttribArray(i);
    }
    size_t offset = 0;
    for (auto & attr : attrs) {
        if (attr.bindingPt < 0 || attr.bindingPt >= CtrSize(attrs)) {
            continue;
        }
        glVertexAttribPointer(attr.bindingPt,
                                attr.elements,
                                attr.type,
                                GL_FALSE,
                                vertStrideBytes,
                                (const void*) offset);
        checkGLError("glVertexAttribPointer");
        glEnableVertexAttribArray(attr.bindingPt);
        checkGLError("glEnableVertexAttribArray");
        offset += attr.elements * sizeof(float);
    }
}
void DrawVBO::genBuffers() {
    ++instanceCount;
    dbgassert(!vboVertId);
    dbgassert(!vboIdxId);
    storeGlContext();
    GLuint buffers[2]{};
    glGenBuffers(2, buffers);
    vboVertId = buffers[0];
    vboIdxId  = buffers[1];
}

#define MIN_BUF_SIZE (16384UL)
#define MAX_BUF_SIZE (268435456UL)
void DrawVBO::uploadBuffer(uint32_t bufferType, void* ptr, size_t len) {
    auto buffer  = (bufferType == GL_ARRAY_BUFFER) ? vboVertId : vboIdxId;
    auto& vboSize = (bufferType == GL_ARRAY_BUFFER) ? vboVertSize : vboIdxSize;
    if (len > MAX_BUF_SIZE) {
        log_lf(Log::L_ERROR, "Buffer size exceeds hard limit: %zd > %zd\n", (int64_t)len, (int64_t) MAX_BUF_SIZE);
        dbgassert(len <= MAX_BUF_SIZE);
        return;
    }
    glBindBuffer(bufferType, buffer);
    const GLenum usage = GL_DYNAMIC_DRAW;
    if (vboSize < MIN_BUF_SIZE && len < MIN_BUF_SIZE) {
        log_lf(Log::L_DEBUG, "Allocating %zd bytes for buffer data of len %zd bytes\n", (int64_t)MIN_BUF_SIZE, (int64_t) len);
        glBufferData(bufferType, MIN_BUF_SIZE, nullptr, usage);
        vboSize = MIN_BUF_SIZE;
    }
    //log_printf("Buffer type %s %d, vboSize %d, len %d, ptr %08X\n",
    //           (bufferType == GL_ARRAY_BUFFER) ? "GL_ARRAY_BUFFER" : "GL_ELEMENT_ARRAY_BUFFER",
    //           buffer, vboSize, len, ptr);
    if (vboSize < len) {
        vboSize = len;
        // log_lf(Log::L_DEBUG, "len changed to %zd bytes. orphan buffer\n", (int64_t)len);
        glBufferData(bufferType, len, nullptr, usage);//invalidate previous buffer ('handoff' to driver as explained by some guru)
        glBufferData(bufferType, len, ptr, usage);
    } else {
        glBufferSubData(bufferType, 0, len, ptr);
    }
    checkGLError("DrawVBO::uploadBuffer");
}
void DrawVBO::destroy() {
    if (makeContextCurrent()) {
        if (vaoId) {
            glDeleteVertexArrays(1, &vaoId);
        }
        if (vboIdxId && vboVertId) {
            const GLuint buffers[] = {vboIdxId, vboVertId};
            glDeleteBuffers(2, buffers);
        } else if (vboVertId) {
            glDeleteBuffers(1, &vboVertId);
        } else if (vboIdxId) {
            glDeleteBuffers(1, &vboIdxId);
        }
    }
    glfwWindowHandle = nullptr;
    if (vboIdxId || vboVertId) {
        --instanceCount;
    }

    vaoId       = 0;
    vboVertId   = 0;
    vboIdxId    = 0;
    nIndices    = 0;
    vboVertSize = 0;
    vboIdxSize  = 0;
}

bool isGLContextPresent() {
    return glfwIsContextPresent();
}

void OpenGLResource::storeGlContext() {
    glfwWindowHandle = glfwGetCurrentContext();
    dbgassert(glfwWindowHandle);
}

bool OpenGLResource::makeContextCurrent() {
    if (glfwWindowHandle) {
        glfwMakeContextCurrent(static_cast<GLFWwindow*>(glfwWindowHandle));
        return true;
    }
    return false;
}

DrawVBO::~DrawVBO() {
    destroy();
}

int32_t FrameBuffer::instanceCount = 0;
int32_t DrawVBO::instanceCount = 0;
