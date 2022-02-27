#include "cliprenderer.h"
#include "cliprenderer_cache.h"
#include "math/seq_math.h"
#include "../host/vst_host.h"
#include "theme.h"
#include "gui.h"
#include "seq_time.h"
#include "project.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"
#include "trackcontent.h"
#include "appconfig.h"
#include <array>
#include <nanovg.h>
#include <nanovg_internal.h>


audioclip_texture_t makeWaveformFromClip(const int32_t tempo100, const samplerate_t samplerate, scaled_grid& grid,
                                         ivec2& trackSize, const clip_t* m_clip,
                                         const ivec2& pos, const ivec2& size, ivec2& posClipped, ivec2& sizeClipped) {

    double lenSamples   = tickToSampleConvert<double, roundmode::none>(m_clip->getLen(), tempo100, samplerate);
    double samplesPerPx = lenSamples / size.x;

    int32_t pxBegin        = posClipped.x;
    int32_t pxEnd          = posClipped.x + sizeClipped.x;
    double tickBegin       = grid.screenToTickD(pos.x);
    double tickBeginOffset = grid.screenToTickD(pxBegin);
    double tickEnd         = grid.screenToTickD(pxEnd);
    if (size.x == sizeClipped.x) {
        tickBeginOffset = m_clip->start();
        tickBegin       = m_clip->start();
        tickEnd         = m_clip->end();
    }

    int64_t sampleBegin       = tickToSampleConvert<int64_t, roundmode::floor>(tickBegin, tempo100, samplerate);
    int64_t sampleStartOffset = tickToSampleConvert<int64_t, roundmode::floor>(tickBeginOffset + m_clip->offsetStart, tempo100, samplerate);
    int64_t sampleEnd  = tickToSampleConvert<int64_t, roundmode::floor>(tickEnd + m_clip->offsetStart, tempo100, samplerate);

    audioclip_texture_t w;
    w.quality = 1;

    double pxPerSample = 1.0 / samplesPerPx;
    constexpr float MAX_RES = 2048;
    w.scaleX                = 1.0f;
    w.pos                   = pos;
    //w.startOffset = startOffset;
    w.size           = ivec2(math::min(sizeClipped.x, FBO_WIDTH), math::min(size.y, FBO_HEIGHT));
    int64_t nSamples = sampleEnd - sampleStartOffset;
    if (nSamples * pxPerSample > FBO_WIDTH) {
        samplesPerPx = (nSamples / FBO_WIDTH);
    }
    if (samplesPerPx > MAX_RES && (nSamples / MAX_RES) <= FBO_WIDTH) {
        w.scaleX     = MAX_RES / samplesPerPx;
        samplesPerPx = MAX_RES;
    }

    dbgassert(w.size.x <= FBO_WIDTH && w.size.y <= FBO_HEIGHT);
    dbgassert(w.size.x > 0);
    w.sampleBegin       = sampleBegin;
    w.sampleBeginOffset = sampleStartOffset;
    w.sampleEnd         = sampleEnd;
    w.samplesPerPx      = samplesPerPx;
    w.linewidth         = 2.0f;

    w.method  = SampleMethod::sample_straight;
    w.audioId = m_clip->audio.id;
    w.clipped = size.x != sizeClipped.x;
    //log_printf("waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%d]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scale);

    return w;
}

void renderAudioClip(NVGcontext* vg, waveformrender* wfrenderer, const guitheme_t* theme, const track_t* tr, const clip_t* cl, const gui_waveform_texture_ref* waveformRef, ivec2 pos, ivec2 size, ivec2 posClipped, ivec2 sizeClipped) {
    if (cl->getLen() <= 0) {
        return;
    }
    NVGcolor color = rgbToNvg(cl->rgb);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
    nvgFillColor(vg, color);
    nvgFill(vg);
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);

    if (cl->name.length()) {
        renderTextLabel(vg,
                        vec2(pos)+vec2(INSET_TITLE, HEIGHT_CLIP_TITLE / 2.0),
                        size-ivec2(INSET_TITLE * 3, 0),
                        cl->name,
                        theme,
                        HEIGHT_CLIP_TITLE,
                        getContrastFontColor(cl->rgb),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
    ivec2 posContents = ivec2(posClipped.x, pos.y + HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
    tick_t clipLen = cl->getLen();
    float numBars  = clipLen / (float) TICKS_BAR;
    float barSize  = size.x / (float) numBars;
    if (sizeClipped.x > 0 && sizeClipped.y > 0 && waveformRef->rendered) {
        nvgSave(vg);
        nvgTranslate(vg, posContents.x, posContents.y);
        wfrenderer->draw(vg, waveformRef, sizeClipped);
        nvgRestore(vg);
    }
    if (cl->loopEnabled && cl->loopLen > 0) {
        tick_t posLoopIndicator = cl->getLoopBegin();
        nvgBeginPath(vg);
        while (posLoopIndicator < clipLen) {
            if (posLoopIndicator >= 0) {
                float objPos = posLoopIndicator / (float) TICKS_BAR;
                float nx     = barSize * objPos;
                nvgMoveTo(vg, pos.x + nx, pos.y);
                nvgLineTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE / 4);
                nvgMoveTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE * 3 / 4);
                nvgLineTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE);
            }
            posLoopIndicator += cl->loopLen;
        }
        nvgStrokeColor(vg, theme->getFrameColorBase());
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
    }

    daw_tls::getTls().renderStats.clipsRendered++;
}

