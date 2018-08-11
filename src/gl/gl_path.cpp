#define _USE_MATH_DEFINES
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

#include "str_util.h"
#include "fileio.h"
#include "audiocache.h"

#if USE_GLAD_GL_HEADERS
#include <glad/glad.h>
#else
#include "glcorearb.h"
#endif

#include "gl_path.h"
#include "gl_util.h"
#include "gl_attr.h"
#include "gl_vbo.h"
#include "gl_tess2d.h"
#include "hires_timer.h"
#include <algorithm>


using glm::vec2;
using glm::vec4;
using vec2list = std::vector<vec2>;


void buildIndices(int nV, int offset, std::vector<int>& _out) {
	static int quadIdx[] = {0,1,2,1,2,3};
	for (int i = 0; i < nV; i++) {
		for (int j = 0; j < 6; j++)
			_out.push_back(offset + quadIdx[j] + i*4);
	}
}

static inline float fast_atan2(float y, float x){
	static const float c1 = M_PI / 4.0;
	static const float c2 = M_PI * 3.0 / 4.0;
	if (y == 0 && x == 0)
		return 0;
	float abs_y = fabsf(y);
	float angle;
	if (x >= 0)
		angle = c1 - c1 * ((x - abs_y) / (x + abs_y));
	else
		angle = c2 - c1 * ((x + abs_y) / (abs_y - x));
	if (y < 0)
		return -angle;
	return angle;
}

struct attribute_data_t {
	std::vector<vec2> pos;
	std::vector<vec2> seg;
	std::vector<vec2> angles;
	std::vector<vec2> tangent0;
	std::vector<vec2> tangent1;
	std::vector<vec2> tex;
	std::vector<float> index;
};
static inline void reserve(attribute_data_t& t, int n) {
	t.pos.resize(n);
	t.seg.resize(n);
	t.angles.resize(n);
	t.tangent0.resize(n);
	t.tangent1.resize(n);
	t.tex.resize(n);
	t.index.resize(n);
	memset(t.pos.data(), 0, n*sizeof(vec2));
	memset(t.seg.data(), 0, n*sizeof(vec2));
	memset(t.angles.data(), 0, n*sizeof(vec2));
	memset(t.tangent0.data(), 0, n*sizeof(vec2));
	memset(t.tangent1.data(), 0, n*sizeof(vec2));
	memset(t.tex.data(), 0, n*sizeof(vec2));
	memset(t.index.data(), 0, n*sizeof(float));
}
static inline void storeVertex(attribute_data_t& data, int idx, vert& v) {
	v.pos = data.pos[idx];
	v.seg = data.seg[idx];
	v.angles = data.angles[idx];
	v.tangent0 = data.tangent0[idx];
	v.tangent1 = data.tangent1[idx];
	v.tex = data.tex[idx];
	v.index = data.index[idx];
}
float packVertexData(vec2list& verticesIn, std::vector<vert>& outVdata, int index = 0, bool closed = false) {
	vec2list vertices = verticesIn;
	float dist = glm::distance(vertices.front(), vertices.back());
	if (closed && dist > 1e-10) {
		vertices.push_back(verticesIn.front());
	}


	int n = vertices.size();
	attribute_data_t data;
	reserve(data, n);
	data.pos = vertices;
	for (int i = 0; i < n; i++) {
		data.index[i] = index;
	}
	vec2list T(n - 1);
	std::vector<float> N(n - 1);
#define OPT_PATH 1
#if OPT_PATH == 0
	for (int i = 1; i < n; i++) {
		vec2 v = vertices[i] - vertices[i-1];
		T[i - 1] = v;
		//		printf("T[%d] = %f %f\n", i-1, T[i-1].x, T[i-1].y);
	}
	for (int i = 1; i < n; i++) {
		vec2 v = T[i - 1];
		N[i - 1] = sqrtf(v.x*v.x+v.y*v.y);
	}
#endif
#if OPT_PATH == 1
	for (int i = 1; i < n; i++) {
		vec2 v = vertices[i] - vertices[i-1];
		T[i - 1] = v;
		N[i - 1] = glm::length(v);
	}
#endif
	auto itOut = data.tangent0.begin()+1;
	auto itOut2 = data.tangent1.begin();
	for (int i = 0; i < n-1; i++) {
		*itOut++ = T[i];
		*itOut2++ = T[i];
//		vectangent1[i - 1] = T[i - 1];
	}
	if (closed) {
		data.tangent0[0] = T[n - 2];
		data.tangent1[n - 1] = T[0];
	}
	else {
		data.tangent0[0] = T[0];
		data.tangent1[n - 1] = T[n - 2];
	}
	std::vector<float> atans(n);
	for (int i = 0; i < n; i++) {
		float x = data.tangent0[i].x*data.tangent1[i].y - data.tangent0[i].y*data.tangent1[i].x;
		float y = data.tangent0[i].x*data.tangent1[i].x + data.tangent0[i].y*data.tangent1[i].y;
		atans[i] = fast_atan2(x, y);
	}
	for (int i = 0; i < n - 1; i++) {
		data.angles[i].x = atans[i];
		data.angles[i].y = atans[i + 1];
	}
	float fLength = 0;
	for (int i = 0; i < n - 1; i++) {
		fLength += N[i];
		data.seg[i + 1].x = fLength;
		data.seg[i].y = fLength;
	}

	int idxOut = 0;
	std::vector<vert> vdata2;
	//	vdata2.reserve(n*2+4);
	vdata2.resize(n * 2 - 2);
	storeVertex(data, 0, vdata2[idxOut++]);

	for (int i = 1; i < n - 1; i++) {
		vert& p = vdata2[idxOut++];
		storeVertex(data, i, p);
		p.seg = data.seg[i - 1];
		p.angles = data.angles[i - 1];
		vert& p2 = vdata2[idxOut++];
		storeVertex(data, i, p2);
	}
	storeVertex(data, n - 1, vdata2[idxOut]);
	assert(idxOut + 1 == (int)vdata2.size());
	assert(idxOut + 1 == n * 2 - 2);
	vert& p = vdata2[idxOut];
	p.seg = data.seg[n - 2];
	p.angles = data.angles[n - 2];

	assert((int)vdata2.size() % 2 == 0);
	n = vdata2.size();
	for (int i = 0; i < n; i += 2) {
		vdata2[i].tex = {-1, -1};
		vdata2[i + 1].tex = {1, 1};
	}
	outVdata.resize(n * 2);
	for (int i = 0; i < n; ++i) {
		outVdata[i * 2 + 0] = vdata2[i];
		outVdata[i * 2 + 1] = vdata2[i];
	}
	for (int i = 0; i < n; ++i) {
		outVdata[i * 2 + 0].tex.y = -1;
		outVdata[i * 2 + 1].tex.y = 1;
	}

	return fLength;
}

