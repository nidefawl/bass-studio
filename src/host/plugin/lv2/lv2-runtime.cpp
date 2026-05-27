#include "host/plugin/lv2/lv2-runtime.hpp"

#include "host/plugin/lv2/lv2-suil-path.hpp"
#include "logging.hpp"

#include <cstdarg>
#include <cstring>
#include <lv2/atom/atom.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/core/lv2.h>
#include <lv2/log/log.h>
#include <lv2/midi/midi.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>
#include <lilv/lilv.h>
#include <atomic>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#ifdef PROJECT_ENABLE_LV2
#include <suil/suil.h>
#if defined(__linux__)
#include <gtk/gtk.h>
#endif
#endif

struct lv2_runtime::Impl {
    LV2_URID_Map map_iface{ nullptr, nullptr };
    LV2_URID_Unmap unmap_iface{ nullptr, nullptr };
    LV2_Log_Log log_iface{ nullptr, nullptr, nullptr };
    LV2_Feature map_feature{ LV2_URID__map, nullptr };
    LV2_Feature unmap_feature{ LV2_URID__unmap, nullptr };
    LV2_Feature log_feature{ LV2_LOG__log, nullptr };
    LV2_Feature block_feature{ LV2_BUF_SIZE__boundedBlockLength, nullptr };
    LV2_Feature min_block_feature{ LV2_BUF_SIZE__minBlockLength, nullptr };
    LV2_Feature max_block_feature{ LV2_BUF_SIZE__maxBlockLength, nullptr };
    LV2_Feature options_feature{ LV2_OPTIONS__options, nullptr };
    LV2_Feature worker_schedule_feature{ LV2_WORKER__schedule, nullptr };
    int32_t nominal_block{ 512 };
    int32_t min_block{ 1 };
    int32_t max_block{ 8192 };
    float sample_rate{ 48000.f };
    std::vector<LV2_Options_Option> options;
    uint32_t urid_sample_rate{ 0 };
    uint32_t urid_min_block{ 0 };
    uint32_t urid_max_block{ 0 };
    uint32_t urid_nominal_block{ 0 };
    uint32_t urid_sequence_size{ 0 };
    uint32_t urid_int{ 0 };
    uint32_t urid_long{ 0 };
    uint32_t urid_float{ 0 };
    uint32_t urid_transient_window{ 0 };

    std::unordered_map<std::string, uint32_t> uri_to_id;
    std::unordered_map<uint32_t, std::string> id_to_uri;
    uint32_t next_urid{ 1 };
};

namespace {

lv2_runtime* g_runtime_for_callbacks = nullptr;

LV2_URID map_uri(LV2_URID_Map_Handle, const char* uri) {
    if (!uri || !uri[0] || !g_runtime_for_callbacks) {
        return 0;
    }
    return g_runtime_for_callbacks->urid(uri);
}

const char* unmap_uri(LV2_URID_Unmap_Handle, LV2_URID urid) {
    return g_runtime_for_callbacks ? g_runtime_for_callbacks->uri_for_urid(urid) : nullptr;
}

int log_print(LV2_Log_Handle, LV2_URID, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    log_lf(Log::L_INFO, "LV2: %s\n", buf);
    return 0;
}

int log_vprint(LV2_Log_Handle, LV2_URID, const char* fmt, va_list ap) {
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    log_lf(Log::L_INFO, "LV2: %s\n", buf);
    return 0;
}

} // namespace

lv2_runtime& lv2_runtime::get() {
    static lv2_runtime instance;
    return instance;
}

