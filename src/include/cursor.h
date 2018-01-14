#pragma once
#include "seq_time.h"
#include <algorithm>
using std::max;
using std::min;

class Cursor {
public:
	Cursor() {
	}
	tick_t cursorPos = 0;
	int32_t cursorTrack = -1;
	tick_t selRange = 0;
	int32_t selTrackRange = 0;
	int32_t cursorSubTrack = -1;
	int32_t selSubTrackRange = 0;
	bool isSubtrackSelection() const {
		return cursorSubTrack > -1;
	}
	tick_t getTickBegin() const {
		return min(cursorPos, cursorPos + selRange);
	}
	tick_t getTickEnd() const {
		return max(cursorPos, cursorPos + selRange);
	}
	tick_t getTrackBegin() const {
		return min(cursorTrack, cursorTrack + selTrackRange);
	}
	tick_t getTrackEnd() const {
		return max(cursorTrack, cursorTrack + selTrackRange);
	}
	tick_t getSubTrackBegin() const {
		return min(cursorSubTrack, cursorSubTrack + selSubTrackRange);
	}
	tick_t getSubTrackEnd() const {
		return max(cursorSubTrack, cursorSubTrack + selSubTrackRange);
	}
	tick_t getRange() {
		if (cursorTrack < 0)
			return 0;
		return getTickEnd()-getTickBegin();
	}
	bool inTrackRange(int32_t track) const {
		return track >= getTrackBegin() && track <= getTrackEnd();
	}
	bool inSubTrackRange(int32_t track, int32_t subTrack) const {
		if (track >= getTrackBegin() && track <= getTrackEnd()) {
			if (isSubtrackSelection()) {
				return subTrack >= getSubTrackBegin() && subTrack <= getSubTrackEnd();
			}
			return true;
		}
		return false;
	}
	bool contains(int32_t track, tick_t tick) const {
		return track >= getTrackBegin() && track <= getTrackEnd() && tick >= getTickBegin() && tick < getTickEnd();
	}
	bool containsSubtrack(int32_t track, int32_t subTrack, tick_t tick) const {
		if (tick >= getTickBegin() && tick < getTickEnd() && track >= getTrackBegin() && track <= getTrackEnd()) {

			if (isSubtrackSelection()) {
				return subTrack >= getSubTrackBegin() && subTrack <= getSubTrackEnd();
			}
			return true;
		}
		return false;
	}
	void setLeftAligned() {
		int32_t stick = getTickBegin();
		int32_t etick = getTickEnd();
		int32_t str = getTrackBegin();
		int32_t etr = getTrackEnd();
		int32_t sstr = getSubTrackBegin();
		int32_t estr = getSubTrackEnd();
		cursorPos = stick;
		selRange = etick - stick;
		cursorTrack = str;
		selTrackRange = etr - str;
		cursorSubTrack = sstr;
		selSubTrackRange = estr - sstr;
	}
	Cursor getLeftAligned() {
		Cursor cursor;
		cursor = *this;
		cursor.setLeftAligned();
		return cursor;
	}
	void copy( const Cursor &obj) {
		this->cursorPos = obj.cursorPos;
		this->cursorTrack = obj.cursorTrack;
		this->cursorSubTrack = obj.cursorSubTrack;
		this->selRange = obj.selRange;
		this->selTrackRange = obj.selTrackRange;
		this->selSubTrackRange = obj.selSubTrackRange;
	}
	Cursor &operator =(const Cursor &a) {
		copy(a);
		return *this;
	}
	Cursor(const Cursor &a) {
		copy(a);
	}
	Cursor operator+(const Cursor &c2) const
	{
		Cursor tmp;
		tmp.cursorTrack = min(getTrackBegin(), c2.getTrackBegin());
		tmp.selTrackRange = max(getTrackEnd(), c2.getTrackEnd()) - tmp.selTrackRange;
		tmp.cursorPos = min(getTickBegin(), c2.getTickBegin());
		tmp.selRange = max(getTickEnd(), c2.getTickEnd()) - tmp.cursorPos;
		tmp.cursorSubTrack = min(getSubTrackBegin(), c2.getSubTrackBegin());
		tmp.selSubTrackRange = max(getSubTrackEnd(), c2.getSubTrackEnd()) - tmp.cursorSubTrack;
		return tmp;
	}
};
inline void fixCursorSubRange(Cursor& cursor, int32_t size) {
	if (!size) {
		cursor.cursorSubTrack = -1;
		cursor.selSubTrackRange = 0;
		return;
	}
	if (cursor.selSubTrackRange < 0) {
		while (cursor.selSubTrackRange <= -size) {
			cursor.selSubTrackRange++;
		}
	} else {
		while (cursor.selSubTrackRange >= size) {
			cursor.selSubTrackRange--;
		}
	}
	while (cursor.getSubTrackBegin() < 0) {
		cursor.cursorSubTrack++;
	}
	while (cursor.getSubTrackEnd() >= size) {
		cursor.cursorSubTrack--;
	}
}
inline void fixCursorTrackRange(Cursor& cursor, int32_t size) {
	if (!size) {
		cursor.cursorTrack = -1;
		cursor.selTrackRange = 0;
		return;
	}
	if (cursor.selTrackRange < 0) {
		while (cursor.selTrackRange <= -size) {
			cursor.selTrackRange++;
		}
	} else {
		while (cursor.selTrackRange >= size) {
			cursor.selTrackRange--;
		}
	}
	while (cursor.getTrackBegin() < 0) {
		cursor.cursorTrack++;
	}
	while (cursor.getTrackEnd() >= size) {
		cursor.cursorTrack--;
	}
}

