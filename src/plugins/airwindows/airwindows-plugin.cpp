#include "airwindows-plugin.hpp"
#include "airwindows-types.hpp"
#include "assert_dbg.h"
#include "plugins/plugin-ui.h"

namespace PluginAirWindows {
    IEffectImpl* createEffectImplGalactic1();
    IEffectImpl* createEffectImplGalactic2();
    IEffectImpl* createEffectImplGalactic3();
    IEffectImpl* createEffectImplMatrixVerb();
    effectbase* makeModuleAirWindowsInstance(PluginType type, int32_t globalid, IHostCallback* hostcallback) {
        return new module_airwindows(type, globalid, hostcallback);
    }
    IEffectImpl* makePluginAirWindowsImpl(PluginType type) {
        switch (type) {
            case PLUGIN_TYPE_AIRWINDOWS_GALACTIC_1:
                return createEffectImplGalactic1();
            case PLUGIN_TYPE_AIRWINDOWS_GALACTIC_2:
                return createEffectImplGalactic2();
            case PLUGIN_TYPE_AIRWINDOWS_GALACTIC_3:
                return createEffectImplGalactic3();
            case PLUGIN_TYPE_AIRWINDOWS_MATRIXVERB:
                return createEffectImplMatrixVerb();
            default:
                break;
        }
        dbgassert(0);
        return nullptr;
    }

    module_airwindows::module_airwindows(PluginType type, int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("TEMPLATE", _projectGlobalId, _hostCallback),
        impl(makePluginAirWindowsImpl(type))
    {
        setName(impl->getName());
        std::vector<paramentry> parameterTypes;
        impl->registerParams(parameterTypes);
        for (const auto& paramEntry : parameterTypes) {
            registerParam(PARAM_OFFSET_IMPL + paramEntry.id)->initValue(paramEntry);
        }
    }

    module_airwindows::~module_airwindows() {
        delete impl;
    }

    void module_airwindows::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
            && out->samples == format.blockSize
            && format.blockSize > 0
            && format.sampleRate > 0);
        if (!assert_expr(in->channels >= 2 && out->channels >= 2)) {
            out->copyFrom(in);
            return;
        }
        out->clear();
        impl->setParameters(this);
        impl->processReplacing(in->buf, out->buf, in->samples, format.sampleRate);
    }

    std::shared_ptr<PluginViewContainer> module_airwindows::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerBasic<guictr_plugin_basic, module_airwindows>>(this, 100, 150);
    }
    PluginType module_airwindows::getPluginType() { return impl->getPluginType(); };

}// namespace PluginAirWindows
