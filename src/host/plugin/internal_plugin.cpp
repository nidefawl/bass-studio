#include <algorithm>
#include <utility>
#include "assert_dbg.h"
#include "automation.h"
#include "modules.h"
#include "seq_util.h"

#include "snapshot.h"
#include "base_plugin.h"
#include "internal_plugin.h"
#include "track.h"
#include "gui/plugin/pluginctr.h"
#include "host/mainctrl.h"
#include "host/history.h"
#include "host/host_plugin_window.h"

namespace {
    void createSnapshot(plugin_snapshot_t& ps, internalplugin* plugin, const tracksnapshot_store_opts_t& opts) {
        ps.version           = 11;
        ps.slot              = 0;
        ps.projectGlobalId   = plugin->projectGlobalId;
        ps.enabled           = plugin->bIsEnabled;
        ps.ioChannels.input  = plugin->inputChannelsDesc;
        ps.ioChannels.output = plugin->outputChannelsDesc;
        ps.uId               = plugin->uId;
        ps.pluginType        = plugin->pluginType;
        ps.name              = plugin->sName;
        if (opts.storePluginPreset) {
            ps.params.reserve(plugin->getNumParameters());
            plugin->visitParams([&ps](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                if (param.inUse) {
                    dbgassert(param.value >= 0.0f && param.value <= 1.0f);
                    ps.params.push_back(param_snapshot_t{ param.idx, param.value, param.inUse ? 1 : 0 });
                }
            });
        }
        if (opts.storeAutomation) {
            storeAutomation(ps.automatedParams, plugin);
        }
    }
}// namespace

void internalplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    createSnapshot(ps, this, opts);
    if (handlesIntPlugin->gui) {
        handlesIntPlugin->gui->makeSnapshot(ps.uiSnapshot, opts);
    }
    ps.slot = this->slot;
    auto dataBuf = storePresetData();
    if (dataBuf) {
        // can't use std::vector value type copy here
        ps.dataChunk.resize(dataBuf->size());
        memcpy(ps.dataChunk.data(), dataBuf->data(), dataBuf->size());
    }
}

void internalplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
    this->uiSnapshot = pluginSnapshot.uiSnapshot;
    this->uiSnapshot.isValidSnapshot = true;
    DAW::loadEffectParamsFromSnapshot(pluginSnapshot, this);
    if (handlesIntPlugin->gui && this->uiSnapshot.isValidSnapshot) {
        handlesIntPlugin->gui->loadSnapshot(this->uiSnapshot);
        this->uiSnapshot.isValidSnapshot = false;
    }
    if (pluginSnapshot.dataChunk.size() > 0) {
        auto dataBuf = std::make_shared<std::vector<std::byte>>();
        dataBuf->resize(pluginSnapshot.dataChunk.size());
        memcpy(dataBuf->data(), pluginSnapshot.dataChunk.data(), dataBuf->size());
        loadPresetData(dataBuf);
    }
}

String internalplugin::getAutomatableName() {
    return this->sName;
}

float internalplugin::getParamValue(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    return param->value;
}

void internalplugin::setParamValue(int32_t idx, float val, int flags) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    float valPre = param->value;
    param->value = val;
    if (param->idx == PARAM_ENABLE) {
        bool wasEnable = this->bIsEnabled;
        bool isEnabled = val > 0;
        updateOnEnableParam(param, wasEnable, isEnabled, flags);
    } else {
        if ((flags & (FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE | FLG_PAR_UPDATE_AUTOMATED)) == 0) {
            param->inUse = true;
        }
        postSetParameter(param->idx, valPre, val, flags);
        for (auto& pviewctr : this->views) {
            if (pviewctr->isInUse()) {
                pviewctr->onSetParameter(idx, val);
            }
        }
    }
}

void internalplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    if (flags & FLG_PAR_UPDATE_FINISH) {
        track_t* track = this->trackImpl ?  this->trackImpl->getTrack() : nullptr;
        if (track) {
            automationlane_snapshot_t ref = toRef();
            parameter_ref_t p             = { track->projectIdx, ref.type, this->projectGlobalId, idx };
            DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
        }
        return;
    }
    
    for (auto& pviewctr : this->views) {
        if (pviewctr->isInUse()) {
            pviewctr->onSetParameter(idx, val);
        }
    }
}

void internalplugin::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    meterIn.update(this->blockInputs, 1.0f);
    meter.update(out, 1.0f);
}
automationlane_snapshot_t internalplugin::toRef() const {
    automationlane_snapshot_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}

internalplugin::internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId, i_host_callback* _hostCallback)
    : effectbase(std::move(_sName), _pluginType, _projectGlobalId, _hostCallback),
      handlesIntPlugin(new internalplugin_handles_t{}) {
    bSupportsWindowResize = true;
}

internalplugin::~internalplugin() {
    delete handlesIntPlugin;
}

guiplugin* internalplugin::makeGui() {
    if (!handlesIntPlugin->gui) {
        handlesIntPlugin->gui = std::make_unique<guiinternalpluginview>(this);
        handlesIntPlugin->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
    }
    return handlesIntPlugin->gui.get();
}

guiplugin* internalplugin::getGui() {
    return handlesIntPlugin->gui.get();
}

window_plugin* createBuildinPluginWindow(std::shared_ptr<PluginControl> _ctrl, int w, int h, void* hostWindowId);
void destroyPluginWindow(window_plugin* pluginWindow);

