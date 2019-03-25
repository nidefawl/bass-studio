#include <memory>

#include "fileloader.h"
#include "track.h"
#include "clipboard.h"

using std::shared_ptr;
using std::make_shared;
using std::move;

tick_t roundUp(tick_t len) {
	if (len > 0 && (len&(TICKS_BAR-1)) == 0)
		return len;
	tick_t rndLen = TICKS_BAR;
	while (len < rndLen && rndLen > TICKS_16TH) {
		rndLen >>= 1;
	}
	return ((len+rndLen)/rndLen)*rndLen;
}
void LoadMidiTask::loadFile() {
	try {
		if (StrEndsWith(path, ".mid") && FileExists(path)) {
			MidiFile midiFile;
			if (midiFile.read(path)) {
				shared_ptr<clip_clipboard> fileClipboard = make_shared<clip_clipboard>();
				printf("read %s success\n", StringAsCStr(path));
				tick_t tpqMidiFile = midiFile.getTicksPerQuarterNote();
				//tick_t scale = TICKS_QUARTER / tpqMidiFile; // they better use multiple of 8 or something
				int tracks = midiFile.getTrackCount();
				if (!tracks) {
					my_printf("No tracks in midi file\n",0);
				}

				tick_t tickClipMin = -1;
				tick_t tickClipMax = -1;
				for (int track = 0; track < tracks; track++) {
					clip_notes_t notes;
					MidiEventList& list = midiFile[track];
					list.linkEventPairs();
					int events = list.size();
					if (!events) {
						my_printf("No events in midi track %d\n", track);
					}
					int noteOnEvents = 0;
					for (int event = 0; event < events; event++) {
						MidiEvent& evt = list[event];
						int evtSize = evt.size();
						if (evtSize)
//						my_printf("Event[%d] = %02X\n", event, evt[0]);
						if (evt.isNoteOn()) {
							noteOnEvents++;
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
								my_printf("midi lib failed to link the events\n", 0);
							}
						}
					}
					my_printf("%d noteOnEvents in midi track %d\n", noteOnEvents, track);
					if (!notes.empty()) {
						shared_ptr<track_clipboard_t> trClipboard = make_shared<track_clipboard_t>();
						notes.updateBounds();
						tick_t clipLength = roundUp(notes.lastNote.end()-notes.firstNote.start());

						String filepath, name, ext;
						SplitPath(path, &filepath, &name, &ext);
//							my_printf("%s %s %s\n", StringAsCStr(path), StringAsCStr(name), StringAsCStr(ext));
						clip_t clip(CLIP_MIDI, name);
						//clip.notes = move(notes);
						clip.notes = notes;
						clip.time = 0;
						clip.offsetStart = 0;
						clip.setLen(clipLength);
						clip.loopEnabled = false;
						if (tickClipMin < 0) {
							tickClipMin = clip.start();
							tickClipMax = clip.end();
						} else {
							tickClipMin = math::min(tickClipMin, clip.start());
							tickClipMax = math::max(tickClipMax, clip.end());
						}
						trClipboard->clips.push_back(make_shared<clip_t>(move(clip)));
						fileClipboard->tracks.push_back(trClipboard);
					}
				}
				if (fileClipboard->tracks.size()) {
					for (auto& track : fileClipboard->tracks) {
						for (auto& clip : track->clips) {
							clip->setLen(tickClipMax-tickClipMin);
							clip->loopLen = clip->getLen();
						}
					}
					fileClipboard->srcTrack = 0;
					fileClipboard->srcPos = 0;
					fileClipboard->selRange = tickClipMax-tickClipMin;
					fileClipboard->selTrackRange = fileClipboard->tracks.size() - 1;
					this->clipboard = fileClipboard;
				}
			} else {

				my_printf("failed reading %s, its hard\n",StringAsCStr(path));
			}

		}
	} catch (...) {
		printf("Exception loading midi file %s\n", StringAsCStr(path));
	}
}
