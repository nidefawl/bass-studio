#pragma once
#include "thread.h"
#include "threadlock.h"
#include "seq_time.h"
#include <memory>
#include <stdexcept>
#include <functional>

#define REQ_STATE 1
#define GUI_CALL 2
class project_controller_t;
class PlaybackThread : public seqthreads::thread_base
{

private:
	class Impl;

public:
	PlaybackThread();
	~PlaybackThread();
    void startThread(project_controller_t* ctrl);
    void stopThread();
	void joinThread();
	ThreadLock lockThread();
	ThreadLock tryLockThread();
	void addRequest(int32_t msgId, int32_t param, bool wait);
	void call(std::function<void()> fn, bool wait);
	playback_state getState();
	int32_t getThreadId() override;
	void setTls(daw_tls::tlsinstance tls) override;

	// just for debug asserts, not synchronization control, since this has race conditions
	bool isLocked();

private:
	Impl* _M_impl;
};

