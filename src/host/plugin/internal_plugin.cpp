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

window_plugin* createBuildinPluginWindow(effectbase* _effect, std::shared_ptr<PluginControl> _ctrl, int w, int h, void* hostWindowId);
void destroyPluginWindow(window_plugin* pluginWindow);

bool internalplugin::onShow(host_plugin_window* _window) {
    auto newView = createInternalView();
    if (!newView) {
        return false;
    }
    internal_plugin_window_client clientWindow;
    clientWindow.view = newView;
    clientWindow.ctrl = std::make_shared<PluginControl>(clientWindow.view);
    clientWindow.ctrl->initApp(std::vector<String>());
#if BUILD_VSTHOST
    auto tls = daw_tls::getTls();
    auto mainCtrl = tls.mainCtrl;
    if(mainCtrl) {
        clientWindow.ctrl->setDawCtrl(mainCtrl);
        clientWindow.ctrl->m_scale     = mainCtrl->m_scale;
        *clientWindow.ctrl->getTheme() = *mainCtrl->getTheme();
    }
#endif
    int32_t ctrlWidth = 0, ctrlHeight = 0;
    clientWindow.view->getFixedSize(&ctrlWidth, &ctrlHeight);
    clientWindow.clientWindowInterface = createBuildinPluginWindow(this, clientWindow.ctrl, ctrlWidth, ctrlHeight, reinterpret_cast<void*>(_window->getHWND()));
    // setEditor(pluginWindow);
    clientWindow.clientWindow = dynamic_cast<window_main*>(clientWindow.clientWindowInterface);
    dbgassert(clientWindow.clientWindow);
    // pluginWindow->setHostWindow(_window->getHWND());
    clientWindow.clientWindow->show();
    windowClient = clientWindow;
    effectbase::onShow(windowHost);
    return true;
}
ivec2 internalplugin::getWindowSize() {
    ivec2 size{ 0, 0 };
    if (windowClient.view)
        windowClient.view->getFixedSize(&size.x, &size.y);
    return size;
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
