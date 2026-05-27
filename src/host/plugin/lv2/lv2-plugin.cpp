#include "host/plugin/lv2/lv2-plugin.hpp"

#include "gui/plugin/plugin.hpp"
#include "host/host_plugin_window.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/plugin/lv2/lv2-catalog.hpp"
#include "host/plugin/lv2/lv2-runtime.hpp"
#include "host/plugin/lv2/lv2-native-x11-ui.hpp"
#include "host/plugin/lv2/lv2-ui.hpp"
#include "host/plugin/lv2/lv2-ui-host.hpp"
#include "logging.hpp"
#include "seq_time.hpp"
#include "plugins/synth/IPlugMidi.hpp"
#include "math/seq_math.hpp"

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/data-access/data-access.h>
#include <lv2/options/options.h>
#include <lv2/resize-port/resize-port.h>
#include <lv2/time/time.h>
#include <lv2/worker/worker.h>
#include <algorithm>
#include <cstring>

#include "config.hpp"
#if defined(__linux__)
#include "platform/linux/x11_util.hpp"
#include <GLFW/glfw3.h>
#endif

#ifdef PROJECT_ENABLE_LV2
#include <suil/suil.h>
#endif

namespace {

bool port_is_a(LilvWorld* w, const LilvPlugin* plugin, const LilvPort* port, const char* typeUri, const char* flowUri) {
    LilvNode* type = lilv_new_uri(w, typeUri);
    LilvNode* flow = lilv_new_uri(w, flowUri);
    const bool ok = lilv_port_is_a(plugin, port, type) && lilv_port_is_a(plugin, port, flow);
    lilv_node_free(flow);
    lilv_node_free(type);
    return ok;
}

bool port_is_atom_events_input(LilvWorld* w, const LilvPlugin* plugin, const LilvPort* port) {
    if (!port_is_a(w, plugin, port, LILV_URI_ATOM_PORT, LILV_URI_INPUT_PORT)) {
        return false;
    }
    LilvNode* midi = lilv_new_uri(w, LV2_MIDI__MidiEvent);
    LilvNode* timePos = lilv_new_uri(w, LV2_TIME__Position);
    const bool supports =
        lilv_port_supports_event(plugin, port, midi) || lilv_port_supports_event(plugin, port, timePos);
    lilv_node_free(timePos);
    lilv_node_free(midi);
    return supports;
}

void write_empty_atom_sequence(std::vector<uint8_t>& storage, LV2_URID_Map* map) {
    if (!map || storage.empty()) {
        return;
    }
    LV2_Atom_Forge forge;
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_init(&forge, map);
    lv2_atom_forge_set_buffer(&forge, storage.data(), static_cast<uint32_t>(storage.size()));
    lv2_atom_forge_sequence_head(&forge, &frame, 0);
    lv2_atom_forge_pop(&forge, &frame);
}

int atom_port_min_buffer_size(LilvWorld* w, const LilvPlugin* plugin, const LilvPort* port) {
    LilvNode* nMinSize = lilv_new_uri(w, LV2_RESIZE_PORT__minimumSize);
    int bufSize       = 8192;
    LilvNodes* minNodes = lilv_port_get_value(plugin, port, nMinSize);
    LilvNode* minNode   = minNodes ? lilv_nodes_get_first(minNodes) : nullptr;
    if (minNode && lilv_node_is_int(minNode)) {
        bufSize = std::max(bufSize, lilv_node_as_int(minNode));
    }
    lilv_nodes_free(minNodes);
    lilv_node_free(nMinSize);
    return bufSize;
}

const void* state_get_port_value(const char* port_symbol, void* user_data, uint32_t* size, uint32_t* type) {
    auto* self = static_cast<lv2plugin*>(user_data);
    if (!self || !port_symbol || !size || !type) {
        return nullptr;
    }
    const uint32_t idx = self->port_index_for_symbol(port_symbol);
    if (idx == UINT32_MAX) {
        return nullptr;
    }
    *size = sizeof(float);
    *type = lv2_runtime::get().urid(LV2_ATOM__Float);
    return self->control_buffer_ptr(idx);
}

void state_set_port_value(const char* port_symbol, void* user_data, const void* value, uint32_t size, uint32_t type) {
    auto* self = static_cast<lv2plugin*>(user_data);
    if (!self || !port_symbol || !value || size < sizeof(float)) {
        return;
    }
    (void)type;
    const uint32_t idx = self->port_index_for_symbol(port_symbol);
    if (idx == UINT32_MAX) {
        return;
    }
    self->apply_ui_control(idx, *static_cast<const float*>(value));
}

LV2_Worker_Status worker_schedule_cb(LV2_Worker_Schedule_Handle handle, uint32_t size, const void* data) {
    auto* self = static_cast<lv2plugin*>(handle);
    return self ? self->schedule_worker_request(size, data) : LV2_WORKER_ERR_UNKNOWN;
}

LV2_Worker_Status worker_respond_cb(LV2_Worker_Respond_Handle handle, uint32_t size, const void* data) {
    auto* self = static_cast<lv2plugin*>(handle);
    return self ? self->queue_worker_response(size, data) : LV2_WORKER_ERR_UNKNOWN;
}

thread_local lv2plugin* g_plugin_data_access = nullptr;

const void* plugin_data_access_cb(const char* uri) {
    if (!g_plugin_data_access || !g_plugin_data_access->lilv_instance_ptr()) {
        return nullptr;
    }
    return lilv_instance_get_extension_data(g_plugin_data_access->lilv_instance_ptr(), uri);
}

} // namespace

lv2plugin::lv2plugin(String instanceUri, int32_t globalId, IHostCallback* hostCallback)
    : effectbase(instanceUri, globalId, hostCallback)
    , instanceUri_(instanceUri) {
}

