#pragma once

#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/lv2/lv2-atoms.hpp"
#include "host/plugin/lv2/lv2-external-ui.hpp"
#include "host/plugin/lv2/lv2-runtime.hpp"
#include "host/plugin/lv2/lv2-x11-embed.hpp"
#include "host/plugin/modules.hpp"
#include "types.hpp"

#include <lilv/lilv.h>
#include <lv2/data-access/data-access.h>
#include <lv2/ui/ui.h>
#include <lv2/worker/worker.h>
#include <mutex>
#include <vector>

class lv2plugin final : public effectbase {
public:
    /** Per-plugin storage for LV2 UI features.
     *
     * These structs MUST be per-plugin (not shared static), because Suil / the
     * plugin UI saves pointers to them at instantiate time and may dereference
     * them later from its own threads (e.g. JUCE's internal MessageThread in
     * Vitalium / Surge XT). If two LV2 plugins were sharing one static set of
     * structs, opening a second plugin would stomp the first plugin's feature
     * pointers (especially data_access) and any later call from the first
     * plugin's UI thread would dispatch into the WRONG plugin -> crash. */
    struct ui_feature_storage_t {
        LV2_Feature instanceFeature{};
        LV2_Feature mapFeature{};
        LV2_Feature unmapFeature{};
        LV2UI_Port_Map portMap{};
        LV2_Feature portMapFeature{};
        LV2UI_Resize resizeIface{};
        LV2_Feature resizeFeature{};
        LV2_Extension_Data_Feature dataAccessIface{};
        LV2_Feature dataAccessFeature{};
        LV2_Feature idleInterfaceFeature{};
        LV2UI_Touch touchIface{};
        LV2_Feature touchFeature{};
        LV2_External_UI_Host externalUiHost{};
        LV2_Feature externalUiHostFeature{};
        LV2_Feature externalUiHostDeprecatedFeature{};
        std::vector<LV2_Options_Option> optionsStorage;
        LV2_Feature optionsFeature{};
    };

    lv2plugin(String instanceUri, int32_t globalId, IHostCallback* hostCallback);
    ~lv2plugin() override;

    bool openInstance();
    void closeInstance();

    ModuleType getModuleType() override { return MODULE_TYPE_LV2; }
    std::shared_ptr<guiplugin> createGuiPlugin(int32_t uuid) override;
    bool showWindow(bool bResetPosition) override;
    String getAutomatableName() override { return sName; }
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    void processMidiMessages(std::vector<IMidiMsg>& midiEvents) override;
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    void onEnable() override;
    void onDisable() override;
    void setSampleFormat(sampleformat_t sampleFormat) override;
    void updateFromMainThread() override;
    samplecount_t getPluginLatency() override;
    String getInfo(std::vector<String>& list) override;
    bool hasWindowEditor() override;
    bool onShow(host_plugin_window* window) override;
    bool usesExternalToplevelWindow() const override { return toplevelUi_; }
    bool onClose() override;
    void onWindowResize(ivec2 size) override;
    void load(DAW::Host::PluginManager* host) override;
    void unload(DAW::Host::PluginManager* host) override;

    const String& instance_uri() const { return instanceUri_; }
    int getModuleCategory() const { return pluginCategory; }

    // Used by lv2_ui bridge (Suil callbacks)
    LilvInstance* lilv_instance_ptr() const { return lilvInstance_; }
    const LilvPlugin* lilv_descriptor_ptr() const { return lilvDescriptor_; }
    void apply_ui_control(uint32_t lilvPortIndex, float value);
    float lilv_control_value(uint32_t lilvPortIndex) const;
    uint32_t port_index_for_symbol(const char* symbol) const;
    void set_editor(void* host, void* ui);
    void set_suil_gtk_bridge(bool gtk) { suilGtkBridge_ = gtk; }
    bool suil_gtk_bridge() const { return suilGtkBridge_; }
    void close_editor();
    void set_native_ui(void* lib,
                       const LV2UI_Descriptor* desc,
                       LV2UI_Handle handle,
                       const LV2UI_Idle_Interface* idle,
                       const LV2UI_Show_Interface* show,
                       bool toplevelUi);
    void clear_native_ui();
    /** Set before native UI instantiate so resize callbacks skip embed/X11. */
    void set_toplevel_ui_mode(bool toplevel) { toplevelUi_ = toplevel; }
    bool toplevel_ui() const { return toplevelUi_; }
    const LV2UI_Show_Interface* native_show_interface() const { return nativeUiShow_; }
    const LV2UI_Idle_Interface* native_ui_idle() const { return nativeUiIdle_; }
    void* native_ui_lib() const { return nativeUiLib_; }
    const LV2UI_Descriptor* native_ui_descriptor() const { return nativeUiDesc_; }
    LV2UI_Handle native_ui_handle() const { return nativeUiHandle_; }
    void* suil_host_ptr() const { return suilHost_; }
    void* suil_instance_ptr() const { return suilUi_; }
    lv2_x11_embed_surface& embed_surface() { return embedSurface_; }
    const lv2_x11_embed_surface& embed_surface() const { return embedSurface_; }
    const float* control_buffer_ptr(uint32_t lilvPortIndex) const;
    LV2_Worker_Status schedule_worker_request(uint32_t size, const void* data);
    LV2_Worker_Status queue_worker_response(uint32_t size, const void* data);
    void set_ui_fit_frames(int frames) { uiFitFrames_ = frames; }
    void set_transient_window_id(int64_t xid) { hostParams_.transient_window_id = xid; }
    const lv2_host_instance_params& host_instance_params() const { return hostParams_; }
    const LV2_Options_Option* instance_options() const { return instanceOptions_.data(); }
    const String& last_open_error() const { return lastOpenError_; }
    /** Size the user last manually set for the editor window (0,0 = not yet set). */
    ivec2 saved_editor_size() const { return savedEditorSize_; }
    void  store_editor_size(ivec2 s) { if (s.x > 64 && s.y > 64) savedEditorSize_ = s; }
    bool try_cache_editor_size(int w, int h) {
        if (w == lastEditorWidth_ && h == lastEditorHeight_) {
            return false;
        }
        lastEditorWidth_  = w;
        lastEditorHeight_ = h;
        return true;
    }
    void reset_cached_editor_size() {
        lastEditorWidth_  = 0;
        lastEditorHeight_ = 0;
    }
    /** Plugin UI closed itself (external-ui ui_closed or idle != 0). */
    void on_ui_requested_close();
    /** Synchronous Carla native UI worker message (show/hide/idle/quit). */
    void run_worker_ui_message(const char* msg);
    bool has_dsp_worker() const { return workerIface_ != nullptr; }
    ui_feature_storage_t& ui_features() { return uiFeatures_; }
    const ui_feature_storage_t& ui_features() const { return uiFeatures_; }

private:
    void emit_worker_responses();
    void update_process_block(int32_t numSamples);
    struct control_port_t {
        uint32_t lilvIndex{ 0 };
        float rangeLo{ 0.f };
        float rangeHi{ 1.f };
        bool isToggled{ false };
    };

