#include "host/host_pluginmanager.h"
#include "assert_dbg.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"
#include "logging.h"
#include "thread.h"
#include "host/daw/history.h"
#include "host/track/track_impl.h"
#include <vstsdk-host-2.4/aeffectx.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>

namespace DAW::VST2 {

namespace {


// #define DBG_PRINT_CALLBACKS
#ifdef DBG_PRINT_CALLBACKS
#define MAX_LEN_MY_DBF 512


    bool filterOpCode(int opcode) {
    //    return opcode == audioMasterUpdateDisplay;
    //    if ( opcode == audioMasterSizeWindow)
    //        return true;
    //    if ( opcode == audioMasterBeginEdit)
    //        return true;
    //    if ( opcode == audioMasterEndEdit)
    //        return true;
    //    if ( opcode == audioMasterAutomate)
    //        return true;
    //    if ( opcode == audioMasterGetInputLatency)
    //        return false;
    //    if ( opcode == audioMasterGetOutputLatency)
    //        return false;
        return true;
    }

    void logPluginCb(vstplugin* plugin, const char* fmt, int opcode, int index, int64_t value, float opt = 0);

    void logPluginCb(vstplugin* plugin, const char* fmt, int opcode, int index, int64_t value, float opt)
    {
        if (filterOpCode(opcode)) {
            char buf[MAX_LEN_MY_DBF];
            snprintf(buf, MAX_LEN_MY_DBF - 1, fmt, opcode, index, value, opt);
            log_lf(Log::L_DEBUG, "%s %s", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), buf);
        }
    }

    #else

    void emptyPrinft(vstplugin* plugin, const char *fmt, ...) {
    }
    #define logPluginCb emptyPrinft

    #endif
}

#define NUM_HOST_CB_SLOTS 4
namespace
{

struct vst_internal_hostslot {
    ::DAW::Host::PluginManager* g_instance = nullptr;
    ::DAW::Host::PluginHostCallback* g_hostCallback = nullptr;
};

static vst_internal_hostslot g_hostslots[NUM_HOST_CB_SLOTS];

static double PPQ24TickToSample(double midiTickPPQ24, uint32_t bpm100, samplerate_t samplerate, uint32_t blocksize) {
    double seconds = (midiTickPPQ24/(double)(bpm100*24.0)) * 100.0 * 60.0;
    double samplePos = seconds * samplerate;
    return samplePos;
}
}

bool SetFlag(int& _out, int flag, bool state) {
    bool curState = _out&flag;
    if (state) {
        _out |= flag;
    } else {
        _out &= ~flag;
    }
    return curState != state;
}

//\note VstTimeInfo::samplesToNextClock :
//MIDI Clock Resolution (24 per Quarter Note), can be negative the distance to the next midi clock
//        (24 ppq, pulses per quarter) in samples. unless samplePos falls precicely on a midi clock,
//        this will either be negative such that the previous MIDI clock is addressed,
//        or positive when referencing the following (future) MIDI clock.

