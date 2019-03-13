#pragma once
#include "threadlock.h"
#include "seq_time.h"
#include <memory>
#include <stdexcept>
#include <functional>

#define REQ_STATE 1
#define GUI_CALL 2
class PlaybackThread
{

private:
	class Impl;

public:
	PlaybackThread();
	~PlaybackThread();
    void startThread();
    void stopThread();
	void joinThread();
	ThreadLock lockThread();
	void addRequest(int32_t msgId, int32_t param, bool wait);
	void call(std::function<void()> fn, bool wait);
	playback_state getState();

private:
	Impl* _M_impl;
};

