#pragma once
#include <list>
#include <vector>
#include "note.h"
#include "seq_time.h"
#include "seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "logging.h"
#include "layout.h"
#include "audiocache.h"
#include "../gui/drawwaveform.h"
#include <assert.h>
#include <memory>

#define CLIP_MIDI 0
#define CLIP_AUDIO 1

class track_t;
class gui_clip;
//struct cachedaudio_t;
class clip_audio_t {
public:
	int32_t id = -1;
	gui_waveform_texture_ref waveformRef;
	std::weak_ptr<cachedaudio_t> weakCachedAudio;

	clip_audio_t() {
	}
	clip_audio_t &operator =(const clip_audio_t &a) {
		copy(a);
		return *this;
	}
	clip_audio_t(const clip_audio_t &a) {
		copy(a);
	}
	~clip_audio_t() {
		if (waveformRef.rendered) {
			waveformrender* renderer = waveformrender::getInstance();
			if (renderer) {
				waveformrender::getInstance()->release(waveformRef.fbId);
			}
		}
	}
	void copy( const clip_audio_t &obj) {
		this->id = obj.id;
		this->waveformRef = obj.waveformRef;
		this->weakCachedAudio = obj.weakCachedAudio;
		this->waveformRef.fbId = -1;
		this->waveformRef.rendered = false;
	}
	tick_t lenSamples();
};
class clip_notes_t {
public:
	clip_notes_t() {
		m_list.reserve(128);
	}
	clip_notes_t &operator =(const clip_notes_t &a) {
		copy(a);
		return *this;
	}
	clip_notes_t(const clip_notes_t &a) {
		copy(a);
	}
	void copy( const clip_notes_t &obj);

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
	note_t& add(note_t& t);			// does not update bounds
	void remove(note_t& t);			// does not update bounds
	void mute(note_t& t);			// does not update bounds
	note_t& addSingle(note_t& t);	// updates bounds
	void removeSingle(note_t& t);	// updates bounds
	int32_t paste(note_t& t, bool eliminateDupes = false);		// updates bounds
	void updateBounds();
	note_t* get(tick_t time, int32_t pitch);
	size_t removeDuplicates();
	int getInRange(tick_t timeS, tick_t timeE, int32_t pitchL, int32_t pitchH, std::vector<note_t*>& list);
	void setTo(std::set<note_t*>& notePtrs, tick_t offset);
	void addAll(std::vector<note_t>& list);
	void removeAll(std::vector<note_t>& list);
	void removeAllKeepDuplicates(std::vector<note_t>& list);
	void selectIdxRange(size_t start, size_t end);
	void selectLastN(size_t num);
	void storeSelection(std::vector<note_t>& selNotes);
	void restoreSelection(std::vector<note_t>& selNotes);


