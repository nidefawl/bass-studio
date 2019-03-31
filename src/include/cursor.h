#pragma once
#include "math/seq_math.h"
#include "seq_time.h"

class Cursor {
public:
	tick_t cursorPos = 0;
	int32_t cursorTrack = -1;
	tick_t selRange = 0;
	int32_t selTrackRange = 0;
	int32_t cursorSubTrack = -1;
	int32_t selSubTrackRange = 0;
public:
	Cursor() = default;
	Cursor(const Cursor& ref) = default;
	Cursor& operator=(const Cursor& ref) = default;
	Cursor(Cursor&& ref) noexcept = default;
	Cursor& operator=(Cursor&& ref) noexcept = default;
	bool isSubtrackSelection() const {
		return cursorSubTrack > -1;
	}
	tick_t getTickBegin() const {
		return math::min(cursorPos, cursorPos + selRange);
	}
	tick_t getTickEnd() const {
		return math::max(cursorPos, cursorPos + selRange);
	}
	tick_t getTrackBegin() const {
		return math::min(cursorTrack, cursorTrack + selTrackRange);
	}
	tick_t getTrackEnd() const {
		return math::max(cursorTrack, cursorTrack + selTrackRange);
	}
	tick_t getSubTrackBegin() const {
		return math::min(cursorSubTrack, cursorSubTrack + selSubTrackRange);
	}
	tick_t getSubTrackEnd() const {
		return math::max(cursorSubTrack, cursorSubTrack + selSubTrackRange);
	}
	tick_t getRange() {
		if (cursorTrack < 0)
			return 0;
		return getTickEnd()-getTickBegin();
	}
	bool inTrackRange(int32_t track) const {
		return track >= getTrackBegin() && track <= getTrackEnd();
	}
	bool inSubTrack(int32_t track, int32_t subTrack) const {
		if (track >= getTrackBegin() && track <= getTrackEnd()) {
			if (isSubtrackSelection()) {
				return subTrack >= getSubTrackBegin() && subTrack <= getSubTrackEnd();
			}
		}
		return false;
	}
	bool inSubTrackAny(int32_t track) const {
		if (track >= getTrackBegin() && track <= getTrackEnd()) {
			if (isSubtrackSelection()) {
				return true;
			}
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
	void setEmptySelection() {
		*this = Cursor();
	}
	Cursor getLeftAligned() {
		Cursor cursor;
		cursor = *this;
		cursor.setLeftAligned();
		return cursor;
	}
//	void copy( const Cursor &obj) {
//		this->cursorPos = obj.cursorPos;
//		this->cursorTrack = obj.cursorTrack;
//		this->cursorSubTrack = obj.cursorSubTrack;
//		this->selRange = obj.selRange;
//		this->selTrackRange = obj.selTrackRange;
//		this->selSubTrackRange = obj.selSubTrackRange;
//	}
//	Cursor &operator =(const Cursor &a) {
//		copy(a);
//		return *this;
//	}
//	Cursor(const Cursor &a) {
//		copy(a);
//	}
	Cursor operator+(const Cursor &c2) const
	{
		Cursor tmp;
		tmp.cursorTrack = math::min(getTrackBegin(), c2.getTrackBegin());
		tmp.selTrackRange = math::max(getTrackEnd(), c2.getTrackEnd()) - tmp.selTrackRange;
		tmp.cursorPos = math::min(getTickBegin(), c2.getTickBegin());
		tmp.selRange = math::max(getTickEnd(), c2.getTickEnd()) - tmp.cursorPos;
		tmp.cursorSubTrack = math::min(getSubTrackBegin(), c2.getSubTrackBegin());
		tmp.selSubTrackRange = math::max(getSubTrackEnd(), c2.getSubTrackEnd()) - tmp.cursorSubTrack;
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

