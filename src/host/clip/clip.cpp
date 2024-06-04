#include <algorithm>
#include <vector>
#include "assert_dbg.h"
#include <cstddef>
#include <limits>
#include "note.h"
#include "plugins/synth/IPlugMidi.h"
#include "rand.h"
#include "seq_util.h"
#include "host/shape/shape.h"
#include "str_util.h"
#include "math/seq_math.h"
#include "exceptions.h"
#include "clip.h"
#include "logging.h"
#include "seq_time.h"
#include "host/audiocache/audiocache.h"
#include "host/project/project.h"
#include "host/project/projectcontroller.h"
#include "tls.h"
#include "types.h"
#include "host/host.h"
#include "util/debug_alloc.h"
#include "gui/track/trackcontent.h"

#ifdef TRACK_ALLOCATIONS_CLIP_T
namespace DebugAlloc {
    Tracker<clip_t> trackerClips;
    template<>
    Tracker<clip_t>* getTracker() {
        return &trackerClips;
    }
}
int32_t getNumClipAllocations() {
    auto tracker = DebugAlloc::getTracker<clip_t>();
    dbgassert(tracker->allocCount < (std::numeric_limits<int32_t>::max()));
    return tracker->allocCount;
}
void printClipAllocations() {
    DebugAlloc::getTracker<clip_t>()->onExit();
}
clip_t::clip_t() {
    allocId = DebugAlloc::getTracker<clip_t>()->objConstructor(this);
}
clip_t::~clip_t() {
    DebugAlloc::getTracker<clip_t>()->objDestructor(this);
}
clip_t::clip_t(const clip_t& a) : clip_t() {
    copy(a);
}
#else
int32_t getNumClipAllocations() {
    return 0;
}
void printClipAllocations() {
}
clip_t::clip_t(const clip_t& a) {
    copy(a);
}
#endif

clip_t::clip_t(tick_t time, tick_t len, int32_t clipType) {
#ifdef TRACK_ALLOCATIONS_CLIP_T
    allocId = DebugAlloc::getTracker<clip_t>()->objConstructor(this);
#endif
    this->time = time;
    this->len = len;
    this->clipType = clipType;
}


note_t& clip_notes_t::addSingle(note_t& t) {
    dbgassert(selection.empty());
    m_list.push_back(t);
    updateBounds();
    return m_list.back();
}

note_t& clip_notes_t::add(note_t& t) {
    dbgassert(selection.empty());
    m_list.push_back(t);
    return m_list.back();
}

int32_t clip_notes_t::paste(note_t& t, bool eliminateDupes) {
    dbgassert(selection.empty());
    cutIntersectingEliminateDupes(m_list, t, eliminateDupes);
    add(t);
    return 0;
}

void clip_notes_t::removeSingle(note_t& t) {
    dbgassert(selection.empty());
    auto it = std::find(m_list.begin(), m_list.end(), t);
    if (it == m_list.end()) {
        throw applogicexception("track - attempt to remove non-present note");
    }
    m_list.erase(it);
    updateBounds();
}

void clip_notes_t::remove(note_t& t) {
    dbgassert(selection.empty());
    auto it = std::find(m_list.begin(), m_list.end(), t);
    if (it == m_list.end()) {
        throw applogicexception("track - attempt to remove non-present note");
    }
    m_list.erase(it);
}

void clip_notes_t::mute(note_t& t) {
    auto it = std::find(m_list.begin(), m_list.end(), t);
    if (it == m_list.end()) {
        throw applogicexception("track - attempt to mute non-present note");
    }
    note_t& noteFound = *it;
    noteFound.toggleFlag(NoteFlags::ENABLED);
}

void clip_notes_t::addAll(std::vector<note_t>& list) {
    dbgassert(selection.empty());
    //TODO: maybe add reserve here?
    m_list.insert(std::end(m_list), std::begin(list), std::end(list));
}

void clip_notes_t::removeAllKeepDuplicates(std::vector<note_t>& a) {
    dbgassert(selection.empty());
    auto aBegin = a.begin();
    auto aEnd   = a.end();
    while (aBegin != aEnd) {
        auto itRemove = std::find(m_list.begin(), m_list.end(), *aBegin);
        //dbgassert(itRemove != m_list.end());
        if (itRemove != m_list.end()) {

            m_list.erase(itRemove);
        }
        aBegin++;
    }
}

void clip_notes_t::removeAll(std::vector<note_t>& a) {
    dbgassert(selection.empty());
    m_list.erase(std::remove_if(m_list.begin(), m_list.end(), [&a](const note_t& x) {
                     return std::find(a.begin(), a.end(), x) != a.end();
                 }),
                 m_list.end());
}

void clip_notes_t::setTo(std::set<note_t*>& notePtrs, tick_t offset) {
    for (note_t* notePtr : notePtrs) {
        note_t note = *notePtr;
        note.time += offset;
        m_list.push_back(note);
    }
    updateBounds();
}

bool clip_notes_t::has(note_t* notePtr) const {
    for (const note_t& note : m_list) {
        if (notePtr == &note)
            return true;
    }
    return false;
}

size_t removeDuplicatesImpl(std::vector<note_t>& m_list) {
    std::sort(m_list.begin(), m_list.end());
    auto itNewEnd  = std::unique(m_list.begin(), m_list.end(), [](const note_t& a, const note_t& b) { return note_t::IsIequalMidi(a, b); });
    size_t removed = m_list.end() - itNewEnd;
    m_list.erase(itNewEnd, m_list.end());
    return removed;
}

int cutIntersectingEliminateDupes(std::vector<note_t>& m_list, note_t& n, bool eliminateDupes) {
    int nErased    = 0;
    auto it        = m_list.begin();
    int exactDupes = 0;
    while (it != m_list.end()) {
        note_t& c = *it;
        if (c == n) {
            if (eliminateDupes || exactDupes) {
                it = m_list.erase(it);
                nErased++;
            } else {
                it++;// allow exact duplicates
            }
            exactDupes++;
        } else if (c.pitch != n.pitch || c.channel != n.channel) {
            it++;
        } else if (c.start() >= n.end() || c.end() <= n.start()) {
            it++;
        } else if (c.time < n.start()) {
            c.cutRight(n.start());
            it++;
        } else {
            it = m_list.erase(it);
            nErased++;
        }
    }
    return nErased;
}

