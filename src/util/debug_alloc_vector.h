#pragma once
#if defined(__GNUC__) && defined(ENABLE_MICHAELS_GLIBCXX_HACKS)
using FnGetVecSize = std::function<std::size_t(void)>;
struct TrackerEntry {
	void* ptr = nullptr;
	FnGetVecSize fnSize = nullptr;
	String name;
};
#endif