lv2plugin::~lv2plugin() {
    close_editor();
    closeInstance();
}

bool lv2plugin::openInstance() {
    lastOpenError_.clear();
    if (lv2_catalog::is_lv2_bundle_path(instanceUri_)) {
        const String bundlePath = instanceUri_;
        const String resolved   = lv2_catalog::resolve_instance_uri(bundlePath, sName);
        if (!resolved.empty()) {
            instanceUri_ = resolved;
        }
        lv2_catalog::load_host_search_path(bundlePath);
    }
    lv2_catalog::ensure_host_plugin_paths_loaded();
    lilvDescriptor_ = lv2_catalog::find_plugin(instanceUri_.c_str());
    if (!lilvDescriptor_) {
        lastOpenError_ = StringFormat("LV2 plugin not in catalog: %s", StringAsCStr(instanceUri_));
        log_lf(Log::L_ERROR, "%s\n", StringAsCStr(lastOpenError_));
        return false;
    }

    LilvWorld* w = lv2_catalog::process_world();

    LilvNode* title = lilv_plugin_get_name(lilvDescriptor_);
    if (title) {
        sName = lilv_node_as_string(title);
        lilv_node_free(title);
    }
    LilvNode* maker = lilv_plugin_get_author_name(lilvDescriptor_);
    if (maker) {
        sVendorName = lilv_node_as_string(maker);
        lilv_node_free(maker);
    }
    sProductName = instanceUri_;

    const uint32_t nPorts = lilv_plugin_get_num_ports(lilvDescriptor_);
    lilvControlBuffer_.assign(nPorts, 0.f);

    build_port_map();
    register_control_parameters();

    float rate = 48000.f;
    if (format.sampleRate > 0) {
        rate = format.sampleRate;
    } else if (hostCallback && hostCallback->m_sampleFormatInternal.sampleRate > 0) {
        rate = hostCallback->m_sampleFormatInternal.sampleRate;
    }
    instanceSampleRate_ = rate;

    auto& rt = lv2_runtime::get();
    featureList_.clear();
    hostParams_.sample_rate   = instanceSampleRate_;
    hostParams_.nominal_block = nominalBlock_ > 0 ? nominalBlock_ : 512;
    hostParams_.min_block     = 1;
    hostParams_.max_block     = std::max(8192, hostParams_.nominal_block);
    hostParams_.sequence_size = std::max(8192, hostParams_.nominal_block * 16);
    rt.append_host_features(featureList_, hostParams_, instanceOptions_, instanceOptionsFeature_);

    instanceAccessFeature_.URI  = LV2_INSTANCE_ACCESS_URI;
    instanceAccessFeature_.data = nullptr;
    dataAccessIface_.data_access = plugin_data_access_cb;
    dataAccessFeature_.URI       = LV2_DATA_ACCESS_URI;
    dataAccessFeature_.data      = &dataAccessIface_;
    featureList_.insert(featureList_.end() - 1, &dataAccessFeature_);
    workerScheduleIface_.handle       = this;
    workerScheduleIface_.schedule_work = worker_schedule_cb;
    workerScheduleFeature_.URI  = LV2_WORKER__schedule;
    workerScheduleFeature_.data = &workerScheduleIface_;
    featureList_.insert(featureList_.end() - 1, &instanceAccessFeature_);
    LilvNode* workerFeat = lilv_new_uri(w, LV2_WORKER__schedule);
    bool wantsWorker = lilv_plugin_has_feature(lilvDescriptor_, workerFeat);
    if (!wantsWorker) {
        LilvNodes* opt = lilv_plugin_get_optional_features(lilvDescriptor_);
        if (opt) {
            LILV_FOREACH(nodes, wi, opt) {
                const LilvNode* f = lilv_nodes_get(opt, wi);
                if (f && lilv_node_equals(f, workerFeat)) {
                    wantsWorker = true;
                    break;
                }
            }
        }
        lilv_nodes_free(opt);
    }
    if (!wantsWorker) {
        LilvNodes* req = lilv_plugin_get_required_features(lilvDescriptor_);
        if (req && lilv_nodes_contains(req, workerFeat)) {
            wantsWorker = true;
        }
        lilv_nodes_free(req);
    }
    if (wantsWorker) {
        featureList_.insert(featureList_.end() - 1, &workerScheduleFeature_);
    }
    lilv_node_free(workerFeat);
    g_plugin_data_access = this;
    lilvInstance_ = lilv_plugin_instantiate(lilvDescriptor_, rate, featureList_.data());
    g_plugin_data_access = nullptr;
    if (!lilvInstance_) {
        lastOpenError_ = StringFormat("LV2 instantiate failed: %s", StringAsCStr(instanceUri_));
        log_lf(Log::L_ERROR, "%s (check required host features)\n", StringAsCStr(lastOpenError_));
        LilvWorld* wlog = lv2_catalog::process_world();
        LilvNodes* req = lilv_plugin_get_required_features(lilvDescriptor_);
        if (wlog && req) {
            LILV_FOREACH(nodes, i, req) {
                const LilvNode* f = lilv_nodes_get(req, i);
                if (f) {
                    log_lf(Log::L_ERROR, "  required: %s\n", lilv_node_as_string(f));
                }
            }
        }
        lilv_nodes_free(req);
        return false;
    }
    instanceAccessFeature_.data = lilvInstance_->lv2_handle;

    if (lilvInstance_->lv2_descriptor && lilvInstance_->lv2_descriptor->extension_data) {
        dataAccessIface_.data_access = lilvInstance_->lv2_descriptor->extension_data;
    }

    workerIface_ = static_cast<const LV2_Worker_Interface*>(
        lilv_instance_get_extension_data(lilvInstance_, LV2_WORKER__interface));
    optsIface_ = static_cast<const LV2_Options_Interface*>(
        lilv_instance_get_extension_data(lilvInstance_, LV2_OPTIONS__interface));

    if (optsIface_) {
        lv2_runtime::get().build_instance_options(instanceOptions_, hostParams_);
        const uint32_t optStatus = optsIface_->set(lilvInstance_->lv2_handle, instanceOptions_.data());
        if (optStatus != LV2_OPTIONS_SUCCESS) {
            log_lf(Log::L_WARN, "LV2 options set returned %u for %s\n", optStatus, StringAsCStr(instanceUri_));
        }
    }

    connect_all_ports();
    setup_atom_ports();

    LilvNode* instrument = lilv_new_uri(w, LV2_CORE__InstrumentPlugin);
    const LilvPluginClass* cls = lilv_plugin_get_class(lilvDescriptor_);
    const LilvNode* clsUri = cls ? lilv_plugin_class_get_uri(cls) : nullptr;
    isSynth = clsUri && lilv_node_equals(clsUri, instrument);
    lilv_node_free(instrument);
    bCanReceiveMidi = isSynth || handles_.atomInIndex != UINT32_MAX;
    pluginCategory  = isSynth ? 1 : 0;

    const LilvUIs* uis = lilv_plugin_get_uis(lilvDescriptor_);
    if (uis && lilv_uis_size(uis) > 0) {
        bSupportsWindowResize = true;
    }

    latency_warmup_run();

    return true;
}

