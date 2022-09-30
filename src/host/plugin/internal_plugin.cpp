#include <algorithm>
#include <utility>
#include <vector>
#include "assert_dbg.h"
#include "automation.h"
#include "modules.h"
#include "seq_util.h"

#include "snapshot/snapshot.h"
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
                    auto paramValue = param.getValue();
                    dbgassert(paramValue >= 0.0f && paramValue <= 1.0f);
                    ps.params.push_back(param_snapshot_t{ param.idx, paramValue, param.inUse ? 1 : 0 });
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

void internalplugin::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    meterIn.update(this->blockInputs, 1.0f);
    meter.update(out, 1.0f);
}

internalplugin::internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId, IHostCallback* _hostCallback)
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
    auto newView = openViewCtr(UID_VIEW_CTR_WINDOW);
    dbgassert(newView);
    if (!newView) {
        return false;
    }
    auto ctrl = std::make_shared<PluginControl>(newView);
    ctrl->setWindowName(getName());
    ctrl->initApp(std::vector<String>());
    auto tls = daw_tls::getTls();
    auto mainCtrl = tls.mainCtrl;
    if(mainCtrl) {
        ctrl->setParentCtrl(mainCtrl);
        ctrl->setDawCtrl(mainCtrl);
        ctrl->m_scale     = mainCtrl->m_scale;
        *ctrl->getTheme() = *mainCtrl->getTheme();
    }
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
void internalplugin::getAllViewCtrs(int32_t uiId, std::vector<std::shared_ptr<PluginViewContainers>>& out) {
    for (auto& existingView : views) {
        if (existingView->getUiId() == uiId) {
            out.push_back(existingView);
        }
    }
    if (out.empty()) {
        auto newView = createViewCtrInternal();
        if (newView && newView->isViewSupported(uiId)) {
            newView->setFree();
            newView->setUiId(uiId);
            views.push_back(newView);
            out.push_back(newView);
        }
    }
}
std::shared_ptr<PluginViewContainers> internalplugin::openViewCtr(int32_t uiId) {
    std::shared_ptr<PluginViewContainers> ptr;
    for (auto& existingView : views) {
        if (!existingView->isInUse() && existingView->getUiId() == uiId) {
            ptr = existingView;
            break;
        }
    }
    if (!ptr) {
        ptr = createViewCtrInternal();
        if (ptr && ptr->isViewSupported(uiId)) {
            ptr->setUiId(uiId);
            views.push_back(ptr);
        } else {
            ptr = nullptr;
        }
    }
    if (ptr) {
        ptr->setUsed();
    }
    return ptr;
}
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