	void getSelectionIndices(std::vector<size_t>& selIdx) const {
		const auto begin = m_list.begin();
		for (note_t* n : selection) {
			auto it = std::find_if(m_list.begin(), m_list.end(),
					[n] (const note_t& note) { return &note == n; });
			if (it == m_list.end()) {
				assert(0);
			}
			selIdx.push_back(it - begin);
		}
		assert(selection.size() == selIdx.size());
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
//		notes.updateBounds();
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
		stable_sort(out.begin(), out.end(), [](
				int32_t const a, int32_t const b) {
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
};
inline bool operator==(const clip_notes_t& lhs, const clip_notes_t& rhs){
	return lhs.m_list == rhs.m_list;
}
inline bool operator!=(const clip_notes_t& lhs, const clip_notes_t& rhs){return !operator==(lhs,rhs);}
class clip_t {
public:
	clip_notes_t notes;
	clip_audio_t audio;
	tick_t time = 0;
//private:
	tick_t len = 0;
public:
	tick_t offsetStart = 0;
	int32_t offsetSamples = 0;
	tick_t lenSamples = 0;
	tick_t loopStart = 0;
	tick_t loopLen = 0;
	int clipType = CLIP_MIDI;
	String name;
	int rgb = 0x2B82AD;
	bool enabled = true;
	bool loopEnabled = true;
	bool noLayout = true;
	clip_editor_layout_t editorLayout;

	clip_t(int _clipType, String _name) : clipType(_clipType) {
		this->name = _name;
	}
	clip_t() {
	}
	void setDirty() {
		this->dirty = true;
	}
	clip_t* clone() const {
		return new clip_t(*this);
	}
	clip_t &operator =(const clip_t &a) {
		copy(a);
		return *this;
	}
	clip_t(const clip_t &a) {
		copy(a);
	}
	void copy( const clip_t &obj) {
		this->name = StringLimit(obj.name, 64);
		clipType = obj.clipType;
		enabled = obj.enabled;
		rgb = obj.rgb;
		time = obj.time;
		len = obj.len;
		offsetStart = obj.offsetStart;
		offsetSamples = obj.offsetSamples;
		lenSamples = obj.lenSamples;
		loopStart = obj.loopStart;
		loopLen = obj.loopLen;
		loopEnabled = obj.loopEnabled;
		notes = obj.notes;
		audio = obj.audio;
		noLayout = obj.noLayout;
		editorLayout = obj.editorLayout;
		gClip = NULL;
		dirty = true;
	}
	tick_t start() const {
		return time;
	}
	tick_t end() const {
		return time+getLen();
	}
	tick_t getOffsetStart() const {
		return time-offsetStart;
	}
	int getInTimeRange(tick_t timeS, tick_t timeE, tick_t loopStart, tick_t loopEnd, std::vector<note_t>& list);
	void getNotesView(tick_t timeS, tick_t timeE, clip_notes_t& notesView, bool forPlayback) const;
	clip_notes_t& getNoteViewRender() const {
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
			adjustStartSamples(offset);
			return;
		}
		if (loopEnabled && offsetStart < loopStart) {
			tick_t lenAdj = min(offset, loopStart - offsetStart);
			offsetStart += lenAdj;
			offset -= lenAdj;
		}
		bool inLoop = loopEnabled && offsetStart >= loopStart;
		this->offsetStart += offset;
		while (inLoop && offsetStart < loopStart) {
			offsetStart += loopLen;
		}
		while (inLoop && offsetStart >= loopStart+loopLen) {
			offsetStart -= loopLen;
		}
	}

	tick_t getLen() const;
	tick_t& getLenRef();
	void setLen(tick_t len = 0);
	void adjustLen(tick_t offset);
	tick_t getLenSamples() const;
	void setLenSamples(tick_t lenSamples = 0);

	gui_clip* gClip = NULL;
//	track_t* tr = NULL;
private:
	mutable bool dirty = true;
	mutable clip_notes_t noteViewPlayback;
	mutable clip_notes_t noteViewRender;
	void updateNoteView() const {
		if (dirty) {
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
//	c->len -= len;
	c->setLen(c->getLen()-len);
	assert(c->time>0);
	assert(c->getLenRef()>0);
}
inline void cutClipRight(clip_t* c, tick_t len) {
//	c->len -= len;
	c->setLen(c->getLen()-len);
	assert(c->getLenRef()>0);
}
inline bool operator==(const clip_t& lhs, const clip_t& rhs){
	return lhs.time == rhs.time; //TODO: watch out!!
}
inline bool operator!=(const clip_t& lhs, const clip_t& rhs){return !operator==(lhs,rhs);}
inline bool operator< (const clip_t& lhs, const clip_t& rhs){
	if (lhs.time == rhs.time) {
		return lhs.getLen() < rhs.getLen();
	}
	return lhs.time < rhs.time;
}
inline bool operator> (const clip_t& lhs, const clip_t& rhs){return  operator< (rhs,lhs);}
inline bool operator<=(const clip_t& lhs, const clip_t& rhs){return !operator> (lhs,rhs);}
inline bool operator>=(const clip_t& lhs, const clip_t& rhs){return !operator< (lhs,rhs);}