bool cutSelfIntersecting(std::vector<note_t>& m_list) {
    removeDuplicatesImpl(m_list);
    bool hadAnyIntersections = false;
    bool hasIntersections = true;
    while (hasIntersections) {
        hasIntersections = false;
        for (note_t& n : m_list) {
            auto it = m_list.begin();
            while (it != m_list.end()) {
                note_t& c = *it;
                if (&n == &c) {
                    it++;
                    continue;
                }
                if (c == n) {
                    it = m_list.erase(it);
                    hasIntersections = true;
                    break;
                } else if (c.pitch != n.pitch || c.channel != n.channel) {
                    it++;
                } else if (c.start() >= n.end() || c.end() <= n.start()) {
                    it++;
                } else if (c.start() < n.start()) {
                    c.cutRight(n.start());
                    it++;
                    hasIntersections = true;
                    break;
                } else {
                    it = m_list.erase(it);
                    // dbgassert(0);
                    break;
                }
            }
            if (hasIntersections) {
                break;
            }
        }
        if (hasIntersections) {
            hadAnyIntersections = true;
        }
    }
    return hadAnyIntersections;
}

void clip_notes_t::storeSelection(std::vector<note_t>& selNotes) {
    for (note_t* n : selection) {
        selNotes.push_back(*n);// copy;
    }
}

size_t clip_notes_t::restoreSelection(std::vector<note_t>& selNotes) {
    size_t numRestored = 0;
    for (note_t& n : selNotes) {
        auto it = std::find_if(m_list.begin(), m_list.end(),
                               [&n](const note_t& note) { return note == n; });
        if (it != m_list.end()) {
            note_t& ref = *it;
            selection.insert(&ref);
            numRestored++;
        }
    }
    return numRestored;
}

size_t clip_notes_t::removeDuplicates() {
    size_t nRemoved;
    if (!selection.empty()) {
        std::vector<note_t> selNotes;
        storeSelection(selNotes);
        selection.clear();
        nRemoved = removeDuplicatesImpl(m_list);
#ifndef NDEBUG
        always_assert(restoreSelection(selNotes));
        for (note_t* n : selection) {
            auto it = std::find_if(m_list.begin(), m_list.end(),
                                   [n](const note_t& note) { return &note == n; });
            dbgassert(it != m_list.end());
        }
#else
        restoreSelection(selNotes);
#endif // NDEBUG
    } else {
        nRemoved = removeDuplicatesImpl(m_list);
    }
    updateBounds();
    return nRemoved;
}

void clip_notes_t::copy(const clip_notes_t& obj) {
    dbgassert(this != &obj);
    m_list = obj.m_list;
    selection.clear();
    if (!obj.selection.empty()) {
        const note_t* baseOther = obj.m_list.data();
        note_t* baseOwn         = m_list.data();
        for (note_t* notePtr : obj.selection) {
            const ptrdiff_t diff = notePtr - baseOther;
            note_t* ownPtr       = baseOwn + diff;
            selection.insert(ownPtr);
        }
    }
    dbgassert(obj.selection.size() == selection.size());
    firstNote = obj.firstNote;
    lastNote  = obj.lastNote;
    minNote   = obj.minNote;
    maxNote   = obj.maxNote;
}

note_t* clip_notes_t::get(tick_t time, int32_t pitch) {
    auto it = m_list.rbegin();
    while (it != m_list.rend()) {
        note_t& note = *it;
        if (pitch == note.pitch && time >= note.start() && time < note.end()) {
            return &note;
        }
        it++;
    }
    return NULL;
}

int clip_notes_t::getStartsInRangeV(tick_t timeS, tick_t timeE, int32_t velL, int32_t velH, int32_t tickDist, std::vector<note_t*>& list) {
    int count = 0;
    std::vector<note_t>::iterator it = m_list.begin();
    while (it != m_list.end()) {
        note_t& note = *it;
        //if (!note.isIntersectVel(velL, velH))
        //    log_lf(Log::L_DEBUG, "note vel %d intersect vel velLow %d, velHigh %d\n", note.velocity, velL, velH);
        //if (note.isIntersectTime(timeS, timeE))
        //    log_lf(Log::L_DEBUG, "note isIntersectTime vel timeS %d, timeE %d\n", timeS, timeE);

        if (note.isIntersectTime(timeS, timeE) && note.start() >= timeS && note.isIntersectVel(velL, velH)) {
            list.push_back(&note);
            count++;
        }
        it++;
    }
    return count;
}

int clip_notes_t::getInRange(tick_t timeS, tick_t timeE, int32_t pitchL, int32_t pitchH, std::vector<note_t*>& list) {
    int count = 0;
    auto it = m_list.begin();
    while (it != m_list.end()) {
        note_t& note = *it;
        if (note.isIntersectTime(timeS, timeE) && note.isIntersectPitch(pitchL, pitchH)) {
            list.push_back(&note);
            count++;
        }
        it++;
    }
    return count;
}

tick_t clip_t::getLoopBegin() const {
    return loopStart - offsetStart;
}

tick_t clip_t::getNumLoops() const {
    const tick_t preLoopLen         = loopStart - offsetStart;
    const tick_t lenClipLoopSection = len - preLoopLen;
    return (lenClipLoopSection + loopLen - 1) / loopLen;
}