void lv2plugin::closeInstance() {
    if (!uiDestroyed_) {
        close_editor();
    }
    clearRegisteredParamsFrom(PARAM_OFFSET_EXTERNAL);
    handles_.controls.clear();
    atomPortBindings_.clear();
    handles_.atomInIndex = UINT32_MAX;
    set_instance_active(false);
    if (lilvInstance_) {
        lilv_instance_free(lilvInstance_);
        lilvInstance_ = nullptr;
    }
    lilvDescriptor_ = nullptr;
    latencyControl_ = nullptr;
    lastProcessBlock_ = 0;
    lastAudioInPtrs_.clear();
    lastAudioOutPtrs_.clear();
    featureList_.clear();
    pendingMidi_.clear();
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workerQueue_.clear();
        workerResponseQueue_.clear();
    }
    workerIface_ = nullptr;
    optsIface_   = nullptr;
    instanceOptions_.clear();
}

void lv2plugin::build_port_map() {
    handles_.audioIns.clear();
    handles_.audioOuts.clear();
    handles_.controls.clear();
    handles_.atomInIndex = UINT32_MAX;

    LilvWorld* w = lv2_catalog::process_world();
    const uint32_t n = lilv_plugin_get_num_ports(lilvDescriptor_);
    for (uint32_t i = 0; i < n; ++i) {
        const LilvPort* port = lilv_plugin_get_port_by_index(lilvDescriptor_, i);
        if (port_is_a(w, lilvDescriptor_, port, LILV_URI_AUDIO_PORT, LILV_URI_INPUT_PORT)) {
            handles_.audioIns.push_back(i);
        } else if (port_is_a(w, lilvDescriptor_, port, LILV_URI_AUDIO_PORT, LILV_URI_OUTPUT_PORT)) {
            handles_.audioOuts.push_back(i);
        } else if (port_is_a(w, lilvDescriptor_, port, LILV_URI_CONTROL_PORT, LILV_URI_INPUT_PORT)) {
            control_port_t cp;
            cp.lilvIndex = i;
            LilvNode* def{};
            LilvNode* min{};
            LilvNode* max{};
            lilv_port_get_range(lilvDescriptor_, port, &def, &min, &max);
            cp.rangeLo = min ? lilv_node_as_float(min) : 0.f;
            cp.rangeHi = max ? lilv_node_as_float(max) : 1.f;
            if (def) {
                lilvControlBuffer_[i] = lilv_node_as_float(def);
            }
            LilvNode* toggled = lilv_new_uri(w, LV2_CORE__toggled);
            cp.isToggled = lilv_port_has_property(lilvDescriptor_, port, toggled);
            lilv_node_free(toggled);
            lilv_node_free(def);
            lilv_node_free(min);
            lilv_node_free(max);
            handles_.controls.push_back(cp);
        }
    }

    inputChannelsDesc.clear();
    outputChannelsDesc.clear();
    for (size_t c = 0; c < handles_.audioIns.size(); ++c) {
        DAW::channel_desc d;
        d.name = StringFormat("In %zu", c + 1);
        inputChannelsDesc.push_back(d);
    }
    for (size_t c = 0; c < handles_.audioOuts.size(); ++c) {
        DAW::channel_desc d;
        d.name = StringFormat("Out %zu", c + 1);
        outputChannelsDesc.push_back(d);
    }
    if (inputChannelsDesc.empty()) {
        DAW::channel_desc d;
        d.name = "In";
        inputChannelsDesc.push_back(d);
    }
    if (outputChannelsDesc.empty()) {
        DAW::channel_desc d;
        d.name = "Out";
        outputChannelsDesc.push_back(d);
    }
    initDefaultIODesc();
}