lv2_runtime::lv2_runtime() : impl_(new Impl) {
    g_runtime_for_callbacks = this;
    urid(LV2_ATOM__Sequence);
    urid(LV2_ATOM__Float);
    urid(LV2_MIDI__MidiEvent);
    impl_->urid_int   = urid(LV2_ATOM__Int);
    impl_->urid_long  = urid(LV2_ATOM__Long);
    impl_->urid_float = urid(LV2_ATOM__Float);
    impl_->urid_transient_window = urid("http://kxstudio.sf.net/ns/lv2ext/props#TransientWindowId");
    impl_->urid_sample_rate   = urid(LV2_PARAMETERS__sampleRate);
    impl_->urid_min_block     = urid(LV2_BUF_SIZE__minBlockLength);
    impl_->urid_max_block     = urid(LV2_BUF_SIZE__maxBlockLength);
    impl_->urid_nominal_block = urid(LV2_BUF_SIZE__nominalBlockLength);
    impl_->urid_sequence_size = urid(LV2_BUF_SIZE__sequenceSize);
    impl_->map_iface.handle   = this;
    impl_->map_iface.map      = map_uri;
    impl_->unmap_iface.handle = this;
    impl_->unmap_iface.unmap  = unmap_uri;
    impl_->log_iface.handle   = this;
    impl_->log_iface.printf  = log_print;
    impl_->log_iface.vprintf = log_vprint;
    impl_->map_feature.data   = &impl_->map_iface;
    impl_->unmap_feature.data = &impl_->unmap_iface;
    impl_->log_feature.data   = &impl_->log_iface;
    impl_->block_feature.data = &impl_->nominal_block;
    impl_->min_block_feature.data = &impl_->min_block;
    impl_->max_block_feature.data = &impl_->max_block;
    impl_->options_feature.data   = nullptr;
    impl_->worker_schedule_feature.data = nullptr;
}

lv2_runtime::~lv2_runtime() {
    if (g_runtime_for_callbacks == this) {
        g_runtime_for_callbacks = nullptr;
    }
    delete impl_;
}

uint32_t lv2_runtime::urid(const char* uri) const {
    if (!uri || !uri[0]) {
        return 0;
    }
    auto found = impl_->uri_to_id.find(uri);
    if (found != impl_->uri_to_id.end()) {
        return found->second;
    }
    const uint32_t id = impl_->next_urid++;
    impl_->uri_to_id.emplace(uri, id);
    impl_->id_to_uri.emplace(id, uri);
    return id;
}

const char* lv2_runtime::uri_for_urid(uint32_t urid) const {
    auto found = impl_->id_to_uri.find(urid);
    return found != impl_->id_to_uri.end() ? found->second.c_str() : nullptr;
}

LV2_URID_Map* lv2_runtime::urid_map() {
    return &impl_->map_iface;
}

const LV2_URID_Map* lv2_runtime::urid_map() const {
    return &impl_->map_iface;
}

LV2_URID_Unmap* lv2_runtime::urid_unmap() {
    return &impl_->unmap_iface;
}

const LV2_URID_Unmap* lv2_runtime::urid_unmap() const {
    return &impl_->unmap_iface;
}

void lv2_runtime::ensure_suil_initialized() {
#ifdef PROJECT_ENABLE_LV2
    static std::atomic<bool> done{ false };
    if (!done.exchange(true)) {
        lv2_suil_path::ensure_module_dir();
#if defined(__linux__)
        int argc = 0;
        char** argv = nullptr;
        // Suil's x11_in_gtk3 bridge embeds X11 plugin UIs with GtkPlug/GtkSocket.
        // GtkPlug is unavailable on the Wayland GDK backend, so force GTK to use
        // X11/Xwayland before gtk_init_check().
        gdk_set_allowed_backends("x11");
        setenv("GDK_BACKEND", "x11", 1);
        if (!gtk_init_check(&argc, &argv)) {
            log_lf(Log::L_WARN, "LV2 UI: gtk_init_check failed; Suil GTK UIs may not open\n");
        }
#endif
        suil_init(nullptr, nullptr, SUIL_ARG_NONE);
    }
#endif
}

LV2_Feature* lv2_runtime::worker_schedule_feature() {
    return &impl_->worker_schedule_feature;
}