void clip_t::applyNoteQuantizationGroove(const groove_data_t& grooveData, note_t& note, const note_t* nextNote) const {
    auto& groovePatternTiming = grooveData.timingData.timePoints;
    auto& groovePatternVelocity = grooveData.timingData.velocityPoints;
    if (groovePatternTiming.empty()) {
        return;
    }
    dbgassert(groovePatternVelocity.size() == groovePatternTiming.size());
    auto grooveLength = grooveData.timingData.loopLength;
    double time = note.time / double(TICKS_QUARTER);
    double timeQuantization = grooveData.lenQuantization / double(TICKS_QUARTER);
    // apply quantization
    double quantizedTime = timeQuantization * math::rounddS64(time / timeQuantization);
    time = time + (quantizedTime - time) * grooveData.strengthQuantization;
    // search for closest, looking also at the previous point (and looping around)
    auto it = groovePatternTiming.begin();
    auto itEnd = groovePatternTiming.end();
    double groovePos = fmod(time, grooveLength);
    double minDist = std::numeric_limits<double>::max();
    size_t minIdx = 0;
    while (it != itEnd) {
        double groovePosCur = *it;
        double dist = std::abs(groovePosCur - groovePos);
        if (dist < minDist) {
            minDist = dist;
            minIdx = std::distance(groovePatternTiming.begin(), it);
        }
        it++;
    }
    double grooveVelocity = groovePatternVelocity[minIdx];
    double grooveOffset = groovePatternTiming[minIdx] - groovePos;
    time = (time + (grooveOffset) * grooveData.strengthGroove);
    
    seq_rand rand;
    rand.rng_seed(math::floordS64(0x69808 + quantizedTime * 0x18d + note.pitch));
    // apply random timing
    double randTime = ((rand.rng_double() * 2.0 - 1.0) * 0.5 * grooveData.randomTiming);
    time += randTime * timeQuantization;
    // apply random velocity
    auto velocity = math::rounddS32(note.velocity + rand.rng_double() * grooveData.randomVelocity * 127);
    velocity = math::rounddS32(velocity + (grooveVelocity - velocity) * grooveData.strengthVelocity);
    note.time = math::rounddS32(time * TICKS_QUARTER);
    note.velocity = math::clamp(velocity, 0, 127);
    if (nextNote) {
        // call recursively for next note
        note_t nextNoteCopy = *nextNote;
        applyNoteQuantizationGroove(grooveData, nextNoteCopy, nullptr);
        // adjust note length to next note
        if (note.end() > nextNoteCopy.start()) {
            note.cutRight(nextNoteCopy.start());
        }
    }
}

/* HOT CODEPATH */
void clip_t::getNotesView(tick_t localStart, tick_t localEnd, clip_notes_t& notesView, NoteSampleOptions options) const {
    notesView.m_list.clear();
    if (!enabled) {
        notesView.updateBounds();
        return;
    }
    //const tick_t preLoopLen = !loopEnabled?len:math::max(0, loopStart - offsetStart);
    const tick_t preLoopLen = !loopEnabled ? len : offsetStart > loopStart ? math::max(0, (/*loopEnd*/ loopStart + loopLen) - offsetStart)
                                                                           : math::max(0, loopStart - offsetStart);

    auto itNoteBegin = notes.m_list.cbegin();
    auto itNoteEnd = notes.m_list.cend();

    /** add all pre-loop notes */
    bool fillLoop  = loopEnabled && localEnd > preLoopLen;
    bool inPreLoop = localStart < preLoopLen;
    static thread_local std::vector<note_t> listLoop;
    auto grooveIdx = selectedGroove;
    auto groove = groove_data_t{};
    if (grooveIdx >= 0) {
        if (daw_tls::isTlsInitialized()) {
            auto project = daw_tls::getTls().project;
            if (project) {
                groove = project->getGrooveData(selectedGroove);
            } else {
                grooveIdx = -1;
            }
        }
    }

    if (listLoop.capacity() == 0) {
        listLoop.reserve(128);
    } else {
        listLoop.clear();
    }
    
    const auto loopLen = this->loopLen;
    const auto loopStart = this->loopStart;
    const auto offsetStart = this->offsetStart;
    for (auto itNote = notes.m_list.cbegin(); itNote != itNoteEnd; itNote++) {
        const note_t& note = *itNote;
        if (options.bCutMutedNotes && !note.isEnabled()) {
            continue;
        }
        if (fillLoop && (note.start() >= loopStart && note.start() < loopStart + loopLen)) {//note.isIntersectTime(loopStart, loopStart + loopLen)) {
            auto nodeLoop = note;
            nodeLoop.id = itNote - itNoteBegin;
            listLoop.push_back(nodeLoop);
        }
        if (!inPreLoop)
            continue;
        auto localEndMin = math::min(localEnd, preLoopLen);
        note_t nnote = note;// copy
        nnote.id = itNote - itNoteBegin;
        if (options.bApplyGroove && grooveIdx >= 0) {
            auto nextNote = getFirstAfter(notes.m_list, nnote.pitch, nnote.time);
            applyNoteQuantizationGroove(groove, nnote, nextNote);
        }
        nnote.time -= offsetStart;
        if (nnote.isIntersectTime(localStart, localEndMin)) {
            if (nnote.start() < localStart) {
                /** cut pre-loop note on the left for render, ignore for playback */
                if (options.bCutNotes) {
                    nnote.cutLeft(localStart);
                }
            }
            if (nnote.end() > localEndMin) {
                /** always cut pre-loop note ends at pre-loop end */
                if (options.bCutNotes || (loopEnabled && localEndMin == preLoopLen)) {
                    nnote.cutRight(localEndMin);
                }
            }
            if (nnote.len > 0)
                notesView.m_list.push_back(nnote);
        }
    }
    if (fillLoop && loopLen > 0 && listLoop.size()) {

        /** add all in-loop notes */
        const tick_t loopLenProcessing = loopLen;
        const int32_t firstLoop        = math::max(0, (localStart - preLoopLen) / loopLenProcessing);
        const int32_t lastLoop         = (localEnd - preLoopLen) / loopLenProcessing;
        const tick_t numLoops          = lastLoop - firstLoop + 1;
        if (notesView.m_list.capacity() < numLoops * listLoop.size())
            notesView.m_list.reserve(numLoops * listLoop.size());
        for (auto i = firstLoop; i < firstLoop + numLoops; i++) {
            auto itNote = listLoop.cbegin();
            itNoteEnd   = listLoop.cend();
            const tick_t posCurLoopStart = preLoopLen + (i * loopLenProcessing);
            const tick_t posCurLoopEnd   = posCurLoopStart + loopLenProcessing;
            const tick_t clipStart       = math::max(posCurLoopStart, localStart);
            const tick_t clipEnd         = math::min(posCurLoopEnd, localEnd);
            for (; itNote != itNoteEnd; itNote++) {
                note_t note = *itNote;// copy
                if (options.bApplyGroove && grooveIdx >= 0) {
                    auto nextNote = getFirstAfter(listLoop, note.pitch, note.time);
                    note.time -= loopStart;
                    note.time += posCurLoopStart;
                    note_t tmp{};
                    if (nextNote) {
                        tmp = *nextNote;
                        tmp.time -= loopStart;
                        tmp.time += posCurLoopStart;
                        nextNote = &tmp;
                    }
                    // don't add note if its past the clip end
                    if (note.start() >= len) // use len instead?
                        continue;
                    applyNoteQuantizationGroove(groove, note, nextNote);
                } else {
                    note.time -= loopStart;
                    note.time += posCurLoopStart;
                }
                if (note.len <= 0)
                    continue;
                if (note.end() > localStart && note.start() < localEnd) {
                    if (note.start() < clipStart) {
                        //if (!options.bCutNotes) {
                        //    continue;
                        //}
                        if (options.bCutNotes) {
                            note.cutLeft(clipStart);
                        }
                    }
                    if (note.end() > clipEnd) {
                        note.cutRight(clipEnd);
                    }
                    if (note.len > 0)
                        notesView.m_list.push_back(note);
                }
            }
        }
    }
    notesView.updateBounds();
}