void lv2plugin::setup_atom_ports() {
    atomPortBindings_.clear();
    handles_.atomInIndex = UINT32_MAX;

    LilvWorld* w = lv2_catalog::process_world();
    if (!w || !lilvDescriptor_) {
        return;
    }

    LilvNode* nAtom   = lilv_new_uri(w, LILV_URI_ATOM_PORT);
    LilvNode* nInput  = lilv_new_uri(w, LILV_URI_INPUT_PORT);
    LilvNode* nOutput = lilv_new_uri(w, LILV_URI_OUTPUT_PORT);
    auto& rt          = lv2_runtime::get();

    const uint32_t n = lilv_plugin_get_num_ports(lilvDescriptor_);
    for (uint32_t i = 0; i < n; ++i) {
        const LilvPort* port = lilv_plugin_get_port_by_index(lilvDescriptor_, i);
        if (!lilv_port_is_a(lilvDescriptor_, port, nAtom)) {
            continue;
        }
        atom_port_binding_t ap;
        ap.lilvIndex = i;
        ap.isInput   = lilv_port_is_a(lilvDescriptor_, port, nInput);
        const bool isOutput = lilv_port_is_a(lilvDescriptor_, port, nOutput);
        if (!ap.isInput && !isOutput) {
            continue;
        }
        const int bufSize = atom_port_min_buffer_size(w, lilvDescriptor_, port);
        ap.buffer.assign(static_cast<size_t>(bufSize), uint8_t(0));
        write_empty_atom_sequence(ap.buffer, rt.urid_map());
        if (ap.isInput && port_is_atom_events_input(w, lilvDescriptor_, port)) {
            handles_.atomInIndex = i;
        }
        atomPortBindings_.push_back(ap);
    }

    lilv_node_free(nOutput);
    lilv_node_free(nInput);
    lilv_node_free(nAtom);

    const uint32_t seqCap = std::max(8192u, static_cast<uint32_t>(hostParams_.sequence_size));
    atomWriter_.configure(rt, seqCap);
}

void lv2plugin::connect_atom_ports(uint32_t numSamples, double samplePos, playback_state state) {
    if (!lilvInstance_ || atomPortBindings_.empty()) {
        return;
    }
    auto& rt = lv2_runtime::get();
    const int64_t hostFrame = static_cast<int64_t>(samplePos);
    const double transportSpeed = DAW::isPlaybackState(state) ? 1.0 : 0.0;
    for (atom_port_binding_t& ap : atomPortBindings_) {
        void* portData = ap.buffer.data();
        if (ap.isInput && ap.lilvIndex == handles_.atomInIndex && handles_.atomInIndex != UINT32_MAX) {
            atomWriter_.begin_block(numSamples);
            atomWriter_.append_time_position(0, hostFrame, transportSpeed);
            for (const IMidiMsg& msg : pendingMidi_) {
                const int32_t offset = math::clamp(msg.mOffset, 0, static_cast<int32_t>(numSamples) - 1);
                atomWriter_.append_midi(static_cast<uint32_t>(offset), msg.mStatus, msg.mData1, msg.mData2);
            }
            if (void* seq = atomWriter_.finish_block()) {
                portData = seq;
            }
        } else if (ap.isInput) {
            write_empty_atom_sequence(ap.buffer, rt.urid_map());
        } else {
            write_empty_atom_sequence(ap.buffer, rt.urid_map());
        }
        if (!ap.connected) {
            lilv_instance_connect_port(lilvInstance_, ap.lilvIndex, portData);
            ap.connected = true;
        }
    }
}

void lv2plugin::register_control_parameters() {
    int hostParamIdx = 0;
    for (const control_port_t& cp : handles_.controls) {
        const LilvPort* port = lilv_plugin_get_port_by_index(lilvDescriptor_, cp.lilvIndex);
        LilvNode* label = lilv_port_get_name(lilvDescriptor_, port);
        const int32_t paramId = PARAM_OFFSET_EXTERNAL + hostParamIdx;
        automatable_param_t* p = registerParam(paramId);
        p->internalIdx = static_cast<int32_t>(cp.lilvIndex);
        p->inUse       = true;
        p->name        = label ? lilv_node_as_string(label) : StringFormat("Control %d", hostParamIdx);
        lilv_node_free(label);
        const float span = cp.rangeHi - cp.rangeLo;
        const float norm = span > 1e-12f ? (lilvControlBuffer_[cp.lilvIndex] - cp.rangeLo) / span : 0.f;
        p->setInitial(norm);
        if (cp.isToggled) {
            p->quantizationSteps = 1;
        }
        ++hostParamIdx;
    }
}

void lv2plugin::connect_all_ports() {
    if (!lilvInstance_) {
        return;
    }
    LilvWorld* w = lv2_catalog::process_world();
    const uint32_t nPorts = lilv_plugin_get_num_ports(lilvDescriptor_);
    for (uint32_t i = 0; i < nPorts; ++i) {
        LilvNode* ctrl = lilv_new_uri(w, LILV_URI_CONTROL_PORT);
        if (lilv_port_is_a(lilvDescriptor_, lilv_plugin_get_port_by_index(lilvDescriptor_, i), ctrl)) {
            lilv_instance_connect_port(lilvInstance_, i, &lilvControlBuffer_[i]);
        }
        lilv_node_free(ctrl);
    }
    if (lilv_plugin_has_latency(lilvDescriptor_)) {
        const uint32_t latIdx = lilv_plugin_get_latency_port_index(lilvDescriptor_);
        if (latIdx < lilvControlBuffer_.size()) {
            latencyControl_ = &lilvControlBuffer_[latIdx];
        }
    }
}

const lv2plugin::control_port_t* lv2plugin::control_for_lilv_index(uint32_t idx) const {
    for (const control_port_t& cp : handles_.controls) {
        if (cp.lilvIndex == idx) {
            return &cp;
        }
    }
    return nullptr;
}

void lv2plugin::write_control(uint32_t lilvIndex, float value, bool notifyUi) {
    if (lilvIndex >= lilvControlBuffer_.size()) {
        return;
    }
    lilvControlBuffer_[lilvIndex] = value;
    if (notifyUi) {
        if (nativeUiHandle_) {
            lv2_native_x11_ui::notify_control(this, lilvIndex, value);
        } else {
            lv2_ui::notify_control(this, lilvIndex, value);
        }
    }
}

