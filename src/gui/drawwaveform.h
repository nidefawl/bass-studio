#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;
#include <vector>
#include "audiocache.h"
#include "audiowaveform.h"
#include "../gl/gl_path.h"

struct NVGcontext;
struct TextureEntry {
	int idx = 0;
	int glTexture = 0;
	NVGLUframebuffer* fb = NULL;
	bool inuse = false;
	audioclip_texture_t props;
};
struct gui_waveform_texture_ref {
	audioclip_texture_t waveform;
	int fbId = -1;
	bool rendered = false;
};
class waveformrender {
	GLPathRenderer renderer;
	BakeGLPath bakedPath;
	std::vector<TextureEntry> textures;
	int32_t nextIdx = 0;
public:
	static waveformrender* getInstance();
	static void setInstance(std::unique_ptr<waveformrender> host);
	static void destroy();
	void init();
	void getRenderedTextures(std::vector<TextureEntry>& rendered);
	int render(NVGcontext* ctxt, cachedaudio_t* audio, audioclip_texture_t* waveform, float pxRatio);
	void draw(NVGcontext* ctxt, int fbId, const audioclip_texture_t* waveImage, glm::ivec2 size);
	void release(int fbId);
};
