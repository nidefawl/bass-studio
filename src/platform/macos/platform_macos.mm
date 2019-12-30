#include <stdint.h>
#include <thread>

namespace seqthreads {
int32_t currentThreadsId() {
	return static_cast<int32_t>(std::this_thread::get_id().get());
}
}