void lv2_runtime::rebuild_options() {
    impl_->options.clear();
    impl_->options.push_back(
        { LV2_OPTIONS_INSTANCE, 0, impl_->urid_sample_rate, sizeof(float), impl_->urid_float, &impl_->sample_rate });
    impl_->options.push_back(
        { LV2_OPTIONS_INSTANCE, 0, impl_->urid_min_block, sizeof(int32_t), impl_->urid_int, &impl_->min_block });
    impl_->options.push_back(
        { LV2_OPTIONS_INSTANCE, 0, impl_->urid_max_block, sizeof(int32_t), impl_->urid_int, &impl_->max_block });
    impl_->options.push_back({ LV2_OPTIONS_INSTANCE,
                               0,
                               impl_->urid_nominal_block,
                               sizeof(int32_t),
                               impl_->urid_int,
                               &impl_->nominal_block });
    impl_->options.push_back({});
    impl_->options_feature.data = impl_->options.data();
}

uint32_t lv2_runtime::urid_int() const {
    return impl_->urid_int;
}

uint32_t lv2_runtime::urid_nominal_block() const {
    return impl_->urid_nominal_block;
}

void lv2_runtime::build_instance_options(std::vector<LV2_Options_Option>& options_storage,
                                         lv2_host_instance_params& params,
                                         bool includeTransientWindow) const {
    options_storage.clear();
    options_storage.push_back({ LV2_OPTIONS_INSTANCE, 0, impl_->urid_sample_rate, sizeof(float), impl_->urid_float, &params.sample_rate });
    options_storage.push_back({ LV2_OPTIONS_INSTANCE, 0, impl_->urid_min_block, sizeof(int32_t), impl_->urid_int, &params.min_block });
    options_storage.push_back({ LV2_OPTIONS_INSTANCE, 0, impl_->urid_max_block, sizeof(int32_t), impl_->urid_int, &params.max_block });
    options_storage.push_back(
        { LV2_OPTIONS_INSTANCE, 0, impl_->urid_nominal_block, sizeof(int32_t), impl_->urid_int, &params.nominal_block });
    options_storage.push_back(
        { LV2_OPTIONS_INSTANCE, 0, impl_->urid_sequence_size, sizeof(int32_t), impl_->urid_int, &params.sequence_size });
    if (includeTransientWindow) {
        options_storage.push_back({ LV2_OPTIONS_INSTANCE,
                                    0,
                                    impl_->urid_transient_window,
                                    sizeof(int64_t),
                                    impl_->urid_long,
                                    &params.transient_window_id });
    }
    options_storage.push_back({});
}

void lv2_runtime::append_host_features(std::vector<LV2_Feature*>& out,
                                       lv2_host_instance_params& params,
                                       std::vector<LV2_Options_Option>& options_storage,
                                       LV2_Feature& options_feature) {
    build_instance_options(options_storage, params, false);
    options_feature = LV2_Feature{ LV2_OPTIONS__options, options_storage.data() };
    impl_->min_block_feature.data = &params.min_block;
    impl_->max_block_feature.data = &params.max_block;
    impl_->block_feature.data     = &params.nominal_block;
    out.push_back(&impl_->map_feature);
    out.push_back(&impl_->unmap_feature);
    out.push_back(&impl_->log_feature);
    out.push_back(&options_feature);
    out.push_back(&impl_->min_block_feature);
    out.push_back(&impl_->max_block_feature);
    out.push_back(&impl_->block_feature);
    out.push_back(nullptr);
}

void lv2_runtime::host_features(std::vector<LV2_Feature*>& out, int32_t nominalBlockSize, float sampleRate) {
    impl_->nominal_block = nominalBlockSize > 0 ? nominalBlockSize : 512;
    impl_->min_block      = 1;
    impl_->max_block      = impl_->nominal_block > 8192 ? impl_->nominal_block : 8192;
    impl_->sample_rate    = sampleRate > 0.f ? sampleRate : 48000.f;
    impl_->block_feature.data = &impl_->nominal_block;
    rebuild_options();
    out.push_back(&impl_->map_feature);
    out.push_back(&impl_->unmap_feature);
    out.push_back(&impl_->log_feature);
    out.push_back(&impl_->options_feature);
    out.push_back(&impl_->min_block_feature);
    out.push_back(&impl_->max_block_feature);
    out.push_back(&impl_->block_feature);
    out.push_back(nullptr);
}