    struct daw_handles_t {
        std::vector<uint32_t> audioIns;
        std::vector<uint32_t> audioOuts;
        std::vector<control_port_t> controls;
        uint32_t atomInIndex{ UINT32_MAX };
        samplecount_t reportedLatency{ 0 };
    };

    struct atom_port_binding_t {
        uint32_t lilvIndex{ UINT32_MAX };
        bool isInput{ false };
        bool connected{ false };
        std::vector<uint8_t> buffer;
    };

    void build_port_map();
    void setup_atom_ports();
    void connect_atom_ports(uint32_t numSamples, double samplePos, playback_state state);
    void register_control_parameters();
    void connect_all_ports();
    void bind_audio_ports(AudioBlock* in, AudioBlock* out, int32_t sampleOffset);
    void push_host_values_to_plugin();
    void pull_plugin_values_from_instance();
    void set_instance_active(bool active);
    void drain_worker_requests();
    void latency_warmup_run();
    bool restore_plugin_state(const std::vector<uint8_t>& blob);
    bool save_plugin_state(std::vector<uint8_t>& blob);
    const control_port_t* control_for_lilv_index(uint32_t idx) const;
    void write_control(uint32_t lilvIndex, float value, bool notifyUi);

    String instanceUri_;
    const LilvPlugin* lilvDescriptor_{ nullptr };
    LilvInstance* lilvInstance_{ nullptr };
    daw_handles_t handles_;
    std::vector<float> lilvControlBuffer_;
    float* latencyControl_{ nullptr };
    std::vector<LV2_Feature*> featureList_;
    LV2_Feature instanceAccessFeature_{};
    LV2_Feature dataAccessFeature_{};
    LV2_Extension_Data_Feature dataAccessIface_{};
    LV2_Worker_Schedule workerScheduleIface_{};
    LV2_Feature workerScheduleFeature_{};
    std::mutex workerMutex_;
    std::vector<std::vector<uint8_t>> workerQueue_;
    std::vector<std::vector<uint8_t>> workerResponseQueue_;
    lv2_host_instance_params hostParams_;
    std::vector<LV2_Options_Option> instanceOptions_;
    LV2_Feature instanceOptionsFeature_{};
    const LV2_Options_Interface* optsIface_{ nullptr };
    const LV2_Worker_Interface* workerIface_{ nullptr };
    lv2_x11_embed_surface embedSurface_;
    lv2_atom_sequence_writer atomWriter_;
    std::vector<atom_port_binding_t> atomPortBindings_;
    std::vector<IMidiMsg> pendingMidi_;
    bool instanceActive_{ false };
    bool editorOpen_{ false };
    bool uiDestroyed_{ false };
    bool uiCloseRequested_{ false };
    int uiFitFrames_{ 0 };
    int32_t nominalBlock_{ 512 };
    float instanceSampleRate_{ 48000.f };
    int pluginCategory = 0;
    void* suilHost_{ nullptr };
    void* suilUi_{ nullptr };
    bool suilGtkBridge_{ false };
    void* nativeUiLib_{ nullptr };
    const LV2UI_Descriptor* nativeUiDesc_{ nullptr };
    LV2UI_Handle nativeUiHandle_{ nullptr };
    const LV2UI_Idle_Interface* nativeUiIdle_{ nullptr };
    const LV2UI_Show_Interface* nativeUiShow_{ nullptr };
    bool toplevelUi_{ false };
    std::vector<std::vector<float>> scratchInputs_;
    std::vector<std::vector<float>> scratchOutputs_;
    int32_t scratchSamples_{ 0 };
    int32_t lastProcessBlock_{ 0 };
    std::vector<float*> lastAudioInPtrs_;
    std::vector<float*> lastAudioOutPtrs_;
    int lastEditorWidth_{ 0 };
    int lastEditorHeight_{ 0 };
    ivec2 savedEditorSize_{ 0, 0 };
    String lastOpenError_;
    ui_feature_storage_t uiFeatures_;
};
