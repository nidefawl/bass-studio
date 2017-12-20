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
	tick_t getRange() {
		if (cursorTrack < 0)
			return 0;
		return getTickEnd()-getTickBegin();
	}
	bool inTrackRange(int32_t track) const {
		return track >= getTrackBegin() && track <= getTrackEnd();
	}
	bool contains(int32_t track, tick_t tick) const {
		return track >= getTrackBegin() && track <= getTrackEnd() && tick >= getTickBegin() && tick < getTickEnd();
	}
	void setLeftAligned() {
		int32_t stick = getTickBegin();
		int32_t etick = getTickEnd();
		int32_t str = getTrackBegin();
		int32_t etr = getTrackEnd();
		cursorPos = stick;
		selRange = etick - stick;
		cursorTrack = str;
		selTrackRange = etr - str;
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
		this->selRange = obj.selRange;
		this->selTrackRange = obj.selTrackRange;
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
		tmp.selTrackRange = max(getTrackEnd(), c2.getTrackEnd()) - cursorTrack;
		tmp.cursorPos = min(getTickBegin(), c2.getTickBegin());
		tmp.selRange = max(getTickEnd(), c2.getTickEnd()) - cursorPos;
		return tmp;
	}
};

