#include <utility>
#include <vector>
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "host/plugin/base/base-plugin.h"
#include "host/daw_channel.h"
#include "host/plugin/modules.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "str_util.h"
#include "logging.h"
#include "snapshot/snapshot.h"
#include "saferef.h"

#include "gui/gui.h"
#include "host/meter/meter.h"
#include "gui/container/container.h"
#include "gui/plugin/plugin.h"
#include "gui/controls/button.h"
#include "gui/plugin/pluginctr.h"
#include "host/daw/mainctrl.h"
#include "host/host_pluginmanager.h"
#include "host/host_plugin_window.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/track/track.h"
#include "gui/plugin/pluginctr.h"
#include "host/daw/mainctrl.h"
#include "host/daw/history.h"
#include "host/host_plugin_window.h"


bool saveHostWindowPos(host_plugin_window* hostWindow, appwindow_size_t* size);
bool restoreHostWindowPos(host_plugin_window* hostWindow, appwindow_size_t* size);


track_t* effectbase::getTrack() {
    audio_stage_t* stage = getTrackLink();
    if (!stage)
        return nullptr;
    return stage->getTrack();
}

SafeRef<effectbase> effectbase::makeSafeRef() {
    if (!safeRef.handler) {
        safeRef.handler = pluginMgr->getSafeRefStore();
        safeRef.refId   = safeRef.handler->safeRefCreate(this);
    }
    return safeRef;
}

effectbase::~effectbase() {
    delete blockInputs;
    delete blockOutputs;
    dbgassert(nLoadCalls == 0);
    if (safeRef.handler) {
        safeRef.handler->safeRefDestroy(safeRef.refId);
    }
}

effectbase::effectbase(String _sName, int32_t _projectGlobalId, IHostCallback* _hostCallback)
    : projectGlobalId(_projectGlobalId), hostCallback(_hostCallback), sName(std::move(_sName)) {
    struct effectbase_param_entry_t {
        int32_t id;
        String name;
        String unit;
        float val;
    };
    const std::array<effectbase_param_entry_t, 1> parameterTypes{ {
            { PARAM_ENABLE, "Enabled", "", 1.0f },
    } };
    for (const auto& paramEntry : parameterTypes) {
        registerParam(paramEntry.id)->initValue(paramEntry);
    }
    getParam(PARAM_ENABLE)->quantizationSteps = 1;
    initDefaultIODesc();
}

void effectbase::initDefaultIODesc() {
    inputChannelsDesc.emplace_back(DAW::channel_desc{0, 2, String("Stereo Input")});
    outputChannelsDesc.emplace_back(DAW::channel_desc{0, 2, String("Stereo Output")});
}

void effectbase::onTick(double since) {
    meter.onTick(since);
    meterIn.onTick(since);
}

sampleformat_t effectbase::getSampleFormat() {
    return format;
}

void effectbase::load(DAW::Host::PluginManager* host) {
    pluginMgr = host;
    if (assert_expr(hostCallback))
        setSampleFormat(hostCallback->m_sampleFormatInternal);
    initBuffers();
    initMeters();
    dbgassert(nLoadCalls == 0);
    nLoadCalls++;
    bIsEnabled = getParam(PARAM_ENABLE)->getValue() > 0;
}

void effectbase::unload(DAW::Host::PluginManager* host) {   
    dbgassert(host == pluginMgr);
    pluginMgr = nullptr;
    dbgassert(nLoadCalls == 1);
    nLoadCalls--;
    if (this->windowHost) {
        this->windowHost->close();
    }
    if (this->windowHost) {
        this->windowHost->destroy();
    }
}

void effectbase::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    meter.update(out, 1.0f);
    meterIn.update(this->blockInputs, 1.0f);
}

void effectbase::sendNotesOff() {
    std::vector<IMidiMsg> messages;
    messages.reserve(heldNotes.size() + 1);
    for (const auto& notePitch : heldNotes) {
        auto deltaFrames = 0;
        messages.emplace_back();
        IMidiMsg& msg = messages.back();
        msg.MakeNoteOffMsg(notePitch, deltaFrames);
    }
    messages.emplace_back(0, 0xB0, 123, 0);// InstantOff
    heldNotes.clear();
    processMidiMessages(messages);
    this->midiEventsDispatched += CtrSize(messages);
}