void lv2plugin::apply_ui_control(uint32_t lilvPortIndex, float value) {
    write_control(lilvPortIndex, value, false);
    const control_port_t* cp = control_for_lilv_index(lilvPortIndex);
    if (!cp) {
        return;
    }
    automatable_param_t* p = getEffectParam(static_cast<int32_t>(lilvPortIndex));
    if (!p) {
        return;
    }
    const float span = cp->rangeHi - cp->rangeLo;
    const float norm = span > 1e-12f ? (value - cp->rangeLo) / span : 0.f;
    setParamValue(p->idx, norm, FLG_PAR_UPDATE_FROM_CLIENT | FLG_PAR_UPDATE_NOSTORE);
}

float lv2plugin::lilv_control_value(uint32_t lilvPortIndex) const {
    return lilvPortIndex < lilvControlBuffer_.size() ? lilvControlBuffer_[lilvPortIndex] : 0.f;
}

const float* lv2plugin::control_buffer_ptr(uint32_t lilvPortIndex) const {
    return lilvPortIndex < lilvControlBuffer_.size() ? &lilvControlBuffer_[lilvPortIndex] : nullptr;
}

uint32_t lv2plugin::port_index_for_symbol(const char* symbol) const {
    if (!lilvDescriptor_ || !symbol) {
        return UINT32_MAX;
    }
    const uint32_t n = lilv_plugin_get_num_ports(lilvDescriptor_);
    for (uint32_t i = 0; i < n; ++i) {
        const LilvPort* port = lilv_plugin_get_port_by_index(lilvDescriptor_, i);
        const LilvNode* sym = lilv_port_get_symbol(lilvDescriptor_, port);
        if (sym) {
            const char* symStr = lilv_node_as_string(sym);
            if (symStr && std::strcmp(symStr, symbol) == 0) {
                return i;
            }
        }
    }
    return UINT32_MAX;
}

void lv2plugin::set_instance_active(bool active) {
    if (!lilvInstance_) {
        return;
    }
    if (active && !instanceActive_) {
        lilv_instance_activate(lilvInstance_);
        instanceActive_ = true;
    } else if (!active && instanceActive_) {
        lilv_instance_deactivate(lilvInstance_);
        instanceActive_ = false;
    }
}

void lv2plugin::push_host_values_to_plugin() {
    for (const control_port_t& cp : handles_.controls) {
        automatable_param_t* p = getEffectParam(static_cast<int32_t>(cp.lilvIndex));
        if (!p) {
            continue;
        }
        const float span = cp.rangeHi - cp.rangeLo;
        write_control(cp.lilvIndex, cp.rangeLo + p->getValue() * span, false);
    }
}

void lv2plugin::bind_audio_ports(AudioBlock* in, AudioBlock* out, int32_t sampleOffset) {
    const size_t nIn = handles_.audioIns.size();
    const int32_t needSamples = scratchSamples_ > 0 ? scratchSamples_ : nominalBlock_;
    if (nIn > 0) {
        if (scratchInputs_.size() < nIn) {
            scratchInputs_.resize(nIn);
        }
        for (size_t c = 0; c < nIn; ++c) {
            if (scratchInputs_[c].size() < static_cast<size_t>(needSamples)) {
                scratchInputs_[c].assign(static_cast<size_t>(needSamples), 0.f);
            }
        }
    }
    if (lastAudioInPtrs_.size() < handles_.audioIns.size()) {
        lastAudioInPtrs_.resize(handles_.audioIns.size(), nullptr);
    }
    for (size_t c = 0; c < handles_.audioIns.size(); ++c) {
        float* buf = nullptr;
        if (in && c < static_cast<size_t>(in->channels) && in->buf[c]) {
            buf = in->buf[c] + sampleOffset;
        } else if (c < scratchInputs_.size()) {
            buf = scratchInputs_[c].data() + sampleOffset;
        }
        if (lastAudioInPtrs_[c] != buf) {
            lilv_instance_connect_port(lilvInstance_, handles_.audioIns[c], buf);
            lastAudioInPtrs_[c] = buf;
        }
    }
    const size_t nOut = handles_.audioOuts.size();
    if (nOut > 0) {
        if (scratchOutputs_.size() < nOut) {
            scratchOutputs_.resize(nOut);
        }
        for (size_t c = 0; c < nOut; ++c) {
            if (scratchOutputs_[c].size() < static_cast<size_t>(needSamples)) {
                scratchOutputs_[c].assign(static_cast<size_t>(needSamples), 0.f);
            }
        }
    }
    if (lastAudioOutPtrs_.size() < handles_.audioOuts.size()) {
        lastAudioOutPtrs_.resize(handles_.audioOuts.size(), nullptr);
    }
    for (size_t c = 0; c < handles_.audioOuts.size(); ++c) {
        float* buf = nullptr;
        if (out && c < static_cast<size_t>(out->channels) && out->buf[c]) {
            buf = out->buf[c] + sampleOffset;
        } else if (c < scratchOutputs_.size()) {
            buf = scratchOutputs_[c].data() + sampleOffset;
        }
        if (lastAudioOutPtrs_[c] != buf) {
            lilv_instance_connect_port(lilvInstance_, handles_.audioOuts[c], buf);
            lastAudioOutPtrs_[c] = buf;
        }
    }
}

void lv2plugin::processMidiMessages(std::vector<IMidiMsg>& midiEvents) {
    pendingMidi_.insert(pendingMidi_.end(), midiEvents.begin(), midiEvents.end());
}

