#if 0 && defined(__GNUC__) && defined(ENABLE_MICHAELS_GLIBCXX_HACKS) && ENABLE_MICHAELS_GLIBCXX_HACKS == 1
#include <cxxabi.h>
#include <cstddef>
#include <typeinfo>
#include <functional>
#include "assert_dbg.h"
#include <stl-debug/vector-tracker.h>
#include <utility>
#include "str_util.h"
#include "threads.h"
#include "debug_alloc_vector.h"
#include "logging.h"

String demangleName(String to_demangle);
namespace STLVectorDebugTracking {
	struct TrackerImpl {
		size_t nCapacity;
		size_t nTracked;
		TrackerEntry* entries;
		std::recursive_mutex mutex;
		TrackerImpl() {
			nCapacity = 10000;
			nTracked = 0;
			entries = new TrackerEntry[nCapacity];
		}
		~TrackerImpl() {
			delete[] entries;
		}
		void add(void* ptr, const char* name, FnGetVecSize&& fn) {
			std::lock_guard<std::recursive_mutex> lock(mutex);
			dbgassert(nTracked < nCapacity);
			for (size_t i = 0; i < nCapacity; i++) {
				if (!entries[i].ptr) {
					nTracked++;
					entries[i] = TrackerEntry { ptr, std::move(fn), demangleName(name) };
					return;
				}
			}
			dbgassert(0);
		}
		void remove(void* ptr) {
			std::lock_guard<std::recursive_mutex> lock(mutex);
			for (size_t i = 0; i < nCapacity; i++) {
				if (entries[i].ptr == ptr) {
					entries[i].ptr = nullptr;
					nTracked--;
					return;
				}
			}
			dbgassert(0);
		}
		Tracker& getTracker() {
			static Tracker tracker;
			return tracker;
		}
	};
	Tracker::Tracker() : impl(new TrackerImpl()) { }
	Tracker::~Tracker() {
		delete impl;
	}
	void Tracker::add(void* ptr, const char* name, FnGetVecSize fn) {
		impl->add(ptr, name, std::move(fn));
	}
	void Tracker::remove(void* ptr) {
		impl->remove(ptr);
	}
	Tracker& getTracker() {
		static Tracker tracker;
		return tracker;
	}
	void dbgPrintVectorAllocs() {
		// PLAY THREAD MUST BE LOCKED AT THIS POINT
		// same applies to any thread doing operations on any std::vector instance
		Tracker& t = getTracker();
		TrackerImpl* ti = t.impl;
		std::lock_guard<std::recursive_mutex> lock(ti->mutex);
		auto cap = ti->nCapacity;
		auto* entries = ti->entries;
		my_printf("%lld vectors alive", ti->nTracked);
		for (size_t i = 0; i < cap; i++) {
			auto& entry = entries[i];
			if (entry.ptr) {
				size_t size = entry.fnSize();
				my_printf("%s\t%lld\n", StringAsCStr(entry.name), size);
			}
		}
	}
}
#endif
