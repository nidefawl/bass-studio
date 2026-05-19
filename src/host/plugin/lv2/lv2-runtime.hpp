#pragma once

#include <cstdint>
#include <vector>

#include <lv2/core/lv2.h>
#include <lv2/options/options.h>
#include <lv2/urid/urid.h>

/** Per-plugin instantiate parameters (LV2_OPTIONS__options). */
struct lv2_host_instance_params {
    int32_t nominal_block        = 512;
    int32_t min_block            = 1;
    int32_t max_block            = 8192;
    int32_t sequence_size        = 8192;
    float   sample_rate          = 48000.f;
    int64_t transient_window_id  = 0; // kxstudio:TransientWindowId (set before UI open)
};

/** Shared LV2 host services (URIDs, log, block-size option) for all instances in this process. */
class lv2_runtime {
public:
    static lv2_runtime& get();

    uint32_t urid(const char* uri) const;
    const char* uri_for_urid(uint32_t urid) const;

    LV2_URID_Map* urid_map();
    const LV2_URID_Map* urid_map() const;
    LV2_URID_Unmap* urid_unmap();
    const LV2_URID_Unmap* urid_unmap() const;

    /** Appends null-terminated feature list pointers into @p out (does not clear @p out). */
    void host_features(std::vector<LV2_Feature*>& out, int32_t nominalBlockSize, float sampleRate);

    /** Build per-instance options; @p options_storage must outlive the plugin instance. */
    void build_instance_options(std::vector<LV2_Options_Option>& options_storage,
                              lv2_host_instance_params& params,
                              bool includeTransientWindow = false) const;

    /** Append map/unmap/log/block/options features using @p options_storage.data(). */
    void append_host_features(std::vector<LV2_Feature*>& out,
                              lv2_host_instance_params& params,
                              std::vector<LV2_Options_Option>& options_storage,
                              LV2_Feature& options_feature);

    uint32_t urid_int() const;
    uint32_t urid_nominal_block() const;

    /** Optional LV2 worker schedule feature; valid after host_features(). */
    LV2_Feature* worker_schedule_feature();

    static void ensure_suil_initialized();

private:
    lv2_runtime();
    ~lv2_runtime();
    lv2_runtime(const lv2_runtime&) = delete;
    lv2_runtime& operator=(const lv2_runtime&) = delete;

    void rebuild_options();

    struct Impl;
    Impl* impl_;
};
