#pragma once
#include <vector>
#include <algorithm>
#include <stdint.h>

namespace DebugAlloc {
	template<typename T>
	class Tracker {
	protected:
		int64_t allocCount = 0;
		int64_t allocId = 0;
		std::vector<T*> allocList;
	public:
		int64_t objConstructor(T* ref) {
			allocCount++;
			allocList.push_back(ref);
			return allocId++;
		}
		void objDestructor(T* ref) {
			auto it = std::find(allocList.begin(), allocList.end(), ref);
			if (it != allocList.end()) {
				allocList.erase(it);
				allocCount--;
				return;
			}
			throwUntrackked(ref);
		}
		void printLeaked();
		void throwUntrackked(T*);
	};
	template<typename T>
	Tracker<T>* getTracker();
}
