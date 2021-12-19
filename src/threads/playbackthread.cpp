#include "playbackthread.h"
#include <atomic>
#include <queue>
#include <thread>
#include <mutex>
#include "assert_dbg.h"

#include "error.h"
#include "thread.h"
#include "threadlock.h"
#include "seq_time.h"
#include "hires_timer.h"
#include "util/readerwriterqueue.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "host/midi_host.h"
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
	//TODO: use atomic for m_status
    playback_state m_status = status_no_process;
	std::recursive_mutex mutex;
	std::atomic<int32_t> mLockCount{0};
	int32_t threadid = 0;
	project_controller_t* ctrl = nullptr;
	bool exited = false;
	daw_tls::tlsinstance threadTLS;
public:
    Impl() : q(128) {
	}
    ~Impl() {
    	// thread has not been started or thread has been started and exited correctly
    	dbgassert(this->ctrl == nullptr || (exited && !t.joinable()));
    }
    int32_t getThreadId() {
    	return threadid;
    }
    void setTls(daw_tls::tlsinstance tls) {
		dbgassert(!t.joinable());
		threadTLS = tls;
    }
	void start(project_controller_t* ctrl) {
		this->ctrl = ctrl;
        t = std::thread([this]() {
    		seqthreads::registerThread("audiothread");
            this->threadid = seqthreads::getCurrentThreadId();
            dbgassert(threadTLS.tlsInitialized);
			daw_tls::setTls(threadTLS);
#ifdef _WIN32
            HANDLE h = reinterpret_cast<HANDLE*>(t.native_handle());
            SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
#endif
			this->run();
		});
	}
	void join() {
		dbgassert(t.joinable());
		t.join();
	}
	bool addRequest(std::shared_ptr<PlaybackThreadReq>& req) {
        if (!exited) { 
			dbgassert(t.joinable());
            if (q.enqueue(req)) {
                return true;
			}
            dbgassert(0 && "Failed enqeueing req");
        }
        return false;
	}

    void stop(){
		dbgassert(t.joinable());
        if (!exited) {
            auto req = std::make_shared<PlaybackThreadReq>(PLAYBACK_THREAD_EXIT, 0);
            if (!q.enqueue(req)) {
                dbgassert(0 && "Failed enqeueing req");
            }
            req->wait();
        }
    }
    playback_state getState() const {
    	return m_status;
    }
    bool isLocked() {
    	return this->mLockCount > 0;
    }
	ThreadLock lockThread() {
		ThreadLock t = ThreadLock::MakeThreadLock(mutex, this->mLockCount, false);
		return std::move(t); //CANNOT RELY ON RVO
	}
	ThreadLock tryLockThread() {
		ThreadLock t = ThreadLock::MakeThreadLock(mutex, this->mLockCount, true);
		return std::move(t); //CANNOT RELY ON RVO
	}
