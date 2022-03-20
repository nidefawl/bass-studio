#include "gl_shader.h"
#include "fileio.h"
#include "logging.h"


int32_t prependGLSL(String& s, const String& src) {
    auto it = s.find("#version");
    if (it != String::npos) {
        it = s.find_first_of('\n', it);
        if (it != String::npos && it < s.length() - 4) {
            it++;
            s.insert(it, src);
            return static_cast<int>(it);
        }
    }
    return -1;
}
bool glshader_srcloader::addStageSrc(int32_t type, const char* fname) {
    String strSrc;
    int64_t ret = ReadFileText(fname, strSrc);
    if (ret > 0) {
        sources.push_back({type, fname, strSrc});
        return true;
    }
    log_printf("failed loading %s\n", fname);
    return false;
}
bool glshader_srcloader::setStageSrc(int32_t type, const String& fname, const String& strSrc) {
    sources.push_back({type, fname, strSrc});
    return true;
}
bool glshader_srcloader::reload() {
    for (auto& srcEntry : sources) {
        int64_t ret = ReadFileText(srcEntry.filepath, srcEntry.source);
        if (ret <= 0) {
            log_printf("failed loading %s\n", StringAsCStr(srcEntry.filepath));
            return false;
        }
    }
    return true;
}
int32_t buildShaderProgram(const std::vector<glshader_src>& srcList) {
    std::vector<GLuint> compiledShaders;
    compiledShaders.reserve(srcList.size());
    for (auto& srcEntry : srcList) {
        GLuint glshader = compileShader(srcEntry.stage, srcEntry.source);
        if (!glshader) {
            log_printf("failed compiling %s\n", StringAsCStr(srcEntry.filepath));
            return -1;
        }
        compiledShaders.push_back(glshader);
    }
    GLuint newprogram = glCreateProgram();
    for (auto shader : compiledShaders) {
        glAttachShader(newprogram, shader);
    }
    glLinkProgram(newprogram);
    for (auto shader : compiledShaders) {
        glDeleteShader(shader);
    }
    String log = getLog(1, newprogram);
    if (getStatus(newprogram, GL_LINK_STATUS) != 1) {
        glDeleteProgram(newprogram);
        checkGLError("getStatus");
        printf("Link error: %s\n", StringAsCStr(log));
        return -1;
    }
    if (!log.empty()) {
        printf("Link log: %s\n", StringAsCStr(log));
    }
    checkGLError("linkProgram");
    return static_cast<int32_t>(newprogram);
}
