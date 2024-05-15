#include "TestBase.hpp"
#include "common/test_common.h"

#ifdef _WIN32
#include <cstdint>
#include <synchapi.h>
#include <vector>
#include <windows.h>
#include <psapi.h>
#endif

namespace {

void test_memory_leak() {
    TEST_BEGIN("test_memory_leak");
    new float[1024L * 1024 * 200];
    TEST_END();
}

#ifdef _WIN32
void printMem(const char* label) {
    PROCESS_MEMORY_COUNTERS memCounter;
    BOOL result = GetProcessMemoryInfo(GetCurrentProcess(),
                                       &memCounter,
                                       sizeof(memCounter));
    if (result) {
        printf("Memory usage %s\n%.3f MB\n", label, memCounter.WorkingSetSize / 1024.0 / 1024.0);
    }
}
#endif

void test_std_vector_mem_usage() {
    TEST_BEGIN("test_std_vector_mem_usage");
#ifdef _WIN32
    {
        std::vector<byte> data;
        printMem("vector<byte> data;");
        {
            // 512MB
            byte* p = new byte[1024 * 1024 * 512];
            std::memset(p, 0, 1024 * 1024 * 512);
            Sleep(500);
            printMem("new byte[1024 * 1024 * 512];");
            delete[] p;
            Sleep(500);
            printMem("delete[] p;");
        }
        {
            // 512MB
            data.resize(1024 * 1024 * 512);
            Sleep(500);
            printMem("data.resize(1024 * 1024 * 512);");
        }
        {
            data.resize(0);
            Sleep(500);
            printMem("data.resize(0);");
        }
        {
            data.resize(1024 * 1024 * 512);
            data.clear();
            Sleep(500);
            printMem("data.resize(1024 * 1024 * 512); data.clear();");
        }
        {
            data = {};
            data.shrink_to_fit();
            Sleep(500);
            printMem("data = {}; data.shrink_to_fit();");
        }
    }
#endif
    TEST_END();
}

}// namespace

int main() {
    using byte = std::byte;
    test_memory_leak();
    test_std_vector_mem_usage();
    return 0;
}
