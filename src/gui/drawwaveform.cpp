#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <math.h>
// glm::translate, glm::rotate, glm::scale, glm::perspective
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "drawwaveform.h"
#include "audiocache.h"
#include "platform.h"
#include "exceptions.h"
#include "logging.h"
#include "color_util.h"
#include "mainctrl.h"


using glm::mat4x4;
using glm::ivec4;
using glm::ivec3;
using glm::ivec2;
using glm::vec3;
using glm::vec2;

const int fboWidth = 1024*2;
const int fboHeight = 1024*2;

bool checkGLError(const char* s);

namespace
{
	std::unique_ptr<waveformrender> g_instance;
}

void waveformrender::destroy() {
	g_instance.reset();
}
waveformrender* waveformrender::getInstance()
{
	return g_instance.get();
}
void waveformrender::setInstance(std::unique_ptr<waveformrender> host)
{
	g_instance = std::move(host);
}
void waveformrender::init() {
	renderer.init();
}
void waveformrender::render(NVGcontext* ctxt, cachedaudio_t* audio, audiowaveform_t* waveform, float pxRatio) {
	if (audio) {
		my_printf("render %s\n", StringAsCStr(audio->path));
	}
	checkGLError("waveformrender::render start");
	if (!waveform->fb) {
		waveform->fb = nvgluCreateFramebuffer(ctxt, fboWidth, fboHeight, 0);
		if (waveform->fb == NULL) {
			throw new appexception("nvgluCreateFramebuffer error");
		}
		checkGLError("waveformrender::render nvgluCreateFramebuffer");
		waveform->image = waveform->fb->image;
		waveform->glTexture = nvgGetGLImageHandle(ctxt, waveform->fb->image);
		waveform->rendered = true;
	}

	nvgluBindFramebuffer(waveform->fb);
	// Draw some stuff to an FBO as a test
	glViewport(0, 0, fboWidth, fboHeight);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	checkGLError("fb prerender");
//
//
//	audiowaveform_t* w = &audio->waveforms.front();
//	w->pos = ivec2(0, 0);
//	w->startOffset = ivec2(0);
//	w->size = ivec2(fbwidth, 200);
//	w->sampleBegin = 0;
//	w->sampleBeginOffset = 0;
//	w->sampleEnd = 1000;
//	w->res = 1000/(float)fbwidth;
//	w->rendered = false;
	std::vector<std::vector<glm::vec2>> tesselatedWaveForms;
	SampleMethod method = waveform->method;
	renderWaveProcessed(audio->sample.get(), 0, 0, waveform, method, tesselatedWaveForms);
	my_printf("%d channels\n", tesselatedWaveForms.size());
	int n = 0;
	for (auto& vec2List : tesselatedWaveForms) {
		my_printf("[%d] size = %d\n", n++, vec2List.size());
	}
	Uniforms bakeOpt;
	bakeOpt.linecaps = vec2(LineCaps::none, LineCaps::none);
	bakeOpt.linejoin = LineJoin::round;
	bakeOpt.miter_limit = 3.0f;
	bakeOpt.color = vec4(vec3(1), 1.0);
	bakeOpt.linewidth = 2.5f;
	renderer.bakePaths(tesselatedWaveForms, bakeOpt, this->bakedPath);


	mat4x4 matView = mat4x4(1.0);
	mat4x4 matModel = mat4x4(1.0);
	mat4x4 matProj;
	matProj = glm::ortho(0.f, (float) fboWidth, (float)fboHeight, 0.f, 1.f, -1.f);
	glUseProgram(renderer.program2dLines);
	glUniformMatrix4fv(renderer.u_projection, 1, GL_FALSE, value_ptr(matProj));
	glUniformMatrix4fv(renderer.u_model, 1, GL_FALSE, value_ptr(matModel));
	glUniformMatrix4fv(renderer.u_view, 1, GL_FALSE, value_ptr(matView));

	glUniform3f ( renderer.u_uniforms_shape, 1, bakedPath.numPaths*renderer.countUniforms, renderer.countUniforms);
	glBindTexture ( GL_TEXTURE_2D, bakedPath.uniforms_texture);
	glBindVertexArray ( bakedPath.vbo.vaoId );
	glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);
	glDrawElements ( GL_TRIANGLES, bakedPath.vbo.nIndices, GL_UNSIGNED_INT, NULL);
//		printf("render .nIndices %d\n", path.nIndices);
	glBindVertexArray(0);

	nvgluBindFramebuffer(NULL);

	checkGLError("fb postrender");
	waveform->renderedSize = waveform->size;
	waveform->rendered = true;

}
void drawImage(NVGcontext* vg, int image, float alpha,
		float sx, float sy, float sw, float sh, // sprite location on texture
		float x, float y, float w, float h) // position and size of the sprite rectangle on screen
{
	float ax, ay;
	int iw,ih;
	NVGpaint img;

	nvgImageSize(vg, image, &iw, &ih);

	// Aspect ration of pixel in x an y dimensions. This allows us to scale
	// the sprite to fill the whole rectangle.
	ax = w / sw;
	ay = h / sh;

	img = nvgImagePattern(vg, x - sx*ax, y - sy*ay, (float)iw*ax, (float)ih*ay,
				0, image, alpha);
	nvgBeginPath(vg);
	nvgRect(vg, x,y, w,h);
	nvgFillPaint(vg, img);
	nvgFill(vg);
}
void waveformrender::setPosScale(cachedaudio_t* audio, ivec2 pos, ivec2 startOffset, ivec2 size, double sampleBegin,  double sampleBeginOffset, double sampleEnd, double res, double gridZoom) {
	if (audio->waveforms.empty()) {
		audiowaveform_t waveform;
		waveform.size = size;
		audio->waveforms.push_back(waveform);
	}
	audiowaveform_t* w = &audio->waveforms.front();
	w->pos = pos;
	w->startOffset = startOffset;
	w->size = size;
	w->sampleBegin = sampleBegin;
	w->sampleBeginOffset = sampleBeginOffset;
	w->sampleEnd = sampleEnd;
	w->res = res;
	double scale = 0.005;
	if (gridZoom < 0.03) {

		w->method = SampleMethod::sample_peakdetect;
		scale = 0.00005;
	} else {

		w->method = SampleMethod::sample_minmax;
	}
	int qu = 1;
	double d = gridZoom;
	while (d > scale && qu < 16) {
		d /= 2.0;
		qu*=2;
	}
	my_printf("zoom: %f, quality: %d\n", gridZoom, qu);
	w->quality = qu;
	w->rendered = false;
}
void waveformrender::draw(NVGcontext* ctxt, cachedaudio_t* audio, ivec2 size) {
	audiowaveform_t* waveImage = NULL;
	for (audiowaveform_t& w : audio->waveforms) {
		waveImage = &w;
		break;
	}
	if (waveImage) {
		drawImage(ctxt, waveImage->image, 1.0f, 0, 0, size.x, size.y, waveImage->startOffset.x, 0, size.x, size.y);
//		drawImage(ctxt, waveImage->image, 1.0f, 0, 0, size.x, size.y, 0, 0, size.x, size.y);

	}
}
