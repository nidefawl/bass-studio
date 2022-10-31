#include "cliprenderer.h"
#include "audiocache.h"
#include "clip.h"
#include "cliprenderer_cache.h"
#include "guiglobals.h"
#include "math/seq_math.h"
#include "host/host_pluginmanager.h"
#include "renderresources.h"
#include "theme.h"
#include "gui/gui.h"
#include "seq_time.h"
#include "project.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"
#include "gui/track/trackcontent.h"
#include "appconfig.h"
#include <array>
#include <math.h>
#include <nanovg.h>
#include <nanovg_internal.h>
#include <nanovg_min.h>


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
    audioclip_texture_t w;
    w.quality = 1;

    double pxPerSample = 1.0 / samplesPerPx;
    constexpr float MAX_RES = 2048;
    w.scaleX                = 1.0f;
    w.pos                   = pos;
    //w.startOffset = startOffset;
    w.size           = ivec2(math::min(sizeClipped.x, FBO_WIDTH), math::min(size.y, FBO_HEIGHT));
    w.clipped = size.x != sizeClipped.x;
    if (m_clip->isLoopEnabled()) {
        auto begin = m_clip->offsetStart > m_clip->loopStart ? m_clip->loopStart : m_clip->offsetStart;
        auto end = m_clip->loopStart + m_clip->getLoopLength();
        auto len = end - begin;
        auto newWidth = grid.tickLenToScreen(len);
        w.size.x = math::min(math::rounddS32(newWidth), FBO_WIDTH);
        w.posPreLoopEnd = (m_clip->loopStart - m_clip->offsetStart) / float(len);
        if (m_clip->offsetStart < m_clip->loopStart) {
            w.samplePreLoopLen = tickToSampleConvert<samplecount_t, roundmode::none>(m_clip->offsetStart, tempo100, samplerate);
        }
        w.sampleLoopLen = tickToSampleConvert<samplecount_t, roundmode::none>(m_clip->getLoopLength(), tempo100, samplerate);
        tickBegin       = m_clip->start();
        tickBeginOffset = begin + m_clip->start();
        tickEnd = end + m_clip->start();
    } else {
        tickBeginOffset += m_clip->offsetStart;
        tickEnd += m_clip->offsetStart;
    }

    int64_t sampleBegin       = tickToSampleConvert<int64_t, roundmode::floor>(tickBegin, tempo100, samplerate);
    int64_t sampleStartOffset = tickToSampleConvert<int64_t, roundmode::floor>(tickBeginOffset, tempo100, samplerate);
    int64_t sampleEnd  = tickToSampleConvert<int64_t, roundmode::floor>(tickEnd, tempo100, samplerate);

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
    if (m_clip->hasFadeIn()) {
        auto fadeRef = m_clip->getSampleFadeIn(tempo100, samplerate);
        w.fades[0] = {*fadeRef.shape, fadeRef.samplesFadePos, fadeRef.samplesFadeDuration};
    } else {
        w.fades[0] = {};
    }
    if (m_clip->hasFadeOut()) {
        auto fadeRef = m_clip->getSampleFadeOut(tempo100, samplerate);
        w.fades[1] = {*fadeRef.shape, fadeRef.samplesFadePos, fadeRef.samplesFadeDuration};
    } else {
        w.fades[1] = {};
    }
    //log_lf(Log::L_DEBUG, "waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%d]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scale);
    return w;
}