float packVertexData2(vec2list& verticesIn, std::vector<vert>& outVdata, int index = 0, bool closed = false) {

	vec2list vertices = verticesIn;
	float dist = glm::distance(vertices.front(), vertices.back());
	if (closed && dist > 1e-10) {
		vertices.push_back(verticesIn.front());
	}

	int n = vertices.size();
	std::vector<vert> vdata(n);
	memset(vdata.data(), 0, vdata.size()*sizeof(vert));
	vec2list T(n-1);
	std::vector<float> N(n-1);
	int idx = 0;
	for (vec2& v : vertices) {
		vert& vd = vdata[idx++];
		vd.pos = v;
		vd.index = index;
	}
	for (int i = 1; i < n; i++) {
		T[i-1] = vertices[i]-vertices[i-1];
		N[i-1] = glm::length(T[i-1]);
//		printf("T[%d] = %f %f\n", i-1, T[i-1].x, T[i-1].y);
	}
	for (int i = 1; i < n; i++) {
		vdata[i].tangent0 = T[i-1];
		vdata[i-1].tangent1 = T[i-1];
	}
	if (closed) {
		vdata[0].tangent0 = T[n-2];
		vdata[n-1].tangent1 = T[0];
	} else {
		vdata[0].tangent0 = T[0];
		vdata[n-1].tangent1 = T[n-2];
	}
	std::vector<float> atans(n);
	for (int i = 0; i < n; i++) {
		vert& p = vdata[i];
		float x = p.tangent0.x*p.tangent1.y-p.tangent0.y*p.tangent1.x;
		float y = p.tangent0.x*p.tangent1.x+p.tangent0.y*p.tangent1.y;
		atans[i] = fast_atan2(x, y);
	}
	for (int i = 0; i < n-1; i++) {
		vdata[i].angles.x = atans[i];
		vdata[i].angles.y = atans[i+1];
	}
	float fLength = 0;
	for (int i = 0; i < n-1; i++) {
		fLength += N[i];
		vdata[i+1].seg.x = fLength;
		vdata[i].seg.y = fLength;
	}

	std::vector<vert> vdata2;
	vdata2.resize(n*2-2);
	vdata2[0] = vdata[0];
	for (int i = 1, j = 1; i < n-1; i++, j+=2) {
		vert p = vdata[i];
		p.seg = vdata[i-1].seg;
		p.angles = vdata[i-1].angles;
		vdata2[j+0] = p;
		vdata2[j+1] = vdata[i];
	}
	vert p = vdata[n-1];
	p.seg = vdata[n-2].seg;
	p.angles = vdata[n-2].angles;
	assert((int)vdata2.size() == n*2-2);
	vdata2[n*2-3] = p;

	n = vdata2.size();
	for (int i = 0; i < n; i+=2) {
		vdata2[i].tex = vec2(-1);
		vdata2[i+1].tex = vec2(1);
	}
	outVdata.resize(n*2);
	for (int i = 0; i < n; ++i) {
		outVdata[i*2+0] = vdata2[i];
		outVdata[i*2+1] = vdata2[i];
		outVdata[i*2].tex.y = -1;
		outVdata[i*2+1].tex.y = 1;
	}

	return fLength;
}


