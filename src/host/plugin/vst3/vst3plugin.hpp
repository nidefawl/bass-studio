#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "gui/plugin/plugin.hpp"
#include "host/daw/history.hpp"
#include "host/host_plugin_window.hpp"
#include "host/track/track_impl.hpp"
#include "math/vec.hpp"
#include "host/plugin/modules.hpp"
#include "platform.hpp"
#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "public.sdk/source/vst/utility/uid.h"
#include "public.sdk/source/vst/vstpresetfile.h"
#include "str_util.hpp"
#include "seq_time.hpp"
#include "host/automation/automation.hpp"
#include "logging.hpp"
#include "snapshot/snapshot.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include <public.sdk/source/vst/hosting/module.h>
#include <vector>
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "thread.hpp"
#include "tls.hpp"
#include "types.hpp"

#ifdef __linux__
#include <cerrno>
#include <sys/epoll.h>
#endif

class track_t;
class guibase;
struct track_impl_t;
class host_plugin_window;

using Module = VST3::Hosting::Module;
using tresult = Steinberg::tresult;
using ParamID = Steinberg::Vst::ParamID;
using ParamValue = Steinberg::Vst::ParamValue;

namespace DAW {
    constexpr bool gVST3UseSampleAccurateModulation = true;
}

class vst3plugin final : public effectbase {
    VST3::UID uid{};
    VST3::Hosting::Module::Ptr module;
    std::shared_ptr<Steinberg::Vst::PlugProvider> pluginProvider;
    Steinberg::Vst::IComponent* vst3Component = nullptr;
    Steinberg::Vst::IAudioProcessor* vst3AudioProcessor = nullptr;
    Steinberg::Vst::IMidiMapping* vst3MidiMapping = nullptr;
    Steinberg::Vst::IEditController* editController = nullptr;
    Steinberg::Vst::IUnitInfo* unitInfo = nullptr;
    Steinberg::Vst::IEditControllerHostEditing* editControllerHostEditing = nullptr;
    Steinberg::Vst::HostProcessData processData = {};
	Steinberg::Vst::ProcessContext processContext = {};
    int32_t numInputEventBuses = 0;
    int32_t numOutputEventBuses = 0;
	Steinberg::IPtr<Steinberg::IPlugView> view = nullptr;
    Steinberg::Vst::ParameterInfo programChangeParameter{};
    bool bHasProgramChangeParameter = false;
    bool bIsPostInit       = false;
    bool bIsLoadingProgram = false;
    bool bIsInSuspend      = true;
    class ComponentHandler : public Steinberg::Vst::IComponentHandler
    {
        DawInstance* const daw;
        vst3plugin* const plugin;
        struct param_editing_t {
            uint32_t paramIdx = 0;
            float   valBefore = 0;
        };
        param_editing_t paramEditing;
    public:
        ComponentHandler(DawInstance* daw, vst3plugin* plugin) : daw(daw), plugin(plugin) {
        }

