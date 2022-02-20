#include <benchmark/benchmark.h>
#include <array>
#include "logging.h"
#include "str_util.h"
#include "exceptions.h"
#include "appconfig.h"
#include "appsettings.h"
#include "platform.h"
#include "basectrl.h"
#include "hires_timer.h"
#include "sse.h"

#include <memory>
#include <cstdio>
#include <numeric>
#include <cmath>

#ifdef _WIN32
#include <Windows.h>
#include <intrin.h>
#include "platform/win/windowsize.h"
#include "platform/win/platform_win.h"
#endif

void generateDenormals(float y[16]) {
    const float x[16] = {1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6};
    const float z[16] = {1.123, 1.234, 1.345, 156.467, 1.578, 1.689, 1.790, 1.812, 1.923, 2.034, 2.145, 2.256, 2.367, 2.478, 2.589, 2.690};
    for (int i = 0; i < 16; i++) {
        y[i] = x[i];
    }
    for (int j = 0; j < 90000; j++) {
        for (int i = 0; i < 16; i++) {
            y[i] *= x[i];
            y[i] /= z[i];
        }
    }
}

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

int main(int argc, char **argv) {
    runSseBenchmarkTests();
    return 0;
}