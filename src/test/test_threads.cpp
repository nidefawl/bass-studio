#include "tests.h"
#include "../threads/workerthread.h"
#ifdef __MINGW32__
#undef _GLIBCXX_HAS_GTHREADS
#include "../threads/mingw.thread.h"
#include <mutex>
#include "../threads/mingw.mutex.h"
#include "../threads/mingw.condition_variable.h"
#else
#include <mutex>
#endif
#include <vector>
#include <stdint.h>

namespace {
static test_rng rng;

class TestTask : public WorkerThread::ThreadTask {
public:
	TestTask() : WorkerThread::ThreadTask() {
	}
	uint32_t id = 0;
	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t result = 0;
	void run() {
		for (uint32_t i = 0; i < 500000; i++) {
			result = ((result*a) >> 1) + rng.randI();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
		//        LOG("work on TestTask %d", result);
		if (id == 3)
			throw std::runtime_error("little error hihi");
	}
};
static void test() {
	std::vector<TestTask> tasks(10);
	int i = 0;
	for (TestTask& task : tasks) {
		task.id = i;
		task.a = i << 2;
		task.b = 10;
		i++;
	}

	WorkerThread wthread;
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
	LOG("post wthread.stop");
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
				}
				catch (const std::exception &ex)
				{
					printf("task[%d] had exception: %s\n", task.id, ex.what());
				}
			}
		}
		else if (task.isGood()) {
			printf("task[%d] isGood: %d\n", task.id, task.result);
		}
		else {
			printf("task[%d] was not processed!\n", task.id);
		}
	}
	auto end = hp_clock::now();
	std::chrono::duration<float, std::milli> duration = end - start;
	printf("%.2f\n", duration.count());

	wthread.stopThread();
	wthread.joinThread();
	LOG("end");
}

}
void testThreads() {
	for (int i = 0; i < 20; i++) {
		test();
		std::this_thread::sleep_for(std::chrono::milliseconds{ 120 });
	}
}
