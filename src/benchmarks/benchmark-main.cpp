#include <cstdio>
#include <numeric>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif
#include "sse.h"
#include "hires_timer.h"
#include "logging.h"

void generateDenormals(float y[16]);

#define NUM_LOOPS 1

static void runTest(bool isFZM) {
    hires_timer_t timer;
    float y[16]{0};
    for (int i = 0; i < NUM_LOOPS; i++) {
        generateDenormals(y);
    }
    int64_t result = timer.getTimeReset();

    double sumOfResult = std::accumulate(std::cbegin(y), std::cend(y), 0.0);
    double sumInDbFS   = 20.0 * std::log10(std::abs(sumOfResult));
    // clang-format off
    log_out("%s %6s %12llu mysec\ty[0] %f\tsum %.2f dBFS\n",
            (isFZM ? "_MM_FLUSH_ZERO_ON " : "_MM_FLUSH_ZERO_OFF"),
            "Denormals",
            result,
            y[0],
            sumInDbFS);
    // clang-format on
}

void runSseBenchmarkTests() {
    RegisterStatus_SSE_CS sseStatus = getSSEControlStatusRegister();
    log_out("SSE registerBits %08X\n", sseStatus.registerBits);
    log_out("SSE FlushZeroMode %02X\n", sseStatus.regFlushZeroMode);
    log_out("SSE DenormalsAreZero %02X\n", sseStatus.regDenormalsAreZero);
    setSSENoFlushDenormals();
    sseStatus = getSSEControlStatusRegister();
    log_out("SSE registerBits %08X\n", sseStatus.registerBits);
    log_out("SSE FlushZeroMode %02X\n", sseStatus.regFlushZeroMode);
    log_out("SSE DenormalsAreZero %02X\n", sseStatus.regDenormalsAreZero);
    runTest(false);
    setSSEFlushDenormals();
    sseStatus = getSSEControlStatusRegister();
    log_out("SSE registerBits %08X\n", sseStatus.registerBits);
    log_out("SSE FlushZeroMode %02X\n", sseStatus.regFlushZeroMode);
    log_out("SSE DenormalsAreZero %02X\n", sseStatus.regDenormalsAreZero);
    runTest(true);
}
