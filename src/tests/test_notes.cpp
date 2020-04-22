#include "TestBase.hpp"
#include <vector>
#include <stdint.h>
#include "seq_time.h"
#include "clip.h"
#include "project.h"
#include "../host/mainctrl.h"
#include "test_common.h"

namespace {

void testDuplicates() {
	ALEPH_TEST_BEGIN("testDuplicates");
	test_rng rng;
	clip_t clip;
	clip_notes_t& notes = clip.notes;
	uint32_t nNotes = 20;
	uint32_t nDuplicates = 3;

	for (uint32_t i = 0; i < nNotes; i++) {
		note_t note;
		note.time = static_cast<tick_t>(2 + i*4) * TICKS_16TH;
		note.len = 6 * TICKS_16TH;
		note.pitch = 36 + (rng.randI() & 0x3F);
		for (uint32_t j = 0; j < nDuplicates; j++) {
			notes.add(note);
		}
	}
	ALEPH_ASSERT_THROW(notes.m_list.size() == nNotes * nDuplicates);
	notes.removeDuplicates();
	ALEPH_ASSERT_THROW(notes.m_list.size() == nNotes * 1);
	notes.updateBounds();
	note_t noteStart;
	note_t noteEnd;
	noteStart.time = 0;
	noteStart.len = TICKS_16TH;
	noteStart.pitch = 0;
	noteEnd.time = TICKS_16TH * 1023;
	noteEnd.len = TICKS_16TH * 1024;
	noteEnd.pitch = 150;
	notes.addSingle(noteStart);
	notes.addSingle(noteEnd);
	ALEPH_ASSERT_THROW(clip.notes.minNote.time == 0);
	ALEPH_ASSERT_THROW(clip.notes.minNote.len == TICKS_16TH);
	ALEPH_ASSERT_THROW(clip.notes.minNote.pitch == 0);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.time == TICKS_16TH * 1023);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.len == TICKS_16TH * 1024);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.pitch == 150);
	notes.addSingle(noteStart);
	notes.addSingle(noteStart);
	notes.addSingle(noteEnd);
	notes.addSingle(noteEnd);
	notes.removeDuplicates();
	ALEPH_ASSERT_THROW(clip.notes.minNote.time == 0);
	ALEPH_ASSERT_THROW(clip.notes.minNote.len == TICKS_16TH);
	ALEPH_ASSERT_THROW(clip.notes.minNote.pitch == 0);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.time == TICKS_16TH * 1023);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.len == TICKS_16TH * 1024);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.pitch == 150);
	notes.removeSingle(noteStart);
	ALEPH_ASSERT_THROW(clip.notes.minNote.time > 0);
	notes.removeSingle(noteEnd);
	ALEPH_ASSERT_THROW(clip.notes.maxNote.time < TICKS_16TH * 1023);
	ALEPH_TEST_END();
}
template<typename T>
void testNoteProperties(T& notes) {

	for (note_t& note : notes) {
		ALEPH_ASSERT_THROW(note.time >= 0);
		ALEPH_ASSERT_THROW(note.len >= 0);
		ALEPH_ASSERT_THROW(note.pitch >= 0);
		ALEPH_ASSERT_THROW(note.velocity >= 0);
	}
}
void testTrackDataMidi() {
	ALEPH_TEST_BEGIN("testNoteView");
	trackdata_midi_t midi;
	clip_t* clip = new clip_t;
	tick_t lastnoteTime = 0;
	for (int i = 0; i < 256; i++) {
		note_t note;
		note.pitch=32+(i%10)*3;
		note.velocity=64+(-1+(i&1)*2)*(i%10)*3;
		note.time = i*TICKS_BAR;
		note.len = i*13*24;
		lastnoteTime = note.time;
		clip->notes.add(note);
	}
	clip->notes.updateBounds();
	midi.addClip(clip);
	ALEPH_ASSERT_THROW(clip->notes.lastNote.time == lastnoteTime);
	for (int iLoopEnabled = 0; iLoopEnabled < 2; iLoopEnabled++) {
		for (int i = 0; i < 14; i++) {
			clip->len = clip->notes.lastNote.time;
			clip->loopLen = clip->len;
			clip->loopEnabled = iLoopEnabled != 0;
			clip->setDirty();
			if (i > 0) {
				clip->loopLen = (i)*TICKS_BAR;
			}

			midi.sortClips();
			std::vector<note_t> notes;
			int step = 1;
			for (int j = 0; j < TICKS_BAR*1024; j+=step) {
				clip->len = j;
				clip->loopLen = clip->len;
				clip->setDirty();
				midi.sortClips();
				notes.clear();
				midi.getNotesInRange(math::max(0, j-TICKS_BAR*64), j*2, -1, -1, notes);
				testNoteProperties(notes);
				clip->setDirty();
				step+=step/60+1;
				clip->loopEnabled = true;
				tick_t rangeBegin;
				tick_t rangeEnd;
				rangeBegin = clip->notes.firstNote.start();
				rangeEnd = clip->notes.lastNote.end();
				notes.clear();
				midi.getNotesInRange(rangeBegin, rangeEnd, -1, -1, notes);
				testNoteProperties(notes);
				for (note_t& note : notes) {
					tick_t noteStart = note.start();
					tick_t noteEnd = note.end();
					ALEPH_ASSERT_THROW(noteEnd >= rangeBegin);
					ALEPH_ASSERT_THROW(noteStart < rangeEnd);
				}
				rangeBegin = clip->notes.firstNote.start() + 1;
				rangeEnd = clip->notes.lastNote.end() - 1;
				notes.clear();
				midi.getNotesInRange(rangeBegin, rangeEnd, -1, -1, notes);
				testNoteProperties(notes);
				for (note_t& note : notes) {
					tick_t noteStart = note.start();
					tick_t noteEnd = note.end();
					ALEPH_ASSERT_THROW(noteEnd >= rangeBegin);
					ALEPH_ASSERT_THROW(noteStart < rangeEnd);
				}
				rangeBegin = clip->notes.firstNote.start();
				rangeEnd = clip->notes.lastNote.end();
				notes.clear();
				midi.getNotesInRange(rangeBegin, rangeEnd, rangeBegin+32, rangeEnd-32, notes);
				testNoteProperties(notes);
				for (note_t& note : notes) {
					tick_t noteStart = note.start();
					tick_t noteEnd = note.end();
					ALEPH_ASSERT_THROW(noteEnd >= rangeBegin+32);
					ALEPH_ASSERT_THROW(noteStart < rangeEnd-32);
				}
				clip_notes_t& clipnotes = clip->getNoteViewRender();
				testNoteProperties(clipnotes.m_list);
				for (note_t& note : clipnotes.m_list) {
					tick_t noteStart = note.start();
					tick_t noteEnd = note.end();
					ALEPH_ASSERT_THROW(noteStart >= clip->start());
					ALEPH_ASSERT_THROW(noteEnd <= clip->end());
				}
			}
		}
	}
	midi.removeClip(clip);
	delete clip;
	clip = new clip_t;
	clip->time = 11354255;
	clip->len = 12145;
	clip->offsetStart = 11272335;
	clip->loopLen = 11272192;
	clip->loopEnabled = false;

	note_t n;
	n = note_t{52, 37, 11232734, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11232734, 6152, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11232734, 6152, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11232734, 6152, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11235903, 6152, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11235903, 6152, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11235903, 6152, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11235903, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11238886, 6151, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11239072, 5965, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11239072, 5965, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11239072, 5965, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11242055, 6151, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11242055, 6151, 1};  clip->notes.add(n);
	n = note_t{94, 37, 11242055, 6151, 1};  clip->notes.add(n);
	n = note_t{106, 37, 11242055, 6151, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11245037, 6338, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11245037, 6338, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{98, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{110, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11245037, 6338, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11245037, 6338, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{98, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{110, 37, 11245224, 6151, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11248393, 5965, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11248393, 5965, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11248393, 5965, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11248393, 5965, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11248393, 5965, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{78, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{90, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{102, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{78, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{90, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{102, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11251375, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11254358, 6152, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11254544, 5966, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11254544, 5966, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11254544, 5966, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11254544, 6152, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11254544, 6152, 1};  clip->notes.add(n);
	n = note_t{80, 37, 11254731, 6151, 1};  clip->notes.add(n);
	n = note_t{92, 37, 11254731, 6151, 1};  clip->notes.add(n);
	n = note_t{104, 37, 11254731, 6151, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11257527, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11260696, 5965, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11260696, 5965, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11260696, 5965, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11260696, 5965, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11260696, 5965, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11263679, 6151, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11263679, 6151, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11263679, 6151, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11263679, 6151, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11263679, 6151, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11263679, 6151, 1};  clip->notes.add(n);
	n = note_t{81, 37, 11266661, 6152, 1};  clip->notes.add(n);
	n = note_t{93, 37, 11266661, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11266848, 5965, 1};  clip->notes.add(n);
	n = note_t{105, 37, 11266848, 5965, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11266848, 6151, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11269830, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11269830, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11270017, 5965, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11270017, 5965, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11270017, 5965, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11270017, 5965, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11272999, 5966, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11272999, 5966, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11272999, 6152, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11272999, 6152, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11272999, 6152, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11272999, 6152, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11272999, 6152, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11272999, 6152, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11275982, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11275982, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11275982, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11275982, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11279151, 2983, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11279151, 2983, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11279151, 2983, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11279151, 2983, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11279151, 2983, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11276168, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11276168, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11282320, 5965, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11282320, 5965, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11282320, 5965, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11282320, 5965, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11285303, 6151, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11285303, 6151, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11285303, 6151, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11285303, 6151, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11288285, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11288285, 6152, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11288285, 6152, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11288285, 6338, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11288285, 6338, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{94, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{106, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{70, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{82, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{94, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{106, 37, 11291454, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{74, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{98, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{110, 37, 11294623, 5966, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11294623, 6152, 1};  clip->notes.add(n);
	n = note_t{86, 37, 11294623, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11297606, 6152, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11297606, 6152, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11297606, 6152, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11297606, 6152, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11297606, 6152, 1};  clip->notes.add(n);
	n = note_t{78, 37, 11300775, 5965, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{90, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{102, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{90, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{102, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11300775, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11303758, 6151, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11303758, 6151, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11303758, 6151, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11303758, 6151, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11303758, 6151, 1};  clip->notes.add(n);
	n = note_t{80, 37, 11304130, 6152, 1};  clip->notes.add(n);
	n = note_t{92, 37, 11304130, 6152, 1};  clip->notes.add(n);
	n = note_t{104, 37, 11304130, 6152, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11306927, 5965, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11306927, 6151, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11309909, 6152, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11309909, 6152, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11309909, 6152, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11309909, 6152, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11309909, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11313078, 5965, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11313078, 5965, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11313078, 5965, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11313078, 5965, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11313078, 5965, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11313078, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11313078, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{81, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{93, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{105, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11316061, 6151, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11316247, 5965, 1};  clip->notes.add(n);
	n = note_t{76, 37, 11319230, 5965, 1};  clip->notes.add(n);
	n = note_t{88, 37, 11319230, 5965, 1};  clip->notes.add(n);
	n = note_t{100, 37, 11319230, 5965, 1};  clip->notes.add(n);
	n = note_t{112, 37, 11319230, 5965, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11319230, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11319230, 6152, 1};  clip->notes.add(n);
	n = note_t{52, 37, 11319230, 6152, 1};  clip->notes.add(n);
	n = note_t{64, 37, 11319230, 6152, 1};  clip->notes.add(n);
	n = note_t{72, 37, 11322212, 6152, 1};  clip->notes.add(n);
	n = note_t{84, 37, 11322212, 6152, 1};  clip->notes.add(n);
	n = note_t{96, 37, 11322212, 6152, 1};  clip->notes.add(n);
	n = note_t{108, 37, 11322212, 6152, 1};  clip->notes.add(n);
	n = note_t{120, 37, 11322212, 6152, 1};  clip->notes.add(n);
	clip->setDirty();
	clip->notes.updateBounds();
	lastnoteTime = n.time;
	ALEPH_ASSERT_THROW(clip->notes.lastNote.time == lastnoteTime);

	midi.addClip(clip);
	clip_notes_t& clipnotes = clip->getNoteViewRender();
	testNoteProperties(clipnotes.m_list);
	for (note_t& note : clipnotes.m_list) {
		tick_t noteStart = note.start();
		tick_t noteEnd = note.end();
		ALEPH_ASSERT_THROW(noteStart >= 0);
		ALEPH_ASSERT_THROW(noteEnd <= clip->len);
	}
	midi.removeClip(clip);
	delete clip;

	ALEPH_TEST_END();
}
void testReferences() {
	ALEPH_TEST_BEGIN("testReferences");
	clip_t clipInstance;
	clipInstance.time = 0;
	clipInstance.len = TICKS_BAR * 4;
	clip_t* clip = &clipInstance;
	clip_view view;

	int32_t cursorPos = 64;

	int32_t pitch = 32;
	tick_t tickL = 128;
	tick_t len = 128;

	// doubleclick == add single note
	note_t note;
	note.pitch = pitch;
	note.time = tickL;
	note.len = len;
	clip->notes.add(note); // add by value
	note_t* contextNote;
	contextNote = clip->notes.get(tickL, pitch); // get reference back
	ALEPH_ASSERT_THROW(contextNote != NULL);
	clip->notes.selection.insert(contextNote); // put the added note reference in selection

	//cut, shift cursor by 64 ticks, paste, repeat
	for (int i = 0; i < 100; i++) {
		//cut notes = ctrl+x
		view.clipboard.setTo(clip->notes.selection, -cursorPos);
		clip->notes.deleteSelectedNotes(clip->notes);

		ALEPH_ASSERT_THROW(clip->notes.get(tickL, pitch) == NULL);
		ALEPH_ASSERT_THROW(clip->notes.empty());

		// move cursor to right
		cursorPos += 64; 
		tickL += 64;
		tick_t cursorToNoteOffset = tickL - cursorPos;
						
		//paste
		clip->notes.selection.clear();
		tick_t pastCursorOffset = cursorPos;
		size_t pos = clip->notes.m_list.size();
		for (note_t note : view.clipboard.m_list) { //not using reference here, copy while iterating
			note.time += pastCursorOffset;
			clip->notes.add(note);
		}
		clip->notes.selectIdxRange(pos, clip->notes.m_list.size());
		ALEPH_ASSERT_THROW(clip->notes.get(tickL + cursorToNoteOffset, pitch) != NULL);
		ALEPH_ASSERT_THROW(!clip->notes.empty());
		ALEPH_ASSERT_THROW(clip->notes.m_list.size() == 1);
	}
	ALEPH_TEST_END();
}

}
int main() {
	testDuplicates();
	testReferences();
	testTrackDataMidi();
	return 0;
}
