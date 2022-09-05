#include <utility>
#include <vector>
#include "base_plugin.h"
#include "host/daw_channel.h"
#include "track.h"
#include "track_impl.h"
#include "str_util.h"
#include "logging.h"
#include "snapshot.h"
#include "saferef.h"

#include "gui/gui.h"
#include "meter.h"
#include "gui/container/container.h"
#include "gui/plugin/plugin.h"
#include "gui/controls/button.h"
#include "gui/plugin/pluginctr.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"

track_t* effectbase::getTrack() {
    audio_stage_t* stage = getTrackLink();
    if (!stage)
        return nullptr;
    return stage->getTrack();
}

SafeRef<effectbase> effectbase::makeSafeRef() {
    if (!safeRef.handler) {
        safeRef.handler = vsthost::getInstance()->getSafeRefStore();
        safeRef.refId   = safeRef.handler->safeRefCreate(this);
    }
    return safeRef;
}
effectbase::~effectbase() {
    assert(nLoadCalls == 0);
    if (safeRef.handler) {
        safeRef.handler->safeRefDestroy(safeRef.refId);
    }
}
effectbase::effectbase(String _sName, int32_t _pluginType, int32_t _projectGlobalId)
    : pluginType(_pluginType), projectGlobalId(_projectGlobalId), sName(std::move(_sName)) {
    struct effectbase_param_entry_t {
        int32_t id;
        String name;
        String unit;
        float val;
    };
    const std::array<effectbase_param_entry_t, 1> parameterTypes{ {
            { PARAM_ENABLE, "Enabled", "", 1.0f },
    } };
    for (const effectbase_param_entry_t& paramEntry : parameterTypes) {
        automatable_param_t* regparam = registerParam(paramEntry.id);

        regparam->defaultValue = paramEntry.val;
        regparam->value = paramEntry.val;
        regparam->name  = paramEntry.name;
        regparam->unit  = paramEntry.unit;
    }
    getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
    initDefaultIODesc();
}
effectbase::effectbase() 
{
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

void effectbase::load(vsthost* host) {
    vstHost = host;
    setSampleFormat(host->m_sampleFormatInternal);
    initBuffers();
    initMeters();
    dbgassert(nLoadCalls == 0);
    nLoadCalls++;
    bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
}
void effectbase::unload(vsthost* host, int flags) {
    dbgassert(host == vstHost);
    vstHost = nullptr;
    dbgassert(nLoadCalls == 1);
    nLoadCalls--;
}

void effectbase::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    meter.update(out, 1.0f);
    meterIn.update(this->blockInputs, 1.0f);
}

void effectbase::processMidi(midi_events_t& midiEvents) {

}

void effectbase::sendNotesOff(int32_t bpm100) {

}

void effectbase::breakTrackLink() {
    trackImpl = nullptr;
}
void effectbase::setTrackLink(audio_stage_t* audioStage) {
    trackImpl = audioStage;
}

