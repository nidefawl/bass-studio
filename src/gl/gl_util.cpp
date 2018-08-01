#include "glheaders.h"
#include <stdio.h>
#include <vector>
#include "str_util.h"
#include "gl_tess2d.h"
#include "gl_attr.h"
#include "gl_vbo.h"
#include "logging.h"

void debugCB(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar *message,
	const void *userParam) {

	if (strstr(message, "Buffer detailed info") == NULL)
		my_printf("%s\n", message);

}
void enableGlDebugCallback() {

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debugCB, NULL);
	GLuint unusedIds = 0;
	glDebugMessageControl(GL_DONT_CARE,
	    GL_DONT_CARE,
	    GL_DONT_CARE,
	    0,
	    &unusedIds,
	    true);
}
static const char* getGlErrorString(int error_code) {
	static char buf[256];
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
			snprintf(buf, 256, "ErrorCode %d", error_code);
			return buf;
	}
}
bool checkGLError(const char* s) {
	int i = glGetError();
	if (i != 0) {
		printf("%s: %s\n", s, getGlErrorString(i));
		return true;
	}
	return false;
}
int getStatus(int obj, int type) {
	GLint n = 0;
	if (type == GL_LINK_STATUS) {
		glGetProgramiv(obj, type, &n);
	} else {
		glGetShaderiv(obj, type, &n);
	}
	return n;
}
String getLog(int logtype, int obj) {
	GLint maxLength = 0;
	if (logtype == 0) {
		glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &maxLength);
	} else {

		glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &maxLength);
	}
    checkGLError("glGetProgramiv");
	if (maxLength <= 0) {
		printf("GL_INFO_LOG_LENGTH: %d\n", maxLength);
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
	if (infoLog.size()) s = infoLog.data();
    return s;
}
int compileShader(int type, String& src) {
    int iShader = glCreateShader(type);
    checkGLError("glCreateShader");
    const GLchar* szSrc = (const GLchar*)StringAsCStr(src);
    glShaderSource(iShader, 1, &szSrc, NULL);
    checkGLError("glShaderSourceARB");
    glCompileShader(iShader);
    checkGLError("glCompileShader");
    String log = getLog(0, iShader);
    checkGLError("getLog");
    if (getStatus(iShader, GL_COMPILE_STATUS) != 1) {
    	glDeleteShader(iShader);
        checkGLError("getStatus");
    	printf("Compile error: %s\n", StringAsCStr(log));
        return 0;
    } else if (!log.empty()) {
    	printf("Compile log: %s\n", StringAsCStr(log));
    }
    return iShader;
}
/*static*/ void tess2d::uploadVBO(tess2d& tess, DrawVBO& out) {
	std::vector<float> vertices;
	std::vector<int> indices;
	tess.store(vertices, indices);
	if (out.vboVertId == 0) {
		glGenBuffers(1, &out.vboVertId);
		glGenBuffers(1, &out.vboIdxId);
	}
	glBindBuffer(GL_ARRAY_BUFFER, out.vboVertId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertices.size(), vertices.data(), GL_STREAM_DRAW);
	checkGLError("upload vertex data");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.vboIdxId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*indices.size(), indices.data(), GL_STREAM_DRAW);
	checkGLError("upload index data");
    out.nIndices = indices.size();

}

void bindVertexAttributes(std::vector<VertexAttr>& attrs, int fixedStride) {
	int32_t vertStrideBytes = fixedStride;
	if (!vertStrideBytes) {
		for (int i = 0; i < (int)attrs.size(); i++) {
			vertStrideBytes += attrs[i].elements*sizeof(float);
		}
	}

	size_t offset = 0;
	for (int i = 0; i < (int)attrs.size(); i++) {
		VertexAttr& attr = attrs[i];
		glEnableVertexAttribArray(attr.bindingPt);
		checkGLError("glEnableVertexAttribArray");
		glVertexAttribPointer(attr.bindingPt,
				attr.elements,
				attr.type,
				GL_FALSE,
				vertStrideBytes,
				(const void*) offset);
		checkGLError("glVertexAttribPointer");
		offset += attr.elements * sizeof(float);
	}
}
