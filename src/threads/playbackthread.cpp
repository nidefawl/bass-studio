#include "playbackthread.h"
#include "types.h"
#include <atomic>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "assert_dbg.h"

#include "error.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "thread.h"
#include "threadlock.h"
#include "seq_time.h"
#include "hires_timer.h"
#include <readerwriterqueue/readerwriterqueue.hpp>
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "host/midi_host.h"
#include "logging.h"
#include "sse.h"
#include "appconfig.h"

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

#define PLAYBACK_THREAD_EXIT 255


class PlaybackThreadReq {
    std::mutex m_mtx;
    std::condition_variable m_cond;
    std::atomic<bool> m_finished{ false };

public:
    int32_t msgId = 0;
    int32_t param = 0;
    std::function<void()> fn;
    PlaybackThreadReq(int32_t _msgId, int32_t _param, std::function<void()>&& _fn)
        : msgId(_msgId), param(_param), fn(_fn) {
    }
    PlaybackThreadReq(int32_t _msgId, int32_t _param)
        : msgId(_msgId), param(_param) {
    }
    ~PlaybackThreadReq() = default;
    void wait() {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (!m_finished) {
            m_cond.wait(lock, [&]() { return m_finished == true; });
        }
    }
    void notify() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_finished = true;
        m_cond.notify_all();
    }
};

class PlaybackThread::Impl {
    std::thread m_t;
    ReaderWriterQueue<std::shared_ptr<PlaybackThreadReq>> m_q;
    //TODO: use atomic for m_status
    playback_state m_status = status_no_process;
    std::recursive_mutex m_mutex;
    std::atomic<int32_t> m_lockCount{ 0 };
    int32_t m_threadId              = 0;
    project_controller_t* m_prjCtrl = nullptr;
    bool m_exited                   = false;
    daw_tls::tlsinstance m_threadTls;

public:
    Impl() : m_q(128) {
    }
    ~Impl() {
        // thread has not been started or thread has been started and exited correctly
        dbgassert(this->m_prjCtrl == nullptr || (m_exited && !m_t.joinable()));
    }
    int32_t getThreadId() {
        return m_threadId;
    }
    void setTls(daw_tls::tlsinstance tls) {
        dbgassert(!m_t.joinable());
        m_threadTls = tls;
    }
    void start(project_controller_t* projCtrl) {
        this->m_prjCtrl = projCtrl;

        m_t = std::thread([this]() {
            seqthreads::registerThread("audiothread");
            setSSEFlushDenormals();
            this->m_threadId = seqthreads::getCurrentThreadId();
            dbgassert(m_threadTls.tlsInitialized);
            daw_tls::setTls(m_threadTls);
#ifdef _WIN32
            HANDLE h = reinterpret_cast<HANDLE*>(GetCurrentThread());
            SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
#endif
            this->run();
        });
    }
    void join() {
        dbgassert(m_t.joinable());
        m_t.join();
    }
    bool addRequest(std::shared_ptr<PlaybackThreadReq>& req) {
        if (!m_exited) {
            dbgassert(m_t.joinable());
            if (m_q.enqueue(req)) {
                return true;
            }
            dbgassert(0 && "Failed enqeueing req");
        }
        return false;
    }

    void stop() {
        dbgassert(m_t.joinable());
        if (!m_exited) {
            auto req = std::make_shared<PlaybackThreadReq>(PLAYBACK_THREAD_EXIT, 0);
            if (!m_q.enqueue(req)) {
                dbgassert(0 && "Failed enqeueing req");
            }
            req->wait();
        }
    }
    playback_state getState() const {
        return m_status;
    }
    bool isLocked() {
        return this->m_lockCount > 0;
    }
    bool isRunning() {
        return this->m_threadId > 0 && !m_exited;
    }
    ThreadLock lockThread() {
        return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, false);
    }
    ThreadLock tryLockThread() {
        return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, true);
    }

