#include "playbackthread.hpp"
#include "types.hpp"
#include <atomic>
#include <cstddef>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "assert_dbg.h"

#include "error.hpp"
#include "math/seq_math.hpp"
#include "str_util.hpp"
#include "thread.hpp"
#include "threadlock.hpp"
#include "seq_time.hpp"
#include "hires_timer.hpp"
#include <readerwriterqueue/readerwriterqueue.hpp>
#include "host/project/projectcontroller.hpp"
#include "host/host.hpp"
#include "host/midihost/midi_host.hpp"
#include "logging.hpp"
#include "sse.hpp"
#include "appconfig.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace moodycamel;

class PlaybackThreadReq {
    std::mutex m_mtx;
    std::condition_variable m_cond;
    std::atomic<bool> m_finished{ false };

public:
    int32_t msgId = 0;
    int32_t param = 0;
    std::function<void()> fn;
    PlaybackThreadReq(int32_t _msgId, int32_t _param, std::function<void()>&& _fn)
        : msgId(_msgId), param(_param), fn(std::move(_fn)) {
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
            seqthreads::registerThread("audiothread", seqthreads::ThreadType::AudioThread);
            setSSEFlushDenormals();
            this->m_threadId = seqthreads::getCurrentThreadId();
            dbgassert(m_threadTls.tlsInitialized);
            daw_tls::setTls(m_threadTls);
#ifdef _WIN32
            HANDLE h = reinterpret_cast<HANDLE>(GetCurrentThread());
            SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
#endif
#ifdef __linux__
            set_thread_priority_realtime();
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
            auto req = std::make_shared<PlaybackThreadReq>(PlaybackThread::REQ_PLAYBACK_THREAD_EXIT, 0);
            if (!m_q.enqueue(req)) {
                dbgassert(0 && "Failed enqeueing req");
            }
            req->wait();
        }
    }
    playback_state getState() const {
        return m_status;
    }
    bool isLockedOrNotProcessing() {
        return this->m_lockCount > 0 || this->m_status == status_no_process;
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
        auto* const host = m_threadTls.host;
        midihost* const midiHost = m_threadTls.midiHost;
        const project_globals_t& projGlobals = host->prjGlobals;

        std::function<void()> renderCompleteFn = nullptr;
        std::shared_ptr<PlaybackThreadReq> req;

        export_settings_t exportSettingsLocal{};

        samplecount_t samplePos = 0;
        double tickPos    = 0;
        int32_t numBlocksRendered = 0;

        hires_timer_t timer2;
        try {
            while (true) {
                if (m_q.try_dequeue(req)) {
                    switch (RequestType(req->msgId)) {
                        case PlaybackThread::REQ_PLAYBACK_STATE: {
                            auto reqState = static_cast<playback_state>(req->param);
                            if (m_status == playback_state::status_render && reqState != playback_state::status_render) {
                                m_status = status_no_process;
                                ctrl->getGlobals().recordArmed = false;
                                host->onStopPlayback(this->m_prjCtrl);
                                host->postExportEnd(ctrl, exportSettingsLocal, true);
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
                                    samplePos              = tickToSampleConvert<samplecount_t, roundmode::floor>(startPos, bpm100, host->m_sampleFormatInternal.sampleRate);
                                    log_printf("START EXPORT ON seconds: %.2f - sample %zd\n", toSeconds(startPos, bpm100), samplePos);

                                    ctrl->getGlobals().recordArmed = false;
                                    host->preExportBegin(ctrl, exportSettingsLocal);
                                    host->onStartPlayback(this->m_prjCtrl);
                                    timer2.reset();
                                    break;
                                }
                                case playback_state::status_playback: {
                                    dbgassert(host->m_sampleFormatInternal.sampleRate != 0);
                                    dbgassert(host->m_sampleFormatInternal.blockSize != 0);
                                    if (req->fn) {
                                        req->fn();
                                    }
                                    // copy start pos to this thread
                                    tick_t startPos        = ctrl->getCursorPos();
                                    int32_t bpm100         = ctrl->getCurrentTempo();
                                    tickPos                = startPos;
                                    ctrl->getPlaybackPos() = startPos;
                                    samplePos              = tickToSampleConvert<samplecount_t, roundmode::floor>(startPos, bpm100, host->m_sampleFormatInternal.sampleRate);
                                    host->onStartPlayback(this->m_prjCtrl);
                                    timer2.reset();
                                    break;
                                }
                                case playback_state::status_stop: {
                                    ctrl->getGlobals().recordArmed = false;
                                    host->onStopPlayback(this->m_prjCtrl);
                                    break;
                                }
                                case playback_state::status_no_process: {
                                    break;
                                }
                            }
                            host->getHostCallback()->m_playbackState = m_status = reqState;
                        } break;
                        case PlaybackThread::REQ_INVOKE_FN:
                            req->fn();
                            break;
                        case PlaybackThread::REQ_PLAYBACK_THREAD_EXIT:
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


                int32_t numBlocksProcessed = 0;
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


                    const samplerate_t sampleRate = host->m_sampleFormatInternal.sampleRate;
                    const blocksize_t  blockSize  = host->m_sampleFormatInternal.blockSize;

                    const int32_t bpm100 = projGlobals.tempo100;


                    const auto props = host->updateAudioStreamProperties();

                    bool inLoop = m_status == status_playback && projGlobals.loopEnabled && (tickPos >= projGlobals.loopStart) && (tickPos < projGlobals.loopStart + projGlobals.loopLen);


                    if (m_status != playback_state::status_render) {
                        midiHost->processMidiInput(this->m_prjCtrl, samplePos, tickPos, m_status, inLoop);
                        if (!host->bypassPlaybackProcessing) {
                            numBlocksProcessed = host->processPlayback(this->m_prjCtrl, samplePos, tickPos, m_status, inLoop);
                            timer2.reset();
                        } else {
                            numBlocksProcessed = 0;
                            double d = timer2.getTimeDouble();
                            if (d > 1.0 * blockSize / sampleRate) {
                                numBlocksProcessed = 1;
                                timer2.reset();
                            }
                        }
                        midiHost->processMidiOutput();
                    }

                    if (m_status == playback_state::status_render) {
                        numBlocksProcessed = host->processRender(this->m_prjCtrl, samplePos, tickPos);
                        // log_lf(Log::L_DEBUG, "processedBlocks: %d, tickpos: %s\n", numBlocksProcessed, tickAsBeatString(tickPos, false).c_str());
                    }

                    if (numBlocksProcessed) {
                        samplePos += samplecount_t(blockSize) * numBlocksProcessed;
                        tickPos += props.ticksPerBlock * numBlocksProcessed;
                        /* auto tickPosFromSample = sampleToTickConvert<double, roundmode::none>(samplePos, bpm100, sampleRate);
                        auto errorTicks = tickPos - tickPosFromSample;
                        static const double maxError = 0.0001;
                        if (errorTicks > maxError) {
                            log_printf("ERROR: tickPos: %.6f, tickPosFromSample: %.6f, errorTicks: %.6f\n", tickPos, tickPosFromSample, errorTicks);
                        } */
                        if (m_status == status_playback) {
                            if (inLoop && tickPos >= projGlobals.loopStart + projGlobals.loopLen) {
                                // if (m_threadTls.dawInstance) {
                                    // m_threadTls.dawInstance->setJumpFromTo(tickPos, projGlobals.loopStart);
                                // }
                                double nextTickPos    = projGlobals.loopStart;
                                int32_t nextSamplePos = tickToSampleConvert<int32_t, roundmode::floor>(nextTickPos, bpm100, sampleRate);
                                host->onPlaybackJumpFromTo(this->m_prjCtrl, samplePos, tickPos, nextSamplePos, nextTickPos);
                                // log_lf(Log::L_DEBUG, "JMP FROM %s to %s\n", StringAsCStr(tickAsBeatString(tickPos, false)), StringAsCStr(tickAsBeatString(nextTickPos, false)));
                                tickPos   = nextTickPos;
                                samplePos = nextSamplePos;
                                // log_lf(Log::L_DEBUG, "JMP LOOPBEGIN seconds: %.2f - BLOCK %d\n", toSeconds(projGlobals.loopStart, bpm100), samplePos / blockSize);
                            }
                        }
                        if (m_status != status_stop) {
                            ctrl->getPlaybackPos() = math::rounddS32(tickPos);
                        }
                        ctrl->getIdleTickPos() = math::rounddS32(tickPos);
                        if (m_status == status_render) {
                            auto outputLatency = host->getLatency();
                            auto ticksLatency = tick_t(0);
                            if (outputLatency > 0) {
                                ticksLatency = sampleToTickConvert<tick_t, roundmode::floor>(outputLatency, bpm100, sampleRate);
                            }
                            if (tickPos >= exportSettingsLocal.exportPos + exportSettingsLocal.exportLen + ticksLatency) {
                                host->getHostCallback()->m_playbackState = m_status = status_no_process;
                                ctrl->getGlobals().recordArmed = false;
                                host->onStopPlayback(this->m_prjCtrl);
                                host->postExportEnd(ctrl, exportSettingsLocal, false);
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
                    if (numBlocksProcessed) {
                        seqthreads::threadSleepMicros(500);
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
    auto r = std::make_shared<PlaybackThreadReq>(PlaybackThread::REQ_INVOKE_FN, 0, std::move(fn));
    bool s = _M_impl->addRequest(r);
    if (s && wait) {
        r->wait();
    }
}
void PlaybackThread::addRequest(RequestType req, int32_t param, bool wait) {
    auto r = std::make_shared<PlaybackThreadReq>(req, param);
    bool s = _M_impl->addRequest(r);
    if (s && wait) {
        r->wait();
    }
}
void PlaybackThread::addRequestWithCallback(RequestType req, int32_t param, std::function<void()> fn, bool wait) {
    auto r = std::make_shared<PlaybackThreadReq>(req, param, std::move(fn));
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

bool PlaybackThread::isLockedOrNotProcessing() {
    return _M_impl->isLockedOrNotProcessing();
}

bool PlaybackThread::isRunning() {
    return _M_impl->isRunning();
}
