#pragma once
#include "../threads/workerthread.h"
#include "../midi/MidiFile.h"
#include "seq_time.h"
#include "str_util.h"
#include "clip.h"
#include "fileio.h"
#include "logging.h"

class LoadMidiTask : public WorkerThread::ThreadTask {
	String path;
	clip_t* loadedClip = NULL;
public:
	LoadMidiTask(String& _path) : ThreadTask() {
		this->path = _path;
	}
	void run() {
		try {
			if (StrEndsWith(path, ".mid") && FileExists(path)) {
				MidiFile midiFile;
				if (midiFile.read(path)) {
					printf("read %s success\n", StringAsCStr(path));
					tick_t tpqMidiFile = midiFile.getTicksPerQuarterNote();
					//tick_t scale = TICKS_QUARTER / tpqMidiFile; // they better use multiple of 8 or something
					int tracks = midiFile.getTrackCount();
					for (int track = 0; track < tracks; track++) {
						clip_notes_t notes;
						MidiEventList& list = midiFile[track];
						list.linkEventPairs();
						int events = list.size();
						for (int event = 0; event < events; event++) {
							MidiEvent& evt = list[event];
							if (evt.isNoteOn()) {
								MidiEvent* evt2 = evt.getLinkedEvent();
								if (evt2 != NULL) {
									if (evt2->isNoteOff()) {
										//yay
										tick_t start = evt.tick;
										tick_t end = evt2->tick;
										int32_t key = evt2->getKeyNumber();
										note_t note;
										note.time = (((start*100)/tpqMidiFile)*TICKS_QUARTER)/100;
										note.len = ((((end-start)*100)/tpqMidiFile)*TICKS_QUARTER)/100;
										note.pitch = key;
										notes.m_list.push_back(note);
//										my_printf("note %d %d - %d\n", key, start, end);
									}
								} else {
//									my_printf("yeah, great the fucking midi lib failed to link the events again, fuck off!!!\n", 0);
								}
							}
						}
						if (!notes.empty()) {
							notes.updateBounds();
							String filepath, name, ext;
							SplitPath(path, &filepath, &name, &ext);
//							my_printf("%s %s %s\n", StringAsCStr(path), StringAsCStr(name), StringAsCStr(ext));
							clip_t* clip = new clip_t(name);
							clip->notes = notes;
							clip->time = 0;
							clip->len = notes.lastNote.end()-notes.firstNote.start();
							clip->loopStart = 0;
							clip->loopLen = clip->len;
							this->loadedClip = clip;
//							MainCtrl::get()->getTrackId(0)->add(clip);
//							MainCtrl::get()->updateVisibleTrackContents();
						}
						break;
					}
				} else {

					my_printf("failed reading %s, its hard\n",StringAsCStr(path));
				}

			}
		} catch (...) {
		}
	}
public:
	clip_t* getClip() {
		return loadedClip;
	}
};
