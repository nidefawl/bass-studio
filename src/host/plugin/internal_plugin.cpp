#include <algorithm>
#include <utility>
#include "assert_dbg.h"
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
    ps.slot = this->slot;
}

void internalplugin::loadSnapshot(const plugin_snapshot_t& ps) {
    loadEffectParamsFromSnapshot(ps, this);
}

String internalplugin::getAutomatableName() {
    return this->sName;
}

float internalplugin::getParamValue(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->internalIdx >= 0) {
        param->value = dispatchGetParameter(param->internalIdx);
    }
    return param->value;
}

void internalplugin::setParamValue(int32_t idx, float val, int flags) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    param->value = val;
    if (param->idx == PARAM_ENABLE) {
        bool wasEnable = this->bIsEnabled;
        bool isEnabled = val > 0;
        updateOnEnableParam(param, wasEnable, isEnabled, flags);
    } else {
        if (!(flags & FLG_PAR_UPDATE_NOSTORE) && !(flags & FLG_PAR_UPDATE_AUTOMATED)) {
            param->inUse = true;
        }
        if (param->internalIdx >= 0) {
            dispatchSetParameter(param->internalIdx, val);
        }
    }
}

void internalplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    if (flags != 2) {
        return;
    }
    dbgassert(this->trackImpl->getTrack());
    track_t* track                = this->trackImpl->getTrack();
    automationlane_snapshot_t ref = toRef();
    parameter_ref_t p             = { track->projectIdx, ref.type, this->projectGlobalId, idx };
    DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
    
    for (auto& pviewctr : this->views) {
        if (pviewctr->isInUse()) {
            pviewctr->onSetParameter(idx, val);
        }
    }
}

automationlane_snapshot_t internalplugin::toRef() const {
    automationlane_snapshot_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}

struct internalplugin::internalplugin_handles_t {
    std::unique_ptr<guiinternalpluginview> gui;
};

internalplugin::internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId)
    : effectbase(std::move(_sName), _pluginType, _projectGlobalId),
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
    dbgassert(!windowHost);
    dbgassert(!windowClient.clientWindow);
    dbgassert(!windowClient.clientWindowInterface);
    dbgassert(!windowClient.hostWindow);
    dbgassert(!windowClient.ctrl);
    dbgassert(!windowClient.view);
    auto newView = createInternalView();
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
    windowClient = {};
    return effectbase::onClose();
}
