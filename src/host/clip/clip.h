#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <vector>
#include <memory>
#include "gui/gui.h"
#include "math/seq_math.h"
#include "seq_time.h"
#include "host/shape/shape.h"
#include "str_util.h"
#include "seq_util.h"
#include "note.h"
#include "layout.h"
#include "host/audiocache/audiocache.h"
#include "types.h"
#include "wave/waveform_render.h"
#include "logging.h"
#include "assert_dbg.h"


#ifndef NDEBUG
// NOTE: This is a debug-only feature, and is not thread-safe.
// #define TRACK_ALLOCATIONS_CLIP_T
#endif

#define CLIP_MIDI 0
#define CLIP_AUDIO 1

class clip_notes_t;
struct track_gui_entry_t;
class track_t;
class gui_clip;
class rendered_audio_clip_t;
struct groove_data_t;
namespace DAW::Host {
    struct midievent_ctrl_t;
}

struct clip_fade_t {
    double durationMs = 0.0;
    DAW::Shape::shape_t shape;
};

class clip_audio_t {
public:
    int32_t id = -1;
    clip_fade_t fadeIn;
    clip_fade_t fadeOut;
    clip_audio_settings_t settings;
    int32_t idDerived = -1;
    rendered_audio_clip_t* renderedAudio = nullptr;
public:
    clip_audio_t();
    ~clip_audio_t();

    clip_audio_t& operator=(const clip_audio_t& obj) {
        if (this != &obj) {
            copy(obj);
        }
        return *this;
    }
    clip_audio_t(const clip_audio_t& obj) {
        copy(obj);
    }
    void copy(const clip_audio_t& obj) {
        this->id = obj.id;
        this->fadeIn = obj.fadeIn;
        this->fadeOut = obj.fadeOut;
        this->settings = obj.settings;
        this->idDerived = obj.idDerived;
    }
    int32_t lenSamples() const;

    bool isEmpty() const {
        return id < 0;
    }
    void setDefaultFade(bool bIn);
    void setEmptyFade(bool bIn);
};
class clip_t;
struct clip_control_data_channel_t {
    DAW::Shape::shape_t shape;
    float defaultValue = 0.0f;
    float minTick = -1;
    float maxTick = -1;
    bool hasData() const {
        return shape.pts.size() > 0;
    }
    float sampleAtTick(float x) {
        if (shape.pts.empty()) {
            return defaultValue;
        }
        return shape.sampleCurveUnclamped(x);
    }
    float sampleAtTick(clip_t* clip, tick_t x);
    void updateBounds() {
        if (shape.pts.empty()) {
            minTick = -1;
            maxTick = -1;
        } else {
            minTick = shape.pts.front().pos.x;
            maxTick = shape.pts.back().pos.x;
        }
    }
};
struct clip_control_data_t {
    clip_control_data_channel_t pitchBend;
    std::map<int32_t, clip_control_data_channel_t> ccChannels;
    clip_control_data_t();
    void updateBounds();
    void eraseDuplicates();
    void sort();
    void clear();
    void createCCChannel(int32_t cc);
    bool hasData() const {
        if (pitchBend.hasData()) {
            return true;
        }
        for (auto& kv : ccChannels) {
            if (kv.second.hasData()) {
                return true;
            }
        }
        return false;
    }
    clip_control_data_channel_t& getOrCreateChannel(int32_t cc);
    int getInTimeRange(clip_t* clip, tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, std::vector<DAW::Host::midievent_ctrl_t>& list);
    void copyRangeFrom(clip_t* clip, tick_t writePos, tick_t tickBegin, tick_t len);
    void cutLeft(tick_t time);
    void cutRight(tick_t time);
};

namespace DAW {
void CopyControlDataChannel(clip_control_data_channel_t& dst, tick_t writePos, const clip_control_data_channel_t& src, tick_t readPos, tick_t len, tick_t offsetStart,  tick_t loopStart, tick_t loopLen);
void CopyControlData(const clip_control_data_t& src, clip_control_data_t& dst, tick_t readPos, tick_t writePos, tick_t readLen);
}

class clip_notes_t {
public:
    std::vector<note_t> m_list;
    std::set<note_t*> selection;
    note_t firstNote;
    note_t lastNote;
    note_t minNote;
    note_t maxNote;

public:
    clip_notes_t() = default;

    clip_notes_t& operator=(const clip_notes_t& obj) {
        if (this != &obj) {
            copy(obj);
        }
        return *this;
    }

    clip_notes_t(const clip_notes_t& obj) {
        copy(obj);
    }

