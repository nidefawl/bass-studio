#include "TestBase.hpp"
#include <vector>
#include "types.hpp"
#include "common/test_common.hpp"
#include "thread.hpp"
#include "threads/workerthread.hpp"

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
            seqthreads::threadSleep(10);
            if (id == 3)
                throw std::runtime_error("TestTask exception. This is expected.");
        }
    };
    static void test_workerthread_tasks() {
        TEST_BEGIN("test_workerthread_tasks");
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
        wthread.startThread("WorkerThread", seqthreads::ThreadType::WorkerThread);
        for (TestTask& task : tasks) {
            TEST_ASSERT_EQUAL(task.hasException(), false);
            TEST_ASSERT_EQUAL(task.isCompleted(), false);
            TEST_ASSERT_EQUAL(task.isCompletedNoLock(), false);
            TEST_ASSERT_THROW(wthread.pushTask(&task));
        }
        for (TestTask& task : tasks) {
            task.wait();
            TEST_ASSERT_EQUAL(task.hasException(), task.id == 3);
            if (task.hasException()) {
                task.logException();
            }
        }
        for (TestTask& task : tasks) {
            task.reset();
            TEST_ASSERT_EQUAL(task.hasException(), false);
            TEST_ASSERT_EQUAL(task.isCompleted(), false);
            TEST_ASSERT_EQUAL(task.isCompletedNoLock(), false);
            TEST_ASSERT_THROW(wthread.pushTask(&task));
        }
        for (TestTask& task : tasks) {
            task.wait();
            TEST_ASSERT_EQUAL(task.hasException(), task.id == 3);
            if (task.hasException()) {
                task.logException();
            }
        }
        wthread.stopThread();
        wthread.joinThread();
        wthread.startThread("WorkerThread", seqthreads::ThreadType::WorkerThread);
        for (TestTask& task : tasks) {
            task.reset();
            TEST_ASSERT_EQUAL(task.hasException(), false);
            TEST_ASSERT_EQUAL(task.isCompleted(), false);
            TEST_ASSERT_EQUAL(task.isCompletedNoLock(), false);
            TEST_ASSERT_THROW(wthread.pushTask(&task));
        }
        for (TestTask& task : tasks) {
            task.wait();
            TEST_ASSERT_EQUAL(task.hasException(), task.id == 3);
            if (task.hasException()) {
                task.logException();
            }
        }
        wthread.stopThread();
        wthread.joinThread();
        TEST_END();
    }
}// namespace

int main() {
    for (int i = 0; i < 10; i++) {
        test_workerthread_tasks();
        seqthreads::threadSleep(20);
    }
    return 0;
}
