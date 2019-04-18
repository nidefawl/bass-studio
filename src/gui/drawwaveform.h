#pragma once
#include "math/vec.h"
#include <vector>
#include "audiocache.h"
#include "audiowaveform.h"
#include "../gl/gl_path.h"

const int FBO_WIDTH = 1024*2;
const int FBO_HEIGHT = 1024*2;
struct NVGcontext;
struct TextureAtlasEntry {
	audioclip_texture_t props;
	ivec2 pos;
	ivec2 size;
	bool inuse = false;
	int id;
	int refCount = 0;
//	std::vector<void*> ptrs;
};
struct gui_waveform_texture_ref {
	audioclip_texture_t waveform;
	int atlasId = -1;
	int atlasEntryId = -1;
//	ivec2 pos{0,0};
//	ivec2 size{0,0};
	bool rendered = false;
	bool queued = false;
};
struct waveform_update_task_t {
	cachedaudio_t* audio;
	gui_waveform_texture_ref* waveformRef;
	ivec2 pos{0,0};
	ivec2 size{0,0};
	int queuedRefCount = 0;
//	std::vector<void*> queuedptrs;
};
struct TextureAtlas {
	std::vector<waveform_update_task_t> queuedTasks;
	std::vector<TextureAtlasEntry> entries;
	int idx = -1;
	int glTexture = -1;
	NVGLUframebuffer* fb = nullptr;
	int nextIdx = 10;
};
class waveformrender {
	GLPathRenderer renderer;
	BakeGLPath bakedPath;
	std::vector<TextureAtlas> atlases;
	std::vector<waveform_update_task_t> queuedTasks;
	std::vector<audioclip_texture_t> prevRendered;
public:
	static waveformrender* getInstance();
	static void destroy();
	void init();
	void getRenderedTextures(std::vector<TextureAtlas>& rendered);
//	int render(NVGcontext* ctxt, cachedaudio_t* audio, audioclip_texture_t* waveform);
	bool findSimiliarWaveform(waveform_update_task_t& waveformQueueEntry);
	void assertWaveformRefIsUnbound(gui_waveform_texture_ref* waveformRef);
	int renderUpdates(NVGcontext* ctxt, float pxRatio);
	int queueUpdate(cachedaudio_t* audio, gui_waveform_texture_ref* waveformRef);
	void draw(NVGcontext* ctxt, const gui_waveform_texture_ref* waveformRef, ivec2 size);
	void release(gui_waveform_texture_ref* waveformRef);
	bool findFreeSpot(ivec2 size, int& atlasIdx, ivec2& pos);
	bool canQueueUpdate();

};
