#include "playbackthread.h"
#ifdef __MINGW32__
#undef _GLIBCXX_HAS_GTHREADS
#include "mingw.thread.h"
#include <mutex>
#include "mingw.mutex.h"
#include "mingw.condition_variable.h"
#else
#include <mutex>
#endif

#include <atomic>
#include <queue>

#include <assert.h>

#include "seq_time.h"
#include "mainctrl.h"
#include "logging.h"
#include "../util/readerwriterqueue.h"
#include "../host/vst_host.h"
#include <windows.h>
#pragma GCC diagnostic push
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
    playback_state m_status = status_stop;
	std::recursive_mutex mutex;
public:
    Impl() : q(128) {
	}
	void start() {
		t = std::thread([this]() {
			this->run();
		});
		HANDLE h = t.native_handle();
		SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
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
    double QPC_TOSECONDS(LARGE_INTEGER& iStart, LARGE_INTEGER& iStop, LARGE_INTEGER& freq) {
    	return ((double) iStop.QuadPart - (double) iStart.QuadPart) / (double) freq.QuadPart;
    }
	void run() {
	    HANDLE hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
	    if (NULL == hTimer)
	    	throw new SystemException(GetLastError(), "CreateWaitableTimer failed");
	    LARGE_INTEGER liDueTime;

		MainCtrl* ctrl = MainCtrl::get();
		vsthost* host = vsthost::getInstance();
		playback_state state = status_stop;
		static double playbackDuration = 0;
        LARGE_INTEGER freq, iStart, iStop;
        if (!QueryPerformanceFrequency(&freq)) {
        	throw new SystemException(GetLastError(), "QueryPerformanceFrequency failed");
        }
		freq.QuadPart /= 1000; // calc milliseconds
		bool firstBlock = false;
		bool isLoopAround = false;
		std::shared_ptr<PlaybackThreadReq> req;
		int32_t samplePos;
		double tickPos;
        while (true){
        	samplerate_t sampleRate = host->lSampleRate;
        	int32_t blockSize = host->lBlockSize;
        	if (q.try_dequeue(req)) {
        		switch (req->msgId) {
        		case PLAYBACK_START:
        			if (state != status_play) {
            			state = m_status = status_play;
                    	tickPos = req->param;
            			ctrl->getPlaybackPos() = req->param;
                    	int32_t bpm100 = ctrl->getCurrentTempo();
                    	samplePos = tickToSample(req->param, bpm100, sampleRate, blockSize);
            			LOG("START ON seconds: %.2f - sample %d\n", toSeconds(req->param, bpm100), samplePos);
            			host->onStartPlayback(0);
                        QueryPerformanceCounter(&iStart);
                        playbackDuration = 0;
                        firstBlock = true;
                        isLoopAround = false;
        			}
        			break;
        		case PLAYBACK_STOP:
        			if (state != status_stop) {
            			state = m_status = status_stop;
            			host->onStopPlayback();
        			}
        			break;
        		case GUI_CALL:
        			req->fn();
        			break;
        		case PLAYBACK_THREAD_EXIT:
#ifndef NDEBUG
    				LOG("PLAYBACK_THREAD_EXIT");
    				Sleep(200);
#endif
            		req->notify();
        			return;
        		}
        		req->notify();
        		req.reset();
        	}
        	int32_t bpm100 = ctrl->getCurrentTempo();


            bool inLoop = tickPos >= ctrl->loopStart && tickPos < ctrl->loopStart+ctrl->loopLen
            		&& state == status_play && ctrl->loopEnabled;

            //this is stupid
//			if (state != status_play) {
//				tickPos = ctrl->cursor.cursorPos;
//			}
            int32_t processedBlock;
            {
            	ThreadLock lock = this->lockThread();
            	processedBlock = host->processPlayback(samplePos, tickPos, state, inLoop, isLoopAround);
            }
            /*
             * at sample rate 44100 and blocksize 512 the block duration is 1.xxms
             * the producer side trys to stay 4 blocks ahead of the consumer (audio thread)
             * We can expect processPlayback to only process one block under normal load
             */
			if (processedBlock > 1) {
				LOG("processedBlock > 1: %d\n", processedBlock);
			} else if (!processedBlock) {

	        	/*
	        	 * always sleep at least 10*1000 * 100ns = 1 000 000ns = 1 000microS = 1ms
	        	 * This should be adjusted depending on samplerate and blocksize (and load)
	        	 */
	    	    liDueTime.QuadPart = -10 * 1000;
	            if (!SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0))
	    	    	throw new SystemException(GetLastError(), "SetWaitableTimer failed");
	            if (WaitForSingleObject(hTimer, INFINITE) != WAIT_OBJECT_0)
	    	    	throw new SystemException(GetLastError(), "WaitForSingleObject failed");
			}
			if (state == status_play) {
				double blocksPerS = sampleRate / (double) blockSize;
				double msPerBlock = 1000.0 / blocksPerS;
				const double ticksPerBlock = toTickPrecise(blockSize/(double)sampleRate, bpm100);
				if (processedBlock) {
		            isLoopAround = false;
					samplePos += blockSize*processedBlock;
					tickPos += ticksPerBlock;
					if (inLoop) {
						if (tickPos >= ctrl->loopStart + ctrl->loopLen) {
							ctrl->setJumpFromTo(tickPos, ctrl->loopStart);
							LOG("JMP FROM %.2f to %d\n", tickPos, ctrl->loopStart);
							tickPos = ctrl->loopStart;
							samplePos = tickToSample(ctrl->loopStart, bpm100, sampleRate, blockSize);
							LOG("JMP LOOPBEGIN seconds: %.2f - BLOCK %d\n", toSeconds(ctrl->loopStart, bpm100), samplePos / blockSize);
							isLoopAround = true;
						}
					}
					ctrl->getPlaybackPos() = (int32_t) floor(tickPos);
					playbackDuration += msPerBlock;
				}
			}
			if (playbackDuration > 10000 && state == status_play) {
				QueryPerformanceCounter(&iStop);
				double wallTime = QPC_TOSECONDS(iStart, iStop, freq);
	            LOG("playbackDuration %.4f wallTime %.4f error %.4f\n", playbackDuration, wallTime, playbackDuration-wallTime);
	            playbackDuration = 0;
                QueryPerformanceCounter(&iStart);
			}

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

ThreadLock& ThreadLock::operator=(ThreadLock&& other) {
	this->_M_impl = other._M_impl;
	other._M_impl = NULL;
	return *this;
}
ThreadLock::ThreadLock(ThreadLock&& other) {
	this->_M_impl = other._M_impl;
	other._M_impl = NULL;
}


#pragma GCC diagnostic pop