    clip_notes_t& operator=(clip_notes_t&& obj) noexcept {
        if (this != &obj) {
            m_list = std::move(obj.m_list);
            selection = std::move(obj.selection);
            firstNote = obj.firstNote;
            lastNote = obj.lastNote;
            minNote = obj.minNote;
            maxNote = obj.maxNote;
        }
        return *this;
    }

    clip_notes_t(clip_notes_t&& obj) noexcept {
        m_list = std::move(obj.m_list);
        selection = std::move(obj.selection);
        firstNote = obj.firstNote;
        lastNote = obj.lastNote;
        minNote = obj.minNote;
        maxNote = obj.maxNote;
    }

    template<typename Functor>
    void visitNotes(Functor f) {
        std::for_each(m_list.begin(), m_list.end(), f);
    }
    template<typename Functor>
    void visitSelection(Functor f) {
        std::for_each(selection.begin(), selection.end(), f);
    }

    void copy(const clip_notes_t& obj);
    void clear();
    void updateBounds();
    bool isEmpty() const;
    bool hasDuplicates() const;
    bool has(note_t* notePtr) const;
    note_t& add(note_t& t);                               // does not update bounds
    void remove(note_t& t);                               // does not update bounds
    void mute(note_t& t);                                 // does not update bounds
    note_t& addSingle(note_t& t);                         // updates bounds
    void removeSingle(note_t& t);                         // updates bounds
    int32_t paste(note_t& t, bool eliminateDupes = false);// updates bounds
    note_t* get(tick_t time, int32_t pitch);
    size_t removeDuplicates();
    int getInRange(tick_t timeS, tick_t timeE, int32_t pitchL, int32_t pitchH, std::vector<note_t*>& list);
    int getStartsInRangeV(tick_t timeS, tick_t timeE, int32_t velL, int32_t velH, int32_t tickDist, std::vector<note_t*>& list);

    void setTo(const std::set<note_t*>& notePtrs, tick_t offset);
    void addAll(const std::vector<note_t>& list);
    void removeAll(const std::vector<note_t>& list);
    void removeAllKeepDuplicates(const std::vector<note_t>& list);
    void selectIdxRange(size_t start, size_t end);
    void selectLastN(size_t num);
    void getNotePitches(std::vector<int32_t>& out);

    void getSelectionIndices(std::vector<size_t>& selIdx) const;
    void copySelectionTo(std::vector<note_t>& _out) const;
    void clearSelection();
    void storeSelection(std::vector<note_t>& selNotes);
    size_t restoreSelection(std::vector<note_t>& selNotes);
    void deleteSelectedNotes(clip_notes_t& notes);
    void muteToggleSelectedNotes(clip_notes_t& notes);
    void addOrRemoveSelection(note_t* note);
};

struct noteview_cache_impl_t;
class noteview_render_t final : public clip_notes_t {
public:
    ~noteview_render_t();
    int32_t reqRevision         = -1;
    int32_t curRevision         = -1;
    noteview_cache_impl_t* data = nullptr;
};

inline bool operator==(const clip_notes_t& lhs, const clip_notes_t& rhs) {
    return lhs.m_list == rhs.m_list;
}
inline bool operator!=(const clip_notes_t& lhs, const clip_notes_t& rhs) { return !operator==(lhs, rhs); }

class clip_t {
    struct NoteSampleOptions {
        bool bCutNotes = false;
        bool bCutMutedNotes = false;
        bool bApplyGroove = false;
        bool bRelative = false;
        bool bEliminateDupes = false;
        tick_t minimalNoteLength = 0;
    };
public:
    clip_notes_t notes;
    clip_audio_t audio;
    clip_control_data_t controlData;
    int32_t selectedGroove = -1;
    tick_t time = 0;
    tick_t len = 0;

public:
    tick_t offsetStart       = 0;
    samplecount_t lenSamples = 0;
    tick_t loopStart         = 0;
    tick_t loopLen           = 0;
    int clipType             = CLIP_MIDI;
    String name;
    uint32_t rgb     = 0x2B82AD;
    bool enabled     = true;
    bool loopEnabled = false;
    clip_editor_layout_t editorLayout;

public:
#ifdef TRACK_ALLOCATIONS_CLIP_T
    int64_t allocId = 0;
    clip_t();
    ~clip_t();
#else
    clip_t() = default;
    ~clip_t() = default;
#endif
    clip_t(tick_t time, tick_t len, int32_t clipType = CLIP_MIDI);
    clip_t(const clip_t&);
    void setDirty() {
        this->dirty = true;
    }
    clip_t* clone() const {
        return new clip_t(*this);
    }
    clip_t& operator=(const clip_t& obj) {
        if (this != &obj) {
            copy(obj);
        }
        return *this;
    }
    void copy(const clip_t& obj);
    tick_t start() const {
        return time;
    }
    tick_t end() const {
        return time + getLen();
    }
    tick_t getLoopLength() const {
        return loopLen;
    }
    tick_t getLoopStart() const {
        return loopStart;
    }
    tick_t getOffsetStart() const {
        return time - offsetStart;
    }
    tick_t getLoopedTick(tick_t tick) const {
        tick_t pos = tick;
        auto preLoopLen = offsetStart < loopStart ? loopStart - offsetStart : 0;
        if (isLoopEnabled()) {
            if (tick >= preLoopLen) {
                pos += math::max<tick_t>(0, offsetStart - loopStart);
                pos = loopStart + ((pos - preLoopLen) % (loopLen));
            }
        } else {
            pos += offsetStart;
        }
        return pos;
    }
   