noteview_render_t::~noteview_render_t() {
    delete data;
}

gui_midi_clip::gui_midi_clip(track_gui_entry_t* _track, clip_t* _clip)
    : gui_clip(_track, _clip), impl(new midi_clip_render_cache_t) {
}

gui_midi_clip::~gui_midi_clip() {
    delete impl;
}

void gui_midi_clip::updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) {
    size   = this->parent->size;
    culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
}

void gui_midi_clip::prerender(NVGcontext* vg) {
    gui_clip::prerender(vg);
}

void gui_midi_clip::updateClipRenderCache(NVGcontext* vg) {
    if (culled) {
        impl->reset();
        return;
    }

    clip_t* const cl  = m_clip;
    if (cl->getLen() <= 0) {
        impl->reset();
        return;
    }

    ivec2 sizeContents  = ivec2(size.x, size.y - HEIGHT_CLIP_TITLE - INSET_CLIP_CONTENT * 2);
    ivec2 clipPosScreen = toScreenSpace(ivec2(0, 0));

    tick_t clipLen               = cl->getLen();
    float numBars                = clipLen / (float) TICKS_BAR;
    float barSize                = sizeContents.x / (float) numBars;
    int64_t notesRendered        = 0;
    noteview_render_t& notesView = cl->getNoteViewRender();
    bool cacheValid              = notesView.reqRevision == notesView.curRevision;
    cacheValid &= impl->valid;
    cacheValid &= impl->pos == clipPosScreen;
    cacheValid &= impl->size == sizeContents;
    if (!cacheValid) {
        notesView.curRevision = -1;
        impl->reset();

        NVGcolor rgbNote        = theme->getColor(GuiColor::COL_CLIP_NOTE);
        NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
        NVGcolor rgbNoteMuted   = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);

        int noteRenderMode = theme->get(GuiConstant::CONST_NOTE_RENDER_MODE);
        nvgSave(vg);
        nvgTranslate(vg, clipPosScreen.x, clipPosScreen.y);
        nvgSave(vg);
        nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);

        if (sizeContents.x > 0 && sizeContents.y > 0) {
            clip_notes_t& notes = notesView;
            if (!notes.empty()) {
                note_t minN      = notesView.minNote;
                note_t maxN      = notesView.maxNote;
                int32_t numNotes = math::max((int32_t) 8, maxN.pitch - minN.pitch);
                float scale      = sizeContents.y / (float) numNotes;
                std::vector<const note_t*> notesClipped;
                std::vector<const note_t*> notesMuted;
                int begin = 0;
                for (const note_t& note : notes.m_list) {
                    tick_t noteTime = note.time;
                    if (noteTime >= clipLen) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (noteTime < 0) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (!note.isEnabled()) {
                        notesMuted.push_back(&note);
                        continue;
                    }
                    if (!begin) {
                        if (noteRenderMode == 0) {
                            nvgBeginPath(vg);
                        }
                        begin++;
                    }
                    float objPosNote = noteTime / (float) TICKS_BAR;
                    float objLenNote = note.len / (float) TICKS_BAR;
                    float ny     = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContents.y);
                    float nx     = math::max(0.0f, objPosNote * barSize);
                    float nw     = math::min(objLenNote * barSize, sizeContents.x - nx);
                    float nh     = scale;
                    float insetx = calcInset(1, nw);
                    float insety = calcInset(1, nh);
                    if (noteRenderMode == 0) {
                        nvgRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                    } else {
                        nvgBatchedRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                    }

                    notesRendered++;
                }
                if (begin) {
                    if (noteRenderMode == 0) {
                        nvgFillColor(vg, rgbNote);
                        nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                        nvgFill(vg);
                    } else {
                        NVGpaint paint;
                        memset(&paint, 0, sizeof(paint));
                        paint.image      = -1;
                        paint.innerColor = rgbNote;
                        paint.outerColor = rgbNote;
                        paint.customPar  = 1234;
                        nvgFillPaint(vg, paint);
                        nvgBatchedRender(vg);
                    }
                    impl->SaveFill(vg, 0);
                }

                for (int j = 0; j < 2; j++) {
                    auto& list = j == 0 ? notesClipped : notesMuted;
                    if (!list.empty()) {
                        if (noteRenderMode == 0) {
                            nvgBeginPath(vg);
                        }
                        for (const note_t* noteClipped : list) {
                            const note_t& note = *noteClipped;
                            tick_t noteTime    = note.time;

                            float objPosNote = noteTime / (float) TICKS_BAR;
                            float objLenNote = note.len / (float) TICKS_BAR;
                            float ny         = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContents.y);
                            float nx         = objPosNote * barSize;
                            float nw         = objLenNote * barSize;
                            float nh         = scale;
                            float insetx     = calcInset(1, nw);
                            float insety     = calcInset(1, nh);
                            if (noteRenderMode == 0) {
                                nvgRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                            } else {
                                nvgBatchedRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                            }
                            notesRendered++;
                        }

                        if (noteRenderMode == 0) {
                            nvgFillColor(vg, j == 0 ? rgbNoteOverlap : rgbNoteMuted);
                            nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                            nvgFill(vg);
                        } else {
                            NVGpaint paint;
                            memset(&paint, 0, sizeof(paint));
                            paint.image      = -1;
                            paint.innerColor = j == 0 ? rgbNoteOverlap : rgbNoteMuted;
                            paint.outerColor = j == 0 ? rgbNoteOverlap : rgbNoteMuted;
                            paint.customPar  = 1234;
                            nvgFillPaint(vg, paint);
                            nvgBatchedRender(vg);
                        }

                        if (j == 0) {
                            impl->SaveFill(vg, 1);
                        } else {
                            impl->SaveFill(vg, 2);
                        }
                    }
                }
            }
        }
        nvgRestore(vg);

        if (cl->isLoopEnabled()) {
            tick_t posLoopIndicator = cl->getLoopBegin();
            int n                   = 0;
            while (posLoopIndicator < clipLen) {
                if (posLoopIndicator >= 0) {
                    float objPos = posLoopIndicator / (float) TICKS_BAR;
                    float nx     = barSize * objPos;
                    if (n == 0) {
                        nvgBeginPath(vg);
                    }
                    nvgMoveTo(vg, nx, 0);
                    nvgLineTo(vg, nx, 0 + HEIGHT_CLIP_TITLE / 4);
                    nvgMoveTo(vg, nx, 0 + HEIGHT_CLIP_TITLE * 3 / 4);
                    nvgLineTo(vg, nx, 0 + HEIGHT_CLIP_TITLE);
                    n++;
                }
                posLoopIndicator += cl->loopLen;
            }
            if (n) {
                nvgStrokeColor(vg, theme->getFrameColorBase());
                nvgStrokeWidth(vg, 1.f);
                nvgStroke(vg);
                impl->SaveFill(vg, 3);
            }
        }
        nvgRestore(vg);
        impl->valid           = true;
        impl->pos             = clipPosScreen;
        impl->size            = sizeContents;
        impl->notesRendered   = notesRendered;
        notesView.curRevision = notesView.reqRevision;
    }
}

