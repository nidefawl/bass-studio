#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "host/plugin/modules.h"
#include "plugins/synth/IPlugMidi.h"
#include "seq_util.h"

#include "snapshot/snapshot.h"
#include "host/plugin/base/base-plugin.h"
#include "internal-plugin.h"
#include "host/track/track.h"
#include "gui/plugin/pluginctr.h"
#include "host/daw/mainctrl.h"
#include "host/daw/history.h"
#include "host/host_plugin_window.h"

namespace {
    void createSnapshot(plugin_snapshot_t& ps, internalplugin* plugin, const tracksnapshot_store_opts_t& opts) {
        ps.slot              = 0;
        ps.projectGlobalId   = plugin->projectGlobalId;
        ps.enabled           = plugin->bIsEnabled;
        ps.ioChannels.input  = plugin->inputChannelsDesc;
        ps.ioChannels.output = plugin->outputChannelsDesc;
        ps.uId               = plugin->getPluginType();
        ps.moduleType        = plugin->getModuleType();
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
    for (auto& [uuid, gui] : uiInstances) {
        plugin_ui_snapshot_t uiSnapshot;
        gui->makeSnapshot(uiSnapshot, opts);
        ps.uiSnapshots[uuid] = uiSnapshot;
    }
    ps.windowLayout = getWindowLayoutSnapshot();
    ps.slot = this->slot;
    auto dataBuf = storePresetData();
    if (dataBuf) {
        // can't use std::vector value type copy here
        ps.dataChunk.resize(dataBuf->size());
        memcpy(ps.dataChunk.data(), dataBuf->data(), dataBuf->size());
    }
}

void internalplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
    DAW::loadEffectParamsFromSnapshot(pluginSnapshot, this);
    if (pluginSnapshot.dataChunk.size() > 0) {
        auto dataBuf = std::make_shared<std::vector<std::byte>>();
        dataBuf->resize(pluginSnapshot.dataChunk.size());
        memcpy(dataBuf->data(), pluginSnapshot.dataChunk.data(), dataBuf->size());
        loadPresetData(dataBuf);
    }
    for (auto& [uuid, snapshot] : pluginSnapshot.uiSnapshots) {
        auto gui = uiInstances.find(uuid);
        if (gui != uiInstances.end()) {
            gui->second->loadSnapshot(snapshot);
        } else {
            this->uiSnapshots[uuid] = snapshot;
            this->uiSnapshots[uuid].isValidSnapshot = true;
        }
    }
    loadWindowLayoutSnapshot(pluginSnapshot.windowLayout);
}

void internalplugin::onEnable() {
    if (bOpenWindowOnEnable) {
        bOpenWindowOnEnable = false;
        showWindow(false);
    }
}

String internalplugin::getAutomatableName() {
    return this->sName;
}

void internalplugin::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    meterIn.update(this->blockInputs, 1.0f);
    meter.update(out, 1.0f);
}

internalplugin::internalplugin(String _sName, int32_t _projectGlobalId, IHostCallback* _hostCallback)
    : effectbase(std::move(_sName), _projectGlobalId, _hostCallback),
      handlesIntPlugin(new internalplugin_handles_t{}) {
    bSupportsWindowResize = true;
}

internalplugin::~internalplugin() {
    delete handlesIntPlugin;
}

std::shared_ptr<guiplugin> internalplugin::createGuiPlugin(int32_t uuid) {
    auto gui = std::make_shared<guiinternalpluginview>(this);
    gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
    return gui;
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
    auto tls = daw_tls::getTls();
    auto mainCtrl = tls.mainCtrl;
    auto ctrl = std::make_shared<PluginControl>(mainCtrl, newView);
    ctrl->setWindowName(getName());
    ctrl->initApp(std::vector<String>());
    if(mainCtrl) {
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

void internalplugin::updateFromMainThread() {
    if (this->windowHost && windowClient.clientWindowInterface) {
        windowClient.clientWindowInterface->onIdle();
    }
    effectbase::updateFromMainThread();
}
void internalplugin::onWindowResize(ivec2 size) {
    if (windowClient.clientWindowInterface) {
        windowClient.clientWindowInterface->onResize(size);
    }
    effectbase::onWindowResize(size);
}

bool internalplugin::onClose() {
    effectbase::onClose();
    if (windowClient.clientWindow) {
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
    return true;
}
void internalplugin::getAllViewCtrs(int32_t uiId, std::vector<std::shared_ptr<PluginViewContainer>>& out) {
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
std::shared_ptr<PluginViewContainer> internalplugin::openViewCtr(int32_t uiId) {
    std::shared_ptr<PluginViewContainer> ptr;
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