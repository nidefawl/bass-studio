#include "assert_dbg.h"
#include "glheaders.h"
#include "seq_util.hpp"
#include "waveform_render_impl.hpp"
#include "gl/gl_util.hpp"
#include "gl/gl_pathrenderer.hpp"

#include <nanovg.h>
#include <nanovg_gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <nanovg_min.h>
#include <utility>
#include <vector>

#include "math/seq_math.hpp"
#include "math/mat.hpp"
#include "platform.hpp"
#include "exceptions.hpp"
#include "host/audiocache/audiocache.hpp"
#include "waveform_render.hpp"
#include "waveform_generate.hpp"
#include "logging.hpp"
#include "appconfig.hpp"
#include "tls.hpp"
#include "gui/gui.hpp"


struct waveformrender::Impl {
    hires_timer_t timer;
    hires_timer_t timer2;
    hires_timer_t timer3;
    waveformrender::render_timings renderTimings;
    IPathRenderer* renderer = nullptr;
    explicit Impl(pathrenderer_type_e t) {
        switch (t) {
            default:
            case pathrenderer_type_e::DASHLINES:
                renderer = new GLPathRendererDashLines();
                break;
            case pathrenderer_type_e::POLYLINE2D:
                renderer = new GLPathRendererPolyline2d();
                break;
            case pathrenderer_type_e::PAR_BASIC:
                renderer = new GLPathRendererParBasic();
                break;
            case pathrenderer_type_e::PAR_ADVANCED:
                renderer = new GLPathRendererParBasic();
                break;
        }
    }
    ~Impl() {
        delete renderer;
    }
};
waveformrender::~waveformrender() {
    delete impl;
}
waveformrender::waveformrender(pathrenderer_type_e t) : impl(new waveformrender::Impl(t)) {
}
void waveformrender::init() {
    impl->renderer->init();
}
void waveformrender::destroy() {
    impl->renderer->destroy();
    for (auto& atlas : atlases) {
        if (atlas.fb) {
            nvgluDeleteFramebuffer(atlas.fb);
        }
    }
    for (BakeGLPath& path : bakedPaths) {
        if (path.uniforms_texture != 0) {
            glDeleteTextures(1, &path.uniforms_texture);
        }
        path.uniforms_texture = 0;
        path.vbo.destroy();
    }
    atlases.clear();
    queuedTasks.clear();
}
bool waveformrender::isValid() const {
    return impl->renderer->isValid();
}

