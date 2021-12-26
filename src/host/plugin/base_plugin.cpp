#include <utility>
#include <vector>
#include "base_plugin.h"
#include "track.h"
#include "track_impl.h"
#include "str_util.h"
#include "logging.h"
#include "snapshot.h"
#include "saferef.h"

#include "gui/gui.h"
#include "meter.h"
#include "gui/guicontainer.h"
#include "gui/guiplugin.h"
#include "gui/button.h"
#include "gui/pluginctr.h"
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
        float val;
    };
    const std::array<effectbase_param_entry_t, 1> parameterTypes{ {
            { PARAM_ENABLE, "Enabled", 1.0f },
    } };
    for (const effectbase_param_entry_t& paramEntry : parameterTypes) {
        automatable_param_t* regparam = registerParam(paramEntry.id);
        regparam->value               = paramEntry.val;
        regparam->label               = paramEntry.name;
        regparam->shortLabel          = paramEntry.name;
    }
    getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
}
effectbase::effectbase() : pluginType(0), projectGlobalId(0) {
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
    setSampleFormat(host->sampleFormat);
    dbgassert(nLoadCalls == 0);
    nLoadCalls++;
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
        meter.setVisible(false);
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
        btnLoad.setFontSize(math::max(12, size.y / 8));
        btnLoad.setText(String("Load\n") + module->getName());
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
void effect_deferred::onPreUnload(int flags) {
    log_printf("onPreUnload effect_deferred %08X %s\n", (int64_t) mImpl, StringAsCStr(mImpl->snapshot.name));
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
String effectbase::formatDisplayValue(int32_t idx) {
    String display = StringFormat("%.3f", getParamValue(idx));
    return display;
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
effect_deferred* effectbase::toDeferred() {
    plugin_snapshot_t snapshot;
    this->makeSnapshot(snapshot, true);
    auto* def   = new effect_deferred();
    def->mImpl             = new effect_deferred_impl();
    def->sName             = snapshot.name;
    def->projectGlobalId   = snapshot.projectGlobalId;
    def->bIsEnabled        = snapshot.enabled;
    def->mImpl->snapshot   = snapshot;
    def->mImpl->moduleType = snapshot.pluginType;
    return def;
}

effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot) {
    auto def               = new effect_deferred();
    def->mImpl             = new effect_deferred_impl();
    def->sName             = snapshot.name;
    def->projectGlobalId   = snapshot.projectGlobalId;
    def->bIsEnabled        = snapshot.enabled;
    def->mImpl->snapshot   = snapshot;
    def->mImpl->moduleType = snapshot.pluginType;
    return def;
}


void effect_deferred::loadSnapshot(const plugin_snapshot_t& snapshot) {
    this->mImpl->snapshot   = snapshot;
    this->mImpl->moduleType = snapshot.pluginType;
}
int32_t effect_deferred::getPluginLatency() {
    return 0;
}
String effect_deferred::getInfo(std::vector<String>& list) {
    return "";
}
int effect_deferred::getModuleType() {
    return PLUGIN_TYPE_DEFERRED;
}
void effect_deferred::makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) {
    ps = this->mImpl->snapshot;
}
void effect_deferred::process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) {
    dbgassert(vstHost->sampleFormat == this->format && in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
}
bool effect_deferred::show() {
    return false;
}
bool effect_deferred::close() {
    return false;
}
void effect_deferred::resume() {
}
void effect_deferred::sleep() {
}
int effect_deferred::getModuleStoredType() const {
    return this->mImpl->moduleType;
}
void effect_deferred::load(vsthost* host) {
    effectbase::load(host);
    this->blockInputs  = new AudioBlock(2, host->sampleFormat.blockSize);
    this->blockOutputs = new AudioBlock(2, host->sampleFormat.blockSize);
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
    btnLoad.setVisible(layoutMode == 0);
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
