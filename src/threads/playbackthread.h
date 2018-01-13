#pragma once
#include "seq_time.h"
#include <memory>
#include <stdexcept>
#include <functional>

#define REQ_STATE 1
#define GUI_CALL 2
class ThreadLock {
public:
	class Impl;
private:
	Impl* _M_impl;
public:
	ThreadLock() = delete;
	ThreadLock(Impl* _M_impl);
	~ThreadLock();
	ThreadLock (const ThreadLock&) = delete;
	ThreadLock& operator= (const ThreadLock&) = delete;
	ThreadLock& operator=(ThreadLock&& other);
	ThreadLock(ThreadLock&& other);

};
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