void lv2plugin::process(const DAW::Host::Host* const /*host*/, AudioBlock* in, AudioBlock* out, double /*tick*/, double samplePos, int32_t numSamples, playback_state state) {
    if (numSamples <= 0) {
        return;
    }
    if (!lilvInstance_ || !bIsEnabled) {
        if (out && in && out != in) {
            out->clear();
            out->addFromOp(in, mix_op::ADD, 1.0f);
        }
        pendingMidi_.clear();
        return;
    }

    if (handles_.audioIns.empty() && out) {
        out->clear();
    }

    scratchSamples_ = numSamples;
    connect_atom_ports(static_cast<uint32_t>(numSamples), samplePos, state);
    pendingMidi_.clear();

    if (!instanceActive_) {
        set_instance_active(true);
    }
    push_host_values_to_plugin();
    bind_audio_ports(in, out, 0);
    update_process_block(numSamples);
    lilv_instance_run(lilvInstance_, static_cast<uint32_t>(numSamples));
    drain_worker_requests();
    emit_worker_responses();

    if (latencyControl_) {
        handles_.reportedLatency = static_cast<samplecount_t>(*latencyControl_);
    }
}

void lv2plugin::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    effectbase::postProcess(out, samples, hasProcessed);
}

LV2_Worker_Status lv2plugin::schedule_worker_request(uint32_t size, const void* data) {
    if (!data || size == 0) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    std::lock_guard<std::mutex> lock(workerMutex_);
    workerQueue_.emplace_back(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    return LV2_WORKER_SUCCESS;
}

LV2_Worker_Status lv2plugin::queue_worker_response(uint32_t size, const void* data) {
    if (size > 0 && !data) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    std::lock_guard<std::mutex> lock(workerMutex_);
    workerResponseQueue_.emplace_back(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    return LV2_WORKER_SUCCESS;
}

void lv2plugin::emit_worker_responses() {
    if (!lilvInstance_ || !workerIface_ || !workerIface_->work_response) {
        return;
    }
    std::vector<std::vector<uint8_t>> responses;
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        responses.swap(workerResponseQueue_);
    }
    for (const std::vector<uint8_t>& response : responses) {
        workerIface_->work_response(lilvInstance_->lv2_handle, static_cast<uint32_t>(response.size()), response.data());
    }
}

void lv2plugin::run_worker_ui_message(const char* msg) {
    if (!msg || !msg[0] || !lilvInstance_ || !workerIface_ || !workerIface_->work) {
        return;
    }
    const size_t size = std::strlen(msg) + 1;
    workerIface_->work(
        lilvInstance_->lv2_handle, worker_respond_cb, this, static_cast<uint32_t>(size), msg);
    if (workerIface_->end_run) {
        workerIface_->end_run(lilvInstance_->lv2_handle);
    }
    emit_worker_responses();
}

void lv2plugin::update_process_block(int32_t numSamples) {
    if (numSamples <= 0) {
        return;
    }
    if (numSamples == lastProcessBlock_) {
        return;
    }
    lastProcessBlock_         = numSamples;
    hostParams_.nominal_block = numSamples;
    if (optsIface_) {
        auto& rt = lv2_runtime::get();
        LV2_Options_Option blockOpt[] = {
            { LV2_OPTIONS_INSTANCE, 0, rt.urid_nominal_block(), sizeof(int32_t), rt.urid_int(), &hostParams_.nominal_block },
            { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr }
        };
        optsIface_->set(lilvInstance_->lv2_handle, blockOpt);
    }
    lv2_runtime::get().build_instance_options(instanceOptions_, hostParams_);
}

void lv2plugin::drain_worker_requests() {
    if (!lilvInstance_ || !workerIface_ || !workerIface_->work) {
        return;
    }

    // Rack/Cardinal can enqueue unbounded worker jobs; running them all in one
    // audio cycle starves PortAudio (100ms+ gaps). Cap work per block.
    static constexpr uint32_t kMaxWorkerJobsPerBlock = 2;

    std::unique_lock<std::mutex> lock(workerMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }

    std::vector<std::vector<uint8_t>> localQueue;
    localQueue.swap(workerQueue_);
    lock.unlock();

    uint32_t processed = 0;
    for (size_t i = 0; i < localQueue.size(); ++i) {
        if (processed >= kMaxWorkerJobsPerBlock) {
            std::lock_guard<std::mutex> relock(workerMutex_);
            workerQueue_.insert(workerQueue_.end(),
                                std::make_move_iterator(localQueue.begin() + static_cast<std::ptrdiff_t>(i)),
                                std::make_move_iterator(localQueue.end()));
            break;
        }
        const std::vector<uint8_t>& request = localQueue[i];
        workerIface_->work(
            lilvInstance_->lv2_handle, worker_respond_cb, this, static_cast<uint32_t>(request.size()), request.data());
        ++processed;
    }

    if (processed > 0 && workerIface_->end_run) {
        workerIface_->end_run(lilvInstance_->lv2_handle);
    }
}

void lv2plugin::latency_warmup_run() {
    if (!lilvInstance_ || !latencyControl_) {
        return;
    }

    const bool wasActive = instanceActive_;
    if (!wasActive) {
        lilv_instance_activate(lilvInstance_);
    }

    const uint32_t block = hostParams_.nominal_block > 0 ? static_cast<uint32_t>(hostParams_.nominal_block) : 512u;
    std::vector<float> silence(block, 0.f);
    LilvWorld* w = lv2_catalog::process_world();

    LilvNode* nAudio   = lilv_new_uri(w, LILV_URI_AUDIO_PORT);
    LilvNode* nAtom    = lilv_new_uri(w, LILV_URI_ATOM_PORT);
    LilvNode* nOptConn = lilv_new_uri(w, LV2_CORE__connectionOptional);

    const uint32_t numPorts = lilv_plugin_get_num_ports(lilvDescriptor_);
    for (uint32_t i = 0; i < numPorts; ++i) {
        const LilvPort* port = lilv_plugin_get_port_by_index(lilvDescriptor_, i);
        if (lilv_port_is_a(lilvDescriptor_, port, nAudio)) {
            lilv_instance_connect_port(lilvInstance_, i, silence.data());
        } else if (lilv_port_is_a(lilvDescriptor_, port, nAtom)) {
            for (const atom_port_binding_t& ap : atomPortBindings_) {
                if (ap.lilvIndex == i) {
                    lilv_instance_connect_port(lilvInstance_, i, const_cast<uint8_t*>(ap.buffer.data()));
                    break;
                }
            }
        } else if (lilv_port_has_property(lilvDescriptor_, port, nOptConn)) {
            lilv_instance_connect_port(lilvInstance_, i, nullptr);
        }
        // control ports already connected by connect_all_ports()
    }

    lilv_node_free(nOptConn);
    lilv_node_free(nAtom);
    lilv_node_free(nAudio);

    *latencyControl_ = 0.f;
    lilv_instance_run(lilvInstance_, block);
    drain_worker_requests();
    emit_worker_responses();

    // Restore normal control-port connections (audio/atom ports will be reconnected in process())
    connect_all_ports();

    if (!wasActive) {
        lilv_instance_deactivate(lilvInstance_);
        instanceActive_ = false;
    } else {
        instanceActive_ = true;
    }

    handles_.reportedLatency = static_cast<samplecount_t>(*latencyControl_);
}

samplecount_t lv2plugin::getPluginLatency() {
    return handles_.reportedLatency;
}

String lv2plugin::getInfo(std::vector<String>& list) {
    list.push_back(StringFormat("URI: %s", StringAsCStr(instanceUri_)));
    return sName;
}

std::shared_ptr<guiplugin> lv2plugin::createGuiPlugin(int32_t /*uuid*/) {
    auto gui = std::make_shared<guilv2plugin>(this);
    gui->setTitle(StringFormat("%s (LV2)", StringAsCStr(sName)));
    return gui;
}

bool lv2plugin::showWindow(bool bResetPosition) {
    return openWindow(bResetPosition, lv2_ui_host::default_editor_size(this));
}

bool lv2plugin::hasWindowEditor() {
    if (!lilvDescriptor_) {
        return false;
    }
    const LilvUIs* uis = lilv_plugin_get_uis(lilvDescriptor_);
    return uis && lilv_uis_size(uis) > 0;
}

bool lv2plugin::onShow(host_plugin_window* window) {
    if (!window) {
        return false;
    }
    windowHost = window;
    if (format.sampleRate > 0.f) {
        hostParams_.sample_rate = format.sampleRate;
        instanceSampleRate_     = format.sampleRate;
    }
    String err;
    if (!lv2_ui::open_editor(this, window, err)) {
        log_lf(Log::L_WARN, "LV2 UI for '%s': %s\n", StringAsCStr(sName), StringAsCStr(err));
        return false;
    }
    editorOpen_ = true;
    bEditOpen   = true;
    if (!toplevelUi_) {
        lv2_ui_host::fit_host_window(this, window);
        lv2_ui_host::schedule_ui_fit(this, 5);
        lv2_ui::refresh_editor_after_show(this, window);
    } else if (GLFWwindow* glfw = window->getGlfwWindow()) {
        // Cardinal / DPF showInterface plugins draw in their own toplevel window.
        glfwHideWindow(glfw);
    }
    return true;
}

void lv2plugin::onWindowResize(ivec2 size) {
    effectbase::onWindowResize(size);
    if (editorOpen_ && !toplevelUi_) {
        store_editor_size(size);
        lv2_ui_host::resize_embed(this, size.x, size.y);
    }
}

bool lv2plugin::onClose() {
    // JUCE LV2 UIs (Vitalium, Surge XT) crash if suil_instance_free runs on
    // window close/reopen. Hide only; full teardown happens in closeInstance().
    editorOpen_ = false;
    bEditOpen   = false;
    lv2_ui::hide_editor(this);
    return effectbase::onClose();
}

void lv2plugin::set_editor(void* host, void* ui) {
    suilHost_ = host;
    suilUi_   = ui;
}

void lv2plugin::set_native_ui(void* lib,
                              const LV2UI_Descriptor* desc,
                              LV2UI_Handle handle,
                              const LV2UI_Idle_Interface* idle,
                              const LV2UI_Show_Interface* show,
                              bool toplevelUi) {
    nativeUiLib_    = lib;
    nativeUiDesc_   = desc;
    nativeUiHandle_ = handle;
    nativeUiIdle_   = idle;
    nativeUiShow_   = show;
    toplevelUi_     = toplevelUi;
}

void lv2plugin::clear_native_ui() {
    nativeUiLib_    = nullptr;
    nativeUiDesc_   = nullptr;
    nativeUiHandle_ = nullptr;
    nativeUiIdle_   = nullptr;
    nativeUiShow_   = nullptr;
    toplevelUi_     = false;
}

void lv2plugin::close_editor() {
    if (uiDestroyed_) {
        return;
    }
    if (windowHost != nullptr) {
        closeWindow();
    }
    uiDestroyed_ = true;
    lv2_ui::close_editor(this);
    editorOpen_  = false;
    uiFitFrames_ = 0;
    reset_cached_editor_size();
}

void lv2plugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    effectbase::postSetParameter(idx, preVal, val, flags);
    automatable_param_t* p = getParamUnchecked(idx);
    if (!p || p->internalIdx < 0) {
        return;
    }
    const control_port_t* cp = control_for_lilv_index(static_cast<uint32_t>(p->internalIdx));
    if (!cp) {
        return;
    }
    const float span = cp->rangeHi - cp->rangeLo;
    write_control(cp->lilvIndex, cp->rangeLo + val * span, true);
}

