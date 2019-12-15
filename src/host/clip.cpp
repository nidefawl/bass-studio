#include <algorithm>
#include <vector>
#include "assert_dbg.h"
#include <stddef.h>
#include <limits>
#include "str_util.h"
#include "math/seq_math.h"
#include "exceptions.h"
#include "clip.h"
#include "logging.h"
#include "seq_time.h"
#include "audiocache.h"
#include "project.h"
#include "projectcontroller.h"
#include "util/debug_alloc.h"

namespace DebugAlloc {
	Tracker<clip_t> trackerClips;
	template<>
	void printLeaked(int64_t allocId, int64_t allocCount, std::vector<clip_t*>& allocList, std::unordered_map<int64_t, DebugAlloc::AllocInfo>& allocInfo) {
		dbgassert(allocList.size() == allocCount);
		my_printf("clip_t allocations: %lld\n", allocCount);
		for (auto clip : allocList) {
			auto it = allocInfo.find(clip->allocId);
			if (it != allocInfo.end()) {
				AllocInfo& info = it->second;

				my_printf("leaked %lld %s\n", clip->allocId, StringAsCStr(clip->name)); // add debug info to clip instance (track/time )
				for (String s : info.stacktrace) {
					my_printf("%s\n", StringAsCStr(s));
				}
				break;
			}
		}
	}
	template<>
	Tracker<clip_t>* getTracker() {
		return &trackerClips;
	}
//	static auto test = []() {
//		DebugAlloc::getTracker<clip_t>()->setRecordAllocationStackTraces(true);
//		return 1;
//	}();
}
int32_t getNumClipAllocations() {
	auto tracker = DebugAlloc::getTracker<clip_t>();
	dbgassert(tracker->allocCount < (std::numeric_limits<int32_t>::max()));
	return tracker->allocCount;
}
void printClipAllocations() {
	DebugAlloc::getTracker<clip_t>()->printAllocations();
}
clip_t::clip_t() {
	allocId = DebugAlloc::getTracker<clip_t>()->objConstructor(this);
}
clip_t::clip_t(const clip_t &a) : clip_t() {
	copy(a);
}
clip_t::~clip_t() {
	DebugAlloc::getTracker<clip_t>()->objDestructor(this);
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


int cutIntersecting(std::vector<note_t>& m_list, note_t& n, bool eliminateDupes) {
	int nErased = 0;
	auto it = m_list.begin();
	int exactDupes = 0;
	while (it != m_list.end()) {
		note_t& c = *it;
		if (c == n) {
			if (eliminateDupes || exactDupes) {
				it = m_list.erase(it);
				nErased++;
			} else {
				it++; // allow exact duplicates
			}
			exactDupes++;
		} else if (c.pitch != n.pitch) {
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
int32_t clip_notes_t::paste(note_t& t, bool eliminateDupes) {
	dbgassert(selection.empty());
	cutIntersecting(m_list, t, eliminateDupes);
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
	auto aEnd = a.end();
	while (aBegin != aEnd) {
		auto itRemove = std::find(m_list.begin(), m_list.end(), *aBegin);
//		dbgassert(itRemove != m_list.end());
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
	}), m_list.end());
}
void clip_notes_t::setTo(std::set<note_t*>& notePtrs, tick_t offset) {
	m_list.clear();
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
	sort( m_list.begin(), m_list.end() );
	auto itNewEnd = unique( m_list.begin(), m_list.end() );
	size_t removed = m_list.end()-itNewEnd;
	m_list.erase( itNewEnd, m_list.end() );
	return removed;
}
void clip_notes_t::storeSelection(std::vector<note_t>& selNotes) {
	for (note_t* n : selection) {
		selNotes.push_back(*n); // copy;
	}
}
size_t clip_notes_t::restoreSelection(std::vector<note_t>& selNotes) {
	size_t numRestored = 0;
	for (note_t& n : selNotes) {
		auto it = std::find_if(m_list.begin(), m_list.end(),
				[&n] (const note_t& note) { return note == n; });
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
		bool allRestored = restoreSelection(selNotes);
		dbgassert(allRestored);
		for (note_t* n : selection) {
			auto it = std::find_if(m_list.begin(), m_list.end(),
					[n] (const note_t& note) { return &note == n; });
			if (it == m_list.end()) {
				dbgassert(0);
			}
		}
	} else {
		nRemoved = removeDuplicatesImpl(m_list);
	}
	updateBounds();
	return nRemoved;
}

void clip_notes_t::copy( const clip_notes_t &obj) {
	//dbgassert(!obj.hasDuplicates());
	m_list = obj.m_list;
	selection.clear();
	if (!obj.selection.empty()) {
		const note_t* baseOther = obj.m_list.data();
		note_t* baseOwn = m_list.data();
		for (note_t* notePtr : obj.selection) {
			const ptrdiff_t diff = notePtr - baseOther;
			note_t* ownPtr = baseOwn + diff;
			selection.insert(ownPtr);
		}
	}
	dbgassert(obj.selection.size() == selection.size());
	firstNote = obj.firstNote;
	lastNote = obj.lastNote;
	minNote = obj.minNote;
	maxNote = obj.maxNote;
}
note_t* clip_notes_t::get(tick_t time, int32_t pitch) {
	auto it = m_list.rbegin();
	while (it != m_list.rend()) {
		note_t& note = *it;
		if (pitch == note.pitch
				&& time >= note.start() && time < note.end()) {
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
//		if (!note.isIntersectVel(velL, velH))
//		log_printf("note vel %d intersect vel velLow %d, velHigh %d\n", note.velocity, velL, velH);
//		if (note.isIntersectTime(timeS, timeE))
//		log_printf("note isIntersectTime vel timeS %d, timeE %d\n", timeS, timeE);

		if (note.isIntersectTime(timeS, timeE) && note.start() > timeS && note.isIntersectVel(velL, velH)) {
			list.push_back(&note);
			count++;
		}
		it++;
	}
	return count;
}
int clip_notes_t::getInRange(tick_t timeS, tick_t timeE, int32_t pitchL, int32_t pitchH, std::vector<note_t*>& list) {
	int count = 0;
	std::vector<note_t>::iterator it = m_list.begin();
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
	const tick_t preLoopLen = loopStart - offsetStart;
	const tick_t lenClipLoopSection = len - preLoopLen;
	return (lenClipLoopSection+loopLen-1) / loopLen;
}
/* HOT CODEPATH */
void clip_t::getNotesView(tick_t localStart, tick_t localEnd, clip_notes_t& notesView, bool forPlayback) const {
	notesView.m_list.clear();
	std::vector<note_t> listLoop;
	listLoop.reserve(128);
	const tick_t preLoopLen = math::max(0, loopStart - offsetStart);
	const tick_t clipEndPre = math::min(preLoopLen, localEnd);
	const tick_t start = offsetStart+localStart;

	auto itNote = notes.m_list.cbegin();
	auto itNoteEnd = notes.m_list.cend();
	for (;itNote != itNoteEnd; itNote++) {
		const note_t& note = *itNote;
		if (forPlayback && !note.isEnabled()){
			continue;
		}
		if (note.isIntersectTime(loopStart, loopStart + loopLen)) {
			listLoop.push_back(note);
		}
		if (start < loopStart && note.isIntersectTime(start, loopStart)) {
			note_t nnote = note; // copy
			nnote.time -= offsetStart;
			if (nnote.start() < localStart) {
				if (forPlayback) {
					continue;
				}
				nnote.cutLeft(localStart);
			}
			if (nnote.end() > clipEndPre) {
				if (!forPlayback || localEnd==clipEndPre) {
					nnote.cutRight(clipEndPre);
				}
			}
			notesView.m_list.push_back(nnote);
		}
	}


	const int loopLenProcessing = loopLen <= 0 ? 0 : loopLen;
	const tick_t lenClipLoopSection = (localEnd - localStart) - preLoopLen;
	const tick_t numLoops = loopEnabled && loopLenProcessing>0 ? (lenClipLoopSection+loopLen-1) / loopLenProcessing : 1;
	if (notesView.m_list.capacity() < numLoops * listLoop.size())
		notesView.m_list.reserve(numLoops * listLoop.size());
	for (int i = 0; i < numLoops; i++) {
		auto itNote = listLoop.cbegin();
		auto itNoteEnd = listLoop.cend();
		const tick_t posCurLoopStart = preLoopLen + (i * loopLenProcessing);
		const tick_t posCurLoopEnd = posCurLoopStart + loopLenProcessing;
		const tick_t clipStart = math::max(posCurLoopStart, localStart);
		const tick_t clipEnd = math::min(posCurLoopEnd, localEnd);
		for (;itNote != itNoteEnd; itNote++) {
			note_t note = *itNote; // copy
			note.time -= loopStart;
			note.time += posCurLoopStart;
			if (note.end() > localStart && note.start() < localEnd) {
				if (note.start() < clipStart) {
					if (forPlayback) {
						continue;
					}
					note.cutLeft(clipStart);
				}
				if (note.end() > clipEnd) {
					note.cutRight(clipEnd);
				}
				notesView.m_list.push_back(note);
			}
		}
	}
	notesView.updateBounds();
}
/* HOT CODEPATH */
int clip_t::getInTimeRange(tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, std::vector<note_t>& list) {
	tick_t clipStart = start();
	tick_t clipEnd = end();
	tick_t relStart = absStart;
	tick_t relEnd = math::min(clipEnd, absEnd);
	relStart -= clipStart;
	relEnd -= clipStart;
	clip_notes_t notesView;
	tick_t cutLeft = 0;
	tick_t cutRight = getLen();
	if (cutStart > -1) {
		cutLeft = math::max(cutLeft, cutStart-start());
	} else {
//		cutLeft = relStart;
	}
	if (cutEnd > -1) {
		cutRight = math::min(cutRight, cutEnd-start());
	} else {
//		cutRight = relEnd;
	}
	if (cutRight <= cutLeft)
		return 0;
	getNotesView(cutLeft, cutRight , notesView, true);

	auto itNote = notesView.m_list.begin();
	auto itNotesEnd = notesView.m_list.end();
	while (itNote != itNotesEnd) {
		note_t& note = *itNote;
		if (note.isIntersectTimeIncludeEnds(relStart, relEnd)) {
			note_t noteOffset(note);
			noteOffset.time += clipStart;
			noteOffset.len = math::min(noteOffset.end(), clipEnd) - noteOffset.time;
			if (!list.capacity()) {
				list.reserve(128);
			}
			list.push_back(noteOffset);
		}
		itNote++;
	}

	return list.size();
}

int getClipNotesInTimeRange(tick_t absStart, tick_t absEnd, tick_t cutStart, tick_t cutEnd, const clip_notes_t notesView, std::vector<note_t>& list) {


	auto itNote = notesView.m_list.begin();
	auto itNotesEnd = notesView.m_list.end();
	while (itNote != itNotesEnd) {
		const note_t& note = *itNote;
		if (note.isIntersectTimeIncludeEnds(absStart, absEnd)) {
			note_t noteOffset(note);
//			noteOffset.time += clipStart;
//			noteOffset.len = math::min(noteOffset.end(), clipEnd) - noteOffset.time;
			if (!list.capacity()) {
				list.reserve(128);
			}
			list.push_back(noteOffset);
		}
		itNote++;
	}

	return list.size();
}
void clip_notes_t::selectLastN(size_t num) {
	dbgassert(num > 0 && num <= m_list.size());
	size_t pos = m_list.size() - num;
	size_t end = m_list.size();
	for (; pos < end; ++pos) {
		selection.insert(&m_list[pos]);
	}
}
void clip_notes_t::selectIdxRange(size_t start, size_t end) {
	dbgassert(start < end && end <= m_list.size());
	for (size_t p = start; p < end; ++p) {
		selection.insert(&m_list[p]);
	}
}
void clip_notes_t::updateBounds() {
	minNote = note_t();
	maxNote = minNote;
	firstNote = minNote;
	lastNote = minNote;
	if (m_list.size()) {
		std::vector<note_t>::iterator it = m_list.begin();
		note_t& note = *it;
		minNote = note;
		maxNote = note;
		firstNote = note;
		lastNote = note;
		it++;
		while (it != m_list.end()) {
			note_t& note = *it;
			if (minNote.pitch > note.pitch) {
				minNote = note;
			}
			if (maxNote.pitch < note.pitch) {
				maxNote = note;
			}
			if (firstNote.time > note.time) {
				firstNote = note;
			}
			if (lastNote.time < note.time) {
				lastNote = note;
			}
			it++;
		}
	}

}
note_t* getFirstAfter(std::vector<note_t>& v, int32_t pitch, tick_t time) {
	auto itStart = v.begin();
	const auto itEnd = v.end();
	note_t* closest = NULL;
	while (itStart != itEnd) {
		note_t& val = *itStart;
		if (val.pitch == pitch)
		if (val.time > time && (closest == NULL || closest->time > val.time)) {
			closest = &val;
		}
		itStart++;
	}
	return closest;
}
note_t* getFirstBefore(std::vector<note_t>& v, int32_t pitch, tick_t time) {
	auto itStart = v.begin();
	const auto itEnd = v.end();
	note_t* closest = NULL;
	while (itStart != itEnd) {
		note_t& val = *itStart;
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
        [] (note_t const& lhs, note_t const& rhs) {return lhs.pitch < rhs.pitch;});
	std::pair<note_t*, note_t*> pairPtr;
    if (minmax.first != notes.end()) {
    	pairPtr.first = &*minmax.first;
    }
    if (minmax.second != notes.end()) {
    	pairPtr.second = &*minmax.second;
    }
    return pairPtr;
}
std::pair<note_t*, note_t*> getMinMaxTime(std::set<note_t*>& notePtrs) {
	auto min = std::min_element(notePtrs.begin(), notePtrs.end(),
        [] (note_t* const& lhs, note_t* const& rhs) { return lhs->time < rhs->time; });
	auto max = std::max_element(notePtrs.begin(), notePtrs.end(),
        [] (note_t* const& lhs, note_t* const& rhs) { return (lhs->time+lhs->len) < (rhs->time+rhs->len); });


    return std::make_pair(*min, *max);
}
std::pair<note_t*, note_t*> getMinMaxTime(std::vector<note_t>& notes) {
	if (notes.empty()) {
		return std::make_pair(nullptr, nullptr);
	}
	auto min = std::min_element(notes.begin(), notes.end(),
        [] (note_t const& lhs, note_t const& rhs) { return lhs.time < rhs.time; });
	auto max = std::max_element(notes.begin(), notes.end(),
        [] (note_t const& lhs, note_t const& rhs) { return (lhs.time+lhs.len) < (rhs.time+rhs.len); });

    return std::make_pair(&*min, &*max);
}

tick_t clip_audio_t::lenSamples() {
	dbgassert(audiocache::getInstance());
	audiofile_t* audio = audiocache::getInstance()->get(this->id);
	auto* sample = audio ? audio->sample.get() : nullptr;
	if (sample)
		return sample->nSamples;
	return 0;
}

void clip_t::adjustStartSamples(tick_t offset) {
	dbgassert(project_controller_t::get());
	int32_t tick = project_controller_t::get()->tickToSamples(offset);
//	if (loopEnabled && offsetStart < loopStart) {
//		tick_t lenAdj = min(offset, loopStart - offsetStart);
//		offsetStart += lenAdj;
//		offset -= lenAdj;
//	}
//	bool inLoop = loopEnabled && offsetStart >= loopStart;
	this->offsetSamples += tick;
//	if (this->offsetSamples < 0)
//		this->offsetSamples = 0;
//	while (inLoop && offsetStart < loopStart) {
//		offsetStart += loopLen;
//	}
//	while (inLoop && offsetStart >= loopStart+loopLen) {
//		offsetStart -= loopLen;
//	}
}

tick_t clip_t::getLen() const {

	if (this->lenSamples>0&&this->clipType == CLIP_AUDIO && project_controller_t::get()) {
		return project_controller_t::get()->samplesToTicks(this->lenSamples);
	}
//	tick_t
	return len;
}

tick_t& clip_t::getLenRef() {
	return len;
}

void clip_t::setLen(tick_t len) {
	if (this->clipType == CLIP_AUDIO && project_controller_t::get()) {
		this->lenSamples = project_controller_t::get()->tickToSamples(len);
	}
	this->len = len;
	dbgassert(this->clipType != CLIP_AUDIO || (!project_controller_t::get()||project_controller_t::get()->samplesToTicks(this->lenSamples) == this->len));
}

void clip_t::adjustLen(tick_t offset) {
	setLen(len+offset);
}
tick_t clip_t::getLenSamples() const {
	return lenSamples;
}

void clip_t::setLenSamples(tick_t lenSamples) {
	if (this->clipType == CLIP_AUDIO && project_controller_t::get()) {
		this->len = project_controller_t::get()->samplesToTicks(len);
	}
	this->lenSamples = lenSamples;
}
