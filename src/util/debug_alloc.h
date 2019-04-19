#pragma once
#include <vector>
#include <algorithm>
#include <stdint.h>
#include <unordered_map>
#include "platform.h"

namespace DebugAlloc {
	struct AllocInfo;
	template<typename T>
	class Tracker;

	template<typename T>
	Tracker<T>* getTracker();


	template<typename T>
	void throwUntrackked(Tracker<T>& t, T* g) {
		my_printf("object with allocId %lld was not tracked\n", g->allocId);
		assert(0);
	}


	template<typename T>
	void printLeaked(int64_t allocId, int64_t allocCount, std::vector<T*>& allocList, std::unordered_map<int64_t, AllocInfo>& allocInfo) {
		my_printf("allocCount %lld\n", allocCount);
	}

	struct AllocInfo {
		std::vector<String> stacktrace;
	};

	template<typename T>
	class Tracker {
	public:
		int64_t allocId = 0;
		int64_t allocCount = 0;
		std::vector<T*> allocList;
		std::unordered_map<int64_t, AllocInfo> allocInfo;
		bool recordAllocStackTraces = false;
		bool printAllocStackTraces = false;
		void setRecordAllocationStackTraces(bool bRecordStacktraces) {
			this->recordAllocStackTraces = bRecordStacktraces;
		}
		bool getRecordAllocationStackTraces() {
			 return this->recordAllocStackTraces;
		}
		void setPrintAllocationStackTraces(bool b) {
			this->printAllocStackTraces = b;
		}
		bool getPrintAllocationStackTraces() {
			 return this->printAllocStackTraces;
		}
		int64_t objConstructor(T* ref) {
			const auto refAllocId = allocId++;
			if (recordAllocStackTraces || printAllocStackTraces) {

				// could be way more efficient
				AllocInfo info;
				getStackTrace(info.stacktrace);
				if (printAllocStackTraces) {
					int len = info.stacktrace.size();
					for (int i = 0; i < len; i++) {
						log_printf("%s\n", StringAsCStr(info.stacktrace[i]));
					}
				}
				if (recordAllocStackTraces) {
					allocInfo[refAllocId] = std::move(info);
				}
			}
			allocCount++;
			allocList.push_back(ref);
			return refAllocId;
		}
		void objDestructor(T* ref) {
			auto it = std::find(allocList.begin(), allocList.end(), ref);
			if (it != allocList.end()) {
				allocList.erase(it);
				allocCount--;
				return;
			}
			throwUntrackked(*this, ref);
		}
		void printAllocations() {
			printLeaked(allocId, allocCount, allocList, allocInfo);
		}
		void onExit() {
			printLeaked(allocId, allocCount, allocList, allocInfo);
			allocList.clear();
		}
	};
}
