#pragma once

#include "str_util.h"
#include <atomic>
namespace DAW {

struct async_task_t {
    enum class state {
        idle,
        running,
        error,
        finished,
        cancelled
    };
    state m_state = state::idle;
    std::atomic<bool> canceled{ false };
    String errorMessage;
    virtual ~async_task_t() = default;
    virtual String getTaskName() const = 0;
    virtual String getProgressDesc() const = 0;
    virtual void getPreciseProgress(double& progressOverall, double& progressDetail) {
        progressOverall = 0;
        progressDetail = 0;
    }
    
    virtual void run() = 0;
    virtual void cancel() {
        canceled = true;
    }
    state getState() const {
        return m_state;
    }
    bool isFinished() const {
        return m_state == state::finished;
    }
    bool isRunning() const {
        return m_state == state::running;
    }
    bool isError() const {
        return m_state == state::error;
    }
    bool isCancelled() const {
        return m_state == state::cancelled;
    }
    bool isIdle() const {
        return m_state == state::idle;
    }
    void setError(String err) {
        errorMessage = std::move(err);
        m_state = state::error;
    }
    String getError() const {
        return errorMessage;
    }
    void setFinished() {
        m_state = state::finished;
    }
    void setRunning() {
        m_state = state::running;
    }
    void setCancelled() {
        m_state = state::cancelled;
    }
};

} // namespace DAW