/* HOT CODEPATH */
int clip_t::getInTimeRange(tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, std::vector<note_t>& list, NoteSampleOptions options) const {
    dbgassert(absStart <= absEnd);
    tick_t clipStart = start();
    tick_t clipEnd   = end();
    tick_t relStart  = absStart;
    tick_t relEnd    = math::min(clipEnd, absEnd);
    relStart -= clipStart;
    relEnd -= clipStart;
    tick_t cutLeft  = 0;
    tick_t cutRight = getLen();
    if (cutStart > -1) {
        cutLeft = math::max(cutLeft, cutStart - start());
    } else {
        //cutLeft = relStart;
    }
    if (cutEnd > -1) {
        cutRight = math::min(cutRight, cutEnd - start());
    } else {
        //cutRight = relEnd;
    }
    //cutLeft = math::max(cutLeft, relStart);
    //cutRight = math::min(cutRight, relEnd);
    if (cutRight <= cutLeft)
        return 0;

    static thread_local clip_notes_t notesView; // TODO: avoid heap allocation
    getNotesView(math::max(cutLeft, relStart), math::min(cutRight, relEnd), notesView, options);
#if 0
    size_t posOld = list.size();
    list.insert(list.end(), notesView.m_list.begin(), notesView.m_list.end());
    size_t posNew = list.size();
    for (size_t pos = posOld; pos < posNew; pos++) {
        list[pos].time += clipStart;
        list[pos].len = math::min(list[pos].end(), clipEnd) - list[pos].time;
    }
    return posNew - posOld;
#endif
    if (options.bRelative) {
        for (auto& note : notesView.m_list) {
            auto it = std::find_if(list.begin(), list.end(), [&note](const note_t& n) {
                return n.time > note.time;
            });
            list.insert(it, note);
        }
    } else {
        for (auto& note : notesView.m_list) {
            note.time += clipStart;
            if (options.bCutNotes)
                note.len = math::min(note.end(), clipEnd) - note.time;
            // if (cutIntersectingNotesFindDupe(list, note) == -1) {
            //     continue;
            // }
            auto it = std::find_if(list.begin(), list.end(), [&note](const note_t& n) {
                return n.time > note.time;
            });
            list.insert(it, note);
        }
    }
    return CtrSize(notesView.m_list);
}

void clip_notes_t::selectLastN(size_t num) {
    dbgassert(num >= 0 && num <= m_list.size());
    size_t pos = m_list.size() - num;
    size_t end = m_list.size();
    for (; pos < end; ++pos) {
        selection.insert(&m_list[pos]);
    }
}

void clip_notes_t::selectIdxRange(size_t start, size_t end) {
    dbgassert(start <= end && end <= m_list.size());
    for (size_t p = start; p < end; ++p) {
        selection.insert(&m_list[p]);
    }
}

void clip_notes_t::updateBounds() {
    minNote   = note_t{.pitch = -1};
    maxNote   = minNote;
    firstNote = minNote;
    lastNote  = minNote;
    if (!m_list.empty()) {
        auto it  = m_list.cbegin();
        const note_t& beginNote = *it;

        minNote                 = beginNote;
        maxNote                 = beginNote;
        firstNote               = beginNote;
        lastNote                = beginNote;

        it++;
        while (it != m_list.cend()) {
            const note_t& itNode = *it;
            if (minNote.pitch > itNode.pitch) {
                minNote = itNode;
            }
            if (maxNote.pitch < itNode.pitch) {
                maxNote = itNode;
            }
            if (firstNote.time > itNode.time) {
                firstNote = itNode;
            }
            if (lastNote.time < itNode.time) {
                lastNote = itNode;
            }
            it++;
        }
    }
}

const note_t* getFirstAfter(const std::vector<note_t>& v, int32_t pitch, tick_t time) {
    auto itStart     = v.begin();
    const auto itEnd = v.end();
    const note_t* closest  = nullptr;
    while (itStart != itEnd) {
        auto& val = *itStart;
        if (val.pitch == pitch)
            if (val.time > time && (closest == NULL || closest->time > val.time)) {
                closest = &val;
            }
        itStart++;
    }
    return closest;
}

const note_t* getFirstBefore(const std::vector<note_t>& v, int32_t pitch, tick_t time) {
    auto itStart     = v.begin();
    const auto itEnd = v.end();
    const note_t* closest  = nullptr;
    while (itStart != itEnd) {
        auto& val = *itStart;
        if (val.pitch == pitch)
            if (val.end() < time && (closest == NULL || closest->end() < val.end())) {
                closest = &val;
            }
        itStart++;
    }
    return closest;
}

std::pair<note_t*, note_t*> getMinMaxSemitones(std::vector<note_t>& notes) {
    auto minmax = std::minmax_element(notes.begin(), notes.end(),
                                      [](note_t const& lhs, note_t const& rhs) { return lhs.pitch < rhs.pitch; });
    std::pair<note_t*, note_t*> pairPtr{nullptr, nullptr};
    if (minmax.first != notes.end()) {
        pairPtr.first = &*minmax.first;
    }
    if (minmax.second != notes.end()) {
        pairPtr.second = &*minmax.second;
    }
    return pairPtr;
}

std::pair<note_t*, note_t*> getMinMaxTime(std::set<note_t*>& notePtrs) {
    if (notePtrs.empty())
        return std::make_pair(nullptr, nullptr);
    auto min = std::min_element(notePtrs.begin(), notePtrs.end(),
                                [](note_t* const& lhs, note_t* const& rhs) { return lhs->time < rhs->time; });
    auto max = std::max_element(notePtrs.begin(), notePtrs.end(),
                                [](note_t* const& lhs, note_t* const& rhs) { return (lhs->time + lhs->len) < (rhs->time + rhs->len); });
    return std::make_pair(*min, *max);
}

