#include "playbackthread.h"
#include <chrono>
#include <atomic>
#include <queue>
#include <assert.h>

#include "error.h"
#include "threadlock.h"
#include "threads.h"
#include "seq_time.h"
#include "hires_timer.h"
#include "util/readerwriterqueue.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "logging.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _MSC_VER
#pragma GCC diagnostic push
#endif
#ifdef __clang__
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif
using namespace moodycamel;

#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)

#define PLAYBACK_THREAD_EXIT 255

class ThreadLock::Impl {
	std::recursive_mutex& mutex;
public:
	Impl(std::recursive_mutex& _mutex) : mutex(_mutex) {mutex.lock(); }
	~Impl() {mutex.unlock(); }
};

class PlaybackThreadReq {
    std::mutex m_mtx;
    std::condition_variable m_cond;
    std::atomic<bool> m_finished{false};
public:
	int32_t msgId = 0;
	int32_t param = 0;
	std::function<void()> fn;
	PlaybackThreadReq(int32_t _msgId, int32_t _param, std::function<void()> _fn)
	: msgId(_msgId), param(_param), fn(_fn)
	{

    }
	PlaybackThreadReq(int32_t _msgId, int32_t _param)
	: msgId(_msgId), param(_param)
	{

    }
	~PlaybackThreadReq() = default;
	void wait() {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (!m_finished) {
            m_cond.wait(lock, [&](){ return m_finished == true; });
        }
	}
	void notify() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_finished = true;
        m_cond.notify_all();
	}
};

class PlaybackThread::Impl {
	std::thread t;
	ReaderWriterQueue<std::shared_ptr<PlaybackThreadReq>> q;
    playback_state m_status = status_no_process;
	std::recursive_mutex mutex;
	int32_t threadid = 0;
public:
    Impl() : q(128) {
	}
    int32_t getThreadId() {
    	return threadid;
    }
	void start() {
		t = std::thread([this]() {
			this->run();
		});
#ifdef _WIN32
		this->threadid = static_cast<int32_t>(t.get_id().get());
		HANDLE h = reinterpret_cast<HANDLE*>(t.native_handle());
		SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
#endif
	}
	void join() {
		assert(t.joinable());
		t.join();
	}
	void addRequest(std::shared_ptr<PlaybackThreadReq>& req) {
		assert(t.joinable());
		if (!q.enqueue(req)) {
			assert(0&&"Failed enqeueing req");
		}
	}

    void stop(){
		assert(t.joinable());
		auto req = std::make_shared<PlaybackThreadReq>(PLAYBACK_THREAD_EXIT, 0);
		if (!q.enqueue(req)) {
			assert(0&&"Failed enqeueing req");
		}
        req->wait();
    }
    playback_state getState() const {
    	return m_status;
    }
    ThreadLock lockThread() {
    	ThreadLock t(new ThreadLock::Impl(mutex));
    	return std::move(t); //CANNOT RELY ON RVO
    }
private:

