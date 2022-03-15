#pragma once
#include <list>
#include <vector>
#include <memory>
#include "math/seq_math.h"
#include "seq_time.h"
#include "str_util.h"
#include "seq_util.h"
#include "note.h"
#include "layout.h"
#include "audiocache.h"
#include "wave/waveform_render.h"
#include "logging.h"
#include "assert_dbg.h"


#ifndef NDEBUG
#define TRACK_ALLOCATIONS_CLIP_T
#endif

#define CLIP_MIDI 0
#define CLIP_AUDIO 1

class clip_notes_t;
struct track_gui_entry_t;
int getClipNotesInTimeRange(tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, const clip_notes_t& notesView, std::vector<note_t>& list);

class track_t;
class gui_clip;

class clip_audio_t {

public:
    int32_t id = -1;
    std::weak_ptr<audiofile_t> weakCachedAudio;

public:
    clip_audio_t()  = default;
    ~clip_audio_t() = default;

    clip_audio_t& operator=(const clip_audio_t& a) {
        copy(a);
        return *this;
    }
    clip_audio_t(const clip_audio_t& a) {
        copy(a);
    }
    void copy(const clip_audio_t& obj) {
        this->id              = obj.id;
        this->weakCachedAudio = obj.weakCachedAudio;
    }
    int32_t lenSamples() const;
};

class clip_notes_t {
public:
    clip_notes_t() {
        m_list.reserve(128);
    }
    clip_notes_t& operator=(const clip_notes_t& obj) {
        copy(obj);
        return *this;
    }
    clip_notes_t(const clip_notes_t& obj) {
        copy(obj);
    }
    void copy(const clip_notes_t& obj);

    std::vector<note_t> m_list;
    std::set<note_t*> selection;
    note_t firstNote;
    note_t lastNote;
    note_t minNote;
    note_t maxNote;

    bool has(note_t* notePtr) const;
    bool empty() const {
        return m_list.empty();
    }
    note_t& add(note_t& t);                               // does not update bounds
    void remove(note_t& t);                               // does not update bounds
    void mute(note_t& t);                                 // does not update bounds
    note_t& addSingle(note_t& t);                         // updates bounds
    void removeSingle(note_t& t);                         // updates bounds
    int32_t paste(note_t& t, bool eliminateDupes = false);// updates bounds
    void updateBounds();
    note_t* get(tick_t time, int32_t pitch);
    size_t removeDuplicates();
    int getInRange(tick_t timeS, tick_t timeE, int32_t pitchL, int32_t pitchH, std::vector<note_t*>& list);
    int getStartsInRangeV(tick_t timeS, tick_t timeE, int32_t velL, int32_t velH, int32_t tickDist, std::vector<note_t*>& list);
    template<typename Functor>
    void visitNotes(Functor f) {
        std::for_each(m_list.begin(), m_list.end(), f);
    }
    template<typename Functor>
    void visitSelection(Functor f) {
        std::for_each(selection.begin(), selection.end(), f);
    }
    void setTo(std::set<note_t*>& notePtrs, tick_t offset);
    void addAll(std::vector<note_t>& list);
    void removeAll(std::vector<note_t>& list);
    void removeAllKeepDuplicates(std::vector<note_t>& list);
    void selectIdxRange(size_t start, size_t end);
    void selectLastN(size_t num);
    void storeSelection(std::vector<note_t>& selNotes);
    size_t restoreSelection(std::vector<note_t>& selNotes);


    void getSelectionIndices(std::vector<size_t>& selIdx) const {
        const auto begin = m_list.begin();
        for (note_t* n : selection) {
            auto it = std::find_if(m_list.begin(), m_list.end(),
                                   [n](const note_t& note) { return &note == n; });
            if (it == m_list.end()) {
                dbgassert(0);
            }
            selIdx.push_back(static_cast<size_t>(it - begin));
        }
        dbgassert(selection.size() == selIdx.size());
    }
    bool hasDuplicates() const {
        return any_duplicates(m_list);
    }