void effectbase::processMidi(midi_data_processing_t& midiEvents) {
    const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, midiEvents.bpm100, format.sampleRate);
    std::vector<IMidiMsg> messages; // TODO: get rid of heap allocation
    messages.reserve(midiEvents.noteEvents->size());
    for (auto& evt : *midiEvents.noteEvents) {
        auto deltaFrames = math::floordS32(evt.tickOffsetInBlock * tickToSamples);
        dbgassert(deltaFrames >= 0 && deltaFrames < format.blockSize);
        bool bContained = std::binary_search(std::begin(heldNotes), std::end(heldNotes), evt.pitch);
        if (evt.isNoteOn && !bContained) {
            insertSorted(heldNotes, evt.pitch);
        } else if (!evt.isNoteOn && bContained) {
            removeEntry(heldNotes, evt.pitch);
        }

        messages.emplace_back();
        IMidiMsg& msg = messages.back();
        if (evt.isNoteOn) {
            msg.MakeNoteOnMsg(evt.pitch, evt.velocity, deltaFrames, evt.channel);
        } else {
            msg.MakeNoteOffMsg(evt.pitch, deltaFrames, evt.channel);
        }
        msg.note = evt.note;
    }
    for (auto& evt : *midiEvents.ctrlEvents) {
        auto offsetInBlock = math::floordS32((evt.tick - midiEvents.tickLatencyCompensated) * tickToSamples);
        if (offsetInBlock < 0 || offsetInBlock >= format.blockSize) {
            log_lf(Log::L_WARN, "ctrl event out of range: %d\n", offsetInBlock);
            continue;
        }
        messages.push_back(IMidiMsg::FromU32AndTick(evt.message, offsetInBlock));
    }
    if (!messages.empty()) {
        std::sort(std::begin(messages), std::end(messages), [](const IMidiMsg& a, const IMidiMsg& b) {
            return a.mOffset < b.mOffset;
        });
    }
    processMidiMessages(messages);
    this->midiEventsDispatched += CtrSize(messages);
}

void effectbase::setTrackLink(audio_stage_t* audioStage) {
    trackImpl = audioStage;
}
bool effectbase::showWindow(bool bResetPosition) {
    return openWindow(bResetPosition, {});
}
bool effectbase::openWindow(bool bResetPosition, ivec2 defaultSize) {
    ivec4 posSize{0, 0, 0, 0};
    auto plugWindowSize = defaultSize;
    ivec2 size = { 160, 120 };
    if (plugWindowSize.x > 0 && plugWindowSize.y > 0) {
        size = plugWindowSize;
    }
    if (bSupportsWindowResize && bWindowPosSizeValid && !bResetPosition) {
        size.x = this->lastWindowPosSize.z;
        size.y = this->lastWindowPosSize.w;
    }
    bool bSetPos = false;
    bool bSetSize = true;
    posSize = { 0, 0, size.x, size.y };
    if (bWindowPosSizeValid && !bResetPosition) {
        bSetPos = true;
        posSize.x = this->lastWindowPosSize.x;
        posSize.y = this->lastWindowPosSize.y;
    }
    if (bResetPosition) {
        this->windowSize = {};
    }
    if (this->windowHost == nullptr && (hasWindowEditor())) {
        this->windowHost = host_plugin_window::make(this, this->sName, size, bSupportsWindowResize);
    }
    if (this->windowHost != nullptr) {
        this->windowHost->show(posSize, bSetPos, bSetSize && bSupportsWindowResize && bWindowPosSizeValid);
    }
    return false;
}

bool effectbase::closeWindow() {
    if (this->windowHost != nullptr) {
        this->windowHost->close();
    }
    return true;
}


bool effectbase::onShow(host_plugin_window* _window) {
    if (this->windowHost == _window) {
        bEditOpen = true;
        restoreHostWindowPos(_window, &windowSize);
    }
    return true;
}

void effectbase::updateFromMainThread() {
    if (this->windowHost != nullptr) {
        this->windowHost->updateFromMainThread();
    }
}

bool effectbase::onClose() {
    saveHostWindowPos(this->windowHost, &windowSize);
    bEditOpen = false;
    return true;
}

void effectbase::onWindowDestroy() {
    this->windowHost = nullptr;
}

void effectbase::onWindowResize(ivec2 size) {
}

std::shared_ptr<guiplugin> effectbase::getPluginGui(int32_t uuid) {
    if (!uiInstances.count(uuid)) {
        std::shared_ptr<guiplugin> newGui = createGuiPlugin(uuid);
        auto it = uiSnapshots.find(uuid);
        if (it != uiSnapshots.end()) {
            newGui->loadSnapshot(it->second);
            it->second.isValidSnapshot = false;
        }
        uiInstances[uuid] = std::move(newGui);
    }
    return uiInstances[uuid];
}

