#pragma once
#include "logging.h"
#include "math/seq_math.h"
#include "seq_time.h"
#include "str_util.h"
#include <set>
#include <vector>
#include "str_util.h"

#include "assert_dbg.h"

struct marker_t {
    tick_t time;
    int32_t color;
    String desc;
    float yOffset = 0;
};

class track_t;
class gui_clip;
inline float velocityToFloat(int32_t velocity) {
    return velocity / 127.0F;
}
#define lenTicksInfinite TICKS_BAR*16;
namespace NoteFlags {
    static constexpr int32_t ENABLED  = 1;
    static constexpr int32_t REALTIME = 2;
    static constexpr int32_t IS_HELD  = 4;
}// namespace NoteFlags
struct note_t {
public:
    int32_t pitch    = 0;
    int32_t velocity = 127;
    tick_t time      = 0;
    tick_t len       = 0;
    int32_t flags    = NoteFlags::ENABLED;
    int8_t channel   = 0;
    inline void setEnabled(bool bIsEnabled) {
        if (bIsEnabled) {
            flags |= NoteFlags::ENABLED;
        } else {
            flags &= ~NoteFlags::ENABLED;
        }
    }
    inline void setRealtime(bool bIsRealtime) {
        if (bIsRealtime) {
            flags |= NoteFlags::REALTIME;
        } else {
            flags &= ~NoteFlags::REALTIME;
        }
    }
    inline void setIsHeld(bool bIsHeld) {
        if (bIsHeld) {
            flags |= NoteFlags::IS_HELD;
        } else {
            flags &= ~NoteFlags::IS_HELD;
        }
    }
    inline bool isHeld() const {
        return flags & NoteFlags::IS_HELD;
    }
    inline bool isEnabled() const {
        return flags & NoteFlags::ENABLED;
    }
    inline bool isRealtime() const {
        return flags & NoteFlags::REALTIME;
    }
    inline void toggleFlag(int32_t flag) {
        flags ^= flag;
    }
    inline tick_t start() const {
        return time;
    }
    inline tick_t end() const {
        return time + len;
    }
    void cutLeft(tick_t absTime) {
        tick_t endTime = end();
        time           = absTime;
        len            = endTime - time;
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
    bool isIntersectVel(int32_t velL, int32_t velH) const {
        return this->velocity >= velL && this->velocity <= velH;
    }
};

inline bool operator==(const note_t& lhs, const note_t& rhs) {
    return lhs.time == rhs.time && lhs.pitch == rhs.pitch && lhs.channel == rhs.channel;
}
inline bool operator!=(const note_t& lhs, const note_t& rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const note_t& lhs, const note_t& rhs) {
    if (lhs.time == rhs.time) {
        if (lhs.pitch == rhs.pitch) {
            if (lhs.channel == rhs.channel) {
                if (lhs.len == rhs.len) {
                    return lhs.velocity < rhs.velocity;
                }
                return lhs.len < rhs.len;
            }
            return lhs.channel < rhs.channel;
        }
        return lhs.pitch < rhs.pitch;
    }
    return lhs.time < rhs.time;
}

std::pair<note_t*, note_t*> getMinMaxSemitones(std::vector<note_t>& notes);
std::pair<note_t*, note_t*> getMinMaxTime(std::set<note_t*>& notePtrs);
std::pair<note_t*, note_t*> getMinMaxTime(std::vector<note_t>& notes);

inline int32_t getFoldedOffsetPitch(std::vector<int32_t>& notesFolded, int32_t curPitch, int32_t direction) {
    auto len = (int32_t) notesFolded.size();
    for (int32_t i = 0; i < len; i++) {
        int32_t pitch = notesFolded[i];
        if (pitch >= curPitch) {
            return direction > 0 ? notesFolded[i + 1 >= len ? len - 1 : i + 1] : notesFolded[i - 1 <= 0 ? 0 : i - 1];
        }
    }
    if (len == 0) return curPitch;
    return direction < 0 ? notesFolded[0] : notesFolded[len - 1];
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
    if (notesPtrs.empty()) return;
    bool bIsEnabled = !notesPtrs[0].isEnabled();
    for (note_t& note : notesPtrs) {
        note.setEnabled(bIsEnabled);
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
void quantizeNoteStartTime(std::vector<note_t>& notesPtrs, tick_t quantize);
void quantizeNoteEndTime(std::vector<note_t>& notesPtrs, tick_t quantize);
int cutIntersectingEliminateDupes(std::vector<note_t>& m_list, note_t& n, bool eliminateDupes);
bool cutSelfIntersecting(std::vector<note_t>& m_list);


/* shortens end of intersecting notes, does not remove any notes, instead looks for exact duplicates
 * returns: -1 if exact duplicate is present
 * otherwise the return value is a positive number and represents the number notes modified in the list
 */
template<typename T>
int cutIntersectingNotesFindDupe(std::vector<T>& m_list, T& n) {
    auto it = m_list.begin();
    // find exact duplicate
    while (it != m_list.end()) {
        const T& val = *it++;
        if (val.isEnabled() && val.pitch == n.pitch && val.time == n.time && val.len == n.len && val.channel == n.channel) {
            return -1;
        }
    }

    int nAdjusted = 0;
    for (it = m_list.begin(); it != m_list.end(); ++it) {
        T& c = *it;
        if (!c.isEnabled())
            continue;
        if (c.pitch != n.pitch) {
            continue;
        }
        if (c.channel != n.channel) {
            continue;
        }
        if (c.start() >= n.end() || c.end() <= n.start()) {
            continue;
        }
        if (c.start() < n.start()) {
            c.cutRight(n.start());
            nAdjusted++;
        } else if (c.start() > n.start()) {
            n.cutRight(c.start());
            nAdjusted++;
        }
    }
    return nAdjusted;
}

namespace DAW {
    inline constexpr int32_t ToNoteNumber(int32_t octave, int32_t note) {
        return ((octave + 2) * 12) + note;
    }
}