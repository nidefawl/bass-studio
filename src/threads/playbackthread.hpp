#pragma once
#include "thread.hpp"
#include "threadlock.hpp"
#include "seq_time.hpp"
#include <memory>
#include <stdexcept>
#include <functional>

class project_controller_t;
class PlaybackThread final : public seqthreads::thread_base {
public:
    enum RequestType {
        REQ_PLAYBACK_STATE = 1,
        REQ_INVOKE_FN = 2,
        REQ_PLAYBACK_THREAD_EXIT = 3
    };
private:
    class Impl;
public:
    PlaybackThread();
    ~PlaybackThread() override;
    void startThread(project_controller_t* ctrl);
    void stopThread();
    void joinThread();
    ThreadLock lockThread();
    ThreadLock tryLockThread();
    void addRequest(RequestType req, int32_t param, bool wait);
    void addRequestWithCallback(RequestType req, int32_t param, std::function<void()> fn, bool wait);
    void call(std::function<void()> fn, bool wait);
    playback_state getState();
    int32_t getThreadId() override;
    void setTls(daw_tls::tlsinstance tls) override;

    // just for debug asserts, not synchronization control, since this has race conditions
    bool isLockedOrNotProcessing();
    bool isRunning();
private:
    Impl* _M_impl;
};