plugin_windowlayout_snapshot_t effectbase::getWindowLayoutSnapshot() {
    if (this->windowHost) {
        this->windowHost->storePosition();
        saveHostWindowPos(this->windowHost, &windowSize);
    }
    plugin_windowlayout_snapshot_t snapshot;
    snapshot.isValidSnapshot = true;
    snapshot.windowPosSizeValid = this->bWindowPosSizeValid;
    snapshot.windowPosSize = this->lastWindowPosSize;
    snapshot.isWindowOpen = this->bEditOpen;
    snapshot.windowSize = this->windowSize;
    return snapshot;
}

void effectbase::loadWindowLayoutSnapshot(const plugin_windowlayout_snapshot_t& snapshot) {
    if (snapshot.isValidSnapshot) {
        this->bWindowPosSizeValid = snapshot.windowPosSizeValid;
        this->lastWindowPosSize = snapshot.windowPosSize;
        this->bOpenWindowOnEnable = snapshot.isWindowOpen;
        this->windowSize = snapshot.windowSize;
    }
}

class guideferred final : public guiplugin {
    effect_deferred* const module;
    guibutton btnLoad;

public:
    explicit guideferred(effect_deferred* _eff) : guiplugin(_eff), module(_eff) {
        isHorizontalTitle        = false;
        buttonBypass.icon        = -1;
        guiMeter.setVisible(false);
        add(&btnLoad);
    }
    ~guideferred() override {
        remove(&btnLoad);
    }
    void render(NVGcontext* vg) override;
    void buttonClicked(guibase* _button) override;
    void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
        btnLoad.pos  = pos;
        btnLoad.size = contentS;
        btnLoad.layout();
        btnLoad.setText(String("Load\n") + module->getName());
    }
    void setLayoutMode(int32_t layoutMode) override {
        this->layoutMode = layoutMode;
        guiMeter.setVisible(layoutMode == 0);
        isHorizontalTitle = false;
        buttonLayout.icon = layoutMode == 0 ? ICON_ARR_RIGHT : ICON_ARR_DOWN;
        btnLoad.setVisible(this->layoutMode == 0);
    }
};

struct effect_deferred_impl {
    plugin_snapshot_t snapshot;
    std::unique_ptr<guideferred> gui = nullptr;
    int moduleType                   = 0;
};

effect_deferred::~effect_deferred() {
    delete mImpl;
}

String effect_deferred::getDfrdPluginName() const {
    return mImpl->snapshot.name;
}

const plugin_snapshot_t& effect_deferred::getSnapshotConst() const {
    return mImpl->snapshot;
}

plugin_snapshot_t& effect_deferred::getSnapshot() {
    return mImpl->snapshot;
}

void guideferred::render(NVGcontext* vg) {
    text = module->mImpl->snapshot.name + " (Unloaded)";
    guiplugin::render(vg);
}

/*static*/ std::shared_ptr<effect_deferred> effect_deferred::fromEffect(effectbase* eff) {
    effect_deferred* effDeferred = eff->toDeferred();
    if (effDeferred) {
        return std::shared_ptr<effect_deferred>(effDeferred);
    }
    return nullptr;
}

void effectbase::updateOnEnableParam(bool wasEnable, bool isEnable, int flags) {
    this->bIsEnabled = isEnable;
    if (this->bIsEnabled != wasEnable) {
        if (this->bIsEnabled) {
            onEnable();
        } else {
            onDisable();
        }
    }
}

void effectbase::initBuffers() {
    channelnum_t maxInputChannels = 1;
    for (auto& desc : inputChannelsDesc) {
        maxInputChannels = math::max<channelnum_t>(maxInputChannels, desc.offset + desc.count);
    }
    delete this->blockInputs;
    this->blockInputs         = new AudioBlock(maxInputChannels, format.blockSize);
    channelnum_t maxOutputChannels = 1;
    for (auto& desc : outputChannelsDesc) {
        maxOutputChannels = math::max<channelnum_t>(maxOutputChannels, desc.offset + desc.count);
    }
    delete this->blockOutputs;
    this->blockOutputs = new AudioBlock(maxOutputChannels, format.blockSize);
}

void effectbase::initMeters() {
    meterDataOutput = std::shared_ptr<DAW::meter_runningsum[]>(new DAW::meter_runningsum[blockOutputs->channels]);
    meterDataInput = std::shared_ptr<DAW::meter_runningsum[]>(new DAW::meter_runningsum[blockInputs->channels]);
    meter = DAW::rmsmeter(&meterDataOutput[0], blockOutputs->channels);
    meterIn = DAW::rmsmeter(&meterDataInput[0], blockInputs->channels);
}

