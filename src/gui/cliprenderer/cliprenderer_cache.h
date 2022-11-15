#pragma once
#include "math/vec.h"
#include "assert_dbg.h"
#include "seq_util.h"
#include "tls.h"
#include "appconfig.h"
#include <array>
#include <nanovg.h>
#include <nanovg_internal.h>

struct noteview_cache_impl_t {
    std::array<nvg_shape_cache*, 4> arr{};
    ivec2 pos             = { -1, -1 };
    ivec2 size            = { -1, -1 };
    int64_t notesRendered = -1;
    int32_t revision = -1;
    bool valid            = false;
    ~noteview_cache_impl_t() {
        reset();
    }
    void SaveFill(NVGcontext* vg, int n) {
        dbgassert(n < CtrSize(arr));
        dbgassert(arr[n] == nullptr);
        arr[n] = nullptr;
        nvgGetLastCacheResult(vg, &arr[n]);
        NVGCacheEntryInfo cacheEntryInfo;
        nvgCacheEntryInfo(nullptr, arr[n], &cacheEntryInfo);
        dbgassert(arr[n]);
        daw_tls::tlsinstance& tls = daw_tls::getTls();
        tls.runtime->renderClipCacheStats.sizeCacheAllocatedMemBytes += cacheEntryInfo.allocationSizeBytes;
    }
    bool isCacheValid(int n) {
        return valid && n < CtrSize(arr) && arr[n] != nullptr;
    }
    void reset() {
        valid = false;
        std::for_each(arr.begin(), arr.end(), [](nvg_shape_cache*& ptr) {
            if (ptr) {
                NVGCacheEntryInfo cacheEntryInfo;
                nvgCacheEntryInfo(nullptr, ptr, &cacheEntryInfo);
                daw_tls::tlsinstance& tls = daw_tls::getTls();
                tls.runtime->renderClipCacheStats.sizeCacheAllocatedMemBytes -= cacheEntryInfo.allocationSizeBytes;

                nvgReleaseCacheResult(ptr);
                ptr = nullptr;
            }
        });
    }
};

struct midi_clip_render_cache_t final : public noteview_cache_impl_t {
    midi_clip_render_cache_t() : noteview_cache_impl_t() {
    }
};