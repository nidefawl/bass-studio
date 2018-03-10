#pragma once
#include <glm/vec2.hpp>
#include "audiocache.h"
#include "../gl/gl_path.h"
struct NVGcontext;
class waveformrender {
	GLPathRenderer renderer;
	BakeGLPath bakedPath;
public:
	static waveformrender* getInstance();
	static void setInstance(std::unique_ptr<waveformrender> host);
	static void destroy();
	void init();
	void render(NVGcontext* ctxt, cachedaudio_t* audio, audiowaveform_t* waveform, float pxRatio);
	void draw(NVGcontext* ctxt, cachedaudio_t* audio, glm::ivec2 size);
	void setPosScale(cachedaudio_t* audio, glm::ivec2 pos, glm::ivec2 startOffset, glm::ivec2 size, double sampleBegin, double sampleBeginOffset, double sampleEnd, double res, double gridZoom);
};