    void deleteSelectedNotes(clip_notes_t& notes) {
        std::vector<note_t> delNotes(selection.size());
        copySelectionTo(delNotes);
        clearSelection();
        for (note_t& note : delNotes) {
            notes.remove(note);
        }
        notes.updateBounds();
    }
    void muteToggleSelectedNotes(clip_notes_t& notes) {
        std::vector<note_t> delNotes(selection.size());
        copySelectionTo(delNotes);
        for (note_t& note : delNotes) {
            notes.mute(note);
        }
        //notes.updateBounds();
    }
    void getNotePitches(std::vector<int32_t>& out) {
        for (note_t& note : m_list) {
            auto it = std::find_if(out.begin(), out.end(), [&note](const int32_t& n) {
                return note.pitch == n;
            });
            if (it == out.end()) {
                out.push_back(note.pitch);
            }
        }
        stable_sort(out.begin(), out.end(), [](int32_t const a, int32_t const b) {
            return a < b;
        });
    }
    void addOrRemoveSelection(note_t* note) {

        auto it = std::find(selection.begin(), selection.end(), note);
        if (it != selection.end()) {
            selection.erase(it);
        } else {
            selection.insert(note);
        }
    }
    void clearSelection() {
        selection.clear();
        removeDuplicates();
    }
    void copySelectionTo(std::vector<note_t>& _out) const {
        _out.clear();
        for (note_t* note : selection) {
            note_t& ref = *note;
            _out.push_back(ref);
        }
    }
    void clear() {
        copy(clip_notes_t());
    }
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
    tick_t time = 0;
    //private:
    tick_t len = 0;

public:
    tick_t offsetStart    = 0;
    int32_t offsetSamples = 0;
    tick_t lenSamples     = 0;
    tick_t loopStart      = 0;
    tick_t loopLen        = 0;
    int clipType          = CLIP_MIDI;
    String name;
    uint32_t rgb     = 0x2B82AD;
    bool enabled     = true;
    bool loopEnabled = true;
    bool noLayout    = true;
    clip_editor_layout_t editorLayout;

public:
#ifdef TRACK_ALLOCATIONS_CLIP_T
    int64_t allocId;
    clip_t();
    ~clip_t();
#else
    clip_t() = default;
    ~clip_t() = default;
#endif
    clip_t(const clip_t&);
    void setDirty() {
        this->dirty = true;
    }
    clip_t* clone() const {
        return new clip_t(*this);
    }
    clip_t& operator=(const clip_t& obj) {
        copy(obj);
        return *this;
    }
    void copy(const clip_t& obj) {
        this->name    = StringLimit(obj.name, 64);
        clipType      = obj.clipType;
        enabled       = obj.enabled;
        rgb           = obj.rgb;
        time          = obj.time;
        len           = obj.len;
        offsetStart   = obj.offsetStart;
        offsetSamples = obj.offsetSamples;
        lenSamples    = obj.lenSamples;
        loopStart     = obj.loopStart;
        loopLen       = obj.loopLen;
        loopEnabled   = obj.loopEnabled;
        notes         = obj.notes;
        audio         = obj.audio;
        noLayout      = obj.noLayout;
        editorLayout  = obj.editorLayout;
        dirty         = true;
    }
    tick_t start() const {
        return time;
    }
    tick_t end() const {
        return time + getLen();
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
    void adjustStartSamples(tick_t offset);
    void adjustStartOffset(tick_t offset) {
        if (clipType == CLIP_AUDIO) {
            //adjustStartSamples(offset);
            //return;
        }
        if (isLoopEnabled() && offsetStart < loopStart) {
            tick_t lenAdj = math::min(offset, loopStart - offsetStart);
            offsetStart += lenAdj;
            offset -= lenAdj;
        }
        bool inLoop = isLoopEnabled() && offsetStart >= loopStart;
        this->offsetStart += offset;
        while (inLoop && offsetStart < loopStart) {
            offsetStart += loopLen;
        }
        while (inLoop && offsetStart >= loopStart + loopLen) {
            offsetStart -= loopLen;
        }
    }

    tick_t getLen() const;
    tick_t& getLenRef();
    void setLen(tick_t _len = 0);
    void adjustLen(tick_t offset);
    tick_t getLenSamples() const;
    void setLenSamples(tick_t _lenSamples = 0);

    bool isLoopEnabled() const {
        return loopEnabled && this->loopLen > 0;
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
