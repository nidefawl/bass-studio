#include "glheaders.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "math/vec.h"
#include "math/mat.h"
#include "fileio.h"
#include "str_util.h"
#include "../gl/gl_util.h"
#include "../gl/gl_attr.h"
#include "../gl/gl_vbo.h"
#include "../gl/gl_tess2d.h"
#include "../gui/drawwaveform.h"
#include "color_util.h"


GLuint program2dTexture;
GLint u_mvp;
GLint u_tex0;

static float wTexPreview = 1024;
static std::vector<VertexAttr> attributes{
	{"in_position", 2, GL_FLOAT},
	{"in_texcoord", 2, GL_FLOAT},
};
DrawVBO vbo;
int loadShader() {
	String srcVertex;
	String srcFragment;
	int64_t ret = ReadFileText("textured.vsh", srcVertex);
	if (ret <= 0) {
		printf("Cannot read file shader.vert\n");
		return 1;
	}
	ret = ReadFileText("textured.fsh", srcFragment);
	if (ret <= 0) {
		printf("Cannot read file shader.frag\n");
		return 1;
	}

	GLuint vertex_shader, fragment_shader;
	vertex_shader = compileShader(GL_VERTEX_SHADER, srcVertex);
	if (!vertex_shader) {
		return 1;
	}
	fragment_shader = compileShader(GL_FRAGMENT_SHADER, srcFragment);
	if (!fragment_shader) {
		return 1;
	}
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
    glBindFragDataLocation(program, 0, "out_Color");
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	String log = getLog(1, program);
	if (getStatus(program, GL_LINK_STATUS) != 1) {
		checkGLError("getStatus");
		printf("Link error: %s\n", StringAsCStr(log));
		return 1;
	} else if (!log.empty()) {
		printf("Link log: %s\n", StringAsCStr(log));
	}
	checkGLError("linkProgram");
	glUseProgram(program);
	u_mvp = glGetUniformLocation(program, "mvp");
	u_tex0 = glGetUniformLocation(program, "tex0");
	for (int i = 0; i < (int)attributes.size(); i++) {
		attributes[i].bindingPt = glGetAttribLocation(program, attributes[i].name);
	}
	checkGLError("glGetAttribLocation");
	program2dTexture = program;
    return 0;
}
int initDebugWindow() {
	glBindVertexArray(0);
	int ret = loadShader();
	if (ret)
		return ret;
	tess2d tess;
	tess.add(wTexPreview, 0.0f, 1, 1);
	tess.add(0.0f, 0.0f, 0, 1);
	tess.add(0.0f, wTexPreview, 0, 0);
	tess.add(wTexPreview, wTexPreview, 1, 0);
	glBindVertexArray(0);
	checkGLError("uploadVBO");
	glGenVertexArrays(1, &vbo.vaoId);
	glBindVertexArray(vbo.vaoId);
	tess2d::uploadVBO(tess, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
	bindVertexAttributes(attributes);
	checkGLError("bindVertexAttributes");
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
    return 0;
}

void drawDebugWindow(NVGcontext* ctx, int winW, int winH, float pxratio) {

	std::vector<TextureAtlas> rendered;
	auto instance = waveformrender::getInstance();
	if (!instance) {
		return;
	}
	instance->getRenderedTextures(rendered);
//	my_printf("nrendered: %d\n", rendered.size());
//	auto* ptr = audiocache::getInstance()->get(0);
//	if (ptr) {
//		for (audiowaveform_t& waveform : ptr->waveforms) {
//			if (waveform.rendered) {
//				n = waveform.glTexture;
//				break;
//			}
//		}
//	}
	float x = 0; float y = 0;
	int nrendered = 0;
	glBindVertexArray(vbo.vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
	glActiveTexture( GL_TEXTURE0 );
	glUseProgram(program2dTexture);
	glUniform1i(u_tex0, 0);
	for (TextureAtlas& e : rendered) {
		int n = e.glTexture;
		if (n > 0 && e.entries.size()) {
			glm::mat4 matProj = glm::ortho(0.f, (float) winW, (float) winH, 0.f, 1.f, -1.f);
			glm::mat4 mvp = matProj * glm::translate(glm::mat4(1.0), glm::vec3(x, y, 0));
//			glDisable(GL_DEPTH_TEST);
			glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
			glBindTexture(GL_TEXTURE_2D, n);
			glDrawElements( GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, NULL);
//			glEnable(GL_DEPTH_TEST);
			nrendered++;
			x += wTexPreview+8;
			if (x >= 1024) {
				x = 0;
				y+= wTexPreview+8;
			}
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glStencilMask(~0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	nvgBeginFrame(ctx, winW, winH, pxratio);

//	nvgBeginFrame(ctx, winW, winH, pxratio);
//	nvgBeginPath(ctx);
//	nvgRect(ctx, 0, 0, winW, winH);
//	nvgFillColor(ctx, rgbToNvg(0x33ff33));
//	nvgFill(ctx);
	 x = 0;
	 y = 0;
	nrendered = 1;
	float scale = (float)wTexPreview / (float) FBO_WIDTH;
	for (TextureAtlas& _atlas : rendered) {
		int n = _atlas.glTexture;
		if (n > 0 && _atlas.entries.size()) {
			for (TextureAtlasEntry& _entry : _atlas.entries) {
				nvgBeginPath(ctx);
				nvgRect(ctx, x+_entry.pos.x*scale, y+_entry.pos.y*scale, _entry.size.x*scale, _entry.size.y*scale);
				nvgStrokeColor(ctx, rgbToNvg(col(nrendered)));
				nvgStrokeWidth(ctx, 2.0f);
				nvgStroke(ctx);
			}
			x += wTexPreview+8;
			if (x >= 1024) {
				x = 0;
				y+= wTexPreview+8;
			}
			nrendered++;
		}
	}
	nvgEndFrame(ctx);
}
