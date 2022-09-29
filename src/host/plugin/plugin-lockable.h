
#include "host/mainctrl.h"

class PluginLockable {
        DawInstance* const daw;
        std::recursive_mutex m_mutex;
        std::atomic<int32_t> m_lockCount{ 0 };
    public:
        explicit PluginLockable(DawInstance* daw) 
            : daw(daw) {
        }
        ThreadLock lock() {
            if (daw)
                return daw->lockPlayThread();
            return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, false);
        }
        ThreadLock lockProcessing() {
            if (daw)
                return ThreadLock::MakeVoidLock();
            return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, false);
        }
        ThreadLock tryLock() {
            if (daw)
                return daw->getPlayThread()->tryLockThread();
            return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, true);
        }
        virtual ~PluginLockable() = default;
    };