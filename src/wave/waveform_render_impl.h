#pragma once
#include "math/vec.h"
#include <vector>
#include <array>
#include "audiocache.h"
#include "waveform_render.h"
#include "gl/gl_path.h"

const int FBO_WIDTH  = 1024 * 2;
const int FBO_HEIGHT = 1024 * 2;
struct NVGcontext;
struct gui_waveform_texture_ref {
    audioclip_texture_t waveform;
    int atlasId      = -1;
    int atlasEntryId = -1;
    bool rendered    = false;
    bool queued      = false;
    int refState     = -1;
};
struct TextureAtlasEntry {
    audioclip_texture_t props;
    ivec2 pos;
    ivec2 size;
    bool inuse = false;
    int id;
    int refCount         = 0;
    uint64_t releaseTime = 0L;
    std::vector<gui_waveform_texture_ref*> ptrs;
};
struct waveform_update_task_t {
    samplesource_t* audio;
    ivec2 pos{ -1, 0 };
    ivec2 size{ 0, 0 };
    int queuedRefCount = 0;
    std::vector<gui_waveform_texture_ref*> queuedptrs;
};
struct TextureAtlas {
    std::vector<waveform_update_task_t> queuedTasks;
    std::vector<TextureAtlasEntry> entries;
    int idx              = -1;
    int glTexture        = -1;
    NVGLUframebuffer* fb = nullptr;
    int nextIdx          = 10;
};

class waveformrender {
    struct Impl;
    Impl* const impl;
    int32_t nextPathIdx = 0;
    std::array<BakeGLPath, 32> bakedPaths;
    std::vector<TextureAtlas> atlases;
    std::vector<waveform_update_task_t> queuedTasks;

public:
    struct render_timings {
        uint64_t tmProcessInputQ = 0;
        uint64_t tmTesselate     = 0;
        uint64_t tmFindSpot      = 0;
        uint64_t tmFindSimiliar  = 0;
        uint64_t tmBakePaths     = 0;
        uint64_t tmDrawGL        = 0;
        uint64_t tmPassed        = 0;
        uint64_t comparisonsA    = 0;
        uint64_t comparisonsB    = 0;
    };
    static waveformrender* getInstance();
    explicit waveformrender(pathrenderer_type_e t);
    ~waveformrender();
    void destroy();
    void init();
    void getRenderedTextures(std::vector<TextureAtlas>& rendered);
    bool findSimiliarWaveform(waveform_update_task_t& waveformQueueEntry);
    void assertWaveformRefIsUnbound(gui_waveform_texture_ref* waveformRef);
    int renderUpdates(NVGcontext* ctxt, float pxRatio);
    int queueUpdate(samplesource_t* audio, gui_waveform_texture_ref* waveformRef);
    void draw(NVGcontext* ctxt, const gui_waveform_texture_ref* waveformRef, ivec2 size);
    bool isValid(const gui_waveform_texture_ref* waveformRef);
    void release(gui_waveform_texture_ref* waveformRef);
    bool findFreeSpot(ivec2 size, int& atlasIdx, ivec2& pos);
    bool canQueueUpdate();
    render_timings getTimings();
};


struct wave_split_layout_t {
    ivec2 pos{ 0 };
    ivec2 size{ 0 };
};
inline bool operator==(const wave_split_layout_t& lhs, const wave_split_layout_t& rhs) {
    return lhs.pos == rhs.pos && lhs.size == rhs.size;
}
inline bool operator!=(const wave_split_layout_t& lhs, const wave_split_layout_t& rhs) {
    return !operator==(lhs, rhs);
}
struct waveform_layout_updated_t {
    audioclip_texture_t waveform;
    wave_split_layout_t layout;
};
