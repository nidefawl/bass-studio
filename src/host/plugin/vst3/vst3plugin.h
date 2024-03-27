#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "gui/plugin/plugin.h"
#include "host/daw/history.h"
#include "host/host_plugin_window.h"
#include "host/track/track_impl.h"
#include "math/vec.h"
#include "host/plugin/modules.h"
#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include "pluginterfaces/vst/vsttypes.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "public.sdk/source/vst/utility/uid.h"
#include "public.sdk/source/vst/vstpresetfile.h"
#include "str_util.h"
#include "seq_time.h"
#include "host/automation/automation.h"
#include "logging.h"
#include "snapshot/snapshot.h"
#include "host/plugin/base/base-plugin.h"
#include <public.sdk/source/vst/hosting/module.h>
#include <vector>
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "thread.h"
#include "tls.h"
#include "types.h"

struct AudioBlock;
class track_t;
class guibase;
struct track_impl_t;
class host_plugin_window;

using Module = VST3::Hosting::Module;
using tresult = Steinberg::tresult;
using ParamID = Steinberg::Vst::ParamID;
using ParamValue = Steinberg::Vst::ParamValue;

class vst3plugin final : public effectbase {
    VST3::UID uid{};
    VST3::Hosting::Module::Ptr module;
    std::shared_ptr<Steinberg::Vst::PlugProvider> pluginProvider;
    Steinberg::Vst::IComponent* vst3Component = nullptr;
    Steinberg::Vst::IAudioProcessor* vst3AudioProcessor = nullptr;
    Steinberg::Vst::IEditController* editController = nullptr;
    Steinberg::Vst::IEditControllerHostEditing* editControllerHostEditing = nullptr;
    Steinberg::Vst::HostProcessData processData = {};
	Steinberg::Vst::ProcessContext processContext = {};
	Steinberg::IPtr<Steinberg::IPlugView> view = nullptr;
    bool bIsPostInit       = false;
    bool bIsLoadingProgram = false;
    class ComponentHandler : public Steinberg::Vst::IComponentHandler
    {
        vst3plugin* plugin;
        struct param_editing_t {
            uint32_t paramIdx = 0;
            float   valBefore = 0;
        };
        param_editing_t paramEditing;
    public:
        ComponentHandler(vst3plugin* plugin) : plugin(plugin) {
        }