void waveformrender::getRenderedTextures(std::vector<TextureAtlas>& rendered) {
    for (auto& atlas : atlases) {
        if (!atlas.entries.empty()) {
            rendered.push_back(atlas);
        }
    }
}
template<typename T>
bool isIn(const std::vector<T*>& ptrs, const void* ptr)  {
    auto it2 = std::find_if(ptrs.cbegin(), ptrs.cend(), [ptr](const void* entry) {
        return entry == ptr;
    });
    return it2 != ptrs.cend();
}
void waveformrender::release(gui_waveform_texture_ref* waveformRef) {
    this->releaseQueued(waveformRef);
    this->releaseRendered(waveformRef);
    waveformRef->atlasId = -1;
    waveformRef->atlasEntryId = -1;
    waveformRef->queued = false;
    waveformRef->rendered = false;

}
static void removeTaskFromQueue(std::vector<waveform_update_task_t>& vecQTasks, gui_waveform_texture_ref* waveformRef) {
    // remove waveform from atlas update queue
    for (auto itTasks = vecQTasks.begin(); itTasks != vecQTasks.end();) {
        auto& taskEntry = *itTasks;
        for (auto it = taskEntry.queuedptrs.begin(); it != taskEntry.queuedptrs.end();) {
            if (*it == waveformRef) {
                it = taskEntry.queuedptrs.erase(it);
                dbgassert(taskEntry.queuedRefCount > 0);
                taskEntry.queuedRefCount--;
            } else {
                ++it;
            }
        }
        if (taskEntry.queuedRefCount <= 0) {
            //log_lf(Log::L_DEBUG, "Atlas QTask refcount <= 0. erasing.\n");
            itTasks = vecQTasks.erase(itTasks);
        } else {
            ++itTasks;
        }
    }
}
void waveformrender::releaseQueued(gui_waveform_texture_ref* waveformRef) {
    if (waveformRef->queued) {
        if (waveformRef->atlasId > -1) {
            dbgassert(waveformRef->atlasId >= 0 && waveformRef->atlasId < (int) atlases.size());
            auto& atlas = this->atlases[waveformRef->atlasId];
            // check if atlas waveform is mapped to was initialized
            dbgassert(atlas.idx > -1);
            // remove waveform from atlas update queue
            removeTaskFromQueue(atlas.queuedTasks, waveformRef);
        }
        removeTaskFromQueue(queuedTasks, waveformRef);
    }
    if (!waveformRef->rendered) {
        waveformRef->atlasId      = -1;
        waveformRef->atlasEntryId = -1;
    }
    waveformRef->queued       = false;
}
void waveformrender::releaseRendered(gui_waveform_texture_ref* waveformRef) {
    if (waveformRef->rendered && !this->atlases.empty()) {
        if (!assert_expr((waveformRef->atlasId >= 0 && waveformRef->atlasId < (int) atlases.size()))) {
            log_lf(Log::L_ERROR, "waveformRef->atlasId out of bounds\n");
            return;
        }
        
        auto& atlas = this->atlases[waveformRef->atlasId];
        // check if atlas waveform is mapped to was initialized
        dbgassert(atlas.idx > -1);
        // check if waveformRef looks plausible
        dbgassert(waveformRef->atlasEntryId >= 10);
        // remove waveform from atlas rendered entries
        auto& vec = atlas.entries;
        auto it   = std::find_if(vec.begin(), vec.end(), [id = waveformRef->atlasEntryId](const TextureAtlasEntry& entry) {
            return entry.id == id;
          });
        dbgassert(it != vec.end());
        TextureAtlasEntry& entry = *it;
        dbgassert(isIn(entry.ptrs, waveformRef));
        auto it2 = std::find(entry.ptrs.begin(), entry.ptrs.end(), waveformRef);
        if (assert_expr(it2 != entry.ptrs.end())) {
            entry.ptrs.erase(it2);
        }
        dbgassert(std::find(entry.ptrs.begin(), entry.ptrs.end(), waveformRef) == entry.ptrs.end());
        entry.refCount--;
        if (entry.refCount <= 0) {
            entry.tmRelease = getTimeMillis();
            //log_lf(Log::L_DEBUG, "AtlasEntry refcount <= 0. erasing.\n");
            //vec.erase(it);
        }
    }
    if (!waveformRef->queued) {
        waveformRef->atlasId      = -1;
        waveformRef->atlasEntryId = -1;
    }
    waveformRef->rendered     = false;
}
bool waveformrender::canQueueUpdate() {
    return queuedTasks.size() < 1000;
}
int waveformrender::queueUpdate(samplesource_t* audio, gui_waveform_texture_ref* waveformRef) {
    if (waveformRef->queued) {
        return 0;
    }

    const bool disableWaveformUpdates = daw_tls::getTls().runtime->disableWaveformUpdates;
    if (disableWaveformUpdates)
        return 0;
    //if (!canQueueUpdate()) {
    //    return 0;
    //}
    //assertWaveformRefIsUnbound(waveformRef);
    dbgassert(waveformRef->waveform.size.x > 0 && waveformRef->waveform.size.y > 0);
    waveform_update_task_t waveform_update_task{};
    waveform_update_task.audio          = audio;
    waveform_update_task.pos            = ivec2(-1, 0);
    waveform_update_task.size           = waveformRef->waveform.size;
    waveform_update_task.queuedRefCount = 1;
    waveform_update_task.queuedptrs.push_back(waveformRef);
    dbgassert(std::find_if(queuedTasks.begin(), queuedTasks.end(), [waveformRef](const waveform_update_task_t& t) {
                  return isIn(t.queuedptrs, waveformRef);
              }) == queuedTasks.end());
    queuedTasks.push_back(waveform_update_task);
    waveformRef->queued = true;
    return 1;
}
bool collides(const ivec2& pos1, const ivec2& size1, const ivec2& pos2, const ivec2& size2, ivec2& offset) {
    ivec2 lhsRightBottom = pos1 + size1;
    ivec2 rhsRightBottom = pos2 + size2;
    if (pos1.x >= rhsRightBottom.x) {
        return false;
    }
    if (lhsRightBottom.x <= pos2.x) {
        return false;
    }
    if (pos1.y >= rhsRightBottom.y) {
        return false;
    }
    if (lhsRightBottom.y <= pos2.y) {
        return false;
    }
    if (pos1.x >= pos2.x && pos1.x < rhsRightBottom.x) {
        offset.x += rhsRightBottom.x - pos1.x;
    } else if (pos1.y >= pos2.y && pos1.y < rhsRightBottom.y) {
        offset.y += rhsRightBottom.y - pos1.y;
    }
    return true;
}
bool anyCollision(TextureAtlas& _atlas, const ivec2 pos1, const ivec2 size1, ivec2& colliderBottomRight) {
    // check against list of existing textures
    for (auto& entry : _atlas.entries) {
        ivec2 offset(0, 0);
        if (collides(pos1, size1, entry.pos, entry.size, offset)) {
            colliderBottomRight.x = entry.pos.x + entry.size.x;
            colliderBottomRight.y = entry.pos.y + entry.size.y;
            return true;
        }
    }
    // check against list of queued textures
    for (auto& entry : _atlas.queuedTasks) {
        ivec2 offset(0, 0);
        if (entry.pos.x >= 0 && collides(pos1, size1, entry.pos, entry.size, offset)) {
            colliderBottomRight.x = entry.pos.x + entry.size.x;
            colliderBottomRight.y = entry.pos.y + entry.size.y;
            return true;
        }
    }
    return false;
}
void waveformrender::findFreeSpot(const ivec2 size, int& atlasIdx, ivec2& pos) {
    for (TextureAtlas& _atlas : atlases) {
        ivec2 tmpPos = { 0, 0 };
        while (true) {
            ivec2 colliderBottomRight(0);
            if (!anyCollision(_atlas, tmpPos, size, colliderBottomRight)) {
                //found a free spot
                pos      = tmpPos;
                atlasIdx = _atlas.idx;
                return;
            } else {
                tmpPos.x = colliderBottomRight.x + 4;
            }
            //advance by 16 pixels in x direction
            //tmpPos.x += 16;
            if (tmpPos.x + size.x >= FBO_WIDTH) {
                //reached end of texture : reset to x 0 and advance y
                tmpPos.x = 0;
                tmpPos.y = colliderBottomRight.y + 4;
                //tmpPos.y += 16;
            }
            if (tmpPos.y + size.y >= FBO_HEIGHT) {// atlas has no space for this textures
                break;
            }
        }
    }
    // create new texture atlas
    TextureAtlas e;
    e.idx = (int)atlases.size();
    atlases.push_back(e);

    // put the texture in the top left corner
    pos      = { 0, 0 };
    atlasIdx = e.idx;
    return;
}