class guideferred : public guiplugin {
    effect_deferred* const module;
    guibutton btnLoad;

public:
    explicit guideferred(effect_deferred* _eff) : guiplugin(_eff), module(_eff) {
        isHorizontalTitle        = false;
        buttonBypass.icon        = -1;
        buttonBypass.colorActive = GuiColor::COL_BTN_BG_DEFAULT_ACTIVE;
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

/*static*/ std::shared_ptr<effect_deferred> effect_deferred::fromEffect(effectbase* eff) {
    effect_deferred* effDeferred = eff->toDeferred();
    if (effDeferred) {
        return std::shared_ptr<effect_deferred>(effDeferred);
    }
    return nullptr;
}

void effectbase::updateOnEnableParam(automatable_param_t* param, bool wasEnable, bool isEnable, int flags) {
    this->bIsEnabled = isEnable;
    if (this->bIsEnabled != wasEnable) {
        if (this->bIsEnabled) {
            onEnable();
        } else {
            onDisable();
        }
        if (!(flags & FLG_PAR_UPDATE_NOSTORE) && !(flags & FLG_PAR_UPDATE_AUTOMATED)) {
            param->inUse = true;
        }
    }
}

void effectbase::initBuffers() {
    channelnum_t maxInputChannels = 1;
    for (auto& desc : inputChannelsDesc) {
        maxInputChannels = math::max<channelnum_t>(maxInputChannels, desc.offset + desc.count);
    }
    this->blockInputs         = new AudioBlock(maxInputChannels, format.blockSize);
    channelnum_t maxOutputChannels = 1;
    for (auto& desc : outputChannelsDesc) {
        maxOutputChannels = math::max<channelnum_t>(maxOutputChannels, desc.offset + desc.count);
    }
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
    auto* def               = new effect_deferred();
    def->mImpl              = new effect_deferred_impl();
    def->sName              = snapshot.name;
    def->projectGlobalId    = snapshot.projectGlobalId;
    def->bIsEnabled         = snapshot.enabled;
    def->mImpl->snapshot    = snapshot;
    def->mImpl->moduleType  = snapshot.pluginType;
    def->inputChannelsDesc  = snapshot.ioChannels.input;
    def->outputChannelsDesc = snapshot.ioChannels.output;
    return def;
}

effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot) {
    auto def                = new effect_deferred();
    def->mImpl              = new effect_deferred_impl();
    def->sName              = snapshot.name;
    def->projectGlobalId    = snapshot.projectGlobalId;
    def->bIsEnabled         = snapshot.enabled;
    def->mImpl->snapshot    = snapshot;
    def->mImpl->moduleType  = snapshot.pluginType;
    if (snapshot.version >= 9) {
        def->inputChannelsDesc  = snapshot.ioChannels.input;
        def->outputChannelsDesc = snapshot.ioChannels.output;
    }
    def->setProductName(snapshot.name);
    return def;
}

void effect_deferred::loadSnapshot(const plugin_snapshot_t& snapshot) {
    this->mImpl->snapshot    = snapshot;
    this->mImpl->moduleType  = snapshot.pluginType;
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
int effect_deferred::getModuleType() {
    return PLUGIN_TYPE_DEFERRED;
}
void effect_deferred::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    ps = this->mImpl->snapshot;
}
void effect_deferred::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    dbgassert(vstHost->m_sampleFormatInternal == this->format && in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
}
bool effect_deferred::show(bool bResetPosition) {
    return false;
}
bool effect_deferred::close() {
    return false;
}
int effect_deferred::getModuleStoredType() const {
    return this->mImpl->moduleType;
}
String effect_deferred::getAutomatableName() {
    return "plugin";
}
float effect_deferred::getParamValue(int32_t idx) {
    return 0;
}
void effect_deferred::setParamValue(int32_t idx, float val, int flags) {
}
automationlane_snapshot_t effect_deferred::toRef() const {
    automationlane_snapshot_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}
void guideferred::render(NVGcontext* vg) {
    // btnLoad.setVisible(layoutMode == 0);
    guiplugin::render(vg);
}


void guideferred::buttonClicked(guibase* _button) {
    guiplugin::buttonClicked(_button);
    if (_button == &btnLoad) {
        ThreadLock lock  = MainCtrl::getPlayThread()->lockThread();
        vsthost* host    = vsthost::getInstance();
        auto dawCtrlCopy = dawCtrl;
        host->activateDeferred(module, vsthost::FLAG_HOST_FORCELOAD_DISABLED_PLUGINS);
        // do not access this from here to function exit
        dawCtrlCopy->onPluginsChanged();
    }
}
guiplugin* effect_deferred::makeGui() {
    dbgassert(this->mImpl);
    if (!this->mImpl->gui) {
        this->mImpl->gui = std::make_unique<guideferred>(this);
        this->mImpl->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
    }
    return this->mImpl->gui.get();
}
guiplugin* effect_deferred::getGui() {
    dbgassert(this->mImpl);
    dbgassert(this->mImpl->gui.get());
    return this->mImpl->gui.get();
}
