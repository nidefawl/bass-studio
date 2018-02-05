#define RENDER_TEST 1
#define RENDER_FB_TEST 2
#define BUILD_APP RENDER_TEST
#if BUILD_APP == RENDER_FB_TEST
//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <glad/glad.h>
#ifdef __APPLE__
#	define GLFW_INCLUDE_GLCOREARB
#endif
#include <GLFW/glfw3.h>
#define NANOVG_GL3
#include <nanovg.h>
#include <nanovg_gl.h>
#define NANOVG_GL_IMPLEMENTATION
#include <nanovg_gl_utils.h>
#include <math.h>

void renderPattern(NVGcontext* vg, NVGLUframebuffer* fb, float t, float pxRatio)
{
	int winWidth, winHeight;
	int fboWidth, fboHeight;
	int pw, ph, x, y;
	float s = 20.0f;
	float sr = (cosf(t)+1)*0.5f;
	float r = s * 0.6f * (0.2f + 0.8f * sr);

	if (fb == NULL) return;

	nvgImageSize(vg, fb->image, &fboWidth, &fboHeight);
	winWidth = (int)(fboWidth / pxRatio);
	winHeight = (int)(fboHeight / pxRatio);

	// Draw some stuff to an FBO as a test
	++98 N                     5 mj r	c^hj+
	 -(fb);
	glViewport(0, 0, fboWidth, fboHeight);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

	pw = (int)ceilf(winWidth / s);
	ph = (int)ceilf(winHeight / s);

	nvgBeginPath(vg);
	for (y = 0; y < ph; y++) {
		for (x = 0; x < pw; x++) {
			float cx = (x+0.5f) * s;
			float cy = (y+0.5f) * s;
			nvgCircle(vg, cx,cy, r);
		}
	}
	nvgFillColor(vg, nvgRGBA(220,160,0,200));
	nvgFill(vg);

	nvgEndFrame(vg);
	nvgluBindFramebuffer(NULL);
}

int loadFonts(NVGcontext* vg)
{
	int font;
	font = nvgCreateFont(vg, "sans", "nvgtest/Roboto-Regular.ttf");
	if (font == -1) {
		printf("Could not add font regular.\n");
		return -1;
	}
	font = nvgCreateFont(vg, "sans-bold", "nvgtest/Roboto-Bold.ttf");
	if (font == -1) {
		printf("Could not add font bold.\n");
		return -1;
	}
	return 0;
}

void errorcb(int error, const char* desc)
{
	printf("GLFW error %d: %s\n", error, desc);
}

static void key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	NVG_NOTUSED(scancode);
	NVG_NOTUSED(mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

int main(int argc, char* argv[]) {
	GLFWwindow* window;
	NVGcontext* vg = NULL;
//	GPUtimer gpuTimer;
//	PerfGraph fps, cpuGraph, gpuGraph;
	double prevt = 0, cpuTime = 0;
	NVGLUframebuffer* fb = NULL;
	int winWidth, winHeight;
	int fbWidth, fbHeight;
	float pxRatio;

	if (!glfwInit()) {
		printf("Failed to init GLFW.");
		return -1;
	}

//	initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
//	initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
//	initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");

	glfwSetErrorCallback(errorcb);
#ifndef _WIN32 // don't require this on win32, and works with more cards
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);

#ifdef DEMO_MSAA
	glfwWindowHint(GLFW_SAMPLES, 4);
#endif
	window = glfwCreateWindow(1000, 600, "NanoVG", NULL, NULL);
//	window = glfwCreateWindow(1000, 600, "NanoVG", glfwGetPrimaryMonitor(), NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}

	glfwSetKeyCallback(window, key);

	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		printf("Required OpenGL extensions not present.\nConsider updating graphics drivers");
		return -1;
	}
#ifdef NANOVG_GLEW
	glewExperimental = GL_TRUE;
	if(glewInit() != GLEW_OK) {
		printf("Could not init glew.\n");
		return -1;
	}
	// GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll consume it here.
	glGetError();
#endif

#ifdef DEMO_MSAA
	vg = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_DEBUG);
#else
	vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
#endif
	if (vg == NULL) {
		printf("Could not init nanovg.\n");
		return -1;
	}

	// Create hi-dpi FBO for hi-dpi screens.
	glfwGetWindowSize(window, &winWidth, &winHeight);
	glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
	// Calculate pixel ration for hi-dpi devices.
	pxRatio = (float)fbWidth / (float)winWidth;

	// The image pattern is tiled, set repeat on x and y.
	fb = nvgluCreateFramebuffer(vg, (int)(100*pxRatio), (int)(100*pxRatio), NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY);
	if (fb == NULL) {
		printf("Could not create FBO.\n");
		return -1;
	}
	nvgluDeleteFramebuffer(fb);

	if (loadFonts(vg) == -1) {
		printf("Could not load fonts\n");
		return -1;
	}

	glfwSwapInterval(0);

