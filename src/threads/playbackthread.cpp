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


class PlaybackThread::Impl {
	std::thread t;
    BlockingReaderWriterQueue<PlaybackRequest> q;
    playback_state m_status = status_stop;
	std::recursive_mutex mutex;
public:
    Impl() : q(128) {
	}
	void start() {
		t = std::thread([this]() {
			this->run();
		});
	}
	void join() {
		t.join();
	}
	void addRequest(PlaybackRequest& r) {
		q.enqueue(r);
	}

    void stop(){
        q.enqueue(PlaybackRequest{PLAYBACK_THREAD_EXIT, 0});
    }
    bool dequeue(PlaybackRequest& r, bool blocking) {
    	if (blocking) {
    		q.wait_dequeue(r);
    		return true;
    	}
    	return q.try_dequeue(r);
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
		tick_t pos = 0;
		playback_state state = status_stop;
		PlaybackRequest req;
		static double approxTimePassed = 0;
		int32_t blockPos = 0;
		tick_t startTick = 0;
        LARGE_INTEGER freq, iStart, iStop;
        if (!QueryPerformanceFrequency(&freq)) {
        	throw new SystemException(GetLastError(), "QueryPerformanceFrequency failed");
        }
		freq.QuadPart /= 1000; // calc milliseconds
		double wallTime = 0;
		bool firstBlock = false;
        while (true){
        	samplerate_t sampleRate = host->lSampleRate;
        	int32_t blockSize = host->lBlockSize;
        	int32_t bpm100 = ctrl->getCurrentTempo();
        	assert(bpm100>0);
        	if (dequeue(req, false)) {
        		switch (req.msgId) {
        		case PLAYBACK_START:
        			if (state != status_play) {
            			state = m_status = status_play;
            			pos = req.param;
            			startTick = req.param;
            			ctrl->getPlaybackPos() = pos;
            			blockPos = tickToBlock(req.param, bpm100, sampleRate, blockSize);
            			LOG("START ON seconds: %.2f - BLOCK %d\n", toSeconds(pos, bpm100), blockPos);
            			host->onStartPlayback(blockPos);
                        QueryPerformanceCounter(&iStart);
                        wallTime = 0;
                        firstBlock = true;
        			}
        			break;
        		case PLAYBACK_STOP:
        			if (state != status_stop) {
            			state = m_status = status_stop;
            			host->onStopPlayback();
        			}
        			break;
        		case PLAYBACK_THREAD_EXIT:
#ifndef NDEBUG
    				LOG("PLAYBACK_THREAD_EXIT");
    				Sleep(200);
#endif
        			return;
        		}
        	}


            bool inLoop = pos >= ctrl->loopStart && pos < ctrl->loopStart+ctrl->loopLen
            		&& state == status_play && ctrl->loopEnabled;
            int32_t processedBlock;
            {
            	ThreadLock lock = this->lockThread();
            	processedBlock = host->processPlayback(blockPos, pos, state, inLoop);
            }
            /*
             * at sample rate 44100 and blocksize 512 the block duration is 1.xxms
             * the producer side trys to stay 4 blocks ahead of the consumer (audio thread)
             * We can expect processPlayback to only process one block under normal load
             */
			if (processedBlock > 1) {
//				LOG("processedBlock > 1: %d\n", processedBlock);
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
			blockPos+=processedBlock;

			if (processedBlock) {
				tick_t tickCur = blockToTick(blockPos, bpm100, sampleRate, blockSize);
				if (inLoop) {
					if (tickCur >= ctrl->loopStart+ctrl->loopLen) {
	        			double d1 = tickToBlockPrecise(tickCur, bpm100, sampleRate, blockSize);
	        			double d2 = tickToBlockPrecise(ctrl->loopStart, bpm100, sampleRate, blockSize);
	        			LOG("JMP FROM %.2f (%d) to %.2f\n", d1, blockPos, d2);
	        			blockPos = tickToBlock(ctrl->loopStart, bpm100, sampleRate, blockSize);
	        			LOG("JMP LOOPBEGIN seconds: %.2f - BLOCK %d\n", toSeconds(blockPos, bpm100), blockPos);
						ctrl->setJumpFromTo(tickCur, blockToTick(blockPos, bpm100, sampleRate, blockSize));
					}
				}

			}
			double blocksPerS = sampleRate / (double) blockSize;
			double msPerBlock = 1000.0 / blocksPerS;
			if (processedBlock) {
				tick_t tickCur = blockToTick(blockPos, bpm100, sampleRate, blockSize);

				pos = tickCur;
	        	if (state == status_play) {
        			ctrl->getPlaybackPos() = pos;
	        	}
				approxTimePassed += msPerBlock;
				QueryPerformanceCounter(&iStop);
				wallTime = QPC_TOSECONDS(iStart, iStop, freq);
			}
			if (approxTimePassed > 60000 && state == status_play) {
				approxTimePassed = 0;
	        	double seconds = (blockPos * blockSize) / (double)sampleRate;
	            LOG("pos %.4f wallTime %.4f error %.4f (msPerBlock %.4f)\n", seconds*1000.0, wallTime, (seconds*1000.0)-wallTime, msPerBlock);
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

void PlaybackThread::addRequest(PlaybackRequest r) {
	_M_impl->addRequest(r);
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
//bool PlaybackThread::pushTask(ThreadTask* task) {
//	return this->_M_impl->push(task->_M_impl);
//}






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


//ThreadLock& ThreadLock::operator=(ThreadLock&& other);
//{
//     return *this;
//}

#pragma GCC diagnostic pop