std::pair<tick_t, tick_t> getMinMaxTimeShape(std::vector<DAW::Shape::shape_pt_t>& shapePt) {
    if (shapePt.empty()) {
        return std::make_pair(0, 0);
    }
    auto comp = [](DAW::Shape::shape_pt_t const& lhs, DAW::Shape::shape_pt_t const& rhs) { return lhs.pos.x < rhs.pos.x; };
    auto min = std::min_element(shapePt.begin(), shapePt.end(), comp);
    auto max = std::max_element(shapePt.begin(), shapePt.end(), comp);
    return std::make_pair(tick_t(min->pos.x), tick_t(max->pos.x));
}

std::pair<note_t*, note_t*> getMinMaxTime(std::vector<note_t>& notes) {
    if (notes.empty()) {
        return std::make_pair(nullptr, nullptr);
    }
    auto min = std::min_element(notes.begin(), notes.end(),
                                [](note_t const& lhs, note_t const& rhs) { return lhs.time < rhs.time; });
    auto max = std::max_element(notes.begin(), notes.end(),
                                [](note_t const& lhs, note_t const& rhs) { return (lhs.time + lhs.len) < (rhs.time + rhs.len); });

    return std::make_pair(&*min, &*max);
}

bool operator==(const sample_fades_t& lhs, const sample_fades_t& rhs) {
    return lhs.samplesFadePos == rhs.samplesFadePos && lhs.samplesFadeDuration == rhs.samplesFadeDuration && lhs.shape.pts == rhs.shape.pts;
}

clip_audio_t::~clip_audio_t() {
    delete renderedAudio;
};

int32_t clip_audio_t::lenSamples() const {
    auto cache = audiocache::getInstance();
    dbgassert(cache);
    audiofile_t* audio = cache->getDerivedSample(*this);
    auto* sample       = audio ? audio->sample.get() : nullptr;
    if (sample)
        return sample->nSamples;
    return 0;
}

tick_t clip_t::getLen() const {

    // if (this->lenSamples > 0 && this->clipType == CLIP_AUDIO) {
    //     auto pc = project_controller_t::get();
    //     auto* host = pluginhost::getInstance();
    //     if (pc && host) {
    //         auto lenConverted  = sampleToTickConvert<tick_t, roundmode::round>(this->lenSamples, pc->getCurrentTempo(), host->m_sampleFormatInternal.sampleRate);
    //         if (lenConverted != len) {
    //             log_printf("tick vs sample len missmatch. Did the samplerate change?\n");
    //             dbgassert(0);
    //         }
    //         dbgassert(len > 0 && lenConverted > 0);
    //     }
    // }
    return len;
}

tick_t& clip_t::getLenRef() {
    return len;
}

void clip_t::setLen(tick_t _len) {
    
    if (this->clipType == CLIP_AUDIO && project_controller_t::get()) {
        //TODO: this should be done explicitly
        auto pc = project_controller_t::get();
        auto host = DAW::Host::getInstance();
        if (host && pc) {
            this->lenSamples = tickToSampleConvert<samplecount_t, roundmode::round>(_len, pc->getCurrentTempo(), host->m_sampleFormatInternal.sampleRate);
            for (auto* fade : {&audio.fadeIn, &audio.fadeOut}) {
                if (fade->durationMs > 0) {
                    auto nSamples = host->m_sampleFormatInternal.sampleRate * 0.001 * fade->durationMs;
                    if (nSamples > this->lenSamples) {
                        fade->durationMs = this->lenSamples * 1000.0 / host->m_sampleFormatInternal.sampleRate;
                    }
                }
            }
        }
    }
    this->len = _len;
    if (!loopEnabled) {
        this->loopStart = offsetStart;
        this->loopLen = len;
    }
}

void clip_t::adjustLen(tick_t offset) {
#ifndef NDEBUG
    auto preLen = len;
    setLen(len + offset);
    dbgassert(getLen() == preLen + offset);
#else
    setLen(len + offset);
#endif // NDEBUG
}
samplecount_t clip_t::getLenSamples() const {
    return lenSamples;
}

void clip_t::setLenSamples(samplecount_t _lenSamples) {
    //TODO: this should be done explicitly
    if (this->clipType == CLIP_AUDIO && project_controller_t::get()) {
        auto pc = project_controller_t::get();
        auto host = DAW::Host::getInstance();
        if (pc && host) {
            auto lenConverted  = sampleToTickConvert<tick_t, roundmode::round>(_lenSamples, pc->getCurrentTempo(), host->m_sampleFormatInternal.sampleRate);
            this->len = lenConverted;
        }
    }
    this->lenSamples = _lenSamples;
}
sample_fades_ref_t clip_t::getSampleFadeIn(int32_t tempo100, samplerate_t sr) const {
    // if (!hasFadeIn()) {
    //     return {};
    // }
    return {   
        &audio.fadeIn.shape, 
        tickToSampleConvert<samplecount_t, roundmode::floor>(0, tempo100, sr),
        secondsToSamplesConvert<samplecount_t, roundmode::ceil>(audio.fadeIn.durationMs * 0.001, sr),
    };
}
sample_fades_ref_t clip_t::getSampleFadeOut(int32_t tempo100, samplerate_t sr) const {
    // if (!hasFadeOut()) {
    //     return {};
    // }
    sample_fades_ref_t fadeOut;
    fadeOut.samplesFadeDuration = secondsToSamplesConvert<samplecount_t, roundmode::ceil>(audio.fadeOut.durationMs * 0.001, sr);
    fadeOut.samplesFadePos = tickToSampleConvert<samplecount_t, roundmode::ceil>(len, tempo100, sr) - fadeOut.samplesFadeDuration;
    fadeOut.shape = &audio.fadeOut.shape;
    return fadeOut;
}

clip_audio_t::clip_audio_t() {
    setEmptyFade(true);
    setEmptyFade(false);
}

void clip_audio_t::setDefaultFade(bool bIn) {
    using DAW::Shape::GetShapeSaw;
    using DAW::Shape::GetShapeSawInverse;
    using DAW::Shape::shape_t;
    using DAW::Shape::ShapeFlags;
    auto flags = ShapeFlags::SHAPE_SHAPED | ShapeFlags::SHAPE_EASEINOUT | ShapeFlags::SHAPE_LOCK_POINTS;
    if (bIn) {
        auto fadeInShape = shape_t(GetShapeSawInverse(flags));
        fadeIn  = { 128.0, fadeInShape };
    } else {
        auto fadeOutShape = shape_t(GetShapeSaw(flags));
        fadeOut = { 128.0, fadeOutShape };
    }
}