void renderAudioClip(NVGcontext* vg, waveformrender* wfrenderer, const guitheme_t* theme, const track_t* tr, const clip_t* cl, const audiofile_t* file, const gui_waveform_texture_ref* waveformRef, ivec2 pos, ivec2 size, ivec2 posClipped, ivec2 sizeClipped) {
    if (cl->getLen() <= 0) {
        return;
    }
    const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    NVGcolor color = rgbToNvg(cl->rgb);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
    nvgFillColor(vg, color);
    nvgFill(vg);
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);
    if (file && (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MISSING)) {
        auto colInvalid = theme->getColor(GuiColor::COL_INVALID_INPUT);
        colInvalid.a = 0.5;
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y+HEIGHT_CLIP_TITLE, size.x, size.y-HEIGHT_CLIP_TITLE);
        nvgFillColor(vg, colInvalid);
        nvgFill(vg);
    }

    auto textPos = vec2(INSET_TITLE, HEIGHT_CLIP_TITLE / 2.0) + vec2(pos);
    auto textBounds = vec2(size.x, HEIGHT_CLIP_TITLE)-vec2(INSET_TITLE + 2, 0);
    if (file && (file->state == audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MISSING)) {
        textPos.x += HEIGHT_CLIP_TITLE;
        textBounds.x -= HEIGHT_CLIP_TITLE;
        auto iconId = ICON_WARNING;
        nvgTranslate(vg, pos.x, pos.y);
        drawIcon(vg, vec2(HEIGHT_CLIP_TITLE), &RenderResources::imgIcons[iconId], -2);
        nvgTranslate(vg, -pos.x, -pos.y);
    }
    if (cl->name.length()) {
        renderTextLabel(vg,
                        textPos,
                        textBounds,
                        cl->name,
                        theme,
                        HEIGHT_CLIP_TITLE,
                        getContrastFontColor(cl->rgb),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
    ivec2 posContents = ivec2(pos.x, pos.y + HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
    tick_t clipLen = cl->getLen();
    float numBars  = clipLen / (float) TICKS_BAR;
    float barSize  = size.x / (float) numBars;
    if (sizeClipped.x > 0 && sizeClipped.y > 0 && waveformRef->rendered) {
        if (waveformRef->waveform.sampleLoopLen) {
            if (waveformRef->waveform.samplePreLoopLen) {
                float x = waveformRef->waveform.posPreLoopEnd;
                if (x > 0) {
                    auto sizePreLoop = ivec2(vec2(waveformRef->waveform.size) * vec2(x, 1));
                    if (sizePreLoop.x > 0 && sizePreLoop.y > 0 && pos.x + sizePreLoop.x + 5 > posClipped.x) {
                        nvgTranslate(vg, pos.x, posContents.y);
                        wfrenderer->drawPart(vg, waveformRef, sizePreLoop, {0, 0}, {x, 1});
                        nvgTranslate(vg, -pos.x, -posContents.y);
                    }
                }
            }
        } else {
            nvgTranslate(vg, posClipped.x, posContents.y);
            wfrenderer->draw(vg, waveformRef, sizeClipped);
            nvgTranslate(vg, -posClipped.x, -posContents.y);
        }
    }
    if (sizeClipped.x > 0
            && sizeClipped.y > 0
            && waveformRef->rendered 
            && cl->loopEnabled
            && cl->loopLen > 0
            && waveformRef->waveform.sampleLoopLen) {
        //TODO: use exact loop-len to pixel conversion
        float stepLen = barSize * cl->loopLen / float(TICKS_BAR);
        float preLoopScale = math::max(waveformRef->waveform.posPreLoopEnd, 0.0f);
        auto sizeLoop = vec2(waveformRef->waveform.size) * vec2(1.0f-preLoopScale, 1.0f);
        float x = 0.0f;
        if (waveformRef->waveform.samplePreLoopLen && waveformRef->waveform.posPreLoopEnd > 0) {
            x = waveformRef->waveform.size.x * preLoopScale;
        } else if (waveformRef->waveform.posPreLoopEnd < 0.0f) {
            float tilingPos = ::fmod(-waveformRef->waveform.posPreLoopEnd, 1.0f);
            nvgSave(vg);
            nvgTranslate(vg, pos.x, posContents.y);
            auto sizeFirstLoop = vec2(sizeLoop)*vec2(1.0f-tilingPos, 1.0f);
            if (sizeFirstLoop.x > 0) {
                wfrenderer->drawPart(vg, waveformRef, sizeFirstLoop, {tilingPos, 0}, {1, 1});
            }
            nvgRestore(vg);
            x = sizeFirstLoop.x;
        }
        auto preLoopLen = cl->offsetStart < cl->loopStart ? cl->loopStart - cl->offsetStart : 0;
        auto loopLen = cl->len - preLoopLen;
        auto numLoops = loopLen / cl->loopLen;
        for (int32_t i = 0; i <= numLoops; ++i) {
            float xLoop = x + stepLen * i;
            if (xLoop > size.x) {
                break;
            }
            float txW = 1.0f;
            if (xLoop + stepLen > size.x) {
                sizeLoop = sizeLoop * vec2((size.x - xLoop) / stepLen, 1);
                txW = (size.x - xLoop) / stepLen;
            }
            xLoop += pos.x;
            if (xLoop + stepLen + 5 < posClipped.x) {
                continue;
            }
            nvgSave(vg);
            nvgTranslate(vg, xLoop, posContents.y);
            if (sizeLoop.x > 0) {
                wfrenderer->drawPart(vg, waveformRef, sizeLoop, {preLoopScale, 0}, {txW, 1});
            }
            nvgRestore(vg);
        }
    }
    if (cl->loopEnabled && cl->loopLen > 0) {
        
        nvgBeginPath(vg);
        for (tick_t posLoopIndicator = cl->getLoopBegin(); posLoopIndicator < clipLen; posLoopIndicator += cl->loopLen) {
            if (posLoopIndicator >= 0) {
                float objPos = posLoopIndicator / (float) TICKS_BAR;
                float nx     = barSize * objPos;
                if (pos.x + nx < -5) {
                    continue;
                }
                if (nx > size.x + 5) {
                    continue;
                }
                nvgMoveTo(vg, pos.x + nx, pos.y);
                nvgLineTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE / 4);
                nvgMoveTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE * 3 / 4);
                nvgLineTo(vg, pos.x + nx, pos.y + HEIGHT_CLIP_TITLE);
            }
        }
        nvgStrokeColor(vg, theme->getFrameColorBase());
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
    }

    daw_tls::getTls().runtime->renderStats.clipsRendered++;
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
    culled = !getClipPositionInt(grid, trackSize, m_clip, pos, size, 0);
}

