#pragma once
#include "seq_time.h"
#include "str_util.h"
#include <set>
#include <vector>

class track_t;
class gui_clip;
class note_t {
public:
	int32_t pitch = 0;
	tick_t time = 0;
	tick_t len = 0;
	bool enabled = true;
	note_t() {
	}
	note_t(const note_t &a) {
		copy(a);
	}
	note_t &operator =(const note_t &a) {
		copy(a);
		return *this;
	}
	void copy( const note_t &obj) {
		enabled = obj.enabled;
		pitch = obj.pitch;
		time = obj.time;
		len = obj.len;
	}
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

inline void changePitch(std::vector<note_t>& notesPtrs, int32_t semitones) {
	for (note_t& note : notesPtrs) {
		note.pitch += semitones;
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