bool internalplugin::showWindow(bool bResetPosition) {
    // dbgassert(!windowHost);
    dbgassert(!windowClient.clientWindow);
    dbgassert(!windowClient.clientWindowInterface);
    dbgassert(!windowClient.hostWindow);
    dbgassert(!windowClient.ctrl);
    dbgassert(!windowClient.view);
    auto newView = getViewCtr(UID_VIEW_CTR_WINDOW);
    dbgassert(newView);
    if (!newView) {
        return false;
    }
    auto ctrl = std::make_shared<PluginControl>(newView);
    ctrl->setWindowName(getName());
    ctrl->initApp(std::vector<String>());
#if BUILD_VSTHOST
    auto tls = daw_tls::getTls();
    auto mainCtrl = tls.mainCtrl;
    if(mainCtrl) {
        ctrl->setParentCtrl(mainCtrl);
        ctrl->setDawCtrl(mainCtrl);
        ctrl->m_scale     = mainCtrl->m_scale;
        *ctrl->getTheme() = *mainCtrl->getTheme();
    }
#endif
    ivec2 defSize{ 0, 0 };
    newView->getFixedSize(&defSize.x, &defSize.y);
    internal_plugin_window_client clientWindow;
    clientWindow.view = std::move(newView);
    clientWindow.ctrl = std::move(ctrl);
    windowClient = clientWindow;
    if (openWindow(bResetPosition, defSize)) {
        return true;
    }
    return false;
}
bool internalplugin::onShow(host_plugin_window* _window) {
    dbgassert(windowClient.ctrl && windowClient.view);
    dbgassert(!windowClient.clientWindowInterface);
    dbgassert(!windowClient.clientWindow);
    dbgassert(!windowClient.hostWindow);
    dbgassert(_window == windowHost);
    if (windowClient.ctrl && windowClient.view && _window) {
        dbgassert(_window);
        windowClient.hostWindow = _window;
        ivec2 defSize{ 0, 0 };
        windowClient.view->getFixedSize(&defSize.x, &defSize.y);
        windowClient.clientWindowInterface = createBuildinPluginWindow(windowClient.ctrl, defSize.x, defSize.y, reinterpret_cast<void*>(_window->getHWND()));
        dbgassert(windowClient.clientWindowInterface);
        windowClient.clientWindow = dynamic_cast<window_main*>(windowClient.clientWindowInterface);
        dbgassert(windowClient.clientWindow);
        windowClient.clientWindow->show();
        effectbase::onShow(_window);
        return true;
    }
    return false;
}

void internalplugin::updateWindow() {
    if (this->windowHost && windowClient.clientWindowInterface) {
        windowClient.clientWindowInterface->onIdle();
    }
    effectbase::updateWindow();
}
void internalplugin::onWindowResize(ivec2 size) {
    if (windowClient.clientWindowInterface) {
        windowClient.clientWindowInterface->onResize(size);
    }
}

bool internalplugin::onClose() {
    if (windowClient.clientWindow) {
        // windowClient.clientWindow->requestClose();
        dbgassert(windowClient.ctrl->isOk());
        windowClient.clientWindow->hide();
        destroyPluginWindow(windowClient.clientWindowInterface);
        dbgassert(!windowClient.ctrl->isOk());
        windowClient.clientWindow = nullptr;
        if (windowClient.view){
            windowClient.view->setFree();
        }
    }
    // windowHost = nullptr;
    windowClient = {};
    return effectbase::onClose();
}
std::shared_ptr<PluginViewContainers> internalplugin::getViewCtr(int32_t uiId) {
    for (auto& existingView : views) {
        if (!existingView->isInUse() && existingView->getUiId() == uiId) {
            existingView->setUsed();
            return existingView;
        }
    }
    auto newView = createViewCtrInternal();
    if (newView && newView->isViewSupported(uiId)) {
        newView->setUiId(uiId);
        newView->setUsed();
        views.push_back(newView);
    } else {
        newView = nullptr;
    }
    return newView;
};
void internalplugin::sendNotesOff() {
    std::vector<IMidiMsg> messages;
    messages.reserve(handlesIntPlugin->heldNotes.size() + 1);
    for (const auto& notePitch : handlesIntPlugin->heldNotes) {
        auto deltaFrames = 0;
        messages.emplace_back();
        IMidiMsg& msg = messages.back();
        msg.MakeNoteOffMsg(notePitch, deltaFrames);
    }
    messages.emplace_back(0, 0xB0, 123, 0);// InstantOff
    handlesIntPlugin->heldNotes.clear();
    processMidiMessages(messages);
    this->midiEventsDispatched += CtrSize(messages);
}
void internalplugin::processMidi(midi_events_t& midiEvents) {
    const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, midiEvents.bpm100, format.sampleRate);
    auto& heldNotes            = handlesIntPlugin->heldNotes;
    std::vector<IMidiMsg> messages;
    messages.reserve(midiEvents.noteEventsProcessed->size());
    for (auto& evt : *midiEvents.noteEventsProcessed) {
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
            msg.MakeNoteOnMsg(evt.pitch, evt.velocity, deltaFrames);
        } else {
            msg.MakeNoteOffMsg(evt.pitch, deltaFrames);
        }
    }
    processMidiMessages(messages);
    this->midiEventsDispatched += CtrSize(messages);
}
