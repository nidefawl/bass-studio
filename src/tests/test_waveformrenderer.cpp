#include "glheaders.h"
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <unistd.h>


#include "TestBase.hpp"
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "audiocache.h"
#include "audiowaveform.h"
#include "gui/drawwaveform.h"
#include "gl/gl_path.h"

#include "tls.h"
#include "logging.h"


String excDescription;
GLPathRenderer renderer;
int benchmark_waverender(audiofile_t* sample, BakeGLPath& bakedPath) {
	audiosample_t* audioSample = sample->sample.get();
	ivec2 size = {700, 120};

	double zoom = 0.5;

	double lenSamples = audioSample->nSamples;
	double samplesPerPx = lenSamples/size.x;
	audioclip_texture_t w;
	w.quality=4;
	if (samplesPerPx >= 256) {
		w.quality *= 2;
//		w.scale = 2;
	}
	constexpr float MAX_RES = 512;
	w.scaleX = 1.0f;
	if (samplesPerPx > MAX_RES) {
		w.scaleX = MAX_RES/samplesPerPx;
		samplesPerPx = MAX_RES;
	}
	w.pos = {0,0};
//	w.startOffset = {0,0};
	w.size = size;
	assert(w.size.x > 0);
	w.sampleBegin = 0;
	w.sampleBeginOffset = 0;
	w.sampleEnd = lenSamples;
	w.samplesPerPx = samplesPerPx;
	w.linewidth = 1.50f+math::min(0.75, math::max(0.0, zoom*32.0));
	w.method = SampleMethod::sample_straight;
	w.audioId = sample->id;




	SampleMethod method = w.method;
	std::vector<std::vector<vec2>> tesselatedWaveForms;
	tesselateWaveform(audioSample, 0, 0, &w, method, tesselatedWaveForms);
	Uniforms bakeOpt;
	bakeOpt.linecaps = vec2(LineCaps::none, LineCaps::none);
	bakeOpt.linejoin = w.linewidth > 1.75 ? LineJoin::round : LineJoin::miter;
	bakeOpt.miter_limit = 1.8f;
	bakeOpt.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//	uint32_t color = colorPalette[(nextIdx++%(COLOR_PALETTE_COLS-2))*COLOR_PALETTE_ROWS+3];
	//	bakeOpt.color = int32vec4(color);
	//	bakeOpt.color.w = 1.0;

	bakeOpt.linewidth = w.linewidth;
	bakeOpt.antialias = 1.0f;
	bakeOpt.scale = 1;
	renderer.bakePaths(tesselatedWaveForms, bakeOpt, bakedPath);


	return 0;
}
void tesselate(audiofile_t* sample, std::vector<std::vector<vec2>>& out) {
	audiosample_t* audioSample = sample->sample.get();
	ivec2 size = {700, 120};

	double zoom = 0.5;

	double lenSamples = audioSample->nSamples;
	double samplesPerPx = lenSamples/size.x;
	audioclip_texture_t w;
	w.quality=4;
	if (samplesPerPx >= 256) {
		w.quality *= 2;
//		w.scale = 2;
	}
	constexpr float MAX_RES = 512;
	w.scaleX = 1.0f;
	if (samplesPerPx > MAX_RES) {
		w.scaleX = MAX_RES/samplesPerPx;
		samplesPerPx = MAX_RES;
	}
	w.pos = {0,0};
//	w.startOffset = {0,0};
	w.size = size;
	assert(w.size.x > 0);
	w.sampleBegin = 0;
	w.sampleBeginOffset = 0;
	w.sampleEnd = lenSamples;
	w.samplesPerPx = samplesPerPx;
	w.linewidth = 1.50f+math::min(0.75, math::max(0.0, zoom*32.0));
	w.method = SampleMethod::sample_straight;
	w.audioId = sample->id;




	SampleMethod method = w.method;
	tesselateWaveform(audioSample, 0, 0, &w, method, out);
//	Uniforms bakeOpt;
//	bakeOpt.linecaps = vec2(LineCaps::none, LineCaps::none);
//	bakeOpt.linejoin = w.linewidth > 1.75 ? LineJoin::round : LineJoin::miter;
//	bakeOpt.miter_limit = 1.8f;
//	bakeOpt.color = vec4(vec3(1), 1.0);
//	//	uint32_t color = colorPalette[(nextIdx++%(COLOR_PALETTE_COLS-2))*COLOR_PALETTE_ROWS+3];
//	//	bakeOpt.color = int32vec4(color);
//	//	bakeOpt.color.w = 1.0;
//
//	bakeOpt.linewidth = w.linewidth;
//	bakeOpt.antialias = 1.0f;
//	bakeOpt.scale = w.scale;

}
float packVertexData(vec2list& verticesIn, std::vector<vert>& outVdata, int index = 0, bool closed = false);
float packVertexData2(vec2list& verticesIn, std::vector<vert>& outVdata, int index = 0, bool closed = false);
void packVertexDataTest(vec2list& verticesIn, std::vector<vert>& outVdata, int index = 0, bool closed = false) {
	outVdata.resize(verticesIn.size()*4);

//	//use output iterator
//	int n = 0;
//	auto* ptr = verticesIn.data();
//	while(((ptrdiff_t)ptr&(1<<n)) == 0) {
//		n++;
//	}
//	n++;
//	my_printf("alignment: %d\n", 1<<n);
//	auto it2 = outVdata.begin();
//	auto it = verticesIn.begin();
//	auto itend = verticesIn.end();
//	for (; it != itend;) {
//		it2++->pos = *it;
//		it2++->pos = *it;
//		it2++->pos = *it;
//		it2++->pos = *it++;
//	}
//	auto it2 = outVdata.begin();
//	for (auto& v : verticesIn) {
//		it2++->pos = v;
//		it2++->pos = v;
//		it2++->pos = v;
//		it2++->pos = v;
//	}
	int idx = 0;
	for (auto& v : verticesIn) {
		outVdata[idx*4+0].pos = v;
		outVdata[idx*4+1].pos = v;
		outVdata[idx*4+2].pos = v;
		outVdata[idx*4+3].pos = v;
		idx++;
	}
}
template <typename F>
void benchmark_packdata(F f, std::vector<std::vector<vec2>>& tesselatedWaveForms) {

	std::vector<vert> outVdata;
	int len = tesselatedWaveForms.size();
	for (int i = 0; i < len; i++) {
		f(tesselatedWaveForms[i], outVdata, i, false);
	}
//	f(tesselatedWaveForms[0], outVdata, 0, false);
}
int main(int argc, char* argv[]) {
	{
		hires_timer_t t;
		t.reset();
		usleep(50000);
		my_printf("usleep(50000) %lu\n", t.getTime());
	}
	audiocache cache(44100);
	daw_tls::tlsinstance& tls = daw_tls::getTls();
	tls.audioCache = &cache;
	audiofile_t* sample = audiocache::getInstance()->loadFile("PHFT_Drum Loop_130_099.wav");
	if (!sample) {
		puts("Failed loading sample");
		return 1;
	}
	ALEPH_TEST_BEGIN("testThreadWorkerTasks");
#define NLOOPS 2111
	hires_timer_t t;
	std::vector<std::vector<vec2>> tesselatedWaveForms;
	t.reset();
	tesselate(sample, tesselatedWaveForms);
	my_printf("tesselate %luus\n", t.getTime());
	{
		t.reset();
		for (int i = 0; i < NLOOPS; i++) {
			benchmark_packdata(packVertexData, tesselatedWaveForms);
		}
		my_printf("packVertexData per loop %luus\n", t.getTime()/NLOOPS);
	}
	{
		t.reset();
		for (int i = 0; i < NLOOPS; i++) {
			benchmark_packdata(packVertexData2, tesselatedWaveForms);
		}
		my_printf("packVertexData2 per loop %luus\n", t.getTime()/NLOOPS);
	}
	{

		t.reset();
		for (int i = 0; i < NLOOPS; i++) {
			benchmark_packdata(packVertexDataTest, tesselatedWaveForms);
		}
		my_printf(u8"packVertexDataTest per loop %luus\n", t.getTime()/NLOOPS);
	}
	ALEPH_TEST_END();
	return 0;
}