int GLPathRenderer::init() {
	String srcVertex;
	String srcFragment;
	int64_t ret = ReadFileText("dash-lines-2D.vsh", srcVertex);
	if (ret <= 0) {
		return 1;
	}
	ret = ReadFileText("dash-lines-2D.fsh", srcFragment);
	if (ret <= 0) {
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
	u_dash_atlas = glGetUniformLocation(program, "u_dash_atlas");
	u_model = glGetUniformLocation(program, "u_model");
	u_view = glGetUniformLocation(program, "u_view");
	u_projection = glGetUniformLocation(program, "u_projection");
	u_uniforms = glGetUniformLocation(program, "u_uniforms");
	u_uniforms_shape = glGetUniformLocation(program, "u_uniforms_shape");
//
//	for (int i = 0; i < attributes.size(); i++) {
//		attributes[i].bindingPt = glGetAttribLocation(program, attributes[i].name);
//	}
	for (int i = 0; i < (int)attributes.size(); i++) {
		VertexAttr& attr = attributes[i];
		attr.bindingPt = glGetAttribLocation(program, attr.name);
		checkGLError("glGetAttribLocation");
//		printf("%s %d\n", attributes[i].name, attr.bindingPt);
	}

	checkGLError("glEnableVertexAttribArray");
	glUniform1i(u_uniforms, 0);
	glUniform1i(u_dash_atlas, 1);
    checkGLError("glUniform1i");
    program2dLines = program;
    return 0;
}
void GLPathRenderer::destroy() {
	glDeleteProgram(program2dLines);
}
void GLPathRenderer::bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) {

//	printf("sizeof(Uniforms) %d\n", sizeof(Uniforms));
	std::vector<vert> outVdata;
	vbuf bufFinal;
	std::vector<float> bufUniforms;
	assert((int)sizeof(Uniforms) <= sizeUniforms);
	bufUniforms.resize(paths.size()*sizeUniforms);
	bufFinal.i.clear();
	const int sizeFloatsVert = sizeof(vert)/sizeof(float);
	size_t flBufUniformsPos = 0;
	size_t flBufVertsPos = 0;
	int packed = 0;
	int idx = 0;
	for (vec2list& list : paths) {
		if (list.size() > 1) {
//			timer.reset();
			float len = packVertexData(list, outVdata, idx);
//			int64_t t = timer.getTime();
//			assert(t);
//			my_printf("%d\n", t+2);
			size_t flBufPos = flBufVertsPos*sizeFloatsVert;
			size_t flBakedSize = outVdata.size()*sizeFloatsVert;
			bufFinal.v.resize(flBufPos+flBakedSize);
			memcpy(bufFinal.v.data()+flBufPos, outVdata.data(), flBakedSize*sizeof(float));
//			my_printf("processed %d into %d bytes of data\n", list.size()*sizeof(vec2), flBakedSize*sizeof(float));
			buildIndices(outVdata.size()/4, flBufVertsPos, bufFinal.i);
			Uniforms uniforms = pathOpt;
			uniforms.length = len;
			memcpy(bufUniforms.data()+flBufUniformsPos, &uniforms, sizeof(Uniforms));
			flBufUniformsPos += sizeUniforms;
			flBufVertsPos += outVdata.size();
			packed++;
			outVdata.clear();
		}
		idx++;
	}
	if (flBufVertsPos == 0) {
		bufFinal.v.resize(0);
		bufFinal.i.resize(0);
		bufUniforms.resize(0);
	}
#ifdef NO_OPENGL
	return;
#endif

//
//	for (float f : bufFinal.v) {
//		assert(!std::isnan(f) && !std::isinf(f));
//	}
	bool newBuffer = false;
	DrawVBO& vbo = out.vbo;
	if (vbo.vaoId == 0) {
		glGenVertexArrays(1, &vbo.vaoId);
		glGenBuffers(1, &vbo.vboVertId);
		glGenBuffers(1, &vbo.vboIdxId);
		newBuffer = true;
	}
	glBindVertexArray(vbo.vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*bufFinal.v.size(), bufFinal.v.data(), GL_STREAM_DRAW);
	checkGLError("upload vertex data");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*bufFinal.i.size(), bufFinal.i.data(), GL_STREAM_DRAW);
	checkGLError("upload index data");

	if (newBuffer) {
		bindVertexAttributes(attributes, ATTR_STRIDE);
	}


	int texSize = bufUniforms.size()/4;
    glActiveTexture( GL_TEXTURE0 );
    if (out.uniforms_texture && out.numPaths*countUniforms != texSize) {
    	printf("tex shape changed %d %d\n", out.numPaths*countUniforms, texSize);
    	glDeleteTextures(1, &out.uniforms_texture);
    	out.uniforms_texture = 0;
    }
    if (out.uniforms_texture == 0)
	glGenTextures(1, &out.uniforms_texture);
	glBindTexture( GL_TEXTURE_2D, out.uniforms_texture);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, 1, 0, GL_RGBA, GL_FLOAT, bufUniforms.data());
    checkGLError("glTexImage2D");

	glBindVertexArray(0);

    out.vbo.nIndices = bufFinal.i.size();
    out.numPaths = paths.size();
//    printf("%d %d\n", out.nIndices, out.numPaths);
}

