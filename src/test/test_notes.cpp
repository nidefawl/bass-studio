#include "tests.h"

#include <vector>
#include <assert.h>
#include <stdint.h>
#include "seq_time.h"
#include "clip.h"
#include "mainctrl.h"
#include "project.h"

namespace {

void testDuplicates() {
	test_rng rng;
	clip_t clip;
	clip_notes_t& notes = clip.notes;
	int nNotes = 20;
	int nDuplicates = 3;

	for (int i = 0; i < nNotes; i++) {
		note_t note;
		note.time = (2 + i*4) * TICKS_16TH;
		note.len = 6 * TICKS_16TH;
		note.pitch = 36 + (rng.randI() & 0x3F);
		for (int j = 0; j < nDuplicates; j++) {
			notes.add(note);
		}
	}
	assert(notes.m_list.size() == nNotes * nDuplicates);
	notes.removeDuplicates();
	assert(notes.m_list.size() == nNotes * 1);
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
	assert(clip.notes.minNote.time == 0);
	assert(clip.notes.minNote.len == TICKS_16TH);
	assert(clip.notes.minNote.pitch == 0);
	assert(clip.notes.maxNote.time == TICKS_16TH * 1023);
	assert(clip.notes.maxNote.len == TICKS_16TH * 1024);
	assert(clip.notes.maxNote.pitch == 150);
	notes.addSingle(noteStart);
	notes.addSingle(noteStart);
	notes.addSingle(noteEnd);
	notes.addSingle(noteEnd);
	notes.removeDuplicates();
	assert(clip.notes.minNote.time == 0);
	assert(clip.notes.minNote.len == TICKS_16TH);
	assert(clip.notes.minNote.pitch == 0);
	assert(clip.notes.maxNote.time == TICKS_16TH * 1023);
	assert(clip.notes.maxNote.len == TICKS_16TH * 1024);
	assert(clip.notes.maxNote.pitch == 150);
	notes.removeSingle(noteStart);
	assert(clip.notes.minNote.time > 0);
	notes.removeSingle(noteEnd);
	assert(clip.notes.maxNote.time < TICKS_16TH * 1023);
}
void testReferences() {
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
	assert(contextNote != NULL);
	clip->notes.selection.insert(contextNote); // put the added note reference in selection

	//cut, shift cursor by 64 ticks, paste, repeat
	for (int i = 0; i < 100; i++) {
		//cut notes = ctrl+x
		view.clipboard.setTo(clip->notes.selection, -cursorPos);
		clip->notes.deleteSelectedNotes(clip->notes);

		assert(clip->notes.get(tickL, pitch) == NULL);
		assert(clip->notes.empty());

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
		assert(clip->notes.get(tickL + cursorToNoteOffset, pitch) != NULL);
		assert(!clip->notes.empty());
		assert(clip->notes.m_list.size() == 1);
	}
}

}
void testNotes() {
	testDuplicates();
	testReferences();
	printf("sizeof(note_t): %llu\n", sizeof(note_t));
}
void testTickConversions() {
	project_globals_t project;
	samplerate_t samplerate = 44100;
	int32_t blocksize = 512;
	int32_t tempo100 = 12800;
	int32_t blockPos = 0;
	for (blockPos = 0; blockPos < 160000; blockPos++) {
		double blockStartTick = blockToTickPrecise(blockPos, tempo100, samplerate, blocksize);
//		printf("BLOCK %d blockToTickPrecise %f\n", blockPos, blockStartTick);
		double block = tickToBlockPrecise(blockStartTick, tempo100, samplerate, blocksize);
//		double rounded = round(block);
//		int32_t blockPosI = (int32_t) rounded;
//		printf("blockStartTick %f tickToBlockPrecise %f, to int %d\n", blockStartTick, block, blockPosI);
		assert(almost_equal((double)blockPos, block, 2));
//		assert((double)blockPos == block);
	}
}