void waveformrender::assertWaveformRefIsUnbound(gui_waveform_texture_ref* waveformRef) {
#ifndef NDEBUG
    for (TextureAtlas& _atlas : atlases) {
        for (auto& entry : _atlas.entries) {
            dbgassert(!isIn(entry.ptrs, waveformRef));
        }
    }
    for (auto& updateTask : queuedTasks) {
        //if (updateTask.waveformRef == waveformRef) {
        //    dbgassert(0);
        //}
        dbgassert(!isIn(updateTask.queuedptrs, waveformRef));
    }
#endif
}

inline bool isAlmostEqualWaveform(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    if (lhs.audioId == rhs.audioId &&
        lhs.sampleVersion == rhs.sampleVersion &&
        lhs.quality == rhs.quality &&
        lhs.method == rhs.method &&
        lhs.size == rhs.size &&
        (lhs.sampleBeginOffset - lhs.sampleBegin) == (rhs.sampleBeginOffset - rhs.sampleBegin) &&
        (lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
        lhs.samplesPerPx == rhs.samplesPerPx &&
        lhs.scaleX == rhs.scaleX) {
        if (lhs.fades != rhs.fades)
            return false;
        if (lhs.loopPos != rhs.loopPos) {
            return false;
        }
        if (lhs.samplesClipped || rhs.samplesClipped)
            return lhs.scaleX == rhs.scaleX && lhs.scaleY == rhs.scaleY && lhs.size == rhs.size && lhs.samplesPerPx == rhs.samplesPerPx;
        vec2 sd    = vec2(math::absvec2(lhs.size - rhs.size));
        vec2 limit = vec2(lhs.size) / 4.0f;
        return sd.x < limit.x && sd.y < limit.y;
    }
    return false;
}
bool waveformrender::findSimiliarWaveform(waveform_update_task_t& waveformQueueEntry) {
    gui_waveform_texture_ref* waveformRef = waveformQueueEntry.queuedptrs[0];
    waveformRef->refState                 = 0;
    for (TextureAtlas& _atlas : atlases) {
        for (auto& entry : _atlas.entries) {
            impl->renderTimings.comparisonsA++;
            if (isAlmostEqualWaveform(entry.props, waveformRef->waveform)) {
                waveformRef->atlasId      = _atlas.idx;
                waveformRef->atlasEntryId = entry.id;
                waveformRef->queued       = false;
                waveformRef->rendered     = true;
                waveformRef->refState     = 1;
                entry.refCount++;
                if (!isIn(entry.ptrs, waveformRef)) {
                    entry.ptrs.push_back(waveformRef);
                } else {
                    dbgassert(waveformRef->rendered);
                }
                return true;
            }
        }
        for (auto& entry : _atlas.queuedTasks) {
            impl->renderTimings.comparisonsB++;
            if (isAlmostEqualWaveform(entry.queuedptrs[0]->waveform, waveformRef->waveform)) {
                waveformRef->atlasId      = _atlas.idx;
                waveformRef->atlasEntryId = entry.queuedptrs[0]->atlasEntryId;
                waveformRef->queued       = true;
                waveformRef->rendered     = false;
                waveformRef->refState     = 2;
                entry.queuedRefCount++;
                if (!isIn(entry.queuedptrs, waveformRef)) {
                    entry.queuedptrs.push_back(waveformRef);
                } else {
                    dbgassert(waveformRef->queued);
                }
                return true;
            }
        }
    }
    return false;
}
void preGLState() {

    checkGLError("waveformrender::render start");

    glViewport(0, 0, FBO_WIDTH, FBO_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    checkGLError("fb prerender");
}
waveformrender::render_timings waveformrender::getTimings() {
    return impl->renderTimings;
}
int waveformrender::renderUpdates(NVGcontext* ctxt, float pxRatio) {
    impl->renderTimings = {};
    auto pathRenderer = impl->renderer;

    bool preGlSet = false;

    int totalRendered = 0;
    impl->timer.reset();

    bool validateUnbound = false;
    if (validateUnbound)
    {
        std::vector<gui_waveform_texture_ref*> queuedTasksRefs;

        for (waveform_update_task_t& waveformQueueEntry : this->queuedTasks) {
            gui_waveform_texture_ref* ref = waveformQueueEntry.queuedptrs.front();
#ifndef NDEBUG
            assertWaveformRefIsUnbound(ref);
#endif
            queuedTasksRefs.push_back(ref);
        }
        std::sort(queuedTasksRefs.begin(), queuedTasksRefs.end());
        dbgassert(adjacent_find(queuedTasksRefs.begin(), queuedTasksRefs.end()) == queuedTasksRefs.end());
    }

    //go over all waveforms that are queued up
    impl->timer2.reset();
    auto itQTask = this->queuedTasks.begin();
    for (; itQTask != this->queuedTasks.end();) {
        waveform_update_task_t& waveformQueueEntry = *itQTask;
        gui_waveform_texture_ref* waveformRef      = waveformQueueEntry.queuedptrs[0];
        if (waveformRef->rendered) {
            releaseRendered(waveformRef);
        }
        // see if we have already have an identical texture rendered or queued
        // if so bind them together and continue with the next queue entry
        impl->timer3.reset();
        bool bFndSimiliar = findSimiliarWaveform(waveformQueueEntry);
        impl->renderTimings.tmFindSimiliar += impl->timer3.getTime();
        if (bFndSimiliar) {
            // bind to similiar waveform
            //log_lf(Log::L_DEBUG, "bind to previous\n");
            itQTask++;
            continue;
        }
        int atlasIdx = -1;
        ivec2 pos;
        vec2 v(waveformQueueEntry.size.x, waveformQueueEntry.size.y);
        //v *= vec2(waveformRef->waveform.scaleX, waveformRef->waveform.scaleY);
        //v.x *= 1.0f / waveformRef->waveform.scaleX;
        ivec2 sizeInternal = ivec2(math::ceilfS32(v.x), math::ceilfS32(v.y));
        //dbgassert(waveformRef->waveform.size.x <= 512);
        //assign texture to free spot in framebuffertextures
        impl->timer3.reset();
        findFreeSpot(sizeInternal, atlasIdx, pos);
        impl->renderTimings.tmFindSpot += impl->timer3.getTime();

        TextureAtlas& _atlas   = this->atlases[atlasIdx];
        waveformQueueEntry.pos = pos;
        dbgassert(sizeInternal.x > 0 && sizeInternal.y > 0);
        waveformQueueEntry.size = sizeInternal;
        _atlas.queuedTasks.push_back(waveformQueueEntry);
        waveformRef->atlasId      = atlasIdx;
        const int id              = _atlas.nextIdx++;
        waveformRef->atlasEntryId = id;
        //itQTask = this->queuedTasks.erase(itQTask);
        itQTask++;
    }
    impl->renderTimings.tmProcessInputQ = impl->timer2.getTime();
    //impl->renderTimings.tmTesselate     = 0;
    //impl->renderTimings.tmDrawGL        = 0;
    //impl->renderTimings.tmPassed        = 0;
    this->queuedTasks.clear();

    //go over all framebuffers (_atlas.fb)
    auto tmNow = getTimeMillis();
    for (TextureAtlas& _atlas : atlases) {
        {
            auto it = _atlas.entries.begin();
            for (; it != _atlas.entries.end();) {
                auto& entry = *it;
                if (entry.refCount <= 0) {
                    auto tmSince = tmNow - entry.tmRelease;
                    if (tmSince >= 100) {
                        it = _atlas.entries.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
        bool clearFB = _atlas.entries.empty();
        if (_atlas.queuedTasks.empty()) {
            continue;
        }
        if (!preGlSet) {
            preGlSet = true;
            preGLState();
            if (pathRenderer->isValid()) {
                glUseProgram(pathRenderer->program2dLines);
            }
        }
        if (!_atlas.fb) {
            preGlSet |= 1;
            _atlas.fb = nvgluCreateFramebuffer(ctxt, FBO_WIDTH, FBO_HEIGHT, NVG_IMAGE_PREMULTIPLIED | NVG_IMAGE_16BIT);
            if (!_atlas.fb) {
                throw appexception("nvgluCreateFramebuffer error");
            }
            checkGLError("waveformrender::render nvgluCreateFramebuffer");
            _atlas.glTexture = nvgGetGLImageHandle(ctxt, _atlas.fb->image);
            clearFB          = true;
        }

        // bind fb
        nvgluBindFramebuffer(_atlas.fb);
        glClearColor(0, 0, 0, 0);
        if (clearFB) {
            glDisable(GL_SCISSOR_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        glEnable(GL_SCISSOR_TEST);
        size_t numRendered = 0;
        int64_t timeoutMikros = 60000;
        auto it = _atlas.queuedTasks.begin();
        for (; it != _atlas.queuedTasks.end() && impl->renderTimings.tmPassed < timeoutMikros; ++it) {
            waveform_update_task_t& waveformQueueEntry = *it;

            gui_waveform_texture_ref* waveformRefUsedForRendering = waveformQueueEntry.queuedptrs[0];
            samplesource_t* audio         = waveformQueueEntry.audio;
            audioclip_texture_t& waveform = waveformRefUsedForRendering->waveform;

            SampleMethod method = waveform.method;
            std::vector<std::vector<glm::vec2>> tesselatedWaveForms;
            impl->timer2.reset();
            tesselateWaveform(audio->getSample(), 0, 0, &waveform, method, tesselatedWaveForms);
            impl->renderTimings.tmTesselate += impl->timer2.getTime();

            Uniforms bakeOpt;
            bakeOpt.linecaps    = vec2(LineCaps::none, LineCaps::none);
            bakeOpt.linejoin    = waveform.linewidth > 1.75 ? LineJoin::round : LineJoin::miter;
            bakeOpt.miter_limit = 1.8f;
            bakeOpt.color       = { 1.0f, 1.0f, 1.0f, 1.0f };
            //uint32_t color      = colorPalette[(nextIdx++ % (COLOR_PALETTE_COLS - 2)) * COLOR_PALETTE_ROWS + 3];
            //bakeOpt.color       = int32vec4(color);
            //bakeOpt.color.w     = 1.0;

            bakeOpt.linewidth     = waveform.linewidth;
            bakeOpt.antialias     = 1.0f;
            bakeOpt.scale         = 1;
            BakeGLPath& bakedPath = this->bakedPaths[this->nextPathIdx++];
            if (this->nextPathIdx >= CtrSize(this->bakedPaths)) {
                this->nextPathIdx = 0;
            }
            impl->timer2.reset();
            std::vector<path_t> paths;
            paths.resize(tesselatedWaveForms.size());
            auto itOut = paths.begin();
            for (auto& twf : tesselatedWaveForms) {
                *itOut++ = path_t{std::move(twf), bakeOpt};
            }
            if (!paths.empty() && pathRenderer->isValid()) {
                pathRenderer->bakePaths(paths, bakedPath);
            }
            impl->renderTimings.tmBakePaths += impl->timer2.getTime();
            impl->timer2.reset();
            ivec2& pos      = waveformQueueEntry.pos;
            ivec2& size     = waveformQueueEntry.size;
            mat4x4 matView  = mat4x4(1.0);
            mat4x4 matModel = mat4x4(1.0);
            matModel[0][0]  = waveform.scaleX;
            matView         = glm::translate(matView, glm::vec3(pos.x, pos.y, 0));
            mat4x4 matProj  = glm::ortho(0.f, (float) FBO_WIDTH, (float) FBO_HEIGHT, 0.f, -1.0f, 1.0f);
            dbgassert(pos.x + size.x <= FBO_WIDTH);
            impl->timer2.reset();
            glScissor(pos.x, FBO_HEIGHT - pos.y - size.y, size.x, size.y);
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!paths.empty() && pathRenderer->isValid()) {
                pathRenderer->render(bakedPath, matProj, matView, matModel);
            }
            impl->renderTimings.tmDrawGL += impl->timer2.getTime();
            TextureAtlasEntry e;
            e.inuse    = true;
            e.pos      = pos;
            e.size     = size;
            e.id       = waveformRefUsedForRendering->atlasEntryId;
            e.props    = waveform;
            e.refCount = 0;
            for (auto* qWaveform : waveformQueueEntry.queuedptrs) {
                qWaveform->queued   = false;
                qWaveform->rendered = true;
                qWaveform->refState = 3;
                e.ptrs.push_back(qWaveform);
                e.refCount++;
            }
            _atlas.entries.push_back(e);
            numRendered++;
            impl->renderTimings.tmPassed = impl->timer.getTime();
        }
        totalRendered += numRendered;
        _atlas.queuedTasks.erase(_atlas.queuedTasks.begin(), it);
        if (impl->renderTimings.tmPassed >= timeoutMikros) {
            break;
        }
    }
    if (preGlSet) {
        glFrontFace(GL_CCW);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(0);
        checkGLError("fb postrender");
        nvgluBindFramebuffer(nullptr);
    }
    return totalRendered;
}

void waveformrender::draw(NVGcontext* ctxt, const gui_waveform_texture_ref* waveformRef, ivec2 sizeClipped, const NVGcolor& rgba) {
    dbgassert(waveformRef->atlasId >= 0 && waveformRef->atlasId < (int) atlases.size());
    auto& atlas = this->atlases[waveformRef->atlasId];
    dbgassert(atlas.fb && atlas.glTexture > -1 && atlas.idx > -1);
    const int atlasEntryId = waveformRef->atlasEntryId;
    auto it                = std::find_if(atlas.entries.cbegin(), atlas.entries.cend(), [atlasEntryId](const TextureAtlasEntry& entry) {
        return entry.id == atlasEntryId;
                   });
    dbgassert(it != atlas.entries.cend());
    auto& entry                          = *it;
    drawTintedImage(ctxt, atlas.fb->image, 1.0f, entry.pos.x, entry.pos.y, entry.size.x, entry.size.y, 0, 0, sizeClipped.x, sizeClipped.y, rgba);
}

void waveformrender::drawPart(NVGcontext* vg, const gui_waveform_texture_ref* waveformRef, ivec2 size, vec2 texTl, vec2 texBr) {
    dbgassert(waveformRef->atlasId >= 0 && waveformRef->atlasId < (int) atlases.size());
    auto& atlas = this->atlases[waveformRef->atlasId];
    dbgassert(atlas.fb && atlas.glTexture > -1 && atlas.idx > -1);
    const int atlasEntryId = waveformRef->atlasEntryId;
    auto it                = std::find_if(atlas.entries.cbegin(), atlas.entries.cend(), [atlasEntryId](const TextureAtlasEntry& entry) {
        return entry.id == atlasEntryId;
                   });
    dbgassert(it != atlas.entries.cend());
    vec2 txSize = texBr - texTl;
    vec2 s = it->pos;
    s += vec2(it->size) * texTl;
    auto a = vec2(size) / (vec2(it->size) * txSize);
    ivec2 i{0, 0};
    NVGpaint img;
    nvgImageSize(vg, atlas.fb->image, &i.x, &i.y);

    float x = 0;
    float y = 0;
    img = nvgImagePattern(vg, x - s.x * a.x, y - s.y * a.y, (float) i.x * a.x, (float) i.y * a.y,
                          0, atlas.fb->image, 1.0f);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, size.x, size.y);
    nvgFillPaint(vg, img);
    nvgFill(vg);
}