void lv2plugin::onEnable() {
    set_instance_active(true);
}

void lv2plugin::onDisable() {
    set_instance_active(false);
    sendNotesOff();
    pendingMidi_.clear();
}

void lv2plugin::setSampleFormat(sampleformat_t sampleFormat) {
    const bool rateChanged = sampleFormat.sampleRate != format.sampleRate;
    const bool blockChanged = sampleFormat.blockSize != format.blockSize;
    const int32_t prevNominalBlock = nominalBlock_;
    effectbase::setSampleFormat(sampleFormat);
    if (blockChanged && format.blockSize > 0) {
        nominalBlock_ = format.blockSize;
    }
    const bool needsReinstantiate =
        lilvInstance_
        && ((rateChanged && format.sampleRate > 0 && format.sampleRate != instanceSampleRate_)
            || (blockChanged && format.blockSize > 0 && format.blockSize != prevNominalBlock));
    if (needsReinstantiate) {
        const bool wasActive = instanceActive_;
        std::vector<uint8_t> saved;
        save_plugin_state(saved);
        closeInstance();
        if (!openInstance()) {
            return;
        }
        if (!saved.empty()) {
            restore_plugin_state(saved);
        }
        if (wasActive && bIsEnabled) {
            set_instance_active(true);
        }
    }
}