//	initGPUTimer(&gpuTimer);

	glfwSetTime(0);
	prevt = glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		double mx, my, t, dt;
		float gpuTimes[3];
		int i, n;

		t = glfwGetTime();
		dt = t - prevt;
		prevt = t;

//		startGPUTimer(&gpuTimer);

		glfwGetCursorPos(window, &mx, &my);
		glfwGetWindowSize(window, &winWidth, &winHeight);
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
		// Calculate pixel ration for hi-dpi devices.
		pxRatio = (float)fbWidth / (float)winWidth;

		renderPattern(vg, fb, t, pxRatio);

		// Update and render
		glViewport(0, 0, fbWidth, fbHeight);
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

		// Use the FBO as image pattern.
		if (fb != NULL) {
			NVGpaint img = nvgImagePattern(vg, 0, 0, 100, 100, 0, fb->image, 1.0f);
			nvgSave(vg);

			for (i = 0; i < 20; i++) {
				nvgBeginPath(vg);
				nvgRect(vg, 10 + i*30,10, 10, winHeight-20);
				nvgFillColor(vg, nvgHSLA(i/19.0f, 0.5f, 0.5f, 255));
				nvgFill(vg);
			}

			nvgBeginPath(vg);
			nvgRoundedRect(vg, 140 + sinf(t*1.3f)*100, 140 + cosf(t*1.71244f)*100, 250, 250, 20);
			nvgFillPaint(vg, img);
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(220,160,0,255));
			nvgStrokeWidth(vg, 3.0f);
			nvgStroke(vg);

			nvgRestore(vg);
		}
//
//		renderGraph(vg, 5,5, &fps);
//		renderGraph(vg, 5+200+5,5, &cpuGraph);
//		if (gpuTimer.supported)
//			renderGraph(vg, 5+200+5+200+5,5, &gpuGraph);

		nvgEndFrame(vg);

		// Measure the CPU time taken excluding swap buffers (as the swap may wait for GPU)
		cpuTime = glfwGetTime() - t;

//		updateGraph(&fps, dt);
//		updateGraph(&cpuGraph, cpuTime);
//
//		// We may get multiple results.
//		n = stopGPUTimer(&gpuTimer, gpuTimes, 3);
//		for (i = 0; i < n; i++)
//			updateGraph(&gpuGraph, gpuTimes[i]);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	nvgluDeleteFramebuffer(fb);

	nvgDeleteGL3(vg);

//	printf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
//	printf("          CPU Time: %.2f ms\n", getGraphAverage(&cpuGraph) * 1000.0f);
//	printf("          GPU Time: %.2f ms\n", getGraphAverage(&gpuGraph) * 1000.0f);

	glfwTerminate();
	return 0;
}
#elif BUILD_APP == RENDER_TEST
#include <nanovg.h>
#include <vector>
#include "color_util.h"
#include "seq_math.h"
#include "logging.h"
#include "platform.h"
#include "../wave/dr_wav.h"
#include "../dsp/CalcKaiserWindow.h"

int mainTest(void (*drawFn)(NVGcontext*,int,int,float));
int offsetX = 0;
int offsetY = 0;

void downsample(float sampleRate, int channels, float* samplesIn, int len, float* samplesOut, int downSampleFactor) {
	double ft = 20000;
	double bt = 8000;
	double ripple = 0.001;
	int lenCoeffs;
	double *coeff = calcLPF(sampleRate, ft, ripple, bt, &lenCoeffs);
	int nStep = 1<<downSampleFactor;
	int lenSamplesDown = len>>downSampleFactor;
	int channelSamples = lenSamplesDown/channels;
	for (int j = 0; j < channelSamples; j++) {
		for (int iChannel = 0; iChannel < channels; iChannel++) {
			int pos = j * nStep;
			float out = 0.0;
			for (int y = 0; y < lenCoeffs; y++) {
				out += samplesIn[max(0, pos) * channels + iChannel] * coeff[y];
				pos--;
			}
			samplesOut[j * channels + iChannel] = out;
		}
	}
}
struct Wave {

