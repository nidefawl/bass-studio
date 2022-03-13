#include <vector>
#include <memory>

#include "group.h"
#include "math/seq_math.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "guicolors.h"
#include "renderresources.h"
#include "keyboard.h"
#include "gui/list.h"
#include "gui/guimeter.h"
#include "gui/guicontainer.h"
#include "gui/guiplugin.h"
#include "gui/pluginctr.h"
#include "gui/pluginlist.h"

#include "base_plugin.h"
#include "internal_plugin.h"

#include "basectrl.h"
#include "audioblock.h"
#include "meter.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"

#include "track.h"
#include "track_impl.h"
#include "snapshot.h"
#include "host/effect_graph.h"
#include "seq_util.h"

class guimodule_group : public guiplugin {
public:
    module_group* const module;
    guictr_plugins ctr;
    explicit guimodule_group(module_group* _vst);
    ~guimodule_group() override {
        remove(&ctr);
    }
    void render(NVGcontext* vg) override;
    void buttonClicked(guibase* _button) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void onChildLayoutChanged(guibase* g) override;
    void determineSize(ivec2& prefSize) override {
        dbgassert(module->getAudioStage());

        const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
        int32_t meterW    = math::max(16, (int32_t) (theme->get(GuiConstant::CONST_METER_WIDTH) * hpt / 32.0));
        ctr.pos           = ivec2(hpt, 0);

        if (layoutMode == 0) {
            ctr.size = { prefSize.y, prefSize.y };
            ctr.layout();
        } else {
            ctr.size = { 0, prefSize.y };
        }
        prefSize.x = hpt + ctr.size.x + meterW;
    }
    void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
    }
    void removeGuis() override {
        removeUNCHECKED(&ctr);
        for (guibase* g : guis) {
            g->onRemove();
            g->setParent(nullptr);
        }
        guis.clear();
        addUNCHECKED(&ctr);
    }
    void onAdded() override {
        ctr.showTrack(module->getAudioStage());
    }
};

guimodule_group::guimodule_group(module_group* _vst)
    : guiplugin(_vst),
      module(_vst) {
    isHorizontalTitle      = false;
    ctr.isDefaultPluginCtr = false;
    ctr.margin = ctr.padding = 0;
    add(&ctr);
}
void guimodule_group::onChildLayoutChanged(guibase* g) {
    layout();
    if (this->parent != nullptr) {
        this->parent->onChildLayoutChanged(this);
    }
}