void lv2plugin::on_ui_requested_close() {
    uiCloseRequested_ = true;
}

void lv2plugin::updateFromMainThread() {
    if (uiCloseRequested_) {
        uiCloseRequested_ = false;
        if (editorOpen_) {
            closeWindow();
        }
        return;
    }
    if (!editorOpen_) {
        return;
    }
    if (nativeUiHandle_) {
        lv2_native_x11_ui::idle(this);
    } else if (suilUi_) {
        lv2_ui::idle(this);
    }
#if defined(__linux__)
    if (uiFitFrames_ > 0 && windowHost && !toplevelUi_) {
        lv2_ui_host::fit_host_window(this, windowHost);
        --uiFitFrames_;
    }
#endif
}

bool lv2plugin::save_plugin_state(std::vector<uint8_t>& blob) {
    blob.clear();
    if (!lilvInstance_ || !lilvDescriptor_) {
        return false;
    }
    auto& rt = lv2_runtime::get();
    LilvState* state = lilv_state_new_from_instance(
        lilvDescriptor_,
        lilvInstance_,
        rt.urid_map(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        state_get_port_value,
        this,
        0,
        featureList_.data());
    if (!state) {
        return false;
    }
    char* str = lilv_state_to_string(lv2_catalog::process_world(), rt.urid_map(), rt.urid_unmap(), state, instanceUri_.c_str(), nullptr);
    lilv_state_free(state);
    if (!str) {
        return false;
    }
    const size_t len = std::strlen(str);
    blob.assign(str, str + len);
    lilv_free(str);
    return true;
}

bool lv2plugin::restore_plugin_state(const std::vector<uint8_t>& blob) {
    if (!lilvInstance_ || blob.empty()) {
        return false;
    }
    auto& rt = lv2_runtime::get();
    String stateText(reinterpret_cast<const char*>(blob.data()), blob.size());
    LilvState* state = lilv_state_new_from_string(lv2_catalog::process_world(), rt.urid_map(), StringAsCStr(stateText));
    if (!state) {
        return false;
    }
    const uint32_t flags = 0;
    lilv_state_restore(state, lilvInstance_, state_set_port_value, this, flags, featureList_.data());
    lilv_state_free(state);
    pull_plugin_values_from_instance();
    return true;
}

void lv2plugin::pull_plugin_values_from_instance() {
    for (const control_port_t& cp : handles_.controls) {
        automatable_param_t* p = getEffectParam(static_cast<int32_t>(cp.lilvIndex));
        if (!p) {
            continue;
        }
        const float span = cp.rangeHi - cp.rangeLo;
        const float norm = span > 1e-12f ? (lilvControlBuffer_[cp.lilvIndex] - cp.rangeLo) / span : 0.f;
        setParamValue(p->idx, norm, FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE);
    }
}

void lv2plugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    ps.moduleType      = MODULE_TYPE_LV2;
    ps.uId             = lv2_catalog::fingerprint_uri(instanceUri_.c_str());
    ps.instanceUri     = instanceUri_;
    ps.name            = sName;
    ps.enabled         = bIsEnabled;
    ps.projectGlobalId = projectGlobalId;
    ps.localDbId       = localDbId;
    ps.ioChannels.input  = inputChannelsDesc;
    ps.ioChannels.output = outputChannelsDesc;
    ps.windowLayout      = getWindowLayoutSnapshot();
    if (opts.storePluginPreset) {
        save_plugin_state(ps.dataChunk);
        visitParams([&ps](auto& entry) {
            if (entry.second.inUse) {
                ps.params.push_back(param_snapshot_t{ entry.second.idx, entry.second.getValue(), 1 });
            }
        });
    }
    if (opts.storeAutomation) {
        storeAutomation(ps.automatedParams, this);
    }
}

void lv2plugin::loadSnapshot(const plugin_snapshot_t& snapshot) {
    if (!snapshot.instanceUri.empty()) {
        instanceUri_ = snapshot.instanceUri;
    }
    sName = snapshot.name;
    loadWindowLayoutSnapshot(snapshot.windowLayout);
    if (!snapshot.dataChunk.empty()) {
        restore_plugin_state(snapshot.dataChunk);
    }
    DAW::loadEffectParamsFromSnapshot(snapshot, this);
}

void lv2plugin::load(DAW::Host::PluginManager* host) {
    effectbase::load(host);
    pluginMgr = host;
    if (format.blockSize > 0) {
        nominalBlock_ = format.blockSize;
    }
}

void lv2plugin::unload(DAW::Host::PluginManager* host) {
    close_editor();
    closeInstance();
    effectbase::unload(host);
    windowHost = nullptr;
}
