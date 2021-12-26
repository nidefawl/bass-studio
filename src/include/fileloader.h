#pragma once
#include "threads/workerthread.h"
#include "midi/MidiFile.h"
#include "seq_time.h"
#include "str_util.h"
#include "clip.h"
#include "fileio.h"
#include "logging.h"
#include "clipboard.h"

class LoadMidiTask : public WorkerThread::ThreadTask {
    String path;
    std::shared_ptr<clip_clipboard> clipboard;
    void loadFile();

public:
    explicit LoadMidiTask(String& _path) : ThreadTask() { this->path = _path; }
    void run() override { loadFile(); }

public:
    std::shared_ptr<clip_clipboard> getClipboard() { return clipboard; }
};
