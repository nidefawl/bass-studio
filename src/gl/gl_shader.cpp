#include "gl_shader.h"
#include "fileio.h"
#include "logging.h"


int prependGLSL(String& s, String src) {
    auto it = s.find("#version");
    if (it != String::npos) {
        it = s.find_first_of('\n', it);
        if (it != String::npos && it < s.length() - 4) {
            it++;
            s.insert(it, src);
            return (int) it;
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
    my_printf("failed loading %s\n", fname);
    return false;
}
bool glshader_srcloader::setStageSrc(int32_t type, const String& fname, const String& strSrc) {
    sources.push_back({type, fname, strSrc});
    return true;
}
bool glshader_srcloader::reload() {
    for (auto& srcEntry: sources) {
        int64_t ret = ReadFileText(srcEntry.filepath, srcEntry.source);
        if (ret <= 0) {
            my_printf("failed loading %s\n", StringAsCStr(srcEntry.filepath));
            return false;
        }
    }
    return true;
}
int buildShaderProgram(const std::vector<glshader_src>& srcList) {
    const int numStages = srcList.size();
    std::vector<GLuint> compiledShaders(numStages);
    for (int i = 0; i < numStages; i++) {
        auto& srcEntry  = srcList[i];
        GLuint glshader = compileShader(srcEntry.stage, srcEntry.source);
        if (!glshader) {
            my_printf("failed compiling %s\n", StringAsCStr(srcEntry.filepath));
            return -1;
        }
        compiledShaders[i] = glshader;
    }
    GLuint newprogram = glCreateProgram();
    for (int i = 0; i < numStages; i++) {
        glAttachShader(newprogram, compiledShaders[i]);
    }
    glLinkProgram(newprogram);
    for (int i = 0; i < numStages; i++) {
        glDeleteShader(compiledShaders[i]);
    }
    String log = getLog(1, newprogram);
    if (getStatus(newprogram, GL_LINK_STATUS) != 1) {
        glDeleteProgram(newprogram);
        checkGLError("getStatus");
        printf("Link error: %s\n", StringAsCStr(log));
        return -1;
    } else if (!log.empty()) {
        printf("Link log: %s\n", StringAsCStr(log));
    }
    checkGLError("linkProgram");
    return newprogram;
}