    int getInTimeRange(tick_t timeS, tick_t timeE, tick_t loopStart, tick_t loopEnd, std::vector<note_t>& list, NoteSampleOptions options) const;
    void getNotesView(tick_t timeS, tick_t timeE, clip_notes_t& notesView, NoteSampleOptions options) const;
    void applyNoteQuantizationGroove(const groove_data_t& grooveData, note_t& note, const note_t* nextNote) const;
    noteview_render_t& getNoteViewRender() const {
        updateNoteView();
        return this->noteViewRender;
    }
    noteview_render_t& getNoteViewFullClip() const {
        updateNoteView();
        return this->noteViewRenderFullClip;
    }
    noteview_render_t& getNoteViewSelection() const;
    void updateNoteViewSelection();
    tick_t getLoopBegin() const;
    tick_t getNumLoops() const;
    void adjustStartOffset(tick_t offset);

    tick_t getLen() const;
    tick_t& getLenRef();
    void setLen(tick_t _len = 0);
    void adjustLen(tick_t offset);
    samplecount_t getLenSamples() const;
    void setLenSamples(samplecount_t _lenSamples = 0);
    sample_fades_ref_t getSampleFadeIn(int32_t tempo100, samplerate_t sr) const;
    sample_fades_ref_t getSampleFadeOut(int32_t tempo100, samplerate_t sr) const;
    bool hasFadeIn() const {
        return audio.fadeIn.durationMs > 0;
    }
    bool hasFadeOut() const {
        return audio.fadeOut.durationMs > 0;
    }
    clip_fade_t& getFade(bool output) {
        return !output ? audio.fadeIn : audio.fadeOut;
    }
    bool isLoopEnabled() const {
        return loopEnabled && this->loopLen > 0;
    }
    bool isEmpty() const {
        return notes.isEmpty() && audio.isEmpty();
    }

    void setLoopEnabled(bool bLoopEnabled = true) {
        this->loopEnabled = bLoopEnabled;
    }

    std::vector<track_gui_entry_t*> trackEntries;

    gui_clip* getGuiClip(DawCtrl* parentCtrl);

private:
    mutable bool dirty = true;
    mutable noteview_render_t noteViewRender;
    mutable noteview_render_t noteViewRenderFullClip;
    mutable noteview_render_t noteViewSelection;
    void updateNoteView() const;
};

const note_t* getFirstAfter(const std::vector<note_t>& v, int32_t pitch, tick_t time);
const note_t* getFirstBefore(const std::vector<note_t>& v, int32_t pitch, tick_t time);
void cutClipLeft(clip_t* c, tick_t len);
void cutClipRight(clip_t* c, tick_t len);
inline bool operator==(const clip_t& lhs, const clip_t& rhs) {
    return lhs.time == rhs.time;//TODO: watch out!!
}
inline bool operator!=(const clip_t& lhs, const clip_t& rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const clip_t& lhs, const clip_t& rhs) {
    if (lhs.time == rhs.time) {
        return lhs.getLen() < rhs.getLen();
    }
    return lhs.time < rhs.time;
}
inline bool operator>(const clip_t& lhs, const clip_t& rhs) { return operator<(rhs, lhs); }
inline bool operator<=(const clip_t& lhs, const clip_t& rhs) { return !operator>(lhs, rhs); }
inline bool operator>=(const clip_t& lhs, const clip_t& rhs) { return !operator<(lhs, rhs); }
