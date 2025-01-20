#pragma once
#include "threads/workerthread.hpp"
#include "str_util.hpp"
#include "host/daw/clipboard.hpp"

namespace DAW {
    std::shared_ptr<clip_clipboard> LoadMidiFile(const String& path);
    bool SaveMidiFile(const String& path, const clip_clipboard& clipboard);
}

class LoadMidiTask final : public WorkerThread::ThreadTask {
    String path;
    std::shared_ptr<clip_clipboard> clipboard;
public:
    explicit LoadMidiTask(String _path) : ThreadTask(), path(std::move(_path)) {}
    void run() override;

public:
    std::shared_ptr<clip_clipboard> getClipboard() { return clipboard; }
};