void gui_midi_clip::renderDebugPass(NVGcontext* vg) {
    this->render(vg);
}

void gui_midi_clip::render(NVGcontext* vg) {
    if (!culled) {
        clip_t* const cl  = m_clip;
        if (cl->getLen() <= 0) {
            return;
        }
        NVGcolor color = rgbToNvg(cl->rgb);
        if (!cl->enabled) {
            color = rgbToNvg(0x333333);
        }
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
        nvgFillColor(vg, color);
        nvgFill(vg);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        if (cl->name.length()) {
            renderTextLabel(vg,
                            vec2(pos)+vec2(INSET_TITLE, HEIGHT_CLIP_TITLE / 2.0),
                            size-ivec2(INSET_TITLE * 3, 0),
                            cl->name,
                            theme,
                            HEIGHT_CLIP_TITLE,
                            getContrastFontColor(cl->rgb),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
        dbgassert(impl->valid);
        if (impl->valid && std::any_of(impl->arr.cbegin(), impl->arr.cend(), [](const auto* ptr) { return !!ptr; })) {
            ivec2 posContents  = ivec2(pos.x, pos.y + HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
            ivec2 sizeContents = ivec2(size.x, size.y - HEIGHT_CLIP_TITLE - INSET_CLIP_CONTENT * 2);

            int64_t notesRendered = 0;

            nvgSave(vg);
            nvgTranslate(vg, pos.x, pos.y);
            nvgSave(vg);
            nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
            if (impl->isCacheValid(0)) {
                nvgFillFromCache(vg, impl->arr[0]);
            }
            if (impl->isCacheValid(1)) {
                nvgFillFromCache(vg, impl->arr[1]);
            }
            if (impl->isCacheValid(2)) {
                nvgFillFromCache(vg, impl->arr[2]);
            }
            notesRendered += impl->notesRendered;
            nvgRestore(vg);
            if (cl->isLoopEnabled()) {
                if (impl->isCacheValid(3)) {
                    nvgFillFromCache(vg, impl->arr[3]);
                }
            }
            nvgRestore(vg);
            daw_tls::getTls().renderStats.notesRendered += notesRendered;
        }
        daw_tls::getTls().renderStats.clipsRendered++;
    }
}
void renderMidiClip(NVGcontext* vg, const guitheme_t* theme, const track_gui_entry_t* const entry, const clip_t* cl, ivec2 pos, ivec2 size) {
    if (cl->getLen() <= 0) {
        return;
    }
    NVGcolor color = rgbToNvg(cl->rgb);
    if (!cl->enabled) {
        color = rgbToNvg(0x333333);
    }
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
    nvgFillColor(vg, color);
    nvgFill(vg);
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);
    if (cl->name.length()) {
        renderTextLabel(vg,
                        vec2(pos)+vec2(INSET_TITLE, HEIGHT_CLIP_TITLE / 2.0),
                        size-ivec2(INSET_TITLE * 3, 0),
                        cl->name,
                        theme,
                        HEIGHT_CLIP_TITLE,
                        getContrastFontColor(cl->rgb),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
    ivec2 posContents  = ivec2(pos.x, pos.y + HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
    ivec2 sizeContents = ivec2(size.x, size.y - HEIGHT_CLIP_TITLE - INSET_CLIP_CONTENT * 2);

    tick_t clipLen               = cl->getLen();
    float numBars                = clipLen / (float) TICKS_BAR;
    float barSize                = sizeContents.x / (float) numBars;
    int64_t notesRendered        = 0;
    const bool useCaching        = daw_tls::getTls().config->enableCache;
    noteview_render_t& notesView = cl->getNoteViewRender();
    bool cacheValid              = notesView.reqRevision == notesView.curRevision;
    cacheValid &= notesView.data != nullptr && notesView.data->valid;
    cacheValid &= notesView.data != nullptr && notesView.data->pos == posContents;
    cacheValid &= notesView.data != nullptr && notesView.data->size == sizeContents;
    cacheValid &= useCaching;
    if (!cacheValid) {
        notesView.curRevision = -1;
        if (notesView.data) {
            notesView.data->reset();
        } else {
            notesView.data = new noteview_cache_impl_t{};
        }
    }

    if (cacheValid) {
        nvgSave(vg);
        nvgTranslate(vg, pos.x, pos.y);
        nvgSave(vg);
        nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
        if (notesView.data->isCacheValid(0)) {
            nvgFillFromCache(vg, notesView.data->arr[0]);
        }
        if (notesView.data->isCacheValid(1)) {
            nvgFillFromCache(vg, notesView.data->arr[1]);
        }
        if (notesView.data->isCacheValid(2)) {
            nvgFillFromCache(vg, notesView.data->arr[2]);
        }
        nvgRestore(vg);
        notesRendered += notesView.data->notesRendered;
        if (notesView.data->isCacheValid(3)) {
            nvgFillFromCache(vg, notesView.data->arr[3]);
        }
        nvgRestore(vg);
    } else if (0) {
        NVGcolor rgbNote        = theme->getColor(GuiColor::COL_CLIP_NOTE);
        NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
        NVGcolor rgbNoteMuted   = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);
        nvgCachePath(vg, useCaching);
        nvgSave(vg);
        nvgTranslate(vg, posContents.x, posContents.y);
        if (sizeContents.x > 0 && sizeContents.y > 0) {
            clip_notes_t& notes = notesView;
            if (!notes.empty()) {
                note_t minN      = notesView.minNote;
                note_t maxN      = notesView.maxNote;
                int32_t numNotes = math::max((int32_t) 8, maxN.pitch - minN.pitch);
                float scale      = sizeContents.y / (float) numNotes;
                std::vector<const note_t*> notesClipped;
                std::vector<const note_t*> notesMuted;
                int begin = 0;
                for (const note_t& note: notes.m_list) {
                    tick_t noteTime = note.time;
                    if (noteTime >= clipLen) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (noteTime < 0) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (!note.isEnabled()) {
                        notesMuted.push_back(&note);
                        continue;
                    }
                    if (!begin) {
                        nvgBeginPath(vg);
                        begin++;
                    }
                    float objPosNote = noteTime / (float) TICKS_BAR;
                    float objLenNote = note.len / (float) TICKS_BAR;
                    float ny     = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContents.y);
                    float nx     = math::max(0.0f, objPosNote * barSize);
                    float nw     = math::min(objLenNote * barSize, sizeContents.x - nx);
                    float nh     = scale;
                    float insetx = calcInset(1, nw);
                    float insety = calcInset(1, nh);
                    nvgRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                    notesRendered++;
                }
                if (begin) {
                    nvgFillColor(vg, rgbNote);
                    nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                    nvgFill(vg);
                    if (useCaching) {
                        notesView.data->SaveFill(vg, 0);
                    }
                }

                for (int j = 0; j < 2; j++) {
                    auto& list = j == 0 ? notesClipped : notesMuted;
                    if (!list.empty()) {
                        nvgBeginPath(vg);
                        for (const note_t* noteClipped: list) {
                            const note_t& note = *noteClipped;
                            tick_t noteTime    = note.time;

                            float objPosNote = noteTime / (float) TICKS_BAR;
                            float objLenNote = note.len / (float) TICKS_BAR;
                            float ny         = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContents.y);
                            float nx         = objPosNote * barSize;
                            float nw         = objLenNote * barSize;
                            float nh         = scale;
                            float insetx     = calcInset(1, nw);
                            float insety     = calcInset(1, nh);
                            nvgRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                            notesRendered++;
                        }
                        nvgFillColor(vg, j == 0 ? rgbNoteOverlap : rgbNoteMuted);
                        nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                        nvgFill(vg);
                        if (useCaching) {
                            if (j == 0) {
                                notesView.data->SaveFill(vg, 1);
                            } else {
                                notesView.data->SaveFill(vg, 2);
                            }
                        }
                    }
                }
            }
        }
        nvgRestore(vg);

        if (cl->isLoopEnabled()) {
            tick_t posLoopIndicator = cl->getLoopBegin();
            int n                   = 0;
            while (posLoopIndicator < clipLen) {
                if (posLoopIndicator >= 0) {
                    float objPos = posLoopIndicator / (float) TICKS_BAR;
                    float nx     = barSize * objPos;
                    if (n == 0) {
                        nvgBeginPath(vg);
                    }
                    nvgMoveTo(vg, pos.x + nx, pos.y);
                    nvgLineTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE / 4);
                    nvgMoveTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE * 3 / 4);
                    nvgLineTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE);
                    n++;
                }
                posLoopIndicator += cl->loopLen;
            }
            if (n) {
                nvgStrokeColor(vg, theme->getFrameColorBase());
                nvgStrokeWidth(vg, 1.f);
                nvgStroke(vg);
                if (useCaching) {
                    notesView.data->SaveFill(vg, 3);
                }
            }
        }
        nvgCachePath(vg, 0);
    }

    if (useCaching && notesView.data) {
        notesView.data->pos           = posContents;
        notesView.data->size          = sizeContents;
        notesView.data->notesRendered = notesRendered;
        notesView.curRevision         = notesView.reqRevision;
    }
    daw_tls::getTls().renderStats.clipsRendered++;
    daw_tls::getTls().renderStats.notesRendered += notesRendered;
}