	std::vector<float> pRenderSamples;
	int nRenderSamples = 0;
	int nRenderChannels = 0;
};
Wave wave1;
Wave wave2;
static void renderWave(NVGcontext* ctx, Wave* wave, float x, float y, int width, int height) {

	if (wave->pRenderSamples.size()) {
		int nSamples = wave->nRenderSamples / wave->nRenderChannels;
		int sampleStep = 1;
		while (width / (float) (nSamples / sampleStep) < 1 / 2.f) {
			sampleStep <<= 1;
		}
		float sampleToPx = width / (float) (nSamples / sampleStep);
		float channelHeight = height / (float) wave->nRenderChannels;
//			float mtrLimit = max(2, 16 - sampleStep * 4);
//			nvgMiterLimit(ctx, 4);
		nvgLineCap(ctx, NVGlineCap::NVG_BUTT);
		nvgLineJoin(ctx, NVGlineCap::NVG_BEVEL);
		int v = 0;
		for (int iChannel = 0; iChannel < wave->nRenderChannels; iChannel++) {
			int p = 0;
			int idx = 0;
			nvgStrokeColor(ctx, rgbaToNvg(0xffffffff));
			nvgStrokeWidth(ctx, 2.0f);
			nvgSave(ctx);
			nvgTranslate(ctx, x, y + channelHeight * iChannel + channelHeight / 2.0f);
			nvgBeginPath(ctx);
			for (int i = 0; i < nSamples; i += sampleStep) {
				float fX = p * sampleToPx;
				if (fX > width)
					break;
				float fMax = wave->pRenderSamples[i * wave->nRenderChannels + iChannel];

				float fY = -fMax * channelHeight / 2.0f;
				if (idx == 0)
					nvgMoveTo(ctx, fX, fY);
				else
					nvgLineTo(ctx, fX, fY);
				p++;
				idx++;
//					if (idx >= 4000) {
//						nvgStroke(ctx);
//						nvgBeginPath(ctx);
//						idx = 0;
//					}
			}
			if (idx > 0) {
				nvgStroke(ctx);
			}
			nvgRestore(ctx);
			v += p;
		}

	}
}
static void render(NVGcontext* ctx, int winW, int winH, float pxratio) {
	nvgBeginFrame(ctx, winW, winH, pxratio);
	/*offsetX = (getTimeMillis()/5)%500;
	nvgSave(ctx);
		nvgTranslate(ctx, offsetX, offsetY);
		nvgSave(ctx);
			nvgTranslate(ctx, 5, 5);
			nvgIntersectScissor(ctx, 0, 0, 50, 50);
			nvgBeginPath(ctx);
			nvgRect(ctx, 0, 0, 50, 50);
			nvgFillColor(ctx, rgbToNvg(0xff00ff));
			nvgFill(ctx);
		nvgRestore(ctx);
		nvgIntersectScissor(ctx, 0, 0, 25, 25);
		nvgSave(ctx);
			nvgTranslate(ctx, 0, 30);
			nvgIntersectScissor(ctx, 0, 0, 50, 50);
			nvgBeginPath(ctx);
			nvgRect(ctx, 0, 0, 50, 50);
			nvgFillColor(ctx, rgbToNvg(0xffffff));
			nvgFill(ctx);
		nvgRestore(ctx);
	nvgRestore(ctx);
*/

//	Wave* rwav = ((getTimeMillis()/1000)%6) < 3 ? &wave1 : &wave2;
//	renderWave(ctx, rwav, winW, winH);
	renderWave(ctx, &wave1, 0, 0, winW, winH/4.0);
	renderWave(ctx, &wave2, 0, winH/4.0, winW, winH/4.0);


	nvgEndFrame(ctx);
}

static int loadWave(Wave& _out, const char* path, int downsampleFactor = 2) {

	drwav wav;
	if (drwav_init_file(&wav, path)) {
		std::vector<float> pSamples(wav.totalSampleCount);
		memset(pSamples.data(), 0, sizeof(float)*pSamples.size());
		size_t nSamples = drwav_read_f32(&wav, wav.totalSampleCount, pSamples.data());

		my_printf("totalSampleCount: %d\n", wav.totalSampleCount);
		my_printf("channels: %d\n", wav.fmt.channels);
		my_printf("sampleRate: %d\n", wav.fmt.sampleRate);
		my_printf("bitsPerSample: %d\n", wav.fmt.bitsPerSample);
		my_printf("samples: %d\n", nSamples);
		// Error opening WAV file.
		std::vector<float> pSamplesDown(wav.totalSampleCount>>downsampleFactor);
		memset(pSamplesDown.data(), 0, sizeof(float)*pSamplesDown.size());
		downsample(wav.fmt.sampleRate, wav.fmt.channels, pSamples.data(), nSamples, pSamplesDown.data(), downsampleFactor);
		_out.pRenderSamples = pSamplesDown;
		_out.nRenderSamples = pSamplesDown.size();
		_out.nRenderChannels = wav.fmt.channels;
		return 0;
	}
	return 1;
}
int main(int argc, char* argv[]) {
//	testTickConversions();
	if (loadWave(wave1, "PHFT_Drum Loop_130_099.wav", 5)) {
		return 1;
	}
	if (loadWave(wave2, "Freeze 1-Omnisphere.wav", 5)) {
		return 1;
	}

	return mainTest(render);
}

#else
int mainHost(int argc, char* argv[]);
int main(int argc, char* argv[]) {
	return mainHost(argc, argv);
}
#endif