	void run() {
		setCurrentThreadName("audiothread");

		MainCtrl* ctrl = MainCtrl::get();
		vsthost* host = vsthost::getInstance();
		double playbackDuration = 0;
		hires_timer_t timer;

		bool firstBlock = false;
		bool isLoopAround = false;
		std::shared_ptr<PlaybackThreadReq> req;
		int32_t samplePos = 0;
		double tickPos = 0;

		try {
        while (true){
        	samplerate_t sampleRate = host->lSampleRate;
        	int32_t blockSize = host->lBlockSize;
        	if (q.try_dequeue(req)) {
        		switch (req->msgId) {
        		case REQ_STATE:
					{
						playback_state reqState = (playback_state) req->param;
						switch (reqState) {
							case playback_state::status_play:
							{
								tick_t startPos = ctrl->cursor.cursorPos;
								tickPos = startPos;
								ctrl->getPlaybackPos() = startPos;
								int32_t bpm100 = ctrl->getCurrentTempo();
								samplePos = tickToSample(startPos, bpm100, sampleRate);
								LOG("START ON seconds: %.2f - sample %d\n", toSeconds(startPos, bpm100), samplePos);
								host->onStartPlayback(0);
								timer.reset();
								playbackDuration = 0;
								firstBlock = true;
								isLoopAround = false;
								break;
							}
							case playback_state::status_stop:
							{
								host->onStopPlayback();
								break;
							}
							case playback_state::status_no_process:
							{
								break;
							}
						}
						m_status = reqState;
					}
					break;
        		case GUI_CALL:
        			req->fn();
        			break;
        		case PLAYBACK_THREAD_EXIT:
#ifndef NDEBUG
    				LOG("PLAYBACK_THREAD_EXIT");
    				std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
            		req->notify();
        			return;
        		}
        		req->notify();
        		req.reset();
        	}


            //this is stupid
//			if (state != status_play) {
//				tickPos = ctrl->cursor.cursorPos;
//			}
            int32_t processedBlock = 0;
            bool inLoop = false;
            if (m_status != playback_state::status_no_process)
            {
				std::unique_lock<std::recursive_mutex> lock(mutex);


            	//ctrl may still alter project settings during copy here
            	project_globals_t& projGlobals = host->project;
            	projGlobals = *static_cast<project_globals_t*>(ctrl);

            	inLoop = (tickPos >= projGlobals.loopStart
            			&& tickPos < projGlobals.loopStart+projGlobals.loopLen
						&& m_status == status_play && projGlobals.loopEnabled);
            	processedBlock = host->processPlayback(samplePos, tickPos, m_status, inLoop, isLoopAround);
            }
            /*
             * at sample rate 44100 and blocksize 512 the block duration is 1.xxms
             * the producer side trys to stay 4 blocks ahead of the consumer (audio thread)
             * We can expect processPlayback to only process one block under normal load
             */
			if (processedBlock > 1) {
				LOG("processedBlock > 1: %d\n", processedBlock);
			} else if (!processedBlock) {

				std::this_thread::sleep_for(std::chrono::microseconds(10000));

			}
			if (m_status == status_play) {
            	project_globals_t& projGlobals = host->project;
            	int32_t bpm100 = projGlobals.tempo100;
				double blocksPerS = sampleRate / (double) blockSize;
				double msPerBlock = 1000.0 / blocksPerS;
				const double ticksPerBlock = toTickPrecise(blockSize/(double)sampleRate, bpm100);
				if (processedBlock) {
		            isLoopAround = false;
					samplePos += blockSize*processedBlock;
					tickPos += ticksPerBlock;
					if (inLoop) {
						if (tickPos >= projGlobals.loopStart + projGlobals.loopLen) {
							ctrl->setJumpFromTo(tickPos, projGlobals.loopStart);
							LOG("JMP FROM %.2f to %d\n", tickPos, projGlobals.loopStart);
							tickPos = projGlobals.loopStart;
							samplePos = tickToSample(projGlobals.loopStart, bpm100, sampleRate);
							LOG("JMP LOOPBEGIN seconds: %.2f - BLOCK %d\n", toSeconds(projGlobals.loopStart, bpm100), samplePos / blockSize);
							isLoopAround = true;
						}
					}
					ctrl->getPlaybackPos() = (int32_t) floor(tickPos);
					playbackDuration += msPerBlock;
				}
			}
			if (playbackDuration > 10000 && m_status == status_play) {
				double wallTimeMs = timer.getTimeDouble() * 1000.0;
	            LOG("playbackDuration %.4f wallTime %.4f error %.4f\n", playbackDuration, wallTimeMs, playbackDuration-wallTimeMs);
	            playbackDuration = 0;
	            timer.reset();
			}

//	    	logEveryMsec(1, 5000, "audio thread loop");
        }
		} catch (std::exception& e) {
			handleStdException(e);
		}
	}
};
PlaybackThread::~PlaybackThread() {
	delete _M_impl;
}

PlaybackThread::PlaybackThread() :
	_M_impl { new PlaybackThread::Impl {  } } {
}

void PlaybackThread::call(std::function<void()> fn, bool wait) {
	auto r = std::make_shared<PlaybackThreadReq>(GUI_CALL, 0, fn);
	_M_impl->addRequest(r);
	if (wait) {
		r->wait();
	}
}
void PlaybackThread::addRequest(int32_t _msgId, int32_t _param, bool wait) {
	auto r = std::make_shared<PlaybackThreadReq>(_msgId, _param);
	_M_impl->addRequest(r);
	if (wait) {
		r->wait();
	}
}
void PlaybackThread::startThread() {
	_M_impl->start();
}
void PlaybackThread::stopThread() {
	_M_impl->stop();
}
void PlaybackThread::joinThread() {
	_M_impl->join();
}
int32_t PlaybackThread::getThreadId() {
	return _M_impl->getThreadId();
}
playback_state PlaybackThread::getState() {
	return _M_impl->getState();
}

ThreadLock::ThreadLock(ThreadLock::Impl* impl) :
	_M_impl(impl) {
}
ThreadLock::~ThreadLock() {
	if (_M_impl)
		delete _M_impl;
}
ThreadLock PlaybackThread::lockThread() {
	ThreadLock t = _M_impl->lockThread();
	return std::move(t); //CANNOT RELY ON RVO
}

ThreadLock& ThreadLock::operator=(ThreadLock&& other) noexcept {
	this->_M_impl = other._M_impl;
	other._M_impl = NULL;
	return *this;
}
ThreadLock::ThreadLock(ThreadLock&& other) noexcept {
	this->_M_impl = other._M_impl;
	other._M_impl = NULL;
}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