        tresult PLUGIN_API beginEdit (ParamID id) override
        {
            if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::MainThread) {
                // log_lf(Log::L_ERROR, "%s: Requires Main Thread!\n", StringAsCStr(plugin->getName()));
                return Steinberg::kResultFalse;
            }
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
            if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::MainThread) {
                // log_lf(Log::L_ERROR, "%s: Requires Main Thread!\n", StringAsCStr(plugin->getName()));
                return Steinberg::kResultFalse;
            }
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
            if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::MainThread) {
                // log_lf(Log::L_ERROR, "%s: Requires Main Thread!\n", StringAsCStr(plugin->getName()));
                return Steinberg::kResultFalse;
            }
            if (paramEditing.paramIdx == id) {
                auto* effParam = plugin->getEffectParam(id);
                if (!effParam) {
                    log_printf("%s audioMasterAutomate unknown param index %u\n", StringAsCStr(plugin->getName()), id);
                } else {
                    auto newVal = effParam->getValue();
                    auto oldVal = paramEditing.valBefore;
                    track_t* track              = plugin->trackImpl->getTrack();
                    automatable_param_ref_t ref = plugin->toRef();
                    parameter_ref_t p           = { track->projectIdx, ref.type, plugin->projectGlobalId, effParam->idx };
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
                // normally we would not allow this to be called from non-main thread,
                // but yabridge does call this from their own ui thread, so we allow it
                // but we have to be careful what state we modify here
                auto lock = daw->lockPlayThread();
                plugin->visitParams([editController=plugin->editController](auto& mapEntry) {
                    automatable_param_t& param = mapEntry.second;
                    if (param.internalIdx >= 0) {
                        param.paramNameState |= PARAM_FLAG_DIRTY;
                        param.paramDisplayValState |= PARAM_FLAG_DIRTY;
                        if (editController) {
                            param.setValue(editController->getParamNormalized(param.internalIdx));
                            param.paramValueState = PARAM_FLAG_SET;
                        }
                    }
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
        tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid, void** obj) override
        {
            QUERY_INTERFACE(_iid, obj, Steinberg::Vst::IComponentHandler::iid, Steinberg::Vst::IComponentHandler)
            return Steinberg::kNoInterface;
        }
        // we do not care here of the ref-counting. A plug-in call of release should not destroy this
        // class!
        Steinberg::uint32 PLUGIN_API addRef () override { return 1000; }
        Steinberg::uint32 PLUGIN_API release () override { return 1000; }
    };
#ifdef __linux__
    class RunLoopLinux : public Steinberg::Linux::IRunLoop
    {
        struct RunLoopTimer {
            Steinberg::Linux::ITimerHandler* handler;
            int64_t periodInMs;
            int64_t lastCallTimeInMs;
        };
        struct RunLoopFDEventHandler
        {
            Steinberg::Linux::IEventHandler* handler;
            int epollfd;
            int pluginfd;
        };
        std::vector<RunLoopTimer> timers;
        std::vector<RunLoopFDEventHandler> fdEventHandlers;
    public:
        virtual ~RunLoopLinux() = default;
        tresult PLUGIN_API registerEventHandler (Steinberg::Linux::IEventHandler* handler, Steinberg::Linux::FileDescriptor pluginFd) override {
            int epollfd = epoll_create1(0);
            if (epollfd < 0) {
                log_lf(Log::L_ERROR, "RunLoopLinux: Failed to create epoll instance!\n");
                return Steinberg::kNoInterface;
            }
            struct ::epoll_event ev = {};
            ev.events = EPOLLIN|EPOLLOUT;
            ev.data.fd = pluginFd;

            if (::epoll_ctl(epollfd, EPOLL_CTL_ADD, pluginFd, &ev) < 0)
            {
                ::close(epollfd);
                return Steinberg::kInternalError;
            }
            fdEventHandlers.push_back(RunLoopFDEventHandler{ handler, epollfd, pluginFd });
            log_lf(Log::L_DEBUG, "RunLoopLinux: Registered event handler for fd %d\n", pluginFd);
            return Steinberg::kResultOk;
        }
        tresult PLUGIN_API unregisterEventHandler (Steinberg::Linux::IEventHandler* handler) override {
            for (auto it = fdEventHandlers.begin(); it != fdEventHandlers.end(); ++it) {
                if (it->handler == handler) {
                    epoll_ctl(it->epollfd, EPOLL_CTL_DEL, it->pluginfd, nullptr);
                    close(it->epollfd);
                    fdEventHandlers.erase(it);
                    log_lf(Log::L_DEBUG, "RunLoopLinux: Unregistered event handler %p for fd %d\n", (void*)handler, it->pluginfd);
                    return Steinberg::kResultOk;
                }
            }
            log_lf(Log::L_ERROR, "RunLoopLinux: Event handler %p not found!\n", (void*)handler);
            return Steinberg::kNoInterface;
        }

        tresult PLUGIN_API registerTimer (Steinberg::Linux::ITimerHandler* handler, Steinberg::Linux::TimerInterval milliseconds) override {
            if (milliseconds <= 0) {
                log_lf(Log::L_ERROR, "RunLoopLinux: Invalid timer interval %zu\n", milliseconds);
                return Steinberg::kInvalidArgument;
            }
            for (auto& timer : timers) {
                if (timer.handler == handler) {
                    log_lf(Log::L_ERROR, "RunLoopLinux: Timer %p registered (%zu, requested: %zu)!\n", (void*)handler, timer.periodInMs, milliseconds);
                    return Steinberg::kResultFalse;
                }
            }
            timers.push_back({ handler, static_cast<int64_t>(milliseconds), 0 });
            log_lf(Log::L_DEBUG, "RunLoopLinux: Registered timer %p with interval %zu ms\n", (void*)handler, milliseconds);
            return Steinberg::kResultOk;
        }
        tresult PLUGIN_API unregisterTimer (Steinberg::Linux::ITimerHandler* handler) override {
            for (auto it = timers.begin(); it != timers.end(); ++it) {
                if (it->handler == handler) {
                    log_lf(Log::L_DEBUG, "RunLoopLinux: Unregistered timer %p\n", (void*)handler);
                    timers.erase(it);
                    return Steinberg::kResultOk;
                }
            }
            log_lf(Log::L_ERROR, "RunLoopLinux: Timer %p not found!\n", (void*)handler);
            return Steinberg::kNoInterface;
        }
        void updateTimersFromMainThread() {
            int64_t currentTimeInMs = getTimeMillis();
            for (auto& timer : timers) {
                if (timer.lastCallTimeInMs == 0 ||
                    currentTimeInMs - timer.lastCallTimeInMs >= timer.periodInMs) {
                    timer.handler->onTimer();
                    timer.lastCallTimeInMs = currentTimeInMs;
                }
            }
            for (auto it = fdEventHandlers.begin(); it != fdEventHandlers.end(); ++it) {
                struct epoll_event events{};
                for (int i = 20; i > 0; --i) {
                    int ret = epoll_wait(it->epollfd, &events, 1, 0);
                    if (ret < 0) {
                        String err = FormatErrorMessage(errno, "RunLoopLinux: epoll_wait failed");
                        log_lf(Log::L_ERROR, "%s\n", StringAsCStr(err));
                    } else if (ret == 0) {
                        i = 0; // no events, exit loop
                    } else {
                        it->handler->onFDIsSet(it->pluginfd);
                    }
                }
            }
        }
    private:
        tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid, void** obj) override
        {
            QUERY_INTERFACE(_iid, obj, Steinberg::Linux::IRunLoop::iid, Steinberg::Linux::IRunLoop)
            return Steinberg::kNoInterface;
        }
        // we do not care here of the ref-counting. A plug-in call of release should not destroy this
        // class!
        Steinberg::uint32 PLUGIN_API addRef () override { return 1000; }
        Steinberg::uint32 PLUGIN_API release () override { return 1000; }
    //------------------------------------------------------------------------
    };
#endif // __linux__
    class PlugFrame : public Steinberg::IPlugFrame
    {
        vst3plugin* const plugin;
#ifdef __linux__
        RunLoopLinux& runLoopLinux;
    public:
        PlugFrame(vst3plugin* plugin, RunLoopLinux& runLoopLinux) : plugin(plugin), runLoopLinux(runLoopLinux) {
        }
#else
    public:
        PlugFrame(vst3plugin* plugin) : plugin(plugin) {
        }
#endif
	    tresult PLUGIN_API resizeView (Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override {
            if (plugin->windowHost && newSize)
                plugin->windowHost->resize(ivec2(newSize->right - newSize->left, newSize->bottom - newSize->top));
            if (view)
                view->onSize(newSize);
            return Steinberg::kResultOk;
        }
    private:
        tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid, void** obj) override
        {
            QUERY_INTERFACE(_iid, obj, Steinberg::IPlugFrame::iid, Steinberg::IPlugFrame)
#ifdef __linux__
            if (::Steinberg::FUnknownPrivate::iidEqual(_iid, Steinberg::Linux::IRunLoop::iid))
            {
                addRef();
                *obj = &runLoopLinux;
                return Steinberg::kResultOk;
            }
#endif // __linux__
            return Steinberg::kNoInterface;
        }
        // we do not care here of the ref-counting. A plug-in call of release should not destroy this
        // class!
        Steinberg::uint32 PLUGIN_API addRef () override { return 1000; }
        Steinberg::uint32 PLUGIN_API release () override { return 1000; }
    };

    ComponentHandler componentHandler;
#ifdef __linux__
    RunLoopLinux runLoop;
#endif
    PlugFrame plugFrame;
    Steinberg::Vst::ParameterChanges inputParameterChanges;
    Steinberg::Vst::ParameterChanges outputParameterChanges;

    struct ParamModulation {
        int32_t index;
        std::vector<float> values;
    };

    std::vector<ParamModulation> paramModulations;
    std::vector<ParamModulation> paramAutomations;
public:
    vst3plugin(VST3::UID uid, VST3::Hosting::Module::Ptr&& _module, std::shared_ptr<Steinberg::Vst::PlugProvider>&& _pluginProvider, int32_t globalId, IHostCallback* hostcallback, String _sDir, int32_t _bugfixFlags) 
        : effectbase(_pluginProvider->getClassInfo().name(), globalId, hostcallback),
            uid(uid),
            module(_module),
            pluginProvider(_pluginProvider),
            componentHandler(daw_tls::getTls().dawInstance, this),
#ifdef __linux__
            plugFrame(this, runLoop)
#else
            plugFrame(this)
#endif
    {
        processData.inputParameterChanges = &inputParameterChanges;
        processData.outputParameterChanges = &outputParameterChanges;
        processData.processContext = &processContext;
    }
    ~vst3plugin() override {
        if (processData.inputEvents) delete[] dynamic_cast<Steinberg::Vst::EventList*>(processData.inputEvents);
        if (processData.outputEvents) delete[] dynamic_cast<Steinberg::Vst::EventList*>(processData.outputEvents);
        if (processData.inputs) delete[] dynamic_cast<Steinberg::Vst::AudioBusBuffers*>(processData.inputs);
        if (processData.outputs) delete[] dynamic_cast<Steinberg::Vst::AudioBusBuffers*>(processData.outputs);
        processData.inputParameterChanges = nullptr;
        processData.outputParameterChanges = nullptr;
        processData.inputEvents = nullptr;
        processData.outputEvents = nullptr;
        processData.inputs = nullptr;
        processData.outputs = nullptr;
        processData.processContext = nullptr;
    }
    /**
     * loadVST3Plugin
     * @brief Load the VST3 plugin. This method is called right after construction.
     *        It will load the plugin and set up the necessary data structures.
     * @return true if the plugin was loaded successfully
     */
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
        vst3MidiMapping = FUnknownPtr<IMidiMapping>(vst3Component);
        editController = pluginProvider->getController();
        if (editController) {
            editController->setComponentHandler(&componentHandler);
            unitInfo = FUnknownPtr<IUnitInfo>(editController);
        }

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
            if (kResultOk != vst3Component->getBusInfo(kEvent, kOutput, i, info))
                return false;
            if (kResultOk != vst3Component->activateBus(kEvent, kOutput, i, false))
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
        recvProgramListUpdate();
        return true;
    }
    void recvProgramListUpdate() {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        this->programNames.resize(0);
        constexpr bool bDebugPrintPrograms = false;
        if (unitInfo) {
            auto unitCount = unitInfo->getUnitCount();
            auto programListCount = unitInfo->getProgramListCount();
            auto selectedUnit = unitInfo->getSelectedUnit();
            auto selectedProgramListId = int32_t(-1);
            if constexpr(bDebugPrintPrograms) 
                log_lf(Log::L_DEBUG, "%s: UnitInfo: %d units, %d program lists\n", StringAsCStr(sName), unitCount, programListCount);
            for (int i = 0; i < unitCount; ++i) {
                UnitInfo info{};
                if (unitInfo->getUnitInfo(i, info) == kResultOk) {
                    if constexpr(bDebugPrintPrograms) {
                        auto cxxName = VST3::StringConvert::convert(info.name);
                        log_lf(Log::L_DEBUG, "Unit %d: id=%d, parentId=%d, name=%s, programListId=%d\n", i, info.id, info.parentUnitId, StringAsCStr(cxxName), info.programListId);
                    }
                    if (info.id == selectedUnit) {
                        selectedProgramListId = info.programListId;
                    }
                }
            }
            bool bFirst = true;
            for (int i = 0; i < programListCount; ++i) {
                ProgramListInfo info{};
                if (unitInfo->getProgramListInfo(i, info) == kResultOk) {
                    if (info.id == selectedProgramListId) {
                        if constexpr(bDebugPrintPrograms) {
                            auto cxxName = VST3::StringConvert::convert(info.name);
                            log_lf(Log::L_DEBUG, "ProgramList %d: id=%d, name=%s, programCount=%d\n", i, info.id, StringAsCStr(cxxName), info.programCount);
                        }
                        for (int j = 0; info.programCount > 1 && j < info.programCount; ++j) {
                            String128 name{};
                            if (unitInfo->getProgramName(info.id, j, name) == kResultOk) {
                                auto cxxName = VST3::StringConvert::convert(name);
                                if constexpr(bDebugPrintPrograms) {
                                    bool bHasPitchNames = unitInfo->hasProgramPitchNames(info.id, j) == kResultTrue;
                                    log_lf(Log::L_DEBUG, "Program %d: name=%s, hasPitchNames=%d\n", j, StringAsCStr(cxxName), bHasPitchNames);
                                }
                                this->programNames.push_back(cxxName);
                                if (bFirst) {
                                    bFirst = false;
                                    this->currentProgramNameStr = cxxName;
                                    this->currentProgramNameSet = true;
                                    updateProgramPitchNames();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void updateProgramPitchNames() {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        programPitchNames.clear();
        auto unitCount = unitInfo->getUnitCount();
        auto selectedUnit = unitInfo->getSelectedUnit();
        auto selectedProgramListId = int32_t(-1);
        for (int i = 0; i < unitCount; ++i) {
            UnitInfo info{};
            if (unitInfo->getUnitInfo(i, info) == kResultOk) {
                if (info.id == selectedUnit) {
                    selectedProgramListId = info.programListId;
                }
            }
        }
        for (int i = 0; i < unitCount; ++i) {
            ProgramListInfo info{};
            if (unitInfo->getProgramListInfo(i, info) == kResultOk) {
                if (info.id == selectedProgramListId) {
                    for (int j = 0; info.programCount > 1 && j < info.programCount; ++j) {
                        String128 name{};
                        if (unitInfo->getProgramName(info.id, j, name) == kResultOk && 
                            VST3::StringConvert::convert(name) == this->currentProgramNameStr) {
                            bool bHasPitchNames = unitInfo->hasProgramPitchNames(info.id, j) == kResultTrue;
                            if (bHasPitchNames) {
                                for (int16 k = 0; k < 128; ++k) {
                                    String128 pitchName{};
                                    if (unitInfo->getProgramPitchName(info.id, j, k, pitchName) == kResultOk) {
                                        auto cxxPitchName = VST3::StringConvert::convert(pitchName);
                                        programPitchNames[k] = cxxPitchName;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    bool getCurrentProgramName(String& out) override {
        out = this->currentProgramNameStr;
        return this->currentProgramNameSet;
    }

    bool setCurrentProgram(uint32_t idx) override {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        if (!editController) {
            return false;
        }
        if (!bHasProgramChangeParameter) {
            return false;
        }
        if (programChangeParameter.id == kNoParamId) {
            return false;
        }
        if (programChangeParameter.stepCount < 1) {
            return false;
        }
        if (int32(idx) > programChangeParameter.stepCount || idx < 0 || idx >= this->programNames.size()) {
            log_lf(Log::L_ERROR, "%s: Invalid program index %d\n", StringAsCStr(sName), idx);
            return false;
        }
        ParamValue normalized = (ParamValue)idx / (ParamValue)programChangeParameter.stepCount;
        if (editController->setParamNormalized(programChangeParameter.id, normalized) != kResultOk) {
            log_lf(Log::L_ERROR, "%s: Failed to set program\n", StringAsCStr(sName));
            return false;
        }
        // set program name from local list
        this->currentProgramNameStr = this->programNames[idx];
        return true;
    }

    String getUID() const { return uid.toString(true); }
    // VST3::Hosting::ClassInfo getClassInfo() const { return pluginProvider->getClassInfo(); }
    ModuleType getModuleType() override { return MODULE_TYPE_VST3; };
    String getAutomatableName() override { return this->sName; }
    Steinberg::Vst::ProcessContext& getVst3ProcessContext() { return processContext; }
    void setProcessingMode(Steinberg::Vst::ProcessModes pm) {
        if (processData.processMode != pm) {
            // checkForMainThread();
            bool bReactivate = !bIsInSuspend;
            if (bReactivate) {
                vst3AudioProcessor->setProcessing(false);
            }
            processData.processMode = pm;
            setupProcessingImpl();
            if (bReactivate) {
                vst3AudioProcessor->setProcessing(true);
            }
        }
    }
private:
    void setupProcessingImpl() {
        checkForMainThread();
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        ProcessSetup processSetup = {};
        processSetup.symbolicSampleSize = processData.symbolicSampleSize;
        processSetup.processMode = processData.processMode;
        processSetup.maxSamplesPerBlock = format.blockSize;
        processSetup.sampleRate = format.sampleRate;

        if (kResultOk != vst3AudioProcessor->setupProcessing(processSetup)) {
            log_lf(Log::L_ERROR, "%s: Failed to setup VST3 plugin processing\n", StringAsCStr(sName));
            bIsEnabled = false;
        }
    }

    void checkForMainThread() {
        if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::MainThread) {
            dbgassert(0);
            throw std::logic_error("Requires Main Thread!");
        }
    }

    void deactivate() {
        if (bIsInSuspend)
            return;
        checkForMainThread();
        vst3AudioProcessor->setProcessing(false);
        if (Steinberg::kResultOk != vst3Component->setActive(false)) {
            log_lf(Log::L_ERROR, "%s: Failed to deactivate VST3 plugin\n", StringAsCStr(sName));
            bIsEnabled = false;
        }
        bIsInSuspend = true;
    }

    void activate() {
        if (!bIsInSuspend)
            return;
        checkForMainThread();
        if (Steinberg::kResultOk != vst3Component->setActive(true)) {
            log_lf(Log::L_ERROR, "%s: Failed to activate VST3 plugin\n", StringAsCStr(sName));
            bIsEnabled = false;
        }
        vst3AudioProcessor->setProcessing(bIsEnabled);
        bIsInSuspend = false;
    }

    void unloadVst3Plugin() {
        checkForMainThread();
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        if (view) {
            view->release();
            view = nullptr;
        }
        if (vst3Component) {
            auto numInAudioBuses = vst3Component->getBusCount(MediaTypes::kAudio, BusDirections::kInput);
            auto numOutAudioBuses = vst3Component->getBusCount(MediaTypes::kAudio, BusDirections::kOutput);
            auto numInEventBuses = vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kInput);
            auto numOutEventBuses = vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kOutput);
            for (int i = 0; i < numInEventBuses; ++i) {
                vst3Component->activateBus(kEvent, kInput, i, false);
            }
            for (int i = 0; i < numOutEventBuses; ++i) {
                vst3Component->activateBus(kEvent, kOutput, i, false);
            }
            for (int i = 0; i < numInAudioBuses; ++i) {
                vst3Component->activateBus(kAudio, kInput, i, false);
            }
            for (int i = 0; i < numOutAudioBuses; ++i) {
                vst3Component->activateBus(kAudio, kOutput, i, false);
            }
        }
		pluginProvider->releasePlugIn (vst3Component, editController);
        editController = nullptr;
        vst3AudioProcessor = nullptr;
        vst3Component = nullptr;
    }

    void scanParams() {
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        auto count = !editController ? 0 : editController->getParameterCount();
        for (int32_t i = 0; i < count; ++i) {
            int32_t paramIdentifier    = PARAM_OFFSET_EXTERNAL + i;
            ParameterInfo info{};
            if (kResultOk != editController->getParameterInfo(i, info)) {
                log_lf(Log::L_ERROR, "%s: Failed to get VST3 plugin parameter info\n", StringAsCStr(sName));
                continue;
            }
            automatable_param_t* param = registerParam(paramIdentifier);
            if (info.flags & ParameterInfo::kIsProgramChange) {
                programChangeParameter = info;
                bHasProgramChangeParameter = true;
                param->isHidden = false;
            }
            else if (info.flags & ParameterInfo::kIsHidden) {
                param->isHidden = true;
            }
            if (info.flags & ParameterInfo::kIsReadOnly) {
                param->isReadOnly = true;
            }
            if (info.flags & ParameterInfo::kIsBypass) {
                param->isHidden = true;
            }
            auto title = VST3::StringConvert::convert(info.title);
            if (title.find("MIDI CC") != std::string::npos) {
                param->isHidden = true;
            }
            param->isAutomatable = info.flags & ParameterInfo::kCanAutomate;
            param->internalIdx = info.id;
            param->name = VST3::StringConvert::convert(info.title);
            param->shortLabel = VST3::StringConvert::convert(info.shortTitle);
            param->paramNameState = PARAM_FLAG_SET;
            param->paramDisplayValState = PARAM_FLAG_SET;
            param->paramValueState = PARAM_FLAG_SET;
            param->unit = VST3::StringConvert::convert(info.units);
            param->vst3UnitId = info.unitId;
            param->quantizationSteps = info.stepCount;
            String128 paramValueStr{};
            if (kResultOk != editController->getParamStringByValue(info.id, info.defaultNormalizedValue, paramValueStr)) {
                log_lf(Log::L_ERROR, "%s: Failed to get VST3 plugin parameter info\n", StringAsCStr(sName));
                continue;
            }
            param->paramDisplayValStr = VST3::StringConvert::convert(paramValueStr);
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
                log_lf(Log::L_ERROR, "%s: Failed to get VST3 plugin input bus info\n", StringAsCStr(sName));
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
                log_lf(Log::L_ERROR, "%s: Failed to get VST3 plugin output bus info\n", StringAsCStr(sName));
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
        
        numInputEventBuses = vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kInput);
        numOutputEventBuses = 0; //vst3Component->getBusCount(MediaTypes::kEvent, BusDirections::kOutput); // INACTIVE
        if (processData.inputEvents) delete[] dynamic_cast<EventList*>(processData.inputEvents);
        if (processData.outputEvents) delete[] dynamic_cast<EventList*>(processData.outputEvents);
        if (numInputEventBuses > 0) {
            processData.inputEvents = new EventList[numInputEventBuses];
        } else {
            processData.inputEvents = nullptr;
        }
        if (numOutputEventBuses > 0) {
            processData.outputEvents = new EventList[numOutputEventBuses];
        } else {
            processData.outputEvents = nullptr;
        }
    }
public:
    void load(DAW::Host::PluginManager* host) override {
        effectbase::load(host);
        activate();
    }

    void onDisable() override { deactivate(); }

    void onEnable() override { 
        activate(); 
        if (!bIsPostInit) {
            bIsPostInit = true;
            visitParams([editController=this->editController](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                if (param.internalIdx >= 0) {
                    param.paramNameState |= PARAM_FLAG_DIRTY;
                    param.paramDisplayValState |= PARAM_FLAG_DIRTY;
                    if (editController) {
                        param.setValue(editController->getParamNormalized(param.internalIdx));
                        param.paramValueState = PARAM_FLAG_SET;
                    }
                }
            });
        }
    }

    void unload(DAW::Host::PluginManager* host) override {
        effectbase::unload(host);
        deactivate();
        unloadVst3Plugin();
    }
    
    void setSampleFormat(sampleformat_t sampleFormat) override {
        effectbase::setSampleFormat(sampleFormat);
    }

    void initBuffers() override {
        configureIOPorts();
        effectbase::initBuffers();
        using namespace Steinberg;
        using namespace Steinberg::Vst;
        processData.symbolicSampleSize = format.sampleformat == sampleformat_bits_t::FLOAT_32 ? SymbolicSampleSizes::kSample32 : SymbolicSampleSizes::kSample64;

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
        setupProcessingImpl();
    }

    void updateVst3FromMainThread() {
#ifdef __linux__
        runLoop.updateTimersFromMainThread();
#endif
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
            if (!(flags & (FLG_PAR_UPDATE_FROM_CLIENT | FLG_PAR_UPDATE_MODULATED | FLG_PAR_UPDATE_AUTOMATED)) && editController) {
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

    const String& getParamName(int32_t idx) override {
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

    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double dTick, double samplePos, int32_t numSamples, playback_state state) override {
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

        auto& automationLanes = getAutomationLanes();
        const auto& mapModulations = getActiveModulations();

        auto tempo100 = host->prjGlobals.tempo100;
        // auto samplesToTicks = sampleToTickConvert<double, roundmode::none>(1.0, tempo100, format.sampleRate);
        auto ticksPerBlock = sampleToTickConvert<double, roundmode::none>(numSamples, tempo100, format.sampleRate);
        auto dTickEnd = dTick + ticksPerBlock;

        size_t numMods = 0;
        size_t numAutomations = 0;
        if (DAW::gVST3UseSampleAccurateModulation) {
            for (const auto& entry : mapModulations) {
                int32_t paramIdx = entry.first;
                auto param = getParam(paramIdx);
                if (!assert_expr(param) || param->internalIdx < 0) {
                    continue;
                }
                ParameterInfo info{};
                if (kResultOk != editController->getParameterInfo(param->idx - PARAM_OFFSET_EXTERNAL, info)) {
                    log_lf(Log::L_ERROR, "%s: Failed to get VST3 plugin parameter info\n", StringAsCStr(sName));
                    continue;
                }
                if (!(info.flags & ParameterInfo::kCanAutomate)
                    || (info.flags & ParameterInfo::kIsReadOnly)) {
                    continue;
                }
                while (paramModulations.size() <= numMods) {
                    paramModulations.emplace_back();
                }
                auto& paramModulation = paramModulations[numMods++];
                paramModulation.index = paramIdx;
                auto& values = paramModulation.values;
                values.resize(numSamples);
                std::fill(values.begin(), values.end(), param->getValue());
                auto* autLane = getRegisteredAutomation(paramIdx);
                if (autLane && autLane->isActive() && DAW::isPlaybackState(state)) {
                    autLane->sampleAutomation(dTick, dTickEnd, numSamples, param->getAutomationScale(), values.data());
                }
                auto& modulations = entry.second;
                for (const auto* mod : modulations) {
                    auto ch = DAW::ResolveModulationChannel(host, *mod);
                    if (ch && ch->isActive()) {
                        ch->sampleAutomation(dTick, dTickEnd, numSamples, mod->scale, values.data());
                    }
                }
            }
            if (DAW::isPlaybackState(state)) {
                for (const auto& automLane : automationLanes) {
                    if (automLane.isActive()) {
                        if (mapModulations.count(automLane.paramIdx) > 0) {
                            continue;
                        }
                        const auto paramIdx = automLane.paramIdx;
                        auto param = getParam(paramIdx);
                        if (!assert_expr(param) || param->internalIdx < 0) {
                            continue;
                        }
                        ParameterInfo info{};
                        if (kResultOk != editController->getParameterInfo(param->idx - PARAM_OFFSET_EXTERNAL, info)) {
                            log_lf(Log::L_ERROR, "%s: Failed to get VST3 plugin parameter info\n", StringAsCStr(sName));
                            continue;
                        }
                        if (!(info.flags & ParameterInfo::kCanAutomate)
                            || (info.flags & ParameterInfo::kIsReadOnly)) {
                            continue;
                        }
                        auto* autLane = getRegisteredAutomation(paramIdx);
                        if (autLane && autLane->isActive()) {
                            while (paramAutomations.size() <= numAutomations) {
                                paramAutomations.emplace_back();
                            }
                            auto& paramAutomation = paramAutomations[numAutomations++];
                            paramAutomation.index = automLane.paramIdx;
                            auto& values = paramAutomation.values;
                            values.resize(numSamples);
                            std::fill(values.begin(), values.end(), param->getValue());
                            autLane->sampleAutomation(dTick, dTickEnd, numSamples, param->getAutomationScale(), values.data());
                        }
                    }
                }
            }
        }
#ifndef NDEBUG
        // assert that none of the parameters are in both paramModulations and paramAutomations
        for (size_t i = 0; i < numMods; ++i) {
            for (size_t j = 0; j < numAutomations; ++j) {
                dbgassert(paramModulations[i].index != paramAutomations[j].index);
            }
        }
#endif
        int32_t queueIndex = 0;
        for (size_t n = 0; n < numAutomations; ++n) {
            auto& paramAutomation = paramAutomations[n];
            auto dawparam = getParam(paramAutomation.index);
            if (!assert_expr(dawparam) || dawparam->internalIdx < 0) {
                continue;
            }
            auto& vecData = paramAutomation.values;
            auto numPoints = samplecount_t(vecData.size());
            auto queue = inputParameterChanges.addParameterData(dawparam->internalIdx, queueIndex);
            for (samplecount_t s = 0; s < numSamples && s < numPoints; ++s) {
                queue->addPoint(s, vecData[s], queueIndex);
            }
        }
        queueIndex = 0;
        for (size_t n = 0; n < numMods; ++n) {
            auto& paramMod = paramModulations[n];
            auto dawparam = getParam(paramMod.index);
            if (!assert_expr(dawparam) || dawparam->internalIdx < 0) {
                continue;
            }
            auto& vecData = paramMod.values;
            auto numPoints = samplecount_t(vecData.size());
            auto queue = inputParameterChanges.addParameterData(dawparam->internalIdx, queueIndex);
            for (samplecount_t s = 0; s < numSamples && s < numPoints; ++s) {
                queue->addPoint(s, vecData[s], queueIndex);
            }
        }

        if (vst3AudioProcessor->process(processData) != kResultOk) {
#ifndef NDEBUG
            log_lf(Log::L_ERROR, "VST3 plugin process failed (%s)\n", StringAsCStr(sName));
#endif
        }

        inputParameterChanges.clearQueue();
        // print output
        int32_t outCount = outputParameterChanges.getParameterCount();
        if (outCount > 0) {
            for (int32_t i = 0; i < outCount; ++i) {
                IParamValueQueue* queue = outputParameterChanges.getParameterData(i);
                if (queue) {
                    int32_t pointCount = queue->getPointCount();
                    for (int32_t j = 0; j < pointCount; ++j) {
                        int32 sampleOffset = 0;
                        ParamValue value = 0;
                        queue->getPoint(j, sampleOffset, value);
                    }
                }
            }
            outputParameterChanges.clearQueue();
        }
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
            plugin->visitParams([&ps, usesBinaryChunks](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                if (param.inUse || !usesBinaryChunks) {
                    float curValue = param.getValue();
                    int paramFlags = param.inUse ? 1 : 0;
                    if (((param.paramValueState & PARAM_FLAG_DIRTY) || !(param.paramValueState & PARAM_FLAG_SET)) && param.internalIdx >= 0) {
                        log_lf(Log::L_WARN, "VST3: Parameter %s is dirty\n", param.name.c_str());
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
        createSnapshot(ps, this, opts);
        for (auto& [uuid, gui] : uiInstances) {
            plugin_ui_snapshot_t uiSnapshot;
            gui->makeSnapshot(uiSnapshot, opts);
            ps.uiSnapshots[uuid] = uiSnapshot;
        }
        ps.windowLayout = getWindowLayoutSnapshot();
        ps.slot = this->slot;
    }

    void loadSnapshot(const plugin_snapshot_t& pluginSnapshot) override {
        this->bIsLoadingProgram = true;
        if (pluginSnapshot.dataChunk.size() > 0) {
            Steinberg::Vst::BufferStream stream;
            Steinberg::int32 bytesWritten = 0;
            auto nonConst = const_cast<std::vector<uint8_t>&>(pluginSnapshot.dataChunk);
            stream.write(nonConst.data(), nonConst.size(), &bytesWritten);
            stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
            if (Steinberg::kResultOk != vst3Component->setState(&stream)) {
                log_lf(Log::L_ERROR, "%s: Failed to load VST3 plugin state 0\n", StringAsCStr(sName));
            }
            if (editController) {
                stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
                if (Steinberg::kResultOk == editController->setComponentState(&stream)) {
                    log_lf(Log::L_ERROR, "%s: Failed to load VST3 plugin state 1\n", StringAsCStr(sName));
                }
                nonConst = const_cast<std::vector<uint8_t>&>(pluginSnapshot.dataChunk2);
                stream.write(nonConst.data(), nonConst.size(), &bytesWritten);
                stream.seek(0, Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
                if (Steinberg::kResultOk == editController->setState(&stream)) {
                    log_lf(Log::L_ERROR, "%s: Failed to load VST3 plugin state 2\n", StringAsCStr(sName));
                }
            }

        }
        DAW::loadEffectParamsFromSnapshot(pluginSnapshot, this);
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
#ifdef _WIN32
        auto platformWindowType = Steinberg::kPlatformTypeHWND;
#elif defined(__linux__)
        auto platformWindowType = Steinberg::kPlatformTypeX11EmbedWindowID;
#elif defined(__APPLE__)
        auto platformWindowType = Steinberg::kPlatformTypeNSView;
#else
        return false;
#endif

        if (!view) {
            view = editController->createView(Vst::ViewType::kEditor);
            if (!view) {
                log_lf(Log::L_ERROR, "%s: Failed to create editor view\n", StringAsCStr(sName));
                return false;
            }
            if (view->isPlatformTypeSupported(platformWindowType) != Steinberg::kResultTrue) {
                view->release();
                view = nullptr; // Clean up on failure
                log_lf(Log::L_ERROR, "%s: Editor view does not support platform type %s\n", StringAsCStr(sName), platformWindowType);
                return false;
            }
        }
        ViewRect viewRect = {};
        if (view->getSize(&viewRect) != kResultOk) {
            view->release();
            view = nullptr; // Clean up on failure
            log_lf(Log::L_ERROR, "%s: Failed to get editor view size\n", StringAsCStr(sName));
            return false;
        }

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
        if (this->windowHost == _window && view) {
            bEditOpen = true;
            this->updateFromMainThread();
            view->setFrame(&plugFrame);
            auto voidWindowHandle = reinterpret_cast<void*>(_window->getWindowHandle());
#ifdef _WIN32
            auto platformWindowType = Steinberg::kPlatformTypeHWND;
#elif defined(__linux__)
            auto platformWindowType = Steinberg::kPlatformTypeX11EmbedWindowID;
#elif defined(__APPLE__)
            auto platformWindowType = Steinberg::kPlatformTypeNSView;
#else
            return false;
#endif
            if (view->attached(voidWindowHandle, platformWindowType) != Steinberg::kResultOk) {
                log_lf(Log::L_ERROR, "%s: Failed to attach editor view to HWND\n", StringAsCStr(sName));
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
                    log_lf(Log::L_ERROR, "%s: Failed to remove editor view\n", StringAsCStr(sName));
                }
                if (Steinberg::kResultOk != view->setFrame(nullptr)) {
                    log_lf(Log::L_ERROR, "%s: Failed to set editor view frame to nullptr\n", StringAsCStr(sName));
                }
            }
        }
        bEditOpen = false;
        return true;
    }

    void processMidi(midi_data_processing_t& midiEvents) override {
        if (!bCanReceiveMidi) {
            return;
        }

        const int32_t midiBusIndex = 0;
        if (!processData.inputEvents) {
            return;
        }

        if (numInputEventBuses <= midiBusIndex) {
            return;
        }

        Steinberg::Vst::EventList& eventList = dynamic_cast<Steinberg::Vst::EventList*>(processData.inputEvents)[midiBusIndex];
        eventList.clear();
        auto numEvents = int32_t(midiEvents.noteEvents->size() + midiEvents.ctrlEvents->size());
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
                    noteOnEvent.channel = noteEvent.channel;
                    noteOnEvent.pitch = noteEvent.pitch;
                    noteOnEvent.tuning = 0.0f;
                    noteOnEvent.velocity = noteEvent.velocity / 127.0f;
                    noteOnEvent.length = 0;
                    noteOnEvent.noteId = -1;
                } else {
                    evt.type = Steinberg::Vst::Event::EventTypes::kNoteOffEvent;
                    auto& noteOffEvent = evt.noteOff;
                    noteOffEvent.channel = noteEvent.channel;
                    noteOffEvent.pitch = noteEvent.pitch;
                    noteOffEvent.velocity = noteEvent.velocity / 127.0f;
                    noteOffEvent.noteId = -1;
                    noteOffEvent.tuning = 0.0f;
                }
                eventList.addEvent(evt);
                this->midiEventsDispatched++;
            }
            if (vst3MidiMapping) {
                for (auto& midiCtrlEvt : *midiEvents.ctrlEvents) {
                    auto offsetInBlock = math::floordS32((midiCtrlEvt.tick - midiEvents.tickLatencyCompensated) * tickToSamples);
                    if (offsetInBlock < 0 || offsetInBlock >= format.blockSize) {
                        log_lf(Log::L_WARN, "ctrl event out of range: %d\n", offsetInBlock);
                        continue;
                    }
                    auto midi = IMidiMsg::FromU32AndTick(midiCtrlEvt.message, offsetInBlock);
                    auto midiCannel = midi.Channel();
                    switch (midi.StatusMsg()) {
                        case IMidiMsg::kChannelAftertouch: {
                            auto cc = Steinberg::Vst::ControllerNumbers::kAfterTouch;
                            ParamID paramId = ParamID(-1);
                            if (Steinberg::kResultOk == vst3MidiMapping->getMidiControllerAssignment(midiBusIndex, midiCannel, cc, paramId)) {
                                auto value = midi.mData1 / 127.0f;
                                int32_t queueIndex = 0;
                                auto queue = inputParameterChanges.addParameterData(paramId, queueIndex);
                                always_assert(Steinberg::kResultOk == queue->addPoint(offsetInBlock, value, queueIndex));
                            }
                            break;
                        }
                        case IMidiMsg::kPitchWheel: {
                            auto cc = Steinberg::Vst::ControllerNumbers::kPitchBend;
                            ParamID paramId = ParamID(-1);
                            if (Steinberg::kResultOk == vst3MidiMapping->getMidiControllerAssignment(midiBusIndex, midiCannel, cc, paramId)) {
                                auto value = midi.PitchWheel() * 0.5 + 0.5;
                                int32_t queueIndex = 0;
                                auto queue = inputParameterChanges.addParameterData(paramId, queueIndex);
                                always_assert(Steinberg::kResultOk == queue->addPoint(offsetInBlock, value, queueIndex));
                            }
                            break;
                        }
                        case IMidiMsg::kControlChange: {
                            auto cc = midi.mData1;
                            ParamID paramId = ParamID(-1);
                            if (Steinberg::kResultOk == vst3MidiMapping->getMidiControllerAssignment(midiBusIndex, midiCannel, cc, paramId)) {
                                auto value = midi.mData2 / 127.0f;
                                int32_t queueIndex = 0;
                                auto queue = inputParameterChanges.addParameterData(paramId, queueIndex);
                                always_assert(Steinberg::kResultOk == queue->addPoint(offsetInBlock, value, queueIndex));
                            }
                            break;
                        }
                        case IMidiMsg::kPolyAftertouch: {
                            Steinberg::Vst::Event evt{};
                            evt.busIndex = midiBusIndex;
                            evt.sampleOffset =  math::floordS32(offsetInBlock * tickToSamples);
                            evt.ppqPosition = midiCtrlEvt.tick / double(TICKS_QUARTER);
                            evt.flags = 0;
                            evt.type = Steinberg::Vst::Event::EventTypes::kPolyPressureEvent;
                            auto& polyPressureEvt = evt.polyPressure;
                            polyPressureEvt.channel = midiCannel;
                            polyPressureEvt.pitch = midi.mData1;
                            polyPressureEvt.pressure = midi.mData2 / 127.0f;
                            polyPressureEvt.noteId = -1;
                            eventList.addEvent(evt);
                            this->midiEventsDispatched++;
                            break;
                        }
                        case IMidiMsg::kNone:
                        case IMidiMsg::kNoteOff:
                        case IMidiMsg::kNoteOn:
                        case IMidiMsg::kProgramChange:
                            break;
                    }
                }
            }
        }
    }

    void sendNotesOff() override {

    }

    // bool setCurrentProgram(uint32_t idx) override;
    // bool getCurrentProgram(uint32_t& idx) override;
    // bool getNumberOfPrograms(uint32_t& numPrograms) override;
    // bool getCurrentProgramName(String& out) override;

    void addPropertiesTooltip(Table::tbl& table) override {
        using Table::tbl_row_t;
        using Table::tblfloat;
        using Table::tblint;
        using Table::tblstr;
        using Table::tblString;
        table.tableWidth = 350;
        table.colSizes.push_back(150);
        table.rows.push_back({ { String( "projectGlobalId"), (int) this->projectGlobalId } });
        table.rows.push_back({ { String( "isSynth"), (int) this->isSynth } });
        table.rows.push_back({ { tblstr{ "UUID" }, getUID() } });
        table.rows.push_back({ { tblstr{ "bIsEnabled" }, tblint{ this->bIsEnabled } } });
        table.rows.push_back({ { String( "bCanReceiveMidi"), (int) this->bCanReceiveMidi } });
        table.rows.push_back({ { String( "midiEventsDispatched"), (int) this->midiEventsDispatched } });
        table.rows.push_back({ { tblstr{ "PARAM_ENABLE" }, tblfloat{ this->getParamValue(PARAM_ENABLE) } } });
        table.rows.push_back({ { tblstr{ "latency" }, tblint{ getPluginLatency() } } });
        for (auto& in : this->inputChannelsDesc) {
            table.rows.push_back({ { tblString{ StringFormat("input[%d,%d]", in.offset, in.count) }, tblstr{ StringAsCStr(in.name) } } });
        }
        for (auto& out : this->outputChannelsDesc) {
            table.rows.push_back({ { tblString{ StringFormat("output[%d,%d]", out.offset, out.count) }, tblstr{ StringAsCStr(out.name) } } });
        }
    }
};
