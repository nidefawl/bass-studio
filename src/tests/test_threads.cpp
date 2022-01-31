#include "TestBase.hpp"
#include <vector>
#include <stdint.h>
#include <chrono>
#include "tests/common/test_common.h"
#include "thread.h"
#include "threads/workerthread.h"

using hp_clock = std::chrono::high_resolution_clock;


namespace {
    static test_rng rng;

    class TestTask : public WorkerThread::ThreadTask {
    public:
        TestTask() : WorkerThread::ThreadTask() {
        }
        uint32_t id     = 0;
        uint32_t a      = 0;
        uint32_t b      = 0;
        uint32_t result = 0;
        void run() override {
            for (uint32_t i = 0; i < 500000; i++) {
                result = ((result * a) >> 1) + rng.randI();
            }
            seqthreads::threadSleep(20);
            //        LOG("work on TestTask %d", result);
            if (id == 3)
                throw std::runtime_error("little error hihi");
        }
    };
    static void test() {
        std::vector<TestTask> tasks(10);
        uint32_t i = 0;
        for (TestTask& task : tasks) {
            task.id = i;
            task.a  = i << 2;
            task.b  = 10;
            i++;
        }
        daw_tls::tlsinstance tls;
        tls.tlsInitialized = true;
        WorkerThread wthread;
        wthread.setTls(tls);
        wthread.startThread();
        for (TestTask& task : tasks) {
            if (task.id == 2) {
                continue;
            }
            wthread.pushTask(&task);
        }
        wthread.stopThread();
        for (TestTask& task : tasks) {
            if (task.id != 2) {
                continue;
            }
            wthread.pushTask(&task);
        }
        printf("post wthread.stop\n");
        auto start = hp_clock::now();
        for (TestTask& task : tasks) {
            if (task.isInQueue()) {
                task.wait();
            }
            if (task.isError()) {
                printf("task[%d] iserror: %d\n", task.id, task.result);
                std::exception_ptr eptr = task.getException();
                if (eptr != nullptr) {
                    printf("task[%d] had exception.. rethrowing\n", task.id);
                    try {
                        std::rethrow_exception(eptr);
                    } catch (const std::exception& ex) {
                        printf("task[%d] had exception: %s\n", task.id, ex.what());
                    }
                }
            } else if (task.isGood()) {
                printf("task[%d] isGood: %d\n", task.id, task.result);
            } else {
                printf("task[%d] was not processed!\n", task.id);
            }
        }
        auto end = hp_clock::now();

        std::chrono::duration<float, std::milli> duration = end - start;
        printf("%.2f\n", duration.count());

        wthread.stopThread();
        wthread.joinThread();
        printf("end\n");
    }

}// namespace

int main() {
    TEST_BEGIN("testThreadWorkerTasks");
    for (int i = 0; i < 10; i++) {
        test();
        seqthreads::threadSleep(40);
    }
    TEST_END();
    return 0;
}