void UpdateTime(VstTimeInfo& timeinfo, int32_t transportStateFlags, const sampleformat_t& m_sampleFormatInternal, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) {
    static const double fSmpteDiv[] =
    {
        24.f,
        25.f,
        24.f,
        30.f,
        29.97f,
        30.f
    };
    timeinfo.samplePos = samplePos;
    timeinfo.sampleRate = (double) m_sampleFormatInternal.sampleRate;
    timeinfo.nanoSeconds = getTimeMicros() * 1000.0;
    timeinfo.ppqPos = (dTickPos/(double)TICKS_QUARTER);
    timeinfo.tempo = prjGlobals.tempo100/100.0;
    timeinfo.barStartPos = floor(dTickPos / (double) TICKS_BAR) * 4;
    timeinfo.cycleStartPos = (prjGlobals.loopStart/(double)TICKS_QUARTER);
    timeinfo.cycleEndPos = ((prjGlobals.loopStart+prjGlobals.loopLen)/(double)TICKS_QUARTER);
    timeinfo.timeSigNumerator = static_cast<VstInt32>(prjGlobals.signatureNum);
    timeinfo.timeSigDenominator = 1 << prjGlobals.signatureDenom;

    bool loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    if (!loopEnabed) {
        timeinfo.cycleStartPos = 0;
        timeinfo.cycleEndPos = 0;
    }

    {
        double dPosSeconds = samplePos / timeinfo.sampleRate;
        /* offset in fractions of a second   */
        double dOffsetInSecond = dPosSeconds - floor(dPosSeconds);
        timeinfo.smpteFrameRate = VstSmpteFrameRate::kVstSmpte24fps;
        timeinfo.smpteOffset = math::floordS32(dOffsetInSecond * fSmpteDiv[timeinfo.smpteFrameRate] * 80.);
    }


    double midiTickPPQ24 = timeinfo.ppqPos*24.0;
    double samplePosMidiTick = PPQ24TickToSample(midiTickPPQ24, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosPrevMidiTick = PPQ24TickToSample(math::floord(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosNextMidiTick = PPQ24TickToSample(math::ceild(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);

    double samplePosClosestPPQ24Tick = math::absMin(samplePosPrevMidiTick - samplePosMidiTick, samplePosNextMidiTick - samplePosMidiTick);
    //TODO: assingn nearest clock (can be negative), not next aka soonest
    timeinfo.samplesToNextClock = math::rounddS32(samplePosClosestPPQ24Tick);

    {
        timeinfo.flags = (transportStateFlags & (kVstTransportPlaying | kVstTransportCycleActive | kVstTransportRecording | kVstTransportChanged));
        SetFlag(timeinfo.flags, kVstAutomationWriting, false);
        SetFlag(timeinfo.flags, kVstAutomationReading, false);
        SetFlag(timeinfo.flags, kVstNanosValid, true);
        SetFlag(timeinfo.flags, kVstPpqPosValid, true);
        SetFlag(timeinfo.flags, kVstTempoValid, true);
        SetFlag(timeinfo.flags, kVstBarsValid, true);
        SetFlag(timeinfo.flags, kVstCyclePosValid, true); //project.loopEnabled
        SetFlag(timeinfo.flags, kVstTimeSigValid, true);
        SetFlag(timeinfo.flags, kVstSmpteValid, true);
        SetFlag(timeinfo.flags, kVstClockValid, true);
    }
}

int32_t HostCanDo(const char* ptr) {
    if ((!strcmp(ptr, HostCanDos::canDoSendVstEvents)) ||
        (!strcmp(ptr, HostCanDos::canDoSendVstMidiEvent)) ||
        (!strcmp(ptr, HostCanDos::canDoSendVstTimeInfo)) ||
        (!strcmp(ptr, HostCanDos::canDoReceiveVstEvents)) ||
        (!strcmp(ptr, HostCanDos::canDoReceiveVstMidiEvent)) ||
        (!strcmp(ptr, HostCanDos::canDoReportConnectionChanges)) ||
        (!strcmp(ptr, HostCanDos::canDoAcceptIOChanges)) ||
        (!strcmp(ptr, HostCanDos::canDoSizeWindow)) ||
        (!strcmp(ptr, HostCanDos::canDoSendVstMidiEventFlagIsRealtime)) ||
        (!strcmp(ptr, HostCanDos::canDoStartStopProcess)) ||
        (!strcmp(ptr, HostCanDos::canDoShellCategory)))
        return 1;
    if (!strcmp(ptr, "NIMKPIVendorSpecificCallbacks")) {
        return -1;
    }
    return 0;
}

/**
 * VST Host AudioMasterCallback
 */
VstIntPtr audioMasterHost(::DAW::Host::PluginManager* host, ::DAW::Host::PluginHostCallback* hostCallback, AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(host);
    // In case a plugin instance outlives the host
    if (!host)
        return 0;

    /**
     * Thread safety is guaranteed by only allowing internal threads to enter the callback
     * TODO: Find out what exact thread we got called from. @see notes
     */
    vstplugin *plugin = host->getPlugin(effect);

    bool bIsKnownThread = false;
    bool bIsInternalThread = false;
    seqthreads::getThreadInfo(bIsKnownThread, bIsInternalThread);
    if (!bIsKnownThread) {
        seqthreads::registerThread("External", seqthreads::ThreadType::Unknown, false);
        daw_tls::setTls(host->getTls());
        log_lf(Log::L_WARN, "(First) Request from external thread: Plugin '%s' opcode %d %d %zd %f\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), opcode, index, value, opt);
        bIsInternalThread = false;
    }
    /* if (!bIsInternalThread) {
        log_lf(Log::L_WARN, "Request from external thread: Plugin '%s' opcode %d %d %zd %f\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), opcode, index, value, opt);
    } */

    /**
     * TODO: Detect reentrance and guard against it. @see notes
     */
    bool throttleLog = false;
    bool validProcessingState = false;
    if (plugin) {
        //TODO: getOpCodeStats is not a threadsafe implementation
        vst_opcode_stats_t& opcodeStats = plugin->getOpCodeStats(true, opcode);
        opcodeStats.numDispatches++;
        int32_t tmMillisS32 = static_cast<int32_t>(static_cast<uint64_t>(getTimeMillis()) & (0x7FFF'FFFFLL));
        int32_t tmSince = tmMillisS32 - opcodeStats.tmMillis;
        if (tmSince < 2000) {
            throttleLog = tmSince > 50 && opcodeStats.numDispatches > 20;
        } else {
            opcodeStats.tmMillis = tmMillisS32;
        }
        /**
         * Validate that the plugin is currently fully loaded and setup and connected to an audiostage that is valid.
         * Currently plugins have an extended lifetime after removal inside the edithistory.
         *
         * TODO: This check is not well implemented
         * Add a lock free thread-safe way to check (from the callback) if a plugin is ready for processing
         */
        /* Ignore audioMasterVersion, audioMasterGetVendorString and audioMasterGetProductString */
        if (opcode == audioMasterVersion || opcode == audioMasterGetVendorString || opcode == audioMasterGetProductString) {
            validProcessingState = true;
        }
        if (!validProcessingState && plugin->hasTrackLink()) {
            auto parent = plugin->trackImpl;
            while (parent->parent) parent = parent->parent;
            auto daw = host->getTls().dawInstance;
            //TODO: add explicit flag for headleass mode. daw == nullptr could mean headless mode or end of lifetime (in which case we don't want to process)
            if (!daw || daw->getTracks().resolveTrack(parent->toRef())) {
                validProcessingState = true;
            }
        }
        if (!validProcessingState) {
            log_lf(Log::L_WARN, "%s opCode %s in !validProcessingState\n", StringAsCStr(plugin->sName), getMasterOpcodeName(opcode));
        }
    }
    switch (opcode)
    {
    case audioMasterAutomate:
        if (plugin) {
            auto* effParam = plugin->getEffectParam(index);
            // log_printf("%s audioMasterAutomate param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            if (!effParam) {
                if (!throttleLog)
                    log_printf("%s audioMasterAutomate unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {
                auto flags = FLG_PAR_UPDATE_FROM_CLIENT;
                plugin->setParamEdit(effParam->idx, opt, flags);
                dbgassert(effParam->getValue() == opt);
                // effParam->value = opt;
                effParam->paramValueState = PARAM_FLAG_SET;
                effParam->paramDisplayValState |= PARAM_FLAG_DIRTY;
                effParam->inUse = true;
            }
        }
        return 1;
    case audioMasterVersion:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterVersion %d %d %zd\n", opcode, index, value, 0);
        return 2400; //VST 2.4
    case audioMasterCurrentId:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterCurrentId %d %d %zd\n", opcode, index, value, 0);
        //return OnGetCurrentUniqueId(nEffect);
        if (plugin) {
            return (VstIntPtr)plugin->getLocalCurrentUniqueId();
        }
        return hostCallback->vstShellCurrentUniqueId;
    case audioMasterIdle:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterIdle %d %d %zd\n", opcode, index, value, 0);
        //return OnIdle(nEffect);
        return 0;
    case audioMasterGetTime:
        //{
        //    int32_t playThreadId = host->getPlayThreadId();
        //    int32_t localThreadId = getCurrentThreadId();
        //    if (localThreadId == playThreadId) {
        //        return (VstIntPtr)plugin->getLocalTimeInfoPtr();
        //    }
        //}
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetTime %d %d %zd\n", opcode, index, value);
        if (plugin) {
            return (VstIntPtr)plugin->getLocalTimeInfoPtr();
        }
        return (VstIntPtr)&hostCallback->m_vstTimeInfo;

    case audioMasterProcessEvents:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterProcessEvents %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterIOChanged:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterIOChanged %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterNeedIdle:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterNeedIdle %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            plugin->bWantsEffIdle = true;
        }
        return 0;
    case audioMasterSizeWindow:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterSizeWindow %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            plugin->updateWindowSize();
        }
        return 1;
    case audioMasterGetSampleRate:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetSampleRate %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return plugin->format.sampleRate;
        }
        if (host) {
            return hostCallback->m_sampleFormatInternal.sampleRate;
        }
        return 0;
    case audioMasterGetBlockSize:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetBlockSize %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return plugin->format.blockSize;
        }
        if (host) {
            return hostCallback->m_sampleFormatInternal.blockSize;
        }
        return 0;
    case audioMasterGetInputLatency:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetInputLatency %d %d %zd\n", opcode, index, value);
        //TODO: find out if other hosts provide this info
        // IL Harmor requests this info
        return 0;
    case audioMasterGetOutputLatency:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetOutputLatency %d %d %zd\n", opcode, index, value);
        //TODO: find out if other hosts provide this info
        // IL Harmor requests this info
        return 0;
    case audioMasterGetCurrentProcessLevel:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetCurrentProcessLevel %d %d %zd\n", opcode, index, value);
        if (hostCallback->isOfflineRendering){
            return VstProcessLevels::kVstProcessLevelOffline;
        }
        return VstProcessLevels::kVstProcessLevelRealtime;
    case audioMasterGetAutomationState:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetAutomationState %d %d %zd\n", opcode, index, value, 0);
        return kVstAutomationRead;
    case audioMasterOfflineStart:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineStart %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineRead:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineRead %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineWrite:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineWrite %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineGetCurrentPass:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineGetCurrentPass %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineGetCurrentMetaPass:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineGetCurrentMetaPass %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterGetVendorString:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetVendorString %d %d %zd\n", opcode, index, value, 0);
        if (ptr) {
            strcpy(static_cast<char*>(ptr), BuildInfo::PRODUCT_VENDOR);
            return 1;
        }
        return 0;
    case audioMasterGetProductString:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetProductString %d %d %zd\n", opcode, index, value, 0);
        if (ptr) {
            strcpy(static_cast<char*>(ptr), BuildInfo::PRODUCT_HOST_NAME);
            return 1;
        }
        return 0;
    case audioMasterGetVendorVersion:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetVendorVersion %d %d %zd\n", opcode, index, value, 0);
        return 1;
    case audioMasterVendorSpecific:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterVendorSpecific %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterCanDo:
        if (!throttleLog) {
            log_lf(Log::L_DEBUG, "%s audioMasterCanDo %s\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), (const char*)ptr);
        }
        return DAW::VST2::HostCanDo((const char*)ptr);
    case audioMasterGetLanguage:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetLanguage %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterGetDirectory:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetDirectory %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return (VstIntPtr) plugin->getDir();
        }
        return 0;
    case audioMasterUpdateDisplay:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterUpdateDisplay %d %d %zd\n", opcode, index, value, 0);
        if (plugin && validProcessingState && !plugin->bIsLoadingProgram) {
            plugin->recvProgramNameUpdate();
            // NOTE: this loop might kill performance
            plugin->visitParams([](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                param.paramNameState |= PARAM_FLAG_DIRTY;
                param.paramValueState |= PARAM_FLAG_DIRTY;
                param.paramDisplayValState |= PARAM_FLAG_DIRTY;
            });
            return 1;
        }
        return 0;
#ifdef VST_2_1_EXTENSIONS
    case audioMasterBeginEdit:
        if (plugin && validProcessingState && !plugin->bIsLoadingProgram) {
            if (!throttleLog)
                logPluginCb(plugin, "audioMasterBeginEdit %d %d %zd %f\n", opcode, index, value, opt);
            auto* effParam = plugin->getEffectParam(index);
            if (!effParam) {
                if (!throttleLog)
                    log_printf("%s audioMasterBeginEdit unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {
              plugin->handle->paramEditing = {effParam->internalIdx, effParam->getValue()};
            }
        }
        return 1;
    case audioMasterEndEdit:
        // if (!throttleLog)
        //     logPluginCb(plugin, "audioMasterEndEdit %d %d %zd %f\n", opcode, index, value, opt);
        if (plugin && validProcessingState && !plugin->bIsLoadingProgram && plugin->handle->paramEditing.paramIdx > -1) {
            if (!throttleLog)
                logPluginCb(plugin, "audioMasterEndEdit %d %d %zd %f\n", opcode, index, value, opt);
            auto* effParam = plugin->getEffectParam(index);
            if (!effParam) {
                if (!throttleLog)
                    log_printf("%s audioMasterEndEdit unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {
                dbgassert(plugin->trackImpl->getTrack());
                auto newVal = effParam->getValue();
                auto oldVal = plugin->handle->paramEditing.valBefore;
                track_t* track                = plugin->trackImpl->getTrack();
                automatable_param_ref_t ref = plugin->toRef();
                parameter_ref_t p             = { track->projectIdx, ref.type, plugin->projectGlobalId, effParam->idx };
                host->getTls().dawInstance->pushHist(new action_modify_effect_parameter("Modify parameter", p, oldVal, newVal));

            }
        }
        if (plugin) {
            plugin->handle->paramEditing = {-1, 0.0f};
        }
        return 1;
    case audioMasterOpenFileSelector:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOpenFileSelector %d %d %zd\n", opcode, index, value, 0);
        return 0;
#endif
#ifdef VST_2_2_EXTENSIONS
    case audioMasterCloseFileSelector:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterCloseFileSelector %d %d %zd\n", opcode, index, value, 0);
        return 0;
#endif
    case audioMasterWantMidi:
        return 0;
    case audioMasterPinConnected:
        return 0;
    default:
        if (!throttleLog)
            logPluginCb(plugin, "unhandled %d %d %zd %f\n", opcode, index, value, opt);

    }
    return 0;
}

VstIntPtr VSTCALLBACK audioMaster1(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[0].g_instance);
    dbgassert(g_hostslots[0].g_hostCallback);
    return audioMasterHost(g_hostslots[0].g_instance, g_hostslots[0].g_hostCallback, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster2(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[1].g_instance);
    dbgassert(g_hostslots[1].g_hostCallback);
    return audioMasterHost(g_hostslots[1].g_instance, g_hostslots[1].g_hostCallback, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster3(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[2].g_instance);
    dbgassert(g_hostslots[2].g_hostCallback);
    return audioMasterHost(g_hostslots[2].g_instance, g_hostslots[2].g_hostCallback, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster4(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[3].g_instance);
    dbgassert(g_hostslots[3].g_hostCallback);
    return audioMasterHost(g_hostslots[3].g_instance, g_hostslots[3].g_hostCallback, effect, opcode, index, value, ptr, opt);
}
} // namespace DAW::VST2

namespace DAW::Host {

void PluginManager::onBeforeBlock(const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) {
    auto loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    auto& vst2TransportState = getTransportStateFlagsVst2();
    bool changed = DAW::VST2::SetFlag(vst2TransportState, kVstTransportPlaying, DAW::isPlaybackState(state));
    changed |= DAW::VST2::SetFlag(vst2TransportState, kVstTransportCycleActive, loopEnabed);
    changed |= DAW::VST2::SetFlag(vst2TransportState, kVstTransportRecording, false);
    DAW::VST2::SetFlag(vst2TransportState, kVstTransportChanged, changed);
}
void PluginManager::UpdateVstTime(VstTimeInfo& timeInfo, const sampleformat_t& sampleFormat, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) const {
    DAW::VST2::UpdateTime(timeInfo,
                            getTransportStateFlagsVst2(),
                            sampleFormat,
                            prjGlobals,
                            samplePos,
                            dTickPos,
                            state);
}

void PluginManager::destroyVST2() {
    dbgassert(hostSlot > -1);
    dbgassert(DAW::VST2::g_hostslots[hostSlot].g_instance);
    DAW::VST2::g_hostslots[hostSlot].g_instance = nullptr;
}

bool PluginManager::assignVST2MasterCallback(PluginManager* host, ::DAW::Host::PluginHostCallback* cb) {
    for (int i = 0; i < NUM_HOST_CB_SLOTS; i++) {
        if (DAW::VST2::g_hostslots[i].g_instance == nullptr) {
            DAW::VST2::g_hostslots[i].g_instance = host;
            DAW::VST2::g_hostslots[i].g_hostCallback = cb;
            host->hostSlot = i;
            if (i == 0) {
                host->masterCallBackSlot = DAW::VST2::audioMaster1;
            }
            if (i == 1) {
                host->masterCallBackSlot = DAW::VST2::audioMaster2;
            }
            if (i == 2) {
                host->masterCallBackSlot = DAW::VST2::audioMaster3;
            }
            if (i == 3) {
                host->masterCallBackSlot = DAW::VST2::audioMaster4;
            }
            return true;
        }
    }
    log_lf(Log::L_ERROR, "No free VST2 host slots\n");
    dbgassert(0);
    return false;
}

} // namespace DAW::Host
