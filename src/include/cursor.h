#pragma once
#include "math/seq_math.h"
#include "seq_time.h"
#include <vector>

namespace DAW {

    class TrackSelection {
    public:
        int32_t trackSelected = 0;
        std::vector<uint32_t> selectedTracks;
    };
    class Cursor {
    public:
        tick_t cursorPos         = 0;
        int32_t cursorTrack      = -1;
        tick_t selRange          = 0;
        int32_t selTrackRange    = 0;
        int32_t cursorSubTrack   = -1;
        int32_t selSubTrackRange = 0;

    public:
        bool isSubtrackSelection() const {
            return cursorSubTrack > -1;
        }
        tick_t getTickBegin() const {
            return math::min(cursorPos, cursorPos + selRange);
        }
        tick_t getTickEnd() const {
            return math::max(cursorPos, cursorPos + selRange);
        }
        int32_t getTrackBegin() const {
            return math::min(cursorTrack, cursorTrack + selTrackRange);
        }
        int32_t getTrackEnd() const {
            return math::max(cursorTrack, cursorTrack + selTrackRange);
        }
        int32_t getSubTrackBegin() const {
            return math::min(cursorSubTrack, cursorSubTrack + selSubTrackRange);
        }
        int32_t getSubTrackEnd() const {
            return math::max(cursorSubTrack, cursorSubTrack + selSubTrackRange);
        }
        int32_t getTrackRange() const {
            return getTrackEnd() - getTrackBegin();
        }
        void setTrack(int32_t track) {
            cursorTrack = track;
        }
        tick_t getRange() const {
            if (cursorTrack < 0)
                return 0;
            return getTickEnd() - getTickBegin();
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
            return track >= getTrackBegin() && track <= getTrackEnd() && tick >= getTickBegin() && tick <= getTickEnd();
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
        void setTrackBegin(int32_t track) {
            auto diff = this->cursorTrack - track;
            this->cursorTrack -= track;
            this->selTrackRange += diff;
        }
        void setTrackEnd(int32_t track) {
            this->selTrackRange = track - getTrackBegin();
        }
        void setBegin(tick_t t) {
            auto diff = this->cursorPos - t;
            this->cursorPos -= diff;
            this->selRange += diff;
        }
        void setEnd(tick_t t) {
            this->selRange = t - getTickBegin();
        }
        void setLeftAligned() {
            int32_t stick    = getTickBegin();
            int32_t etick    = getTickEnd();
            int32_t str      = getTrackBegin();
            int32_t etr      = getTrackEnd();
            int32_t sstr     = getSubTrackBegin();
            int32_t estr     = getSubTrackEnd();
            cursorPos        = stick;
            selRange         = etick - stick;
            cursorTrack      = str;
            selTrackRange    = etr - str;
            cursorSubTrack   = sstr;
            selSubTrackRange = estr - sstr;
        }
        void setEmptySelection() {
            *this = Cursor();
        }
        Cursor getLeftAligned() const {
            Cursor cursor;
            cursor = *this;
            cursor.setLeftAligned();
            return cursor;
        }
        Cursor expandTo(const Cursor& c) const {
            dbgassert(!isSubtrackSelection());
            dbgassert(!c.isSubtrackSelection());
            Cursor cursor  = *this;
            Cursor cursor2 = c;
            cursor.setLeftAligned();
            cursor2.setLeftAligned();
            cursor.setTrackBegin(math::min(cursor.getTrackBegin(), cursor2.getTrackBegin()));
            cursor.setTrackEnd(math::max(cursor.getTrackEnd(), cursor2.getTrackEnd()));
            cursor.setBegin(math::max(cursor.getTickBegin(), cursor2.getTickBegin()));
            cursor.setEnd(math::max(cursor.getTickEnd(), cursor2.getTickEnd()));
            return cursor;
        }
        void fixCursorSubRange(int32_t size) {
            DAW::Cursor& cursor = *this;
            if (!size) {
                cursor.cursorSubTrack   = -1;
                cursor.selSubTrackRange = 0;
                return;
            }
            if (cursor.selSubTrackRange < 0) {
                while (cursor.cursorSubTrack + cursor.selSubTrackRange < 0) {
                    cursor.selSubTrackRange++;
                }
            } else if (cursor.selSubTrackRange > 0) {
                while (cursor.cursorSubTrack + cursor.selSubTrackRange >= size) {
                    cursor.selSubTrackRange--;
                }
            }
            while (cursor.cursorSubTrack < 0) {
                cursor.cursorSubTrack++;
            }
            while (cursor.cursorSubTrack >= size) {
                cursor.cursorSubTrack--;
            }
        }
        void fixCursorTrackRange(int32_t size) {
            DAW::Cursor& cursor = *this;
            if (!size) {
                cursor.setTrack(-1);
                cursor.selTrackRange = 0;
                return;
            }
            if (cursor.selTrackRange < 0) {
                while (cursor.cursorTrack + cursor.selTrackRange < 0) {
                    cursor.selTrackRange++;
                }
            } else if (cursor.selTrackRange > 0) {
                while (cursor.cursorTrack + cursor.selTrackRange >= size) {
                    cursor.selTrackRange--;
                }
            }
            while (cursor.cursorTrack < 0) {
                cursor.cursorTrack++;
            }
            while (cursor.cursorTrack >= size) {
                cursor.cursorTrack--;
            }
        }
    };

}// namespace DAW

