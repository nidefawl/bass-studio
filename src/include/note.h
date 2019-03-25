#pragma once
#include "math/seq_math.h"
#include "seq_time.h"
#include "str_util.h"
#include <set>
#include <vector>
#include "str_util.h"

struct marker_t {
	tick_t time;
	int32_t color;
	String desc;
};

class track_t;
class gui_clip;
struct note_t {
public:
	int32_t pitch = 0;
	int32_t velocity = 127;
	tick_t time = 0;
	tick_t len = 0;
	bool enabled = true;
	inline tick_t start() const {
		return time;
	}
	inline tick_t end() const {
		return time+len;
	}
	void cutLeft(tick_t absTime) {
		tick_t endTime = end();
		time = absTime;
		len = endTime - time;
	}
	void cutRight(tick_t cutTime) {
		len = cutTime - time;
	}

	bool isIntersectTimeIncludeEnds(tick_t tickStart, tick_t tickEnd) const {
		return end() >= tickStart && start() < tickEnd;
	}
	bool isIntersectTime(tick_t tickStart, tick_t tickEnd) const {
		return end() > tickStart && start() < tickEnd;
	}
	bool isIntersectPitch(int32_t pitchL, int32_t pitchH) const {
		return this->pitch >= pitchL && this->pitch <= pitchH;
	}
};

struct noteevent_t {
	int32_t pitch = 0;
	int32_t velocity = 127;
	tick_t tickOffsetInBlock;
	bool isNoteOn;
	bool isLoopNoteOff;
	noteevent_t(int32_t p, int32_t v, tick_t t, bool b, bool b2) : pitch(p), velocity(v), tickOffsetInBlock(t), isNoteOn(b), isLoopNoteOff(b2) {

	}
};

inline bool operator==(const note_t& lhs, const note_t& rhs){
	return lhs.time == rhs.time && lhs.pitch == rhs.pitch;
}
inline bool operator!=(const note_t& lhs, const note_t& rhs){return !operator==(lhs,rhs);}
inline bool operator< (const note_t& lhs, const note_t& rhs){
	if (lhs.time == rhs.time) {
		if (lhs.pitch == rhs.pitch) {
			return lhs.len > rhs.len;
		}
		return lhs.pitch < rhs.pitch;
	}
	return lhs.time < rhs.time;
}
inline bool operator> (const note_t& lhs, const note_t& rhs){return  operator< (rhs,lhs);}
inline bool operator<=(const note_t& lhs, const note_t& rhs){return !operator> (lhs,rhs);}
inline bool operator>=(const note_t& lhs, const note_t& rhs){return !operator< (lhs,rhs);}


std::pair<note_t*, note_t*> getMinMaxSemitones(std::vector<note_t>& notes);
std::pair<note_t*, note_t*> getMinMaxTime(std::set<note_t*>& notePtrs);
std::pair<note_t*, note_t*> getMinMaxTime(std::vector<note_t>& notes);

inline int32_t getFoldedOffsetPitch(std::vector<int32_t>& notesFolded, int32_t curPitch, int32_t direction) {
	int len = (int) notesFolded.size();
	for (int i = 0; i < len; i++) {
		int32_t pitch = notesFolded[i];
		if (pitch >= curPitch) {
			return direction > 0 ? notesFolded[i+1 >= len ? len-1 : i+1] : notesFolded[i-1 <= 0 ? 0 : i-1];
		}
	}
	if (len == 0) return curPitch;
	return direction < 0 ? notesFolded[0] : notesFolded[len-1];
}
inline void changePitch(std::vector<note_t>& notesPtrs, int32_t semitones, bool fold, std::vector<int32_t> notesFolded) {
	if (fold && math::abs(semitones) == 1) {
		for (note_t& note : notesPtrs) {
			note.pitch = getFoldedOffsetPitch(notesFolded, note.pitch, semitones >= 0 ? 1 : -1);
		}
	} else {
		for (note_t& note : notesPtrs) {
			note.pitch += semitones;
		}
	}
}
inline void muteNotesToggle(std::vector<note_t>& notesPtrs) {
	for (note_t& note : notesPtrs) {
		note.enabled = !note.enabled;
	}
}
inline void offsetStartTime(std::vector<note_t>& notesPtrs, tick_t offset) {
	for (note_t& note : notesPtrs) {
		note.time += offset;
	}
}
inline void offsetEndTime(std::vector<note_t>& notesPtrs, tick_t offset, tick_t minLen) {
	for (note_t& note : notesPtrs) {
		if (note.len + offset < minLen) {
			note.len = note.len < minLen ? note.len : minLen;
		} else {
			note.len += offset;
		}
	}
}
int cutIntersecting(std::vector<note_t>& m_list, note_t& n, bool eliminateDupes);

void sortNoteEvents(std::vector<noteevent_t>& noteEvents);
