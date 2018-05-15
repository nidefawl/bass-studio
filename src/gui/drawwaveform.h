#pragma once
#include <glm/vec2.hpp>
#include <vector>
#include "audiocache.h"
#include "../gl/gl_path.h"
struct NVGcontext;
struct TextureEntry {
	int idx = 0;
	int glTexture = 0;
	NVGLUframebuffer* fb = NULL;
	bool inuse = false;
	audioclip_texture_t props;
};
class waveformrender {
	GLPathRenderer renderer;
	BakeGLPath bakedPath;
	std::vector<TextureEntry> textures;
public:
	static waveformrender* getInstance();
	static void setInstance(std::unique_ptr<waveformrender> host);
	static void destroy();
	void init();
	void getRenderedTextures(std::vector<TextureEntry>& rendered);
	int render(NVGcontext* ctxt, cachedaudio_t* audio, audioclip_texture_t* waveform, float pxRatio);
	void draw(NVGcontext* ctxt, int fbId, audioclip_texture_t* waveImage, glm::ivec2 size);
	void release(int fbId);
};
