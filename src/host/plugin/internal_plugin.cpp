#include <algorithm>
#include <utility>
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
        ps.version           = 9;
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

bool internalplugin::close() {
    return true;
}

bool internalplugin::show() {
    return false;
}

struct internalplugin::internalplugin_handles_t {
    std::unique_ptr<guiinternalpluginview> gui;
};

internalplugin::internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId)
    : effectbase(std::move(_sName), _pluginType, _projectGlobalId),
      handlesIntPlugin(new internalplugin_handles_t{}) {
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
