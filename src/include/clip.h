#pragma once
#include <list>
#include <map>
#include <vector>
#include <memory>
#include "math/seq_math.h"
#include "seq_time.h"
#include "shape.h"
#include "str_util.h"
#include "seq_util.h"
#include "note.h"
#include "layout.h"
#include "audiocache.h"
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
    rendered_audio_clip_t* renderedAudio = nullptr;
    clip_fade_t fadeIn;
    clip_fade_t fadeOut;

#ifndef NDEBUG
    /* debug only */
    samplecount_t lastReadBegin = -1;
    samplecount_t lastReadEnd = -1;
    samplecount_t lastReadLen = -1;
    samplecount_t lastWriteBegin = -1;
    samplecount_t lastWriteEnd = -1;
#endif
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
    }
    int32_t lenSamples() const;

    bool isEmpty() const {
        return id == -1;
    }
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
    int getInTimeRange(clip_t* clip, tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, std::vector<DAW::Host::midievent_ctrl_t>& list);
    void setFrom(clip_t* clip, tick_t tickBegin, tick_t len);
};

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

    void setTo(std::set<note_t*>& notePtrs, tick_t offset);
    void addAll(std::vector<note_t>& list);
    void removeAll(std::vector<note_t>& list);
    void removeAllKeepDuplicates(std::vector<note_t>& list);
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
class noteview_render_t : public clip_notes_t {
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
public:
    clip_notes_t notes;
    clip_audio_t audio;
    clip_control_data_t controlData;
    tick_t time = 0;
    //private:
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
    bool noLayout    = true;
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
    int getInTimeRange(tick_t timeS, tick_t timeE, tick_t loopStart, tick_t loopEnd, std::vector<note_t>& list);
    void getNotesView(tick_t timeS, tick_t timeE, clip_notes_t& notesView, bool forPlayback) const;
    noteview_render_t& getNoteViewRender() const {
        updateNoteView();
        return this->noteViewRender;
    }
    clip_notes_t& getNoteViewPlayback() const {
        updateNoteView();
        return this->noteViewPlayback;
    }
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

private:
    mutable bool dirty = true;
    mutable clip_notes_t noteViewPlayback;
    mutable noteview_render_t noteViewRender;
    void updateNoteView() const {
        if (dirty) {
            noteViewRender.reqRevision++;
            dirty = false;
            getNotesView(0, getLen(), noteViewPlayback, true);
            getNotesView(0, getLen(), noteViewRender, false);
        }
    }
};

note_t* getFirstAfter(std::vector<note_t>& v, int32_t pitch, tick_t time);
note_t* getFirstBefore(std::vector<note_t>& v, int32_t pitch, tick_t time);
inline void cutClipLeft(clip_t* c, tick_t len) {
    c->adjustStartOffset(len);
    c->time += len;
    c->setLen(c->getLen() - len);
    dbgassert(c->time > 0);
    dbgassert(c->getLenRef() > 0);
}
inline void cutClipRight(clip_t* c, tick_t len) {
    c->setLen(c->getLen() - len);
    dbgassert(c->getLenRef() > 0);
}
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