        tresult PLUGIN_API beginEdit (ParamID id) override
        {
            auto* effParam = plugin->getEffectParam(id);
            if (!effParam) {
                log_printf("%s audioMasterAutomate unknown param index %u\n", StringAsCStr(plugin->getName()), id);
                paramEditing = {};
            } else {
                paramEditing = { id, effParam->getValue() };
            }
            return Steinberg::kResultOk;
        }
        tresult PLUGIN_API performEdit (ParamID id, ParamValue valueNormalized) override
        {
            auto* effParam = plugin->getEffectParam(id);
            if (!effParam) {
                log_printf("%s audioMasterAutomate unknown param index %u %f\n", StringAsCStr(plugin->getName()), id, valueNormalized);
            } else {
                auto flags = FLG_PAR_UPDATE_FROM_CLIENT;
                plugin->setParamEdit(effParam->idx, valueNormalized, flags);
                effParam->paramValueState = PARAM_FLAG_SET;
                effParam->paramDisplayValState |= PARAM_FLAG_DIRTY;
                effParam->inUse = true;
            }
            return Steinberg::kResultOk;
        }
        tresult PLUGIN_API endEdit (ParamID id) override
        {
            if (paramEditing.paramIdx == id) {
                auto* effParam = plugin->getEffectParam(id);
                if (!effParam) {
                    log_printf("%s audioMasterAutomate unknown param index %u\n", StringAsCStr(plugin->getName()), id);
                } else {
                    auto newVal = effParam->getValue();
                    auto oldVal = paramEditing.valBefore;
                    track_t* track                = plugin->trackImpl->getTrack();
                    automatable_param_ref_t ref = plugin->toRef();
                    parameter_ref_t p             = { track->projectIdx, ref.type, plugin->projectGlobalId, effParam->idx };
                    auto daw = daw_tls::getTls().dawInstance;
                    daw->pushHist(new action_modify_effect_parameter("Modify parameter", p, oldVal, newVal));
                }
            }
            paramEditing = {};
            return Steinberg::kResultOk;
        }
        tresult PLUGIN_API restartComponent (Steinberg::int32 flags) override
        {
            if ((flags & Steinberg::Vst::RestartFlags::kParamValuesChanged) ||
                (flags & Steinberg::Vst::RestartFlags::kParamTitlesChanged)) {
                plugin->visitParams([](auto& mapEntry) {
                    automatable_param_t& param = mapEntry.second;
                    param.paramNameState |= PARAM_FLAG_DIRTY;
                    param.paramValueState |= PARAM_FLAG_DIRTY;
                    param.paramDisplayValState |= PARAM_FLAG_DIRTY;
                });
            }
            auto name = plugin->getName();
            const char* szName = StringAsCStr(name);
            if (flags & Steinberg::Vst::RestartFlags::kReloadComponent)
                log_lf(Log::L_WARN, "%s: kReloadComponent\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kIoChanged)
                log_lf(Log::L_WARN, "%s: kIoChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kParamValuesChanged)
                log_lf(Log::L_WARN, "%s: kParamValuesChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kLatencyChanged)
                log_lf(Log::L_WARN, "%s: kLatencyChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kParamTitlesChanged)
                log_lf(Log::L_WARN, "%s: kParamTitlesChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kMidiCCAssignmentChanged)
                log_lf(Log::L_WARN, "%s: kMidiCCAssignmentChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kNoteExpressionChanged)
                log_lf(Log::L_WARN, "%s: kNoteExpressionChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kIoTitlesChanged)
                log_lf(Log::L_WARN, "%s: kIoTitlesChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kPrefetchableSupportChanged)
                log_lf(Log::L_WARN, "%s: kPrefetchableSupportChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kRoutingInfoChanged)
                log_lf(Log::L_WARN, "%s: kRoutingInfoChanged\n", szName);
            if (flags & Steinberg::Vst::RestartFlags::kKeyswitchChanged)
                log_lf(Log::L_WARN, "%s: kKeyswitchChanged\n", szName);
            return Steinberg::kResultOk;
        }

    private:
        tresult PLUGIN_API queryInterface (const Steinberg::TUID /*_iid*/, void** /*obj*/) override
        {
            return Steinberg::kNoInterface;
        }
        // we do not care here of the ref-counting. A plug-in call of release should not destroy this
        // class!
        Steinberg::uint32 PLUGIN_API addRef () override { return 1000; }
        Steinberg::uint32 PLUGIN_API release () override { return 1000; }
    };
    
    class PlugFrame : public Steinberg::IPlugFrame
    {
        vst3plugin* plugin;
    public:
        PlugFrame(vst3plugin* plugin) : plugin(plugin) {
        }
	    tresult PLUGIN_API resizeView (Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override {
            if (plugin->windowHost && newSize)
                plugin->windowHost->resize(ivec2(newSize->right - newSize->left, newSize->bottom - newSize->top));
            if (view)
                view->onSize(newSize);
            return Steinberg::kResultOk;
        }
    private:
        tresult PLUGIN_API queryInterface (const Steinberg::TUID /*_iid*/, void** /*obj*/) override
        {
            return Steinberg::kNoInterface;
        }
        // we do not care here of the ref-counting. A plug-in call of release should not destroy this
        // class!
        Steinberg::uint32 PLUGIN_API addRef () override { return 1000; }
        Steinberg::uint32 PLUGIN_API release () override { return 1000; }
    };
    ComponentHandler componentHandler;
    PlugFrame plugFrame;
    Steinberg::Vst::ParameterChanges inputParameterChanges;
    Steinberg::Vst::ParameterChanges outputParameterChanges;
public:
    vst3plugin(VST3::UID uid, VST3::Hosting::Module::Ptr&& _module, std::shared_ptr<Steinberg::Vst::PlugProvider>&& _pluginProvider, int32_t globalId, IHostCallback* hostcallback, String _sDir, int32_t _bugfixFlags) 
        : effectbase(_pluginProvider->getClassInfo().name(), globalId, hostcallback),
            uid(uid),
            module(_module),
            pluginProvider(_pluginProvider),
            componentHandler(this),
            plugFrame(this)
    {
    }
    ~vst3plugin() override {
        if (processData.inputEvents) delete[] dynamic_cast<Steinberg::Vst::EventList*>(processData.inputEvents);
        if (processData.outputEvents) delete[] dynamic_cast<Steinberg::Vst::EventList*>(processData.outputEvents);
    }
    String getUID() const { return uid.toString(true); }
    VST3::Hosting::ClassInfo getClassInfo() const { return pluginProvider->getClassInfo(); }
    ModuleType getModuleType() override { return MODULE_TYPE_VST3; };
    String getAutomatableName() override { return this->sName; }
    Steinberg::Vst::ProcessContext& getVst3ProcessContext() { return processContext; }
    void checkForMainThread() {
        if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::MainThread) {
            dbgassert(0);
            throw std::logic_error("Requires Main Thread!");
        }
    }

    void deactivate() {
        checkForMainThread();
        if (Steinberg::kResultOk != vst3Component->setActive(false)) {
            log_lf(Log::L_ERROR, "Failed to deactivate VST3 plugin\n");
            bIsEnabled = false;
        }
        vst3AudioProcessor->setProcessing(false);
    }

    void activate() {
        checkForMainThread();
        if (Steinberg::kResultOk != vst3Component->setActive(true)) {
            log_lf(Log::L_ERROR, "Failed to activate VST3 plugin\n");
            bIsEnabled = false;
        }
        vst3AudioProcessor->setProcessing(bIsEnabled);
    }

    void unloadVst3Plugin() {
        checkForMainThread();
        processData = {};
        processContext = {};
        editController = nullptr;
        vst3AudioProcessor = nullptr;
        vst3Component = nullptr;
        pluginProvider = nullptr;
        module = nullptr;
    }

    bool loadVST3Plugin() {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        vst3Component = pluginProvider->getComponent();
        if (vst3Component == nullptr) {
            return false;
        }
        vst3AudioProcessor = FUnknownPtr<IAudioProcessor>(vst3Component);
        if (vst3AudioProcessor == nullptr) {
            return false;
        }
        editController = pluginProvider->getController();
        if (editController)
            editController->setComponentHandler(&componentHandler);

        editControllerHostEditing = FUnknownPtr<IEditControllerHostEditing>(editController);

        auto numInAudioBuses = vst3Component->getBusCount(MediaTypes::kAudio, BusDirections::kInput);
        auto numOutAudioBuses = vst3Component->getBusCount(MediaTypes::kAudio, BusDirections::kOutput);
        auto numInEventBuses = vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kInput);
        auto numOutEventBuses = vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kOutput);
        auto classInfo = pluginProvider->getClassInfo();
        for (auto& subcat : classInfo.subCategories()) {
            if (subcat == "Instrument" || subcat == "Synth" || subcat == "Sampler" || subcat == "Drum") {
                isSynth = true;
                break;
            }
        }
        for (int i = 0; i < numInEventBuses; ++i) {
            BusInfo info{};
            if (kResultOk != vst3Component->getBusInfo(kEvent, kInput, i, info))
                return false;
            if (kResultOk != vst3Component->activateBus(kEvent, kInput, i, true))
                return false;
            bCanReceiveMidi = true;
        }

        for (int i = 0; i < numOutEventBuses; ++i) {
            BusInfo info{};
            if (kResultOk != vst3Component->getBusInfo(kEvent, kInput, i, info))
                return false;
            if (kResultOk != vst3Component->activateBus(kEvent, kInput, i, false))
                return false;
            bCanSendMidi = false;
            // isSynth = true;
        }

        for (int i = 0; i < numInAudioBuses; ++i) {
            BusInfo info{};
            if (kResultOk != vst3Component->getBusInfo(kAudio, kInput, i, info))
                return false;
            if (kResultOk != vst3Component->activateBus(kAudio, kInput, i, true))
                return false;
        }
        for (int i = 0; i < numOutAudioBuses; ++i) {
            BusInfo info{};
            if (kResultOk != vst3Component->getBusInfo(kAudio, kOutput, i, info))
                return false;
            if (kResultOk != vst3Component->activateBus(kAudio, kOutput, i, true))
                return false;
        }
        scanParams();
        return true;
    }

    void scanParams() {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        auto count = !editController ? 0 : editController->getParameterCount();
        for (int32_t i = 0; i < count; ++i) {
            int32_t paramIdentifier    = PARAM_OFFSET_EXTERNAL + i;
            automatable_param_t* param = registerParam(paramIdentifier);
            ParameterInfo info{};
            if (kResultOk != editController->getParameterInfo(i, info)) {
                log_lf(Log::L_ERROR, "Failed to get VST3 plugin parameter display value\n");
                continue;
            }
            param->internalIdx = info.id;
            param->name = VST3::StringConvert::convert(info.title);
            param->shortLabel = VST3::StringConvert::convert(info.shortTitle);
            param->paramNameState = PARAM_FLAG_SET;
            param->paramDisplayValState = PARAM_FLAG_SET;
            param->paramValueState = PARAM_FLAG_SET;
            param->unit = VST3::StringConvert::convert(info.units);
            String128 paramValueStr{};
            if (kResultOk != editController->getParamStringByValue(info.id, info.defaultNormalizedValue, paramValueStr)) {
                log_lf(Log::L_ERROR, "Failed to get VST3 plugin parameter display value\n");
                continue;
            }
            param->paramDisplayValStr = VST3::StringConvert::convert(paramValueStr);
        }
    }

    void load(DAW::Host::PluginManager* host) override {
        effectbase::load(host);
        activate();
    }

    void onDisable() override { deactivate(); }

    void onEnable() override { 
        activate(); 
        if (!bIsPostInit) {
            bIsPostInit = true;
            visitParams([](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                param.paramNameState |= PARAM_FLAG_DIRTY;
                param.paramValueState |= PARAM_FLAG_DIRTY;
                param.paramDisplayValState |= PARAM_FLAG_DIRTY;
            });
        }
    }

    automatable_param_t* getParam(int32_t idx) override {
        auto param = effectbase::getParam(idx);
        if (param && param->internalIdx >= 0) {
            if (param->paramValueState & PARAM_FLAG_DIRTY && editController) {
                param->setValue(editController->getParamNormalized(param->internalIdx));
                param->paramValueState = PARAM_FLAG_SET;
            }
        }
        return param;
    }

    void postSetParameter(int32_t idx, float preVal, float val, int flags) override {
        effectbase::postSetParameter(idx, preVal, val, flags);
        automatable_param_t* param = getParamUnchecked(idx);
        if (param->internalIdx >= 0) {
            if (!(flags & FLG_PAR_UPDATE_FROM_CLIENT) && editController) {
                editController->setParamNormalized(param->internalIdx, val);
                int32_t index;
                inputParameterChanges.addParameterData(param->internalIdx, index)->addPoint(0, val, index);
            }
            param->paramDisplayValState |= PARAM_FLAG_DIRTY;
            param->paramValueState = PARAM_FLAG_SET;
        }
    }

    void recvParamDisplayValueUpdate(int32_t idx) {
        automatable_param_t* param = getEffectParam(idx);
        dbgassert(param && param->internalIdx >= 0);
        param->paramDisplayValState &= ~PARAM_FLAG_DIRTY;
        Steinberg::Vst::String128 paramValueStr{};
        if (editController && Steinberg::kResultOk == editController->getParamStringByValue(param->internalIdx, param->getValue(), paramValueStr) 
                && paramValueStr[0]) {
            param->paramDisplayValStr = VST3::StringConvert::convert(paramValueStr);
            param->paramDisplayValState |= PARAM_FLAG_SET;
        }
    }

    void recvParamNameUpdate(int32_t idx) {
        if (bIsLoadingProgram) {
            return;
        }
        automatable_param_t* param = getEffectParam(idx);
        dbgassert(param && param->internalIdx >= 0);
        param->paramNameState &= ~PARAM_FLAG_DIRTY;
        Steinberg::Vst::ParameterInfo info{};
        if (Steinberg::kResultOk == editController->getParameterInfo(param->idx - PARAM_OFFSET_EXTERNAL, info)) {
            param->name = VST3::StringConvert::convert(info.title);
            param->shortLabel = VST3::StringConvert::convert(info.shortTitle);
            param->unit = VST3::StringConvert::convert(info.units);
        }
    }

    String getParamName(int32_t idx) override {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->internalIdx >= 0) {
            if (param->paramNameState & PARAM_FLAG_DIRTY) {
                recvParamNameUpdate(param->internalIdx);
            }
            if (param->paramNameState & PARAM_FLAG_SET) {
                return param->name;
            }
        }
        return effectbase::getParamName(param->idx);
    }

    param_unit_t getParamValueDisplay(int32_t idx) override {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->internalIdx >= 0) {
            if (param->paramDisplayValState & PARAM_FLAG_DIRTY) {
                recvParamDisplayValueUpdate(param->internalIdx);
            }
            if (param->paramDisplayValState & PARAM_FLAG_SET) {
                return {param->paramDisplayValStr, param->unit};
            }
        }
        return effectbase::getParamValueDisplay(param->idx);
    }

    void unload(DAW::Host::PluginManager* host) override {
        effectbase::unload(host);
        deactivate();
        unloadVst3Plugin();
    }

    void initBuffers() override {
        configureIOPorts();
        effectbase::initBuffers();
        using namespace Steinberg;
        using namespace Steinberg::Vst;
	    Steinberg::Vst::ProcessSetup processSetup = {};
        processSetup.processMode = ProcessModes::kRealtime;
        processSetup.symbolicSampleSize = format.sampleformat == sampleformat_bits_t::FLOAT_32 ? SymbolicSampleSizes::kSample32 : SymbolicSampleSizes::kSample64;
        processSetup.maxSamplesPerBlock = format.blockSize;
        processSetup.sampleRate = format.sampleRate;
        processData.symbolicSampleSize = processSetup.symbolicSampleSize;
        processData.processMode = processSetup.processMode;
        processData.processContext = &processContext;

        /* Only assingn pointers here once. This is ok for now */
        channelnum_t busIdx = 0;
        for (channelnum_t ch = 0; ch < blockInputs->channels && busIdx < processData.numInputs; ++busIdx) {
            auto& bus = processData.inputs[busIdx];
            for (channelnum_t busCh = 0; busCh < bus.numChannels; ++busCh) {
                bus.channelBuffers32[busCh] = blockInputs->buf[ch++];
            }
        }
        busIdx = 0;
        for (channelnum_t ch = 0; ch < blockOutputs->channels && busIdx < processData.numOutputs; ++busIdx) {
            auto& bus = processData.outputs[busIdx];
            for (channelnum_t busCh = 0; busCh < bus.numChannels; ++busCh) {
                bus.channelBuffers32[busCh] = blockOutputs->buf[ch++];
            }
        }

        if (kResultOk != vst3AudioProcessor->setupProcessing(processSetup)) {
            log_lf(Log::L_ERROR, "Failed to setup VST3 plugin processing\n");
            bIsEnabled = false;
        }
    }

    void configureIOPorts() {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        inputChannelsDesc.clear();
        outputChannelsDesc.clear();
        uint32_t numInAudioBuses = vst3Component->getBusCount(MediaTypes::kAudio, BusDirections::kInput);
        uint32_t numOutAudioBuses = vst3Component->getBusCount(MediaTypes::kAudio, BusDirections::kOutput);
        channelnum_t inputCount  = static_cast<channelnum_t>(math::clamp(numInAudioBuses, 0U, 255U));
        channelnum_t outputCount = static_cast<channelnum_t>(math::clamp(numOutAudioBuses, 0U, 255U));
        channelnum_t portOffsetInput = 0;
        for (channelnum_t i = 0; i < inputCount; ++i) {
            BusInfo info{};
            if (kResultOk != vst3Component->getBusInfo(kAudio, kInput, i, info)) {
                log_lf(Log::L_ERROR, "Failed to get VST3 plugin input bus info\n");
                bIsEnabled = false;
                return;
            }
            DAW::channel_desc desc;
            desc.offset = portOffsetInput;
            desc.count = info.channelCount;
            desc.name = VST3::StringConvert::convert(info.name);
            inputChannelsDesc.push_back(desc);
            portOffsetInput += desc.count;
        }
        channelnum_t portOffsetOutput = 0;
        for (channelnum_t i = 0; i < outputCount; ++i) {
            BusInfo info{};
            if (kResultOk != vst3Component->getBusInfo(kAudio, kOutput, i, info)) {
                log_lf(Log::L_ERROR, "Failed to get VST3 plugin output bus info\n");
                bIsEnabled = false;
                return;
            }
            DAW::channel_desc desc;
            desc.offset = portOffsetOutput;
            desc.count = info.channelCount;
            desc.name = VST3::StringConvert::convert(info.name);
            outputChannelsDesc.push_back(desc);
            portOffsetOutput += desc.count;
        }

        processData.numInputs = inputChannelsDesc.size();
        processData.numOutputs = outputChannelsDesc.size();
        if (processData.inputs) delete[] dynamic_cast<AudioBusBuffers*>(processData.inputs);
        if (processData.outputs) delete[] dynamic_cast<AudioBusBuffers*>(processData.outputs);
        if (processData.numInputs > 0) {
            processData.inputs = new AudioBusBuffers[processData.numInputs];
            for (channelnum_t ch = 0; ch < processData.numInputs; ++ch) {
                auto& desc = inputChannelsDesc[ch];
                AudioBusBuffers& busBuffers = processData.inputs[ch];
                busBuffers.numChannels = desc.count;
                busBuffers.silenceFlags = 0;
                busBuffers.channelBuffers32 = new Sample32*[desc.count];
                for (channelnum_t i = 0; i < desc.count; ++i) {
                    busBuffers.channelBuffers32[i] = nullptr;
                }
            }
        } else {
            processData.inputs = nullptr;
        }
        if (processData.numOutputs > 0) {
            processData.outputs = new AudioBusBuffers[processData.numOutputs];
            for (channelnum_t ch = 0; ch < processData.numOutputs; ++ch) {
                auto& desc = outputChannelsDesc[ch];
                AudioBusBuffers& busBuffers = processData.outputs[ch];
                busBuffers.numChannels = desc.count;
                busBuffers.silenceFlags = 0;
                busBuffers.channelBuffers32 = new Sample32*[desc.count];
                for (channelnum_t i = 0; i < desc.count; ++i) {
                    busBuffers.channelBuffers32[i] = nullptr;
                }
            }
        } else {
            processData.outputs = nullptr;
        }
        
        auto numInEventBuses = vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kInput);
        auto numOutEventBuses = 0; //vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kOutput); // INACTIVE
        if (processData.inputEvents) delete[] dynamic_cast<EventList*>(processData.inputEvents);
        if (processData.outputEvents) delete[] dynamic_cast<EventList*>(processData.outputEvents);
        if (numInEventBuses > 0) {
            processData.inputEvents = new EventList[numInEventBuses];
        } else {
            processData.inputEvents = nullptr;
        }
        if (numOutEventBuses > 0) {
            processData.outputEvents = new EventList[numOutEventBuses];
        } else {
            processData.outputEvents = nullptr;
        }
        processData.inputParameterChanges = &inputParameterChanges;
        processData.outputParameterChanges = &outputParameterChanges;
    }
    
    void setSampleFormat(sampleformat_t sampleFormat) override {
        effectbase::setSampleFormat(sampleFormat);
    }

    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        
        /**
         * These asserts needs to hold true. 
         * If not, then processData input/output buffer pointers need 
         * to be reassinged here instead of in initBuffers
         */
        dbgassert(in == blockInputs);
        dbgassert(out == blockOutputs);

        processData.numSamples = numSamples;
        tresult result = vst3AudioProcessor->process(processData);
#ifndef NDEBUG
        if (result != kResultOk) {
            log_lf(Log::L_ERROR, "VST3 plugin process failed (%s)\n", StringAsCStr(sName));
        }
#endif
        inputParameterChanges.clearQueue();
        outputParameterChanges.clearQueue();
    }

    void createSnapshot(plugin_snapshot_t& ps, vst3plugin* plugin, const tracksnapshot_store_opts_t& opts) {
        ps.slot              = 0;
        ps.projectGlobalId   = plugin->projectGlobalId;
        ps.enabled           = plugin->bIsEnabled;
        ps.ioChannels.input  = plugin->inputChannelsDesc;
        ps.ioChannels.output = plugin->outputChannelsDesc;
        ps.moduleType        = MODULE_TYPE_VST3;
        ps.clapId           = plugin->getUID();
        ps.localDbId     = plugin->localDbId;
        ps.name = plugin->sName;

        bool usesBinaryChunks = false;
        if (opts.storePluginPreset) {
            Steinberg::Vst::BufferStream stream;
            if (Steinberg::kResultOk == plugin->vst3Component->getState(&stream)) {
                Steinberg::int64 size = 0;
                if (Steinberg::kResultOk == stream.tell(&size) && size > 0) {
                    ps.dataChunk.resize(size);
                    stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
                    stream.read(ps.dataChunk.data(), size);
                    usesBinaryChunks = true;
                }
            }
            stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
            if (plugin->editController && Steinberg::kResultOk == plugin->editController->getState(&stream)) {
                Steinberg::int64 size = 0;
                if (Steinberg::kResultOk == stream.tell(&size) && size > 0) {
                    ps.dataChunk2.resize(size);
                    stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
                    stream.read(ps.dataChunk2.data(), size);
                    usesBinaryChunks = true;
                }
            }
        }
        if (opts.storePluginPreset) {
            auto numParamsReserve = math::min<int32_t>(150, plugin->getNumParameters());
            ps.params.reserve(numParamsReserve);
            plugin->visitParams([&ps, vstplugin = plugin, usesBinaryChunks](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                if (param.inUse || !usesBinaryChunks) {
                    float curValue = param.getValue();
                    int paramFlags = param.inUse ? 1 : 0;
                    if (param.internalIdx >= 0) {
                        curValue = vstplugin->editController->getParamNormalized(param.internalIdx);
                    }
                    ps.params.push_back(param_snapshot_t{ param.idx, curValue, paramFlags });
                }
            });
            if (plugin->programNames.size() > 1) {
                uint32_t curProgramNr = 0;
                plugin->getCurrentProgram(curProgramNr);
                ps.currentProgram = curProgramNr;
            }
        }

        if (opts.storeAutomation) {
            storeAutomation(ps.automatedParams, plugin);
        }
    }

    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override {
        vst3AudioProcessor->setProcessing(false);
        createSnapshot(ps, this, opts);
        vst3AudioProcessor->setProcessing(true);
        for (auto& [uuid, gui] : uiInstances) {
            plugin_ui_snapshot_t uiSnapshot;
            gui->makeSnapshot(uiSnapshot, opts);
            ps.uiSnapshots[uuid] = uiSnapshot;
        }
        ps.windowLayout = getWindowLayoutSnapshot();
        ps.slot = this->slot;
    }

    void loadSnapshot(const plugin_snapshot_t& pluginSnapshot) override {
        if (vst3AudioProcessor) vst3AudioProcessor->setProcessing(false);
        vst3Component->setActive(false);
        this->bIsLoadingProgram = true;
        bool bLoadProgramDataChunk = false;
        if (pluginSnapshot.dataChunk.size() > 0) {
            Steinberg::Vst::BufferStream stream;
            Steinberg::int32 bytesWritten = 0;
            auto nonConst = const_cast<std::vector<uint8_t>&>(pluginSnapshot.dataChunk);
            stream.write(nonConst.data(), nonConst.size(), &bytesWritten);
            stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
            if (Steinberg::kResultOk == vst3Component->setState(&stream)) {
                bLoadProgramDataChunk = true;
            }
            if (editController) {
                stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
                if (Steinberg::kResultOk == editController->setComponentState(&stream)) {
                    bLoadProgramDataChunk = true;
                }
                nonConst = const_cast<std::vector<uint8_t>&>(pluginSnapshot.dataChunk2);
                stream.write(nonConst.data(), nonConst.size(), &bytesWritten);
                stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
                if (Steinberg::kResultOk == editController->setState(&stream)) {
                    bLoadProgramDataChunk = true;
                }
            }

        }
        if (!bLoadProgramDataChunk) {
            DAW::loadEffectParamsFromSnapshot(pluginSnapshot, this);
        }
        this->bIsLoadingProgram = false;
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
        vst3Component->setActive(true);
        if (vst3AudioProcessor) vst3AudioProcessor->setProcessing(true);
    }

    samplecount_t getPluginLatency() override { 
        return vst3AudioProcessor->getLatencySamples();
    }

    std::shared_ptr<guiplugin> createGuiPlugin(int32_t uuid) override {
        auto gui = std::make_shared<guivst3plugin>(this);
        gui->setTitle(StringFormat("%s (VST3)", StringAsCStr(this->sName)));
        return gui;
    }

    bool hasWindowEditor() override {
        return this->editController != nullptr;
    }

    bool showWindow(bool bResetPosition) override {
        checkForMainThread();
        using namespace Steinberg;
        if (!editController) {
            return false;
        }

        if (view || windowHost) {
            return false;
        }

        view = editController->createView(Vst::ViewType::kEditor);
        if (!view) {
            return false;
        }

        ViewRect viewRect = {};
        if (view->getSize(&viewRect) != kResultOk) {
            return false;
        }

#ifdef _WIN32
        if (view->isPlatformTypeSupported(Steinberg::kPlatformTypeHWND) != Steinberg::kResultTrue) {
            return false;
        }
#else
    #error "TODO: Implement for other platforms"
#endif
        bSupportsWindowResize = false;// view->canResize();

        this->openWindow(bResetPosition, { viewRect.getWidth(), viewRect.getHeight() });
        return true;
    }
    void onWindowResize(ivec2 size) override {
        if (view && view->canResize()) {
            Steinberg::ViewRect viewRect = { 0, 0, size.x, size.y };
            view->onSize(&viewRect);
        }
    }
    bool onShow(host_plugin_window* _window) override {
        if (this->windowHost == _window) {
            bEditOpen = true;
            this->updateFromMainThread();
            view->setFrame(&plugFrame);
            if (view->attached(_window->getWindowHandle(), Steinberg::kPlatformTypeHWND) != Steinberg::kResultOk) {
                log_lf(Log::L_ERROR, "Failed to attach editor view to HWND\n");
                return false;
            }
            this->updateFromMainThread();
        }
        return true;
    }
    bool onClose() override {
        if (this->windowHost != nullptr && bEditOpen) {
            if (view) {
                if (Steinberg::kResultOk != view->removed()) {
                    log_lf(Log::L_ERROR, "Failed to remove editor view\n");
                }
                view->setFrame(nullptr);
            }
        }
        view = nullptr;
        bEditOpen = false;
        return true;
    }
    void processMidi(midi_data_processing_t& midiEvents) override {
        if (!bCanReceiveMidi) {
            return;
        }

        const int32_t midiBusIndex = 0;
        Steinberg::Vst::EventList& eventList = dynamic_cast<Steinberg::Vst::EventList*>(processData.inputEvents)[midiBusIndex];
        eventList.clear();
        auto numEvents = int32_t(midiEvents.noteEvents->size());
        if (numEvents) {
            if (eventList.getMaxSize() < numEvents) {
                eventList.setMaxSize(math::max<int32_t>(eventList.getMaxSize() * 2, numEvents));
            }
            const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, midiEvents.bpm100, format.sampleRate);
            for (auto& noteEvent : *midiEvents.noteEvents) {
                Steinberg::Vst::Event evt{};
                evt.busIndex = midiBusIndex;
                evt.sampleOffset =  math::floordS32(noteEvent.tickOffsetInBlock * tickToSamples);
                evt.ppqPosition = noteEvent.globalTick / double(TICKS_QUARTER);
                evt.flags = 0;
                if (noteEvent.isNoteOn) {
                    evt.type = Steinberg::Vst::Event::EventTypes::kNoteOnEvent;
                    auto& noteOnEvent = evt.noteOn;
                    noteOnEvent.channel = 0;
                    noteOnEvent.pitch = noteEvent.pitch;
                    noteOnEvent.tuning = 0.0f;
                    noteOnEvent.velocity = noteEvent.velocity / 127.0f;
                    noteOnEvent.length = 0;
                    noteOnEvent.noteId = -1;
                } else {
                    evt.type = Steinberg::Vst::Event::EventTypes::kNoteOffEvent;
                    auto& noteOffEvent = evt.noteOff;
                    noteOffEvent.channel = 0;
                    noteOffEvent.pitch = noteEvent.pitch;
                    noteOffEvent.velocity = noteEvent.velocity / 127.0f;
                    noteOffEvent.noteId = -1;
                    noteOffEvent.tuning = 0.0f;
                }
                eventList.addEvent(evt);
            }
        }
    }
    void sendNotesOff() override {

    }

    // param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    // param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;

    // bool setCurrentProgram(uint32_t idx) override;
    // bool getCurrentProgram(uint32_t& idx) override;
    // bool getNumberOfPrograms(uint32_t& numPrograms) override;
    // bool getCurrentProgramName(String& out) override;

    // void addPropertiesTooltip(Table::tbl& table) override;
    // void addPropertiesParameterTooltip(Table::tbl& table, int idx) override;
};
