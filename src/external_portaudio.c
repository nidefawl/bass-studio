#include <pa_allocation.c>
#include <pa_converters.c>
#include <pa_cpuload.c>
#include <pa_dither.c>
#include <pa_front.c>
#include <pa_process.c>
#include <pa_ringbuffer.c>
#include <pa_stream.c>
#include <pa_trace.c>
#ifdef _WIN32
#include <pa_win_coinitialize.c>
#include <pa_win_ds.c>
#include <pa_win_ds_dynlink.c>
#include <pa_win_hostapis.c>
#include <pa_win_util.c>
#include <pa_win_waveformat.c>
#include <pa_x86_plain_converters.c>
#endif