void clip_audio_t::setEmptyFade(bool bIn) {
    using DAW::Shape::GetShapeSaw;
    using DAW::Shape::GetShapeSawInverse;
    using DAW::Shape::shape_t;
    using DAW::Shape::ShapeFlags;
    auto flags = ShapeFlags::SHAPE_SHAPED | ShapeFlags::SHAPE_EASEINOUT | ShapeFlags::SHAPE_LOCK_POINTS;
    if (bIn) {
        auto fadeInShape = shape_t(GetShapeSawInverse(flags));
        fadeIn  = { 0.0, fadeInShape };
    } else {
        auto fadeOutShape = shape_t(GetShapeSaw(flags));
        fadeOut = { 0.0, fadeOutShape };
    }
}

void clip_t::adjustStartOffset(tick_t offset) {
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
    while (inLoop && offsetStart >= loopStart + loopLen && offsetStart >= loopLen) {
        offsetStart -= loopLen;
    }
    offsetStart = math::max(offsetStart, 0);
}

void clip_t::copy(const clip_t& obj) {
    dbgassert(this != &obj);
    this->name   = StringLimit(obj.name, 64);
    clipType     = obj.clipType;
    enabled      = obj.enabled;
    rgb          = obj.rgb;
    time         = obj.time;
    len          = obj.len;
    offsetStart  = obj.offsetStart;
    lenSamples   = obj.lenSamples;
    loopStart    = obj.loopStart;
    loopLen      = obj.loopLen;
    loopEnabled  = obj.loopEnabled;
    notes        = obj.notes;
    audio        = obj.audio;
    controlData  = obj.controlData;
    selectedGroove = obj.selectedGroove;
    editorLayout = obj.editorLayout;
    selectedGroove = obj.selectedGroove;
    dirty        = true;
}

void clip_notes_t::clear() {
    firstNote = lastNote = minNote = maxNote = note_t();
    selection.clear();
    m_list.clear();
}

bool clip_notes_t::isEmpty() const {
    return m_list.empty();
}

bool clip_notes_t::hasDuplicates() const {
    return any_duplicates(m_list);
}


void clip_notes_t::getSelectionIndices(std::vector<size_t>& selIdx) const {
    const auto begin = m_list.cbegin();
    for (note_t* n : selection) {
        auto it = std::find_if(m_list.cbegin(), m_list.cend(),
                               [n](const note_t& note) { return &note == n; });
        if (it == m_list.cend()) {
            dbgassert(0);
        }
        selIdx.push_back(static_cast<size_t>(it - begin));
    }
    dbgassert(selection.size() == selIdx.size());
}

void clip_notes_t::deleteSelectedNotes(clip_notes_t& notes) {
    std::vector<note_t> delNotes(selection.size());
    copySelectionTo(delNotes);
    clearSelection();
    for (note_t& note : delNotes) {
        notes.remove(note);
    }
    notes.updateBounds();
}

void clip_notes_t::muteToggleSelectedNotes(clip_notes_t& notes) {
    std::vector<note_t> delNotes(selection.size());
    copySelectionTo(delNotes);
    for (note_t& note : delNotes) {
        notes.mute(note);
    }
    //notes.updateBounds();
}