effect_deferred* effectbase::toDeferred() {
    plugin_snapshot_t snapshot;
    this->makeSnapshot(snapshot, tracksnapshot_store_opts_t::All());
    auto* def               = new effect_deferred(snapshot.projectGlobalId, this->getHostCallback());
    def->mImpl              = new effect_deferred_impl();
    def->sName              = snapshot.name;
    def->projectGlobalId    = snapshot.projectGlobalId;
    def->bIsEnabled         = snapshot.enabled;
    def->mImpl->snapshot    = snapshot;
    def->mImpl->moduleType  = snapshot.moduleType;
    def->inputChannelsDesc  = snapshot.ioChannels.input;
    def->outputChannelsDesc = snapshot.ioChannels.output;
    return def;
}

namespace DAW::Host {

effect_deferred* PluginManager::loadPluginDeferred(const plugin_snapshot_t& snapshot) {
    auto def                = new effect_deferred(snapshot.projectGlobalId, getHostCallback());
    def->mImpl              = new effect_deferred_impl();
    def->sName              = snapshot.name;
    def->projectGlobalId    = snapshot.projectGlobalId;
    def->bIsEnabled         = snapshot.enabled;
    def->mImpl->snapshot    = snapshot;
    def->mImpl->moduleType  = snapshot.moduleType;
    if (snapshot.version >= 9) {
        def->inputChannelsDesc  = snapshot.ioChannels.input;
        def->outputChannelsDesc = snapshot.ioChannels.output;
    }
    def->setProductName(snapshot.name);
    return def;
}

} // namespace DAW::Host

effect_deferred::effect_deferred(int32_t _projectGlobalId, IHostCallback* _hostCallback) 
: effectbase("Deferred", _projectGlobalId, _hostCallback)
{
    initDefaultIODesc();
}

void effect_deferred::loadSnapshot(const plugin_snapshot_t& snapshot) {
    this->mImpl->snapshot    = snapshot;
    this->mImpl->moduleType  = snapshot.moduleType;
    if (snapshot.version >= 9) {
        this->inputChannelsDesc  = snapshot.ioChannels.input;
        this->outputChannelsDesc = snapshot.ioChannels.output;
    }
}

samplecount_t effect_deferred::getPluginLatency() {
    return 0;
}

String effect_deferred::getInfo(std::vector<String>& list) {
    return "";
}

ModuleType effect_deferred::getModuleType() {
    return MODULE_TYPE_DEFERRED;
}

void effect_deferred::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    ps = this->mImpl->snapshot;
}

void effect_deferred::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
}

int effect_deferred::getModuleStoredType() const {
    return this->mImpl->moduleType;
}

String effect_deferred::getAutomatableName() {
    return getDfrdPluginName() + " (Unloaded)";
}

automatable_param_ref_t effect_deferred::toRef() const {
    automatable_param_ref_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}

void guideferred::buttonClicked(guibase* _button) {
    guiplugin::buttonClicked(_button);
    if (_button == &btnLoad) {
        auto daw = dawCtrl->getDaw();
        daw->closeContextMenus();
        auto lock = daw->lockPlayThread();
        auto pluginMgr = daw->getPluginManager();
        auto moduleHandle = module;
        pluginMgr->activateDeferred(moduleHandle, DAW::Host::PluginManager::FLAG_HOST_FORCELOAD_DISABLED_PLUGINS);
        daw->onPluginsChanged();
    }
}

std::shared_ptr<guiplugin> effect_deferred::createGuiPlugin(int32_t uuid) {
    auto gui = std::make_unique<guideferred>(this);
    gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
    return gui;
}

void effectbase::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    dbgassert(!fp_math::isNanOrInfd(val));
    if (!(flags & FLG_PAR_UPDATE_MODULATED)) {
        if (idx == PARAM_ENABLE) {
            bool wasEnable = this->bIsEnabled;
            bool isEnabled = val > 0;
            updateOnEnableParam(wasEnable, isEnabled, flags);
        }
    }
    if (flags & FLG_PAR_UPDATE_FINISH) {
        track_t* track = this->trackImpl ?  this->trackImpl->getTrack() : nullptr;
        if (track) {
            automatable_param_ref_t ref = toRef();
            parameter_ref_t p           = { track->projectIdx, ref.type, this->projectGlobalId, idx };
            DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
        }
    }
    
    for (auto& pviewctr : this->views) {
        if (pviewctr->isInUse()) {
            pviewctr->onSetParameter(idx, val);
        }
    }
}

automatable_param_ref_t effectbase::toRef() const {
    automatable_param_ref_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}