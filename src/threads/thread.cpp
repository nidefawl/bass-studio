#include <thread>
#include <mutex>
#include <stdint.h>
#include <unordered_map>
//#include "thread.h"
#include "str_util.h"

namespace {

struct threadnames_t {
	std::mutex gThreadMutex;
	std::unordered_map<int32_t, String> gThreadNames;
	threadnames_t() {
		std::lock_guard<std::mutex> lock(gThreadMutex);
	}
	void setThreadName(int32_t threadId, const String& str) {
		std::lock_guard<std::mutex> lock(gThreadMutex);
		gThreadNames[threadId] = str;
	}
	String getThreadName(int32_t threadId) {
		std::lock_guard<std::mutex> lock(gThreadMutex);
		auto it = gThreadNames.find(threadId);
		if (it == gThreadNames.end()) {
            return StringFormat("thread-%X", static_cast<int32_t>(threadId));
		}
        return it->second + StringFormat("-%X", static_cast<int32_t>(threadId));
	}
};

threadnames_t& getThreadNames() {
	static threadnames_t threadnames;
	return threadnames;
}

}

namespace seqthreads {

//TODO: implement this using TLS
int32_t get_thread_id() noexcept {
    static int32_t thread_idx = 0;
    static std::mutex thread_mutex;
    static std::unordered_map<std::thread::id, int32_t> thread_ids;

    std::lock_guard<std::mutex> lock(thread_mutex);
    std::thread::id id = std::this_thread::get_id();
    auto iter = thread_ids.find(id);
    if (iter == thread_ids.end()) {
        iter = thread_ids.insert(std::pair<std::thread::id, int32_t>(id, thread_idx++)).first;
    }
    return iter->second;
}

void setCurrentThreadName(String str) {
	int32_t threadId = get_thread_id();
	getThreadNames().setThreadName(threadId, str);
}

String getCurrentThreadName() {
	return getThreadNames().getThreadName(get_thread_id());
}

void threadSleep(int32_t millis) {
	std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

void threadSleepMicros(int32_t microSeconds) {
	std::this_thread::sleep_for(std::chrono::microseconds(microSeconds));
}

}