void guimodule_group::render(NVGcontext* vg) {
    dbgassert(ctr.parent == this);
    dragdrop_target_indicator_t& target = dawCtrl->getDragDropTarget();
    bool extend                         = target.src == &this->ctr;
    int extX                            = 8;
    if (extend) {
        size.x += extX;
    }
    renderBase(vg);
    if (layoutMode == 0) {
        nvgSave(vg);
        ctr.render(vg);
        nvgRestore(vg);
    }
    for (auto* btn : guiButtonsTitlebar) {
        if (btn->isVisible())
            btn->render(vg);
    }
    if (extend) {
        nvgTranslate(vg, extX, 0);
    }

    guiMeter.render(vg);
    if (extend) {
        size.x -= extX;
    }
}
bool guimodule_group::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (contains(mpos)) {
        if (evt.getDraggedThing() == this)
            return false;
        ivec2 localMouse = this->toContainerSpace(mpos);
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
            if (layoutMode != 0) {
                return false;
            }
            if (ctr.mouseHitTest(localMouse, evt)) {
                return true;
            }
            const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
            if (localMouse.x <= hpt - 10 || localMouse.x > size.x - hpt + 10)
                return false;
            evt.requestFocus(&this->ctr);
            return true;
        }
        for (guibase* gui : guis) {
            if (!gui->isVisible())
                continue;
            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (ctr.mouseHitTest(localMouse, evt)) {
            return true;
        }
        if (isShift(evt.kbmods)) {
            if (MainCtrl::get()->getPluginSel().pluginCtr != this->parent) {
                return true;
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}
void guimodule_group::buttonClicked(guibase* _button) {
    if (_button == &buttonLayout) {
        layoutMode        = (layoutMode + 1) % 2;
        buttonLayout.icon = layoutMode == 0 ? ICON_ARR_RIGHT : ICON_ARR_DOWN;
        parent->onChildLayoutChanged(this);
        return;
    }
    guiplugin::buttonClicked(_button);
}


struct module_group::internal_handles_t {
    std::unique_ptr<guimodule_group> gui;
};

module_group::module_group(int32_t _projectGlobalId)
    : internalplugin("Group", PLUGIN_TYPE_GROUP, _projectGlobalId),
      handle(new module_group::internal_handles_t{ nullptr }),
      audio(nullptr)
{
#define PARAM_GROUPPLUGIN_INPUT_GAIN PARAM_OFFSET_IMPL
    struct effectgroup_param_entry_t {
        int32_t id;
        String name;
        String unit;
        float val;
    };
    const std::array<effectgroup_param_entry_t, 3> parameterTypes{ {
            { PARAM_GAIN, "Output Gain", "dB", 1.0f },
            { PARAM_PAN, "Pan", "", 0.5f },
            { PARAM_GROUPPLUGIN_INPUT_GAIN, "Input Gain", "dB", 1.0f },
    } };
    for (const effectgroup_param_entry_t& paramEntry : parameterTypes) {
        automatable_param_t* regparam = registerParam(paramEntry.id);

        regparam->value = paramEntry.val;
        regparam->name  = paramEntry.name;
        regparam->unit  = paramEntry.unit;
    }
}

module_group::~module_group() {
    delete handle;
    delete blockInputs;
    delete blockOutputs;
}

float module_group::dispatchGetParameter(int32_t idx) {
    return 0;
}

void module_group::dispatchSetParameter(int32_t idx, float val) {
}

guiplugin* module_group::makeGui() {
    if (!handle->gui) {
        dbgassert(this->audio);
        handle->gui = std::make_unique<guimodule_group>(this);
        handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
        this->audio->m_pluginCtr = &this->handle->gui->ctr;
        handle->gui->ctr.stage = this->audio;
        handle->gui->ctr.track = this->audio->getTrack();
    }
    return handle->gui.get();
    //return handle->gui;
}

guiplugin* module_group::getGui() {
    return handle->gui.get();
    //return handle->gui;
}

int32_t module_group::getPluginLatency() {
    return static_cast<int32_t>(audio->getInternalLatency());
}

void module_group::onEnable() {
}

void module_group::onDisable() {
}

void module_group::onPreUnload(int flags) {
    dbgassert(this->audio);
    if (this->audio->m_pluginCtr == &this->handle->gui->ctr) {
        this->handle->gui->ctr.showTrack(nullptr);
        this->audio->m_pluginCtr = nullptr;
    }
    std::vector<effectbase*> effects = this->audio->effects;// make a copy before unloading plugins
    for (effectbase* effect : effects) {
        vstHost->unloadPlugin(effect, flags);
    }
}

void module_group::load(vsthost* host) {
    effectbase::load(host);
    this->audio = host->createAudioStage();
}

void module_group::unload(vsthost* host, int flags) {
    effectbase::unload(host, flags);
    //onPreunload(flags);
    host->releaseAudioStage(audio);
    this->audio = nullptr;
}

void module_group::breakTrackLink() {
    dbgassert(this->audio);
    dbgassert(this->audio->parent);
    this->audio->parent->removeAudioStage(this->audio);
    dbgassert(this->audio->parent == nullptr);
    this->audio->owner = nullptr;
    internalplugin::breakTrackLink();
}

void module_group::setTrackLink(audio_stage_t* trImpl) {
    dbgassert(this->audio);
    dbgassert(trImpl != this->audio);
    trImpl->addAudioStage(this->audio);
    dbgassert(this->audio->parent == trImpl);
    this->audio->owner = this;
    internalplugin::setTrackLink(trImpl);
}

String module_group::getInfo(std::vector<String>& list) {
    return "";
}

void module_group::onTick(double since) {
    meter.onTick(since);
    meterIn.onTick(since);
    audio->onTick(since);
}

void module_group::getChildAudioStages(std::vector<audio_stage_t*>& targets) {
    targets.push_back(this->audio);
}

void module_group::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    dbgassert(getTrackLink()->sampleFormat == this->format && in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
    audio->input.copyFrom(in);

    std::shared_ptr<DAW::effect_processing_graph_t> effProcessingGraph;
    if (!DAW::buildEffectProcessingGraph(vstHost, nullptr, audio, effProcessingGraph)) {
        log_printf("Failed building effect graph\n");
    }

    vstHost->processAudio(audio, &audio->input, &audio->output, tick, samplePos, numSamples, state, effProcessingGraph.get());

#ifdef DAW_DEBUG_TRACK_GRAPHS
    //TODO: this code path runs on a workerthread. Store processing-graph add to vsthost::lastProcessingGraphs from playback-thread
#endif

    audio->outputPost.clear();
    /* Calculate group gain level */
    float fGain;
    if (dsp_util::getGainLvl(getParamValue(PARAM_GAIN), fGain)) {
        audio->outputPost.addFromOp(&audio->output, AudioBlock::mix_op::ADD, math::clamp(fGain, 0.0f, 1.0f));
    }
    out->copyFrom(&audio->outputPost);
}

void module_group::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    //float fGainGroup;
    //dsp_util::getGainLvl(audio->mixer.getParamValue(PARAM_TRACK_GAIN), fGainGroup);
    meterIn.update(this->blockInputs, 1.0f);
    meter.update(out, 1.0f);
    if (!hasProcessed) {
        for (effectbase* effect : audio->effects) {
            effect->postProcess(out, samples, hasProcessed);
        }
    }
}

void module_group::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
    dbgassert(audio);
    audio->loadPlugins(pluginSnapshot.pluginSnapshots);
    audio->pluginsChanged();
}

void module_group::getDeferredEffects(std::vector<effectbase*>& targets) {
    audio->getDeferredEffects(targets);
}

void module_group::makeSnapshot(plugin_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
    dbgassert(audio);
    internalplugin::makeSnapshot(snapshot, opts);
    std::vector<effectbase*> effects = audio->effects;
    snapshot.pluginSnapshots.reserve(effects.size());
    for (effectbase* effect : effects) {
        plugin_snapshot_t ps;
        effect->makeSnapshot(ps, opts);
        snapshot.pluginSnapshots.push_back(std::move(ps));
    }
}

template<>
effectbase* makeInstance<module_group>(int32_t _projectGlobalId) {
    return new module_group(_projectGlobalId);
}