private:

	void run() {
		project_controller_t* const ctrl = this->ctrl;
		vsthost* host = vsthost::getInstance();
		midihost* midiHost = midihost::getInstance();
		export_settings_t exportSettingsLocal{};
		double playbackDuration = 0;
		hires_timer_t timer;
		hires_timer_t timer2;

		std::shared_ptr<PlaybackThreadReq> req;
		int32_t samplePos = 0;
		double tickPos = 0;

		try {
        while (true){
        	if (q.try_dequeue(req)) {
        		switch (req->msgId) {
        		case REQ_STATE:
					{
						playback_state reqState = (playback_state) req->param;

						if (m_status == playback_state::status_render && reqState != playback_state::status_render) {
							host->postExportEnd(ctrl, exportSettingsLocal);
						}

						switch (reqState) {
							case playback_state::status_render:
							{
								dbgassert(host->sampleFormat.sampleRate != 0);
								dbgassert(host->sampleFormat.blockSize != 0);
								// copy export settings to this thread
								exportSettingsLocal = ctrl->getExportSettings();
								tick_t startPos = exportSettingsLocal.exportPos;
								int32_t bpm100 = ctrl->getCurrentTempo();
								tickPos = startPos;
								ctrl->getPlaybackPos() = startPos;
								samplePos = tickToSample(startPos, bpm100, host->sampleFormat.sampleRate);
								LOG("START EXPORT ON seconds: %.2f - sample %d\n", toSeconds(startPos, bpm100), samplePos);
								host->preExportBegin(ctrl, exportSettingsLocal);
								host->onStartPlayback(this->ctrl);
								timer.reset();
								timer2.reset();
								playbackDuration = 0;
								break;
							}
							case playback_state::status_playback:
							{
								dbgassert(host->sampleFormat.sampleRate != 0);
								dbgassert(host->sampleFormat.blockSize != 0);
								// copy start pos to this thread
								tick_t startPos = ctrl->getCursorPos();
								int32_t bpm100 = ctrl->getCurrentTempo();
								tickPos = startPos;
								ctrl->getPlaybackPos() = startPos;
								samplePos = tickToSample(startPos, bpm100, host->sampleFormat.sampleRate);
								LOG("START ON seconds: %.2f - sample %d\n", toSeconds(startPos, bpm100), samplePos);
								host->onStartPlayback(this->ctrl);
								timer.reset();
								timer2.reset();
								playbackDuration = 0;
								break;
							}
							case playback_state::status_stop:
							{
								host->onStopPlayback(this->ctrl);
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
    				seqthreads::threadSleep(200);
#endif
            		req->notify();
            		exited = true;
        			return;
        		}
        		req->notify();
        		req.reset();
        	}


        	/**
        	 * TODO: for the case m_status == playback_state::status_stop the tickPos
        	 * should be synced to the current cursor pos for automation reading.
        	 * Automation reading should not move forward in time. But normal audio processing
        	 * must move the tickPos as if we are playing.
        	 * This ensure that audio is processing can function while in stop and automation
        	 * is always read from the current editing position on the UI
        	 */

            if (m_status != playback_state::status_no_process)
            {
            	// aquire lock so data does not get modified during processing
				std::unique_lock<std::recursive_mutex> lock(mutex);

            	/**
            	 * Copy project globals from controller to host
            	 *
            	 * Any subsequent reads inside the audio processing threads
            	 * are protected from changes on the UI/controller side.
            	 *
            	 * TODO: Some of the parameters are not guarded by the lock against
            	 * the playback thread. They either have to happen with a lock
            	 * or guarantee to get written atomically and be sanity checked here
            	 * before processing.
            	 * @see project_globals_t in project.h
            	 */
            	host->prjGlobals = ctrl->getGlobals();

            	const project_globals_t& projGlobals = host->prjGlobals;

                const samplerate_t sampleRate = host->sampleFormat.sampleRate;
            	const int32_t blockSize = host->sampleFormat.blockSize;
            	const double blocksPerS = sampleRate / (double) blockSize;
    			const double msPerBlock = 1000.0 / blocksPerS;


            	const int32_t bpm100 = projGlobals.tempo100;
    			const double ticksPerBlock = toTickPrecise(blockSize/(double)sampleRate, bpm100);

    			const bool isLoopAround = tickPos+ticksPerBlock >= projGlobals.loopStart + projGlobals.loopLen;

                int32_t numBlocksProcessed = 0;

                bool inLoop = m_status == status_playback
            			&& projGlobals.loopEnabled
            			&& (tickPos >= projGlobals.loopStart)
            			&& (tickPos < projGlobals.loopStart+projGlobals.loopLen);


            	if (m_status != playback_state::status_render) {
            		midiHost->processMidi(this->ctrl, samplePos, tickPos, m_status, inLoop, isLoopAround);
                	if (!host->bypassPlaybackProcessing) {
                    	numBlocksProcessed = host->processPlayback(this->ctrl, samplePos, tickPos, m_status, inLoop, isLoopAround);
    					timer2.reset();
                	} else {
                		numBlocksProcessed = 0;
                        double d = timer2.getTimeDouble();
						if (d > 1.0 * blockSize / sampleRate) {
                        	numBlocksProcessed = 1;
                        	timer2.reset();
                        }
                	}
            	}

            	if (m_status == playback_state::status_render) {
                	numBlocksProcessed = host->processRender(this->ctrl, samplePos, tickPos);
            	}
//    			LOG("processedBlocks: %d, play: %d, tickpos: %f\n", processedBlock, (m_status==playback_state::status_play), tickPos);

				if (numBlocksProcessed) {
					samplePos += blockSize * numBlocksProcessed;
					tickPos += ticksPerBlock * numBlocksProcessed;
					if (m_status == status_playback) {
						if (inLoop) {
							if (tickPos >= projGlobals.loopStart + projGlobals.loopLen) {
								if (DawInstance::get()) {
									DawInstance::get()->setJumpFromTo(tickPos, projGlobals.loopStart);
								}
								double nextTickPos = projGlobals.loopStart;
								int32_t nextSamplePos = tickToSample(nextTickPos, bpm100, sampleRate);
								host->onPlaybackJumpFromTo(this->ctrl, samplePos, tickPos, nextSamplePos, nextTickPos);
								LOG("JMP FROM %.2f to %.2f\n", tickPos, nextTickPos);
								tickPos = nextTickPos;
								samplePos = nextSamplePos;
								LOG("JMP LOOPBEGIN seconds: %.2f - BLOCK %d\n", toSeconds(projGlobals.loopStart, bpm100), samplePos / blockSize);

							}
						}
						ctrl->getPlaybackPos() = (int32_t) floor(tickPos);
					}
					if (m_status == status_render) {
						if (tickPos >= exportSettingsLocal.exportPos + exportSettingsLocal.exportLen) {
							m_status = status_stop;
							host->postExportEnd(ctrl, exportSettingsLocal);
							ctrl->getPlaybackPos() = exportSettingsLocal.exportPos+exportSettingsLocal.exportLen;
						}
					}
					playbackDuration += msPerBlock*numBlocksProcessed;
				}
            }

            if (m_status != playback_state::status_render)
            {
                host_stats_t stats;
                host->getStats(stats);
                if (!host->bypassPlaybackProcessing && stats.outputQueueLen < 2) {
                	if (stats.outputQueueLen != 0) {
                		seqthreads::threadSleepMicros(500);
                	}
                } else {

                	seqthreads::threadSleep(1);
                }
            }



			if (playbackDuration > 10000 && m_status == status_playback) {
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
        exited = true;
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
    bool s = _M_impl->addRequest(r);
	if (s && wait) {
		r->wait();
	}
}
void PlaybackThread::addRequest(int32_t _msgId, int32_t _param, bool wait) {
	auto r = std::make_shared<PlaybackThreadReq>(_msgId, _param);
    bool s = _M_impl->addRequest(r);
    if (s && wait) {
		r->wait();
	}
}
void PlaybackThread::startThread(project_controller_t* ctrl) {
	_M_impl->start(ctrl);
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
void PlaybackThread::setTls(daw_tls::tlsinstance tls) {
	_M_impl->setTls(tls);
}
playback_state PlaybackThread::getState() {
	return _M_impl->getState();
}
ThreadLock PlaybackThread::lockThread() {
	daw_tls::getTls().renderStats.playThreadLockCount++;
	ThreadLock t = _M_impl->lockThread();
	return std::move(t); //CANNOT RELY ON RVO
}

ThreadLock PlaybackThread::tryLockThread() {
	ThreadLock t = _M_impl->tryLockThread();
	if (t.isLocked()) {
		daw_tls::getTls().renderStats.playThreadLockCount++;
	}
	return std::move(t); //CANNOT RELY ON RVO
}
bool PlaybackThread::isLocked() {
	return _M_impl->isLocked();
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