void gui_midi_clip::prerender(NVGcontext* vg) {
    gui_clip::prerender(vg);
}
void renderMidiClipToCache(NVGcontext* vg, noteview_cache_impl_t* impl, const guitheme_t* theme, const clip_t* cl, ivec2 pos, ivec2 size, tick_t clipLen, int32_t HEIGHT_CLIP_TITLE) {
    noteview_render_t& notesView = cl->getNoteViewRender();
    float numBars                = clipLen / (float) TICKS_BAR;
    float barSize                = size.x / (float) numBars;
    int64_t notesRendered        = 0;
    bool cacheValid              = notesView.reqRevision == notesView.curRevision;
    cacheValid &= impl->valid;
    cacheValid &= impl->pos == pos;
    cacheValid &= impl->size == size;
    if (!cacheValid) {
        notesView.curRevision = -1;
        impl->reset();

        NVGcolor rgbNote        = theme->getColor(GuiColor::COL_CLIP_NOTE);
        NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
        NVGcolor rgbNoteMuted   = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);

        int noteRenderMode = theme->get(GuiConstant::CONST_NOTE_RENDER_MODE);
        nvgSave(vg);
        nvgTranslate(vg, pos.x, pos.y + INSET_CLIP_CONTENT + HEIGHT_CLIP_TITLE);
        auto sizeContent = size - ivec2(0, HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2);
        if (sizeContent.x > 0 && sizeContent.y > 0) {
            clip_notes_t& notes = notesView;
            if (!notes.isEmpty()) {
                note_t minN      = notesView.minNote;
                note_t maxN      = notesView.maxNote;
                int32_t numNotes = math::max((int32_t) 8, maxN.pitch - minN.pitch);
                float scale      = sizeContent.y / (float) numNotes;
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
                    float ny     = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContent.y);
                    float nx     = math::max(0.0f, objPosNote * barSize);
                    float nw     = math::min(objLenNote * barSize, sizeContent.x - nx);
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
                        nvgSetShapeExtents(vg, 0, 0, sizeContent.x, sizeContent.y);
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
                            float ny         = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContent.y);
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
                            nvgSetShapeExtents(vg, 0, 0, sizeContent.x, sizeContent.y);
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

        if (HEIGHT_CLIP_TITLE) {
            nvgTranslate(vg, 0, -HEIGHT_CLIP_TITLE);
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
        }
        nvgRestore(vg);
        impl->valid           = true;
        impl->pos             = pos;
        impl->size            = size;
        impl->notesRendered   = notesRendered;
        notesView.curRevision = notesView.reqRevision;
    }
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

    const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    renderMidiClipToCache(vg, impl, theme, cl, toScreenSpace(ivec2(0, 0)), size, cl->getLen(), HEIGHT_CLIP_TITLE);
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
        const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
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
                            vec2(size.x, HEIGHT_CLIP_TITLE)-vec2(INSET_TITLE + 2, 0),
                            cl->name,
                            theme,
                            HEIGHT_CLIP_TITLE,
                            getContrastFontColor(cl->rgb),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
        auto& tls = daw_tls::getTls();
        auto& renderStats = tls.runtime->renderStats;
        dbgassert(impl->valid);
        if (impl->valid && std::any_of(impl->arr.cbegin(), impl->arr.cend(), [](const auto* ptr) { return !!ptr; })) {
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
            renderStats.notesRendered += notesRendered;
        }
        renderStats.clipsRendered++;
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
    
    const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP) / 2;
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
                        vec2(size.x, HEIGHT_CLIP_TITLE)-vec2(INSET_TITLE + 2, 0),
                        cl->name,
                        theme,
                        HEIGHT_CLIP_TITLE,
                        getContrastFontColor(cl->rgb),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
    noteview_render_t& notesView = cl->getNoteViewRender();
    if (!notesView.data) {
        notesView.data = new noteview_cache_impl_t{};
    }
    auto cache = notesView.data;

    nvgCachePath(vg, 1);
    renderMidiClipToCache(vg, cache, theme, cl, pos, size, cl->getLen(), HEIGHT_CLIP_TITLE);
    nvgCachePath(vg, 0);
    auto& tls = daw_tls::getTls();
    auto& renderStats = tls.runtime->renderStats;
    if (cache->valid && std::any_of(cache->arr.cbegin(), cache->arr.cend(), [](const auto* ptr) { return !!ptr; })) {
        int64_t notesRendered = 0;

        nvgSave(vg);
        nvgTranslate(vg, pos.x, pos.y);
        nvgSave(vg);
        nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
        if (cache->isCacheValid(0)) {
            nvgFillFromCache(vg, cache->arr[0]);
        }
        if (cache->isCacheValid(1)) {
            nvgFillFromCache(vg, cache->arr[1]);
        }
        if (cache->isCacheValid(2)) {
            nvgFillFromCache(vg, cache->arr[2]);
        }
        notesRendered += cache->notesRendered;
        nvgRestore(vg);
        if (cl->isLoopEnabled()) {
            if (cache->isCacheValid(3)) {
                nvgFillFromCache(vg, cache->arr[3]);
            }
        }
        nvgRestore(vg);
        renderStats.notesRendered += notesRendered;
    }
    renderStats.clipsRendered++;
}
