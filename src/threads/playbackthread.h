#pragma once
#include "seq_time.h"
#include <memory>
#include <stdexcept>
#define PLAYBACK_STOP 0
#define PLAYBACK_START 1
struct PlaybackRequest {
	int32_t msgId;
	int32_t param;
};
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

public:

private:
	class Impl;

public:
	PlaybackThread();
	~PlaybackThread();
    void startThread();
    void stopThread();
	void joinThread();
	ThreadLock lockThread();
	void addRequest(PlaybackRequest r);
	playback_state getState();

private:
	Impl* _M_impl;
};