private:
    void run() {
        project_controller_t* const ctrl = this->m_prjCtrl;
        vsthost* host        = m_threadTls.host;
        midihost* midiHost   = m_threadTls.midiHost;
        std::function<void()> renderCompleteFn = nullptr;
        export_settings_t exportSettingsLocal{};
        double playbackDuration = 0;
        hires_timer_t timer;
        hires_timer_t timer2;

        std::shared_ptr<PlaybackThreadReq> req;
        int32_t samplePos = 0;
        double tickPos    = 0;
        int32_t numBlocksRendered = 0;
        try {
            while (true) {
                if (m_q.try_dequeue(req)) {
                    switch (req->msgId) {
                        case REQ_STATE: {
                            auto reqState = static_cast<playback_state>(req->param);
                            if (m_status == playback_state::status_render && reqState != playback_state::status_render) {
                                log_lf(Log::L_WARN, "status_render cancelled\n");
                                host->postExportEnd(ctrl, exportSettingsLocal);
                                if (renderCompleteFn) {
                                    renderCompleteFn();
                                    renderCompleteFn = nullptr;
                                }
                            }
                            switch (reqState) {
                                case playback_state::status_render: {
                                    numBlocksRendered = 0;
                                    dbgassert(host->m_sampleFormatInternal.sampleRate != 0);
                                    dbgassert(host->m_sampleFormatInternal.blockSize != 0);
                                    renderCompleteFn = std::move(req->fn);
                                    // copy export settings to this thread
                                    exportSettingsLocal    = ctrl->getExportSettings();
                                    tick_t startPos        = exportSettingsLocal.exportPos;
                                    int32_t bpm100         = ctrl->getCurrentTempo();
                                    tickPos                = startPos;
                                    ctrl->getPlaybackPos() = startPos;
                                    samplePos              = tickToSampleConvert<int32_t, roundmode::floor>(startPos, bpm100, host->m_sampleFormatInternal.sampleRate);
                                    log_printf("START EXPORT ON seconds: %.2f - sample %d\n", toSeconds(startPos, bpm100), samplePos);

                                    host->preExportBegin(ctrl, exportSettingsLocal);
                                    host->onStartPlayback(this->m_prjCtrl);
                                    timer.reset();
                                    timer2.reset();
                                    playbackDuration = 0;
                                    break;
                                }
                                case playback_state::status_playback: {
                                    dbgassert(host->m_sampleFormatInternal.sampleRate != 0);
                                    dbgassert(host->m_sampleFormatInternal.blockSize != 0);
                                    // copy start pos to this thread
                                    tick_t startPos        = ctrl->getCursorPos();
                                    int32_t bpm100         = ctrl->getCurrentTempo();
                                    tickPos                = startPos;
                                    ctrl->getPlaybackPos() = startPos;
                                    samplePos              = tickToSampleConvert<int32_t, roundmode::floor>(startPos, bpm100, host->m_sampleFormatInternal.sampleRate);
                                    log_printf("START ON %s seconds: %.2f - sample %d\n", StringAsCStr(tickAsBeatString(startPos)), toSeconds(startPos, bpm100), samplePos);
                                    host->onStartPlayback(this->m_prjCtrl);
                                    timer.reset();
                                    timer2.reset();
                                    playbackDuration = 0;
                                    break;
                                }
                                case playback_state::status_stop: {
                                    host->onStopPlayback(this->m_prjCtrl);
                                    break;
                                }
                                case playback_state::status_no_process: {
                                    break;
                                }
                            }
                            m_status = reqState;
                        } break;
                        case GUI_CALL:
                            req->fn();
                            break;
                        case PLAYBACK_THREAD_EXIT:
#ifndef NDEBUG
                            log_printf("PLAYBACK_THREAD_EXIT\n");
                            seqthreads::threadSleep(200);
#endif
                            req->notify();
                            m_exited = true;
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

                if (m_status != playback_state::status_no_process) {
                    // aquire lock so data does not get modified during processing
                    std::unique_lock<std::recursive_mutex> lock(m_mutex);

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

                    const samplerate_t sampleRate = host->m_sampleFormatInternal.sampleRate;
                    const int32_t blockSize       = host->m_sampleFormatInternal.blockSize;

                    const int32_t bpm100 = projGlobals.tempo100;


                    const auto props = host->getAudioStreamProperties();


                    const bool isLoopAround = tickPos + props.ticksPerBlock >= projGlobals.loopStart + projGlobals.loopLen;

                    int32_t numBlocksProcessed = 0;

                    bool inLoop = m_status == status_playback && projGlobals.loopEnabled && (tickPos >= projGlobals.loopStart) && (tickPos < projGlobals.loopStart + projGlobals.loopLen);


                    if (m_status != playback_state::status_render) {
                        midiHost->processMidi(this->m_prjCtrl, samplePos, tickPos, m_status, inLoop, isLoopAround);
                        if (!host->bypassPlaybackProcessing) {
                            numBlocksProcessed = host->processPlayback(this->m_prjCtrl, samplePos, tickPos, m_status, inLoop, isLoopAround);
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
                        numBlocksProcessed = host->processRender(this->m_prjCtrl, samplePos, tickPos);
                    }
                    //LOG("processedBlocks: %d, play: %d, tickpos: %f\n", processedBlock, (m_status==playback_state::status_play), tickPos);

                    if (numBlocksProcessed) {
                        samplePos += blockSize * numBlocksProcessed;
                        tickPos += props.ticksPerBlock * numBlocksProcessed;
                        if (m_status == status_playback) {
                            if (inLoop && tickPos >= projGlobals.loopStart + projGlobals.loopLen) {
                                // if (DawInstance::get()) {
                                //     DawInstance::get()->setJumpFromTo(tickPos, projGlobals.loopStart);
                                // }
                                double nextTickPos    = projGlobals.loopStart;
                                int32_t nextSamplePos = tickToSampleConvert<int32_t, roundmode::floor>(nextTickPos, bpm100, sampleRate);
                                host->onPlaybackJumpFromTo(this->m_prjCtrl, samplePos, tickPos, nextSamplePos, nextTickPos);
                                log_lf(Log::L_DEBUG, "JMP FROM %s to %s\n", StringAsCStr(tickAsBeatString(tickPos)), StringAsCStr(tickAsBeatString(nextTickPos)));
                                tickPos   = nextTickPos;
                                samplePos = nextSamplePos;
                                log_lf(Log::L_DEBUG, "JMP LOOPBEGIN seconds: %.2f - BLOCK %d\n", toSeconds(projGlobals.loopStart, bpm100), samplePos / blockSize);
                            }
                        }
                        if (m_status != status_stop) {
                            ctrl->getPlaybackPos() = math::rounddS32(tickPos);
                            playbackDuration += props.microSecsPerBlock * 0.001 * numBlocksProcessed;
                        }
                        if (m_status == status_render) {
                            if (tickPos >= exportSettingsLocal.exportPos + exportSettingsLocal.exportLen) {
                                m_status = status_no_process;
                                host->postExportEnd(ctrl, exportSettingsLocal);
                                if (renderCompleteFn) {
                                    renderCompleteFn();
                                    renderCompleteFn = nullptr;
                                }
                            }
                            numBlocksRendered += numBlocksProcessed;
                        }
                    }
                }

                if (m_status != playback_state::status_render) {
                    host_stats_t stats;
                    host->getStats(stats);
                    if (!host->bypassPlaybackProcessing && stats.outputQueueLen < 2) {
                        if (stats.outputQueueLen != 0) {
                            seqthreads::threadSleepMicros(500);
                        }
                    } else {

                        seqthreads::threadSleep(1);
                    }
                } else {
                    if (numBlocksRendered > 100) {
                        numBlocksRendered = 0;
                        seqthreads::threadSleep(10);
                    }
                }
                if (m_status == playback_state::status_no_process) {
                    seqthreads::threadSleep(100);
                }


                if (playbackDuration > 10000 && m_status == status_playback) {
                    double wallTimeMs = timer.getTimeDouble() * 1000.0;
                    log_lf(Log::L_DEBUG, "playbackDuration %.4f wallTime %.4f error %.4f\n", playbackDuration, wallTimeMs, playbackDuration - wallTimeMs);
                    playbackDuration = 0;
                    timer.reset();
                }
            }
        } catch (std::exception& e) {
            handleStdException(e);
        }
        m_exited = true;
    }
};
PlaybackThread::~PlaybackThread() {
    delete _M_impl;
}

PlaybackThread::PlaybackThread() : _M_impl{ new PlaybackThread::Impl{} } {
}

void PlaybackThread::call(std::function<void()> fn, bool wait) {
    auto r = std::make_shared<PlaybackThreadReq>(GUI_CALL, 0, std::move(fn));
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
void PlaybackThread::addRequestWithCallback(int32_t _msgId, int32_t _param, std::function<void()> fn, bool wait) {
    auto r = std::make_shared<PlaybackThreadReq>(_msgId, _param, std::move(fn));
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
    daw_tls::getTls().runtime->renderStats.playThreadLockCount++;
    return _M_impl->lockThread();
}

ThreadLock PlaybackThread::tryLockThread() {
    ThreadLock t = _M_impl->tryLockThread();
    if (t.isLocked()) {
        daw_tls::getTls().runtime->renderStats.playThreadLockCount++;
    }
    return t;
}

bool PlaybackThread::isLocked() {
    return _M_impl->isLocked();
}

bool PlaybackThread::isRunning() {
    return _M_impl->isLocked();
}

ThreadLock& ThreadLock::operator=(ThreadLock&& other) noexcept {
    this->_M_impl = other._M_impl;
    other._M_impl = nullptr;
    return *this;
}
ThreadLock::ThreadLock(ThreadLock&& other) noexcept {
    this->_M_impl = other._M_impl;
    other._M_impl = nullptr;
}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