void clip_notes_t::getNotePitches(std::vector<int32_t>& out) {
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

void clip_notes_t::addOrRemoveSelection(note_t* note) {

    auto it = std::find(selection.begin(), selection.end(), note);
    if (it != selection.end()) {
        selection.erase(it);
    } else {
        selection.insert(note);
    }
}

void clip_notes_t::clearSelection() {
    selection.clear();
    // removeDuplicates();
}

void clip_notes_t::copySelectionTo(std::vector<note_t>& _out) const {
    _out.clear();
    for (note_t* note : selection) {
        note_t& ref = *note;
        _out.push_back(ref);
    }
}

clip_control_data_t::clip_control_data_t() {
    pitchBend.shape.flags = DAW::Shape::ShapeFlags::SHAPE_UNCLAMPPED | DAW::Shape::ShapeFlags::SHAPE_SHAPED;
    pitchBend.defaultValue = 0.5f;
    // pitchBend.shape.pts   = { { { 0, 0.5f }, 0.5f } };
    pitchBend.updateBounds();
    updateBounds();
}
void clip_control_data_t::createCCChannel(int32_t cc) {
    clip_control_data_channel_t newChannel;
    newChannel.shape.flags = DAW::Shape::ShapeFlags::SHAPE_UNCLAMPPED | DAW::Shape::ShapeFlags::SHAPE_SHAPED;
    newChannel.defaultValue = 0.0f;
    newChannel.updateBounds();
    ccChannels[cc] = std::move(newChannel);
    updateBounds();
}

clip_control_data_channel_t& clip_control_data_t::getOrCreateChannel(int32_t cc) {
    auto it = ccChannels.find(cc);
    if (it == ccChannels.end()) {
        createCCChannel(cc);
        return ccChannels[cc];
    }
    return it->second;
}

float clip_control_data_channel_t::sampleAtTick(clip_t* clip, tick_t tOffset) {
    if (shape.pts.empty()) {
        return defaultValue;
    }
    if (clip->isLoopEnabled()) {
        const tick_t preLoopLen = clip->offsetStart > clip->loopStart ? math::max(0, (/*loopEnd*/ clip->loopStart + clip->loopLen) - clip->offsetStart) : math::max(0, clip->loopStart - clip->offsetStart);
        if (clip->loopLen > 0 && tOffset >= preLoopLen) {
            tOffset = math::floorfS32(tOffset - preLoopLen) % clip->loopLen;
        }
    }
    tOffset += clip->offsetStart;
    return shape.sampleCurveUnclamped(tOffset);
}

int clip_control_data_t::getInTimeRange(clip_t* clip, tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, std::vector<DAW::Host::midievent_ctrl_t>& list) {
    if (!hasData()) {
        return 0;
    }
    tick_t clipStart = clip->start();
    tick_t clipEnd   = clip->end();
    tick_t relStart  = absStart;
    tick_t relEnd    = math::min(clipEnd, absEnd);
    relStart -= clipStart;
    relEnd -= clipStart;
    tick_t cutLeft  = 0;
    tick_t cutRight = clip->getLen();
    if (cutStart > -1) {
        cutLeft = math::max(cutLeft, cutStart - clip->start());
    } else {
        //cutLeft = relStart;
    }
    if (cutEnd > -1) {
        cutRight = math::min(cutRight, cutEnd - clip->start());
    } else {
        //cutRight = relEnd;
    }
    if (cutRight <= cutLeft)
        return 0;
    auto absMin = math::max(cutLeft, relStart);
    auto absMax = math::min(cutRight, relEnd);
    int count = 0;

    /**
     * controlDataSampleRate
     * 4 Ticks is quite high and causes a lot of events
     * This should be configurable to allow higher resolution when
     * rendering to audio. And lower resolution when CPU load is too high.
     */
    tick_t controlDataSampleRate = 4;

    for (tick_t t = absMin; t < absMax; t += controlDataSampleRate) {
        tick_t tOffset = t;
        if (pitchBend.hasData()) {
            float f1 = pitchBend.sampleAtTick(clip, tOffset);
            IMidiMsg msg;
            msg.MakePitchWheelMsg(f1*2.0-1.0f, 0, t);
            DAW::Host::midievent_ctrl_t e;
            e.message = msg.ToU32();
            e.tick = clipStart + t;
            e.midiTime = 0;
            list.push_back(e);
            ++count;
        }
        for (auto& it : ccChannels) {
            if (it.second.hasData()) {
                float f1 = it.second.sampleAtTick(clip, tOffset);
                IMidiMsg msg;
                msg.MakeControlChangeMsg(static_cast<IMidiMsg::EControlChangeMsg>(it.first), f1, 0, t);
                DAW::Host::midievent_ctrl_t e;
                e.message = msg.ToU32();
                e.tick = clipStart + t;
                e.midiTime = 0;
                list.push_back(e);
                ++count;
            }
        }
    }
    return count;
}
void clip_control_data_t::cutLeft(tick_t time) {
    if (pitchBend.hasData()) {
        CutShapeLeft(pitchBend.shape, time);
    }
    for (auto& it : ccChannels) {
        if (it.second.hasData()) {
            CutShapeLeft(it.second.shape, time);
        }
    }
}
void clip_control_data_t::cutRight(tick_t time) {
    if (pitchBend.hasData()) {
        CutShapeRight(pitchBend.shape, time);
    }
    for (auto& it : ccChannels) {
        if (it.second.hasData()) {
            CutShapeRight(it.second.shape, time);
        }
    }
}

namespace DAW {
void CopyControlDataChannel(clip_control_data_channel_t& dst, tick_t writePos, const clip_control_data_channel_t& src, tick_t readPos, tick_t len, tick_t offsetStart,  tick_t loopStart, tick_t loopLen) {
    tick_t preLoopLen = 0;
    if (offsetStart < loopStart) {
        preLoopLen = loopStart - offsetStart;
        if (preLoopLen) {
            for (auto& pt : src.shape.pts) {
                if (pt.pos.x >= 0 && pt.pos.x < preLoopLen && pt.pos.x >= readPos && pt.pos.x <= readPos + len) {
                    dst.shape.pts.push_back({{pt.pos.x + writePos - readPos, pt.pos.y}, pt.shape});
                }
            }
        }
    } else {
        readPos += offsetStart;
    }
    for (tick_t t = preLoopLen; t < len; ) {
        for (auto& pt : src.shape.pts) {
            if (pt.pos.x >= loopStart && pt.pos.x <= loopStart+loopLen && pt.pos.x+t >= readPos && pt.pos.x+t <= readPos + len) {
                dst.shape.pts.push_back({{pt.pos.x + t + writePos - readPos, pt.pos.y}, pt.shape});
            }
        }
        t += loopLen;
    }
}

void CopyControlData(const clip_control_data_t& src, clip_control_data_t& dst, tick_t readPos, tick_t writePos, tick_t readLen) {
    for (auto& pt : src.pitchBend.shape.pts) {
        auto& pitchBend = dst.pitchBend;
        if (pt.pos.x >= readPos && pt.pos.x <= readPos + readLen) {
            pitchBend.shape.pts.push_back({{pt.pos.x + writePos - readPos, pt.pos.y}, pt.shape});
        }
    }
    for (auto& it : src.ccChannels) {
        auto& cc = dst.getOrCreateChannel(it.first);
        for (auto& pt : it.second.shape.pts) {
            if (pt.pos.x >= readPos && pt.pos.x <= readPos + readLen) {
                cc.shape.pts.push_back({{pt.pos.x + writePos - readPos, pt.pos.y}, pt.shape});
            }
        }
    }
    dst.eraseDuplicates();
    dst.updateBounds();
}
}

void clip_control_data_t::updateBounds() {
    pitchBend.updateBounds();
    for (auto& cc : ccChannels) {
        cc.second.updateBounds();
    }
}

void clip_control_data_t::eraseDuplicates() {
    pitchBend.shape.assertSorted();
    pitchBend.shape.eraseDuplicates();
    for (auto& cc : ccChannels) {
        cc.second.shape.assertSorted();
        cc.second.shape.eraseDuplicates();
    }
}

void clip_control_data_t::sort() {
    pitchBend.shape.sort();
    for (auto& cc : ccChannels) {
        cc.second.shape.sort();
    }
}

void clip_control_data_t::clear() {
    pitchBend.shape.pts.clear();
    ccChannels.clear();
}

void clip_control_data_t::copyRangeFrom(clip_t* clip, tick_t writePos, tick_t readPos, tick_t len) {
    auto readEnd = readPos + len;
    auto readLen = readEnd - readPos;
    auto& dataIn = clip->controlData;
    if (clip->isLoopEnabled()) {
        DAW::CopyControlDataChannel(pitchBend, writePos, dataIn.pitchBend, readPos, readLen, clip->offsetStart, clip->loopStart, clip->loopLen);
        for (auto& it : dataIn.ccChannels) {
            if (!ccChannels.count(it.first)) {
                createCCChannel(it.first);
            }
            auto& cc = ccChannels[it.first];
            DAW::CopyControlDataChannel(cc, writePos, it.second, readPos, readLen, clip->offsetStart, clip->loopStart, clip->loopLen);
        }
    } else {
        readPos += clip->offsetStart;
        for (auto& pt : dataIn.pitchBend.shape.pts) {
            if (pt.pos.x >= readPos && pt.pos.x <= readPos + readLen) {
                pitchBend.shape.pts.push_back({{pt.pos.x + writePos - readPos, pt.pos.y}, pt.shape});
            }
        }
        for (auto& it : dataIn.ccChannels) {
            if (!ccChannels.count(it.first)) {
                createCCChannel(it.first);
            }
            auto& cc = ccChannels[it.first];
            for (auto& pt : it.second.shape.pts) {
                if (pt.pos.x >= readPos && pt.pos.x <= readPos + readLen) {
                    cc.shape.pts.push_back({{pt.pos.x + writePos - readPos, pt.pos.y}, pt.shape});
                }
            }
        }
    }
}

namespace DAW {

float AudioClipFadeLoopProcessor::get(channelnum_t ch, samplecount_t samplePos) const {
    float fade = 1.0f;
    if (fadeIn.samplesFadeDuration > 0 && samplePos >= fadeIn.samplesFadePos && samplePos < fadeIn.samplesFadePos + fadeIn.samplesFadeDuration) {
        float fadePos = (samplePos - fadeIn.samplesFadePos) / float(fadeIn.samplesFadeDuration);
        fade *= fadeIn.shape->sampleCurveOneShot(fadePos);
    }
    if (fadeOut.samplesFadeDuration > 0 && samplePos >= fadeOut.samplesFadePos) {
        float fadePos = (samplePos - fadeOut.samplesFadePos) / float(fadeOut.samplesFadeDuration);
        fade *= fadeOut.shape->sampleCurveOneShot(fadePos);
    }
    dbgassert(samplePos < fadeOut.samplesFadePos + fadeOut.samplesFadeDuration + 5);
    auto readPos = samplePos;
    if (loopEnd - loopStart > 0) {
        if (samplePos >= preLoopLen) {
            readPos += math::max<samplecount_t>(0, offsetStart - loopStart);
            readPos = loopStart + ((readPos - preLoopLen) % (loopEnd - loopStart));
        }
    } else {
        readPos += offsetStart;
    }
    if (readPos >= 0 && readPos < sample->nSamples && ch < sample->samples.size()) {
        return sample->samples[ch][readPos] * fade;
    }
    return 0.0f;
}

float AudioClipFadeLoopProcessor::getDownsampled(channelnum_t ch, samplecount_t samplePos, uint8_t nDownLevel, int32_t& status) const {
    float fade = 1.0f;
    status = 0;
    for (auto* clipFade : { &fadeIn, &fadeOut }) {
        if (clipFade->samplesFadeDuration > 0 && samplePos >= clipFade->samplesFadePos && samplePos < clipFade->samplesFadePos + clipFade->samplesFadeDuration) {
            float fadePos = (samplePos - clipFade->samplesFadePos) / float(clipFade->samplesFadeDuration);
            fade *= clipFade->shape->sampleCurveOneShot(fadePos);
            status |= 2;
        }
    }
    auto readPos = samplePos;
    if (loopEnd - loopStart > 0) {
        if (samplePos >= preLoopLen) {
            readPos += math::max<samplecount_t>(0, offsetStart - loopStart);
            readPos = loopStart + ((readPos - preLoopLen) % (loopEnd - loopStart));
        }
    } else {
        readPos += offsetStart;
    }
    const std::vector<samplechannel_t>* vec = nullptr;
    if (nDownLevel) {
        vec = &sample->downsampled[nDownLevel - 1];
    } else {
        vec = &sample->samples;
    }
    if (readPos >= 0 && vec && ch < vec->size() && readPos < samplecount_t(vec->at(ch).size())) {
        const auto& channel = (*vec)[ch];
        status |= 1;
        return channel[readPos] * fade;
    }
    status = 0;
    return 0.0f;
}

} // namespace DAW

void cutClipLeft(clip_t* c, tick_t len) {
    c->adjustStartOffset(len);
    c->time += len;
    c->setLen(c->getLen() - len);
    c->audio.setEmptyFade(true);
    dbgassert(c->time >= 0);
    dbgassert(c->getLenRef() > 0);
}

void cutClipRight(clip_t* c, tick_t len) {
    c->setLen(c->getLen() - len);
    c->audio.setEmptyFade(false);
    dbgassert(c->getLenRef() > 0);
}
gui_clip* clip_t::getGuiClip(DawCtrl* parentCtrl) {
    for (auto& entry : trackEntries) {
        if (entry->parentCtrl == parentCtrl) {
            auto it = entry->clipsGuis.find(this);
            if (it != entry->clipsGuis.end()) {
                return it->second;
            }
        }
    }
    return nullptr;
}

void clip_t::updateNoteView() const {
    if (dirty) {
        noteViewRender.reqRevision++;
        noteViewRenderFullClip.reqRevision++;
        noteViewSelection.reqRevision++;
        dirty = false;
        getNotesView(0, getLen(), noteViewRender, {.bCutNotes = true, .bCutMutedNotes = false, .bApplyGroove = true});
        static_cast<clip_notes_t*>(&noteViewRenderFullClip)->copy(this->notes);
        noteViewSelection.clear();
    }
}

void quantizeNoteEndTime(std::vector<note_t>& notesPtrs, tick_t quantize) {
    for (auto& note : notesPtrs) {
        auto newEnd = math::roundfS32(note.end() / static_cast<float>(quantize)) * quantize;
        if (newEnd - note.time > 0) {
            note.len = newEnd - note.time;
        }
    }
}

void quantizeNoteStartTime(std::vector<note_t>& notesPtrs, tick_t quantize) {
    for (auto& note : notesPtrs) {
        auto newTime = math::roundfS32(note.start() / static_cast<float>(quantize)) * quantize;
        note.time    = newTime;
    }
}

void clip_t::updateNoteViewSelection() {
    noteViewSelection.reqRevision++;
}

noteview_render_t& clip_t::getNoteViewSelection() const {
    if (noteViewSelection.curRevision != noteViewSelection.reqRevision) {
        noteViewSelection.clear();
        getInTimeRange(start(), end(), -1, -1, noteViewSelection.m_list, {
            .bCutNotes = false,
            .bCutMutedNotes = false,
            .bApplyGroove = false,
        });
        std::vector<note_t> sel;
        for (note_t* pnote: notes.selection) {
            auto idx = pnote - notes.m_list.data();
            for (auto& note : noteViewSelection.m_list) {
                if (note.id == idx) {
                    sel.push_back(note);
                }
            }
        }
        noteViewSelection.m_list = std::move(sel);
        noteViewSelection.curRevision = noteViewSelection.reqRevision;
    }
    return this->noteViewSelection;
}
