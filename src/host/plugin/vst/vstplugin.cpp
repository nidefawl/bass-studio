#include <algorithm>
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-host-2.4/aeffectx.h>
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "config.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/modules.h"
#include "plugins/synth/IPlugMidi.h"
#include "vstplugin.h"
#include "host/vst2/vst_event.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "logging.h"
#include "host/audiobuffer/audioblock.h"
#include "snapshot/snapshot.h"
#include "vstplugin-handles.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/host_pluginmanager.h"
#include "host/host_plugin_window.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "plugins/plugin-base.h"
#include "host/daw/mainctrl.h"
#include "host/daw/history.h"
#include "host/host_pluginmanager.h"


FUNC_NOINLINE float vst_getParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx);
FUNC_NOINLINE void vst_setParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx, float value);
FUNC_NOINLINE void vst_process(vstplugin* plugin, AEffect* aeffect, float** bufIn, float** bufOut, int32_t numSamples);
FUNC_NOINLINE int64_t vst_dispatch(vstplugin* plugin,
                  AEffect* aeffect,
                  int32_t opcode,
                  int32_t index,
                  int64_t value,
                  void* ptr,
                  float opt);

bool vstplugin::onShow(host_plugin_window* _window) {
    if (this->windowHost == _window) {
        bEditOpen = true;
        this->dispatch(effEditOpen, 0, 0, (void*) _window->getWindowHandle());
        this->updateFromMainThread();
    }
    return true;
}

bool vstplugin::onClose() {
    if (this->windowHost != nullptr && bEditOpen) {
        this->dispatch(effEditClose);
    }
    bEditOpen = false;
    return true;
}

ivec2 vstplugin::constrainWindowSize(host_plugin_window*, ivec2 size) {
    if (!bSupportsWindowResize) {
        ERect* prc = nullptr;
        this->dispatch(effEditGetRect, 0, 0, (void*) &prc);
        if (prc) {
            if (size.x > (prc->right - prc->left)) {
                size.x = prc->right - prc->left;
            }
            if (size.y > (prc->bottom - prc->top)) {
                size.y = prc->bottom - prc->top;
            }
        }
    }
    return size;
}

void vstplugin::onWindowResize(ivec2 size) {
    if (handle->axEffect) {
        handle->axEffect->onWindowResize(size);
    }
}

void vstplugin::onEnable() {
    if (!bIsPostInit) {
        bIsPostInit = true;
        postLoad();
        bool bShowWindow = this->bugfixFlags & VST2_BUG_NEED_SHOW_WINDOW_TO_LOAD_PRESET;
        bShowWindow |= bOpenWindowOnEnable;
        if (bShowWindow && seqthreads::CurrentThreadType() == seqthreads::ThreadType::MainThread) {
            bOpenWindowOnEnable = false;
            showWindow(false);
        }
    }
    if (!isInSuspend) {
        log_lf(Log::L_WARN, "Plugin %s is already in active state\n", StringAsCStr(getName()));
        return;
    }
    isInSuspend = false;
    this->dispatch(effMainsChanged, 0, true);
    this->dispatch(effStartProcess);
}

void vstplugin::onDisable() {
    if (isInSuspend) {
        log_lf(Log::L_WARN, "Plugin %s is already in suspended state\n", StringAsCStr(getName()));
        return;
    }
    isInSuspend = true;
    this->dispatch(effStopProcess);
    this->dispatch(effMainsChanged, 0, false);
    sendNotesOff();
}

bool vstplugin::getNameString(char* szBuf) {
    if (this->dispatch(effGetProductString, 0, 0, (void*) szBuf) && szBuf[0] != 0) {
        return true;
    }
    if (this->dispatch(effGetEffectName, 0, 0, (void*) szBuf) && szBuf[0] != 0) {
        return true;
    }
    return false;
}

bool vstplugin::updateWindowSize() {
    if (this->windowHost != nullptr) {
        ERect* prc = nullptr;
        this->dispatch(effEditGetRect, 0, 0, (void*) &prc);
        if (prc) {
            this->windowHost->resize({ prc->right - prc->left, prc->bottom - prc->top });
            return true;
        }
    }
    return false;
}

void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();

void vstplugin::unload(DAW::Host::PluginManager* host) {
    effectbase::unload(host);
    AppWndProc_enableBlockReentrant();
    this->dispatch(effClose);
    AppWndProc_disableBlockReentrant();
}
void vstplugin::configureIOChannels() {
    const bool useGetPinProperties = (this->bugfixFlags & vst_workarounds::VST2_R4_BUG_STEREO_PLUGIN_REPORTS_MONO) == 0;

    const auto inputCount  = static_cast<channelnum_t>(math::clamp(this->handle->aeffect->numInputs, 0, 255));
    const auto outputCount = static_cast<channelnum_t>(math::clamp(this->handle->aeffect->numOutputs, 0, 255));
    for (int side = 0; side < 2; ++side) {
        const auto channelCountSide = side == 0 ? inputCount : outputCount;
        auto& channelDescs          = side == 0 ? inputChannelsDesc : outputChannelsDesc;
        const char* sideName        = side == 0 ? "Input" : "Output";
        const auto dispatchOpCode   = side == 0 ? effGetInputProperties : effGetOutputProperties;

        channelDescs.clear();
        channelnum_t pinIndex = 0;
        VstPinProperties pin{};

        if (useGetPinProperties && pinIndex < channelCountSide && this->dispatch(dispatchOpCode, pinIndex, 0, &pin)) {
            channelnum_t channelOffset = 0;
            do {
                log_lf(Log::L_DEBUG, "Pin %d: %s Active %d Stereo %d UseSpeaker %d ArrangementType %d\n", 
                    pinIndex,
                    pin.label,
                    (pin.flags&VstPinPropertiesFlags::kVstPinIsActive)!=0,
                    (pin.flags&VstPinPropertiesFlags::kVstPinIsStereo)!=0,
                    (pin.flags&VstPinPropertiesFlags::kVstPinUseSpeaker)!=0,
                    pin.arrangementType);
                const String pinName = pin.label;
                bool handled = false;
                if (pin.flags&VstPinPropertiesFlags::kVstPinIsStereo) {
                    channelDescs.emplace_back(DAW::channel_desc{channelOffset, 2, pinName});
                    channelOffset += 2;
                    pinIndex += 2;
                    handled = true;
                } else if (pin.flags&VstPinPropertiesFlags::kVstPinUseSpeaker && pin.arrangementType > 0) {
                    switch (pin.arrangementType) {
                        case kSpeakerArr40Music:       ///< L R Ls  Rs (Quadro)
                            channelDescs.emplace_back(DAW::channel_desc{channelOffset, 2, "L R"});
                            channelOffset += 2;
                            channelDescs.emplace_back(DAW::channel_desc{channelOffset, 2, "Ls Rs"});
                            channelOffset += 2;
                            pinIndex += 4;
                            handled = true;
                            break;
                        case kSpeakerArrStereo:        ///< L R
                        case kSpeakerArrStereoSurround:///< Ls Rs
                        case kSpeakerArrStereoCenter:  ///< Lc Rc
                        case kSpeakerArrStereoSide:    ///< Sl Sr
                        case kSpeakerArr30Cine:        ///< L R C
                        case kSpeakerArr30Music:       ///< L R S
                        case kSpeakerArr31Cine:        ///< L R C Lfe
                        case kSpeakerArr31Music:       ///< L R Lfe S
                        case kSpeakerArr40Cine:        ///< L R C   S (LCRS)
                        case kSpeakerArr41Cine:        ///< L R C   Lfe S (LCRS+Lfe)
                        case kSpeakerArr41Music:       ///< L R Lfe Ls Rs (Quadro+Lfe)
                        case kSpeakerArr50:            ///< L R C Ls  Rs
                        case kSpeakerArr51:            ///< L R C Lfe Ls Rs
                        case kSpeakerArr60Cine:        ///< L R C   Ls  Rs Cs
                        case kSpeakerArr60Music:       ///< L R Ls  Rs  Sl Sr
                        case kSpeakerArr61Cine:        ///< L R C   Lfe Ls Rs Cs
                        case kSpeakerArr61Music:       ///< L R Lfe Ls  Rs Sl Sr
                        case kSpeakerArr70Cine:        ///< L R C Ls  Rs Lc Rc
                        case kSpeakerArr70Music:       ///< L R C Ls  Rs Sl Sr
                        case kSpeakerArr71Cine:        ///< L R C Lfe Ls Rs Lc Rc
                        case kSpeakerArr71Music:       ///< L R C Lfe Ls Rs Sl Sr
                        case kSpeakerArr80Cine:        ///< L R C Ls  Rs Lc Rc Cs
                        case kSpeakerArr80Music:       ///< L R C Ls  Rs Cs Sl Sr
                        case kSpeakerArr81Cine:        ///< L R C Lfe Ls Rs Lc Rc Cs
                        case kSpeakerArr81Music:       ///< L R C Lfe Ls Rs Cs Sl Sr
                        case kSpeakerArr102:           ///< L R C Lfe Ls Rs Tfl Tfc Tfr Trl Trr Lfe2
                            channelDescs.emplace_back(DAW::channel_desc{channelOffset, 2, pinName});
                            channelOffset += 2;
                            pinIndex += 2;
                            handled = true;
                            break;
                        case kSpeakerArrStereoCLfe:    ///< C Lfe
                            break;
                    }
                } 
                if (!handled) {
                    channelDescs.emplace_back(DAW::channel_desc{channelOffset, 1, pinName});
                    channelOffset += 1;
                    pinIndex += 1;
                }
                pin = {};
            } while (pinIndex < channelCountSide && this->dispatch(dispatchOpCode, pinIndex, 0, &pin));
        } else {
            if (channelCountSide == 1) {
                channelDescs.emplace_back(DAW::channel_desc{0, 1, StringFormat("Mono %s", sideName)});
            } else if (channelCountSide == 2) {
                channelDescs.emplace_back(DAW::channel_desc{0, 2, StringFormat("Stereo %s", sideName)});
            } else if (channelCountSide == 4) {
                channelDescs.emplace_back(DAW::channel_desc{0, 2, StringFormat("Stereo %s", sideName)});
                if (side == 0) {
                    channelDescs.emplace_back(DAW::channel_desc{2, 2, StringFormat("Sidechain %s", sideName)});
                } else {
                    channelDescs.emplace_back(DAW::channel_desc{2, 2, StringFormat("Stereo %s", sideName)});
                }
            } else {
                for (channelnum_t i = 0; i < channelCountSide;) {
                    if (i + 1 < channelCountSide) {
                        channelDescs.emplace_back(DAW::channel_desc{i, 2, StringFormat("Stereo %s #%u", sideName, i)});
                        i += 2;
                    } else {
                        channelDescs.emplace_back(DAW::channel_desc{i, 1, StringFormat("Mono %s #%u", sideName, i)});
                        i += 1;
                    }
                }
            }
        }
        for (auto& chanDesc : channelDescs) {
            if (chanDesc.count + chanDesc.offset > channelCountSide) {
                log_lf(Log::L_WARN, "Invalid %s channel configuration for %s: Channel %s with offset %d, count %d exceeds advertised input channel count of %d\n", 
                    sideName, StringAsCStr(getName()), StringAsCStr(chanDesc.name), chanDesc.offset, chanDesc. count, channelCountSide);
            }
        }
    }
}

void vstplugin::load(DAW::Host::PluginManager* mgr) {
    dbgassert(nLoadCalls == 0);
    nLoadCalls++;
    pluginMgr = mgr;
    if (assert_expr(hostCallback))
        setSampleFormat(hostCallback->m_sampleFormatInternal);
    auto aeffect = handle->aeffect;

    aeffect->resvd2     = 0;
    this->uId           = aeffect->uniqueID;
    this->isSynth         = (handle->aeffect->flags & effFlagsIsSynth) != 0;
    this->bCanReceiveMidi = this->isSynth;
    this->vstVersion    = dispatch(effGetVstVersion);
    this->vendorVersion = dispatch(effGetVendorVersion);
    this->numMidiInputChannels = dispatch(effGetNumMidiInputChannels);
    if (this->numMidiInputChannels == 0 && this->bCanReceiveMidi) {
        this->numMidiInputChannels = 1;
    }
    this->numMidiOutputChannels = dispatch(effGetNumMidiOutputChannels);
    this->bCanReceiveMidi |= this->numMidiInputChannels > 0;
// this->dispatch(effIdentify, 0, 0, nullptr, 0);
    this->pluginCategory  = this->dispatch(effGetPlugCategory);

    #if 0
        this->dispatch(effIdentify, 0, 0, nullptr, 0);
        this->dispatch(effSetSampleRate, 0, 0, nullptr, (float) format.sampleRate);
        this->dispatch(effSetBlockSize, 0, format.blockSize, nullptr, 0);

        this->dispatch(effOpen);
        this->dispatch(effSetSampleRate, 0, 0, nullptr, (float) format.sampleRate);
        this->dispatch(effSetBlockSize, 0, format.blockSize, nullptr, 0);
        this->bCanSendMidi    |= this->dispatch(effCanDo, 0, 0, (void*) PlugCanDos::canDoSendVstMidiEvent) > 0;
        this->bCanReceiveMidi |= this->dispatch(effCanDo, 0, 0, (void*) PlugCanDos::canDoReceiveVstMidiEvent) > 0;
        this->bMPESupport     |= this->dispatch(effCanDo, 0, 0, (void*) "MPE") > 0;
        VstPinProperties pin{};
        this->dispatch(effGetInputProperties, 0, 0, &pin);
        pin = {};
        this->dispatch(effGetOutputProperties, 0, 0, &pin);
        this->dispatch(effMainsChanged, 0, true);
        this->dispatch(effMainsChanged, 0, false);
        configureIOChannels();  
    #else
        this->dispatch(effSetSampleRate, 0, 0, nullptr, (float) format.sampleRate);
        this->dispatch(effSetBlockSize, 0, format.blockSize, nullptr, 0);
        this->dispatch(effOpen);
        this->dispatch(effSetSampleRate, 0, 0, nullptr, (float) format.sampleRate);
        this->dispatch(effSetBlockSize, 0, format.blockSize, nullptr, 0);
        this->dispatch(effMainsChanged, 0, true);
        this->dispatch(effMainsChanged, 0, false);
        this->dispatch(effStopProcess);
        this->bCanSendMidi    |= this->dispatch(effCanDo, 0, 0, (void*) PlugCanDos::canDoSendVstMidiEvent) > 0;
        this->bCanReceiveMidi |= this->dispatch(effCanDo, 0, 0, (void*) PlugCanDos::canDoReceiveVstMidiEvent) > 0;
        this->bMPESupport     |= this->dispatch(effCanDo, 0, 0, (void*) "MPE") > 0;

        configureIOChannels();  
    #endif

    initBuffers();
    initMeters();

    char buf[1024];
    vst_param_category fallbackCat = { 0, 0, "Parameters" };
    for (int i = 0; i < aeffect->numParams; i++) {
        int32_t paramIdentifier    = PARAM_OFFSET_EXTERNAL + i;
        automatable_param_t* param = registerParam(paramIdentifier);
        param->internalIdx         = i;
        param->paramNameState = PARAM_FLAG_DIRTY;
        param->paramDisplayValState = PARAM_FLAG_DIRTY;
        param->paramValueState = PARAM_FLAG_DIRTY;
        memset(buf, 0, sizeof(buf));
        this->dispatch(effGetParamName, i, 0, buf);
        String paramName = buf[0] ? buf : StringFormat("Parameter %d", i);
        param->name = paramName;
        memset(buf, 0, sizeof(buf));
        this->dispatch(effGetParamLabel, i, 0, buf);
        String paramUnit = buf[0] ? buf : "";
        param->unit = paramUnit;
        VstParameterProperties properties = {};
        if (this->dispatch(effGetParameterProperties, i, 0, &properties, 0)) {
            param->flags = properties.flags | (ParamIsAdvanced);
            if (properties.label[0]) {
                // param->unit = properties.label;
            }
            if (properties.shortLabel[0]) {
                // param->shortLabel = properties.shortLabel;
            }
#if 0 
            if (param->flags & ParamUsesFloatStep) {
                //param.min.valFloat = 0.0f;
                //param.max.valFloat = 1.0f;
                param->step.valFloat      = properties.stepFloat;
                param->stepSmall.valFloat = properties.smallStepFloat;
                param->stepLarge.valFloat = properties.largeStepFloat;
            }
            if (param->flags & ParamUsesIntStep) {
                param->min.valInt       = std::numeric_limits<int32_t>::min();
                param->max.valInt       = std::numeric_limits<int32_t>::max();
                param->step.valInt      = properties.stepInteger;
                param->stepSmall.valInt = 1;
                param->stepLarge.valInt = properties.largeStepInteger;
            }
            if (param->flags & ParamUsesIntegerMinMax) {
                param->min.valInt = properties.minInteger;
                param->max.valInt = properties.maxInteger;
            }
            if (param->flags & ParamSupportsDisplayCategory) {
                param->category = properties.category + 1;
                if (getCategory(param->category) == nullptr && properties.categoryLabel[0]) {
                    vst_param_category paramCat = { param->category, properties.numParametersInCategory, properties.categoryLabel };
                    paramsCategories.push_back(paramCat);
                }
            }
#endif
            if (param->flags & ParamSupportsDisplayIndex) {
                param->displayIndex = properties.displayIndex;
            }
        } else {
            param->flags = 0;
            fallbackCat.nParams++;
        }
        //TODO: wrap getParameter call in exception handler
        auto parDispatch = handle->aeffect->getParameter(handle->aeffect, param->internalIdx);
        param->setInitial(parDispatch);
    }
    paramsCategories.push_back(fallbackCat);

    if (aeffect->numParams)
        getRegisteredAutomation(65536);

    bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
}
void vstplugin::postLoad() {
    this->recvProgramListUpdate();
    this->recvProgramNameUpdate();
    visitParams([](auto& mapEntry) {
        automatable_param_t& param = mapEntry.second;
        param.paramNameState |= PARAM_FLAG_DIRTY;
        param.paramValueState |= PARAM_FLAG_DIRTY;
        param.paramDisplayValState |= PARAM_FLAG_DIRTY;
    });
}

namespace {

    void createSnapshot(plugin_snapshot_t& ps, vstplugin* plugin, const tracksnapshot_store_opts_t& opts) {
        ps.slot              = 0;
        ps.projectGlobalId   = plugin->projectGlobalId;
        ps.enabled           = plugin->bIsEnabled;
        ps.ioChannels.input  = plugin->inputChannelsDesc;
        ps.ioChannels.output = plugin->outputChannelsDesc;
        ps.moduleType        = MODULE_TYPE_VST2;
        if (plugin->internalModuleId >= 0) {
            ps.uId        = static_cast<uint32_t>(plugin->internalModuleId);
        } else {
            ps.vendorVersion = plugin->vendorVersion;
            ps.uId           = plugin->uId;
            ps.localDbId     = plugin->localDbId;
        }
        ps.name = plugin->sName;

        bool usesBinaryChunks = plugin->getFlagsVST() & effFlagsProgramChunks;
        if (opts.storePluginPreset && (usesBinaryChunks)) {
            {
                void* pluginData       = nullptr;
                int32_t pluginDataSize = plugin->dispatch(effGetChunk, 0, 0, &pluginData, 0);
                if (pluginDataSize > 0 && pluginData) {
                    auto* ptrData = reinterpret_cast<uint8_t*>(pluginData);
                    ps.dataChunk.reserve(pluginDataSize);
                    ps.dataChunk.assign(ptrData, ptrData + pluginDataSize);
                    // log_lf(Log::L_TRACE, "Plugin %s: Save data1[%d]\n", StringAsCStr(plugin->sName), pluginDataSize);
                }
            }
            if (storePluginPresetWithSnapshot) {
                void* pluginData2       = nullptr;
                int32_t pluginDataSize2 = plugin->dispatch(effGetChunk, 1, 0, &pluginData2, 0);
                if (pluginDataSize2 > 0 && pluginData2) {
                    auto* ptrData = reinterpret_cast<uint8_t*>(pluginData2);
                    ps.dataChunk2.reserve(pluginDataSize2);
                    ps.dataChunk2.assign(ptrData, ptrData + pluginDataSize2);
                    // log_lf(Log::L_TRACE, "Plugin %s: Save data2[%d]\n", StringAsCStr(plugin->sName), pluginDataSize2);
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
                        curValue = vst_getParameter(vstplugin, vstplugin->handle->aeffect, param.internalIdx);
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

}// namespace

void vstplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
    this->bIsLoadingProgram = true;
    const int32_t programIdx = pluginSnapshot.currentProgram >= 0 ? pluginSnapshot.currentProgram : 0;
    VstPatchChunkInfo info{};
    info.version = 1;
    info.numElements = 1;
    info.pluginVersion = pluginSnapshot.vendorVersion;
    info.pluginUniqueID = pluginSnapshot.uId;
    auto retBeginLoadBank = this->dispatch(effBeginLoadBank, 0, 0, (void*)&info);
    auto retBeginSetProgram = this->dispatch(effBeginSetProgram);
    this->dispatch(effSetProgram, 0, programIdx);
    log_lf(Log::L_DEBUG, "Plugin %s: effBeginLoadBank %zd\n", StringAsCStr(this->sName), retBeginLoadBank);
    log_lf(Log::L_DEBUG, "Plugin %s: retBeginSetProgram %zd\n", StringAsCStr(this->sName), retBeginSetProgram);
    bool bLoadProgramDataChunk = (this->getFlagsVST() & effFlagsProgramChunks) != 0;
    if (bLoadProgramDataChunk) {
        if (!pluginSnapshot.dataChunk.empty()) {
            auto& localMem = this->handle->dataChunkLocalMemory;
            localMem       = pluginSnapshot.dataChunk;
            log_lf(Log::L_DEBUG, "Plugin %s: Load data1[%zu]\n", StringAsCStr(this->sName), localMem.size());
            this->dispatch(effSetChunk, 0, (int64_t) localMem.size(), (void*) localMem.data());
        }
        if (loadPluginPresetWithSnapshot && !pluginSnapshot.dataChunk2.empty()) {
            log_lf(Log::L_DEBUG, "Plugin %s: Load data2[%zu]\n", StringAsCStr(this->sName), pluginSnapshot.dataChunk2.size());
            this->dispatch(effSetChunk, 1, (int64_t) pluginSnapshot.dataChunk2.size(), (void*) pluginSnapshot.dataChunk2.data());
        }
    } else {
        this->dispatch(effSetProgram, 0, programIdx);
        if (!pluginSnapshot.currentProgramName.empty()) {
            this->dispatch(effSetProgramName, 0, 0, (void*)StringAsCStr(pluginSnapshot.currentProgramName));
        }
    }
    if (!bLoadProgramDataChunk || (this->bugfixFlags & VST2_BUG_NEED_SHOW_WINDOW_TO_LOAD_PRESET)) {
        DAW::loadEffectParamsFromSnapshot(pluginSnapshot, this);
    }
    this->dispatch(effEndSetProgram);
    this->dispatch(effSetProgram, 0, programIdx);
    // this->dispatch(effSetProgram, 0, programIdx);
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

void vstplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    createSnapshot(ps, this, opts);
    for (auto& [uuid, gui] : uiInstances) {
        plugin_ui_snapshot_t uiSnapshot;
        gui->makeSnapshot(uiSnapshot, opts);
        ps.uiSnapshots[uuid] = uiSnapshot;
    }
    ps.windowLayout = getWindowLayoutSnapshot();
    ps.slot = this->slot;
}

#if 0
class guivstplugin_empty : public guiplugin {
    vstplugin* const module;

public:
    explicit guivstplugin_empty(vstplugin* _vst)
        : guiplugin(_vst),
          module(_vst) {
    }
    ~guivstplugin_empty() override = default;

    void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
    }
};
#endif

std::shared_ptr<guiplugin> vstplugin::createGuiPlugin(int32_t uuid) {
    auto gui = std::make_shared<guivstplugin>(this);
    gui->setTitle(StringFormat("%s (VST2)", StringAsCStr(this->sName)));
    if (handle->axEffect) {//only provided by internal vst2 instance (not a DLL)
        guiplugin* pGuiPlugin = gui.get();
        auto* pGuiVstPlugin   = dynamic_cast<guivstplugin*>(pGuiPlugin);
        dbgassert(pGuiVstPlugin);
        auto* baseVst2 = dynamic_cast<BasePluginVST2*>(handle->axEffect);
        dbgassert(baseVst2);
        auto viewCtr = baseVst2->openViewCtrVst2(UID_VIEW_CTR_PLUGIN_CTR);
        if (viewCtr && baseVst2 && pGuiVstPlugin) {
            pGuiVstPlugin->viewCtr = viewCtr;
            viewCtr->addTo(pGuiVstPlugin->viewCtrs);
        }
    }
    return gui;
}

vstplugin::~vstplugin() {
    delete handle;
}

samplecount_t vstplugin::getPluginLatency() {
    return handle && handle->aeffect ? handle->aeffect->initialDelay : 0;
}

int32_t vstplugin::getFlagsVST() {
    return handle && handle->aeffect ? handle->aeffect->flags : 0;
}

VstTimeInfo* vstplugin::getLocalTimeInfoPtr() {
    return handle ? &handle->localTimeInfo : nullptr;
}

uint32_t vstplugin::getLocalCurrentUniqueId() {
    return handle ? handle->localCurrentUniqueId : 0;
}

vst_param_category* vstplugin::getCategory(int idx) {
    if (idx >= 0 && idx < (int) paramsCategories.size()) {
        return &paramsCategories[idx];
    }
    return nullptr;
}

String vstplugin::getAutomatableName() {
    return this->sName;
}

automatable_param_t* vstplugin::getParam(int32_t idx) {
    auto param = effectbase::getParam(idx);
    if (param && param->internalIdx >= 0) {
        if (param->paramValueState & PARAM_FLAG_DIRTY) {
            param->setValue(vst_getParameter(this, handle->aeffect, param->internalIdx));
            param->paramValueState = PARAM_FLAG_SET;
        }
    }
    return param;
}
float vstplugin::getParamValue(int32_t idx) {
    return effectbase::getParamValue(idx);
}

param_unit_t vstplugin::getParamValueDisplay(int32_t idx) {
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

String vstplugin::getParamName(int32_t idx) {
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

param_unit_t vstplugin::convertParamValueToDisplay(int32_t idx, float value) {
    return effectbase::convertParamValueToDisplay(idx, value);
}

param_converted_t vstplugin::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
    if (idx >= PARAM_OFFSET_EXTERNAL) {
        if (handle->axEffect) {
            return handle->axEffect->convertParamValueDisplay(idx - PARAM_OFFSET_EXTERNAL, displayValue);
        }
        return {0.0f, false};
    }
    return effectbase::convertParamValueDisplay(idx, displayValue);
}

void vstplugin::addPropertiesParameterTooltip(Table::tbl& table, int idx) {
    if (idx >= PARAM_OFFSET_EXTERNAL) {
        if (handle->axEffect) {
            handle->axEffect->addPropertiesParameterTooltip(table, idx - PARAM_OFFSET_EXTERNAL);
            return;
        }
    }
    effectbase::addPropertiesParameterTooltip(table, idx);
}

void vstplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    effectbase::postSetParameter(idx, preVal, val, flags);
    automatable_param_t* param = getParamUnchecked(idx);
    if (param->internalIdx >= 0) {
        if (!(flags & FLG_PAR_UPDATE_FROM_CLIENT)) {
            vst_setParameter(this, handle->aeffect, param->internalIdx, val);
        }
        param->paramDisplayValState |= PARAM_FLAG_DIRTY;
        param->paramValueState = PARAM_FLAG_SET;
    }
}

bool vstplugin::setCurrentProgram(uint32_t idx) {
    if (idx < this->programNames.size()) {
        dispatch(effSetProgram, 0, idx, nullptr, 0);
        visitParams([](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            param.paramNameState |= PARAM_FLAG_DIRTY;
            param.paramValueState |= PARAM_FLAG_DIRTY;
            param.paramDisplayValState |= PARAM_FLAG_DIRTY;
        });
        this->recvProgramNameUpdate();
    }
    return false;
}

bool vstplugin::getCurrentProgram(uint32_t& idx) {
    auto curProgram = dispatch(effGetProgram, 0, 0, nullptr, 0);
    dbgassert(curProgram >= 0);
    idx = (uint32_t) curProgram;
    return true;
}

bool vstplugin::getNumberOfPrograms(uint32_t& numPrograms) {
    numPrograms = this->programNames.size();
    return true;
}

bool vstplugin::getCurrentProgramName(String& out) {
    out = this->currentProgramNameStr;
    return this->currentProgramNameSet;
}

void vstplugin::recvParamDisplayValueUpdate(int32_t idx) {
    automatable_param_t* param = getEffectParam(idx);
    dbgassert(param && param->internalIdx >= 0);
    param->paramDisplayValState &= ~PARAM_FLAG_DIRTY;
    char buf[PLUGIN_PARAM_STR_MAX_LEN+1]{};
    this->dispatch(effGetParamDisplay, param->internalIdx, 0, buf);
    if (buf[0]) {
        param->paramDisplayValStr = buf;
        param->paramDisplayValState |= PARAM_FLAG_SET;
    }
}

void vstplugin::recvProgramListUpdate() {
    if (bIsLoadingProgram) {
        return;
    }
    this->programNames.resize(0);
    this->programNames.reserve(handle->aeffect->numPrograms);
    char buf[PLUGIN_PROGRAM_STR_MAX_LEN+1];
    for (int i = 0; i < handle->aeffect->numPrograms; i++) {
        memset(buf, 0, sizeof(buf));
        if (this->dispatch(effGetProgramNameIndexed, i, 0, buf)) {
            this->programNames.emplace_back(buf);
        }
    }
}

void vstplugin::recvParamNameUpdate(int32_t idx) {
    if (bIsLoadingProgram) {
        return;
    }
    automatable_param_t* param = getEffectParam(idx);
    dbgassert(param && param->internalIdx >= 0);
    param->paramNameState &= ~PARAM_FLAG_DIRTY;
    char buf[PLUGIN_PARAM_STR_MAX_LEN+1]{};
    this->dispatch(effGetParamName, param->internalIdx, 0, buf);
    String paramName = buf[0] ? buf : StringFormat("Parameter %d", param->internalIdx);
    param->name = paramName;
    param->paramNameState |= PARAM_FLAG_SET;
}

void vstplugin::recvProgramNameUpdate() {
    if (bIsLoadingProgram) {
        return;
    }
    char buf[PLUGIN_PROGRAM_STR_MAX_LEN+1];
    memset(buf, 0, sizeof(buf));
    auto curProgram = dispatch(effGetProgram);
    if (curProgram >= 0 && dispatch(effGetProgramNameIndexed, curProgram, 0, buf)) {
        if (programNames.size() < size_t(curProgram) && curProgram+1 < (4096)) {
            programNames.resize(curProgram+1);
        }
        if (curProgram >= 0 && size_t(curProgram) < programNames.size()) {
            programNames[curProgram] = buf;
        }
        this->currentProgramNameStr = buf;
        this->currentProgramNameSet = true;
        return;
    }
    memset(buf, 0, sizeof(buf));
    if (dispatch(effGetProgramName, 0, 0, buf)) {
        this->currentProgramNameStr = buf;
        this->currentProgramNameSet = true;
    }
}

bool vstplugin::hasWindowEditor() {
    return handle->aeffect->flags & effFlagsHasEditor;
}
void vstplugin::updateFromMainThread() {
    if (this->windowHost) {
        this->dispatch(effEditIdle);
    }
    effectbase::updateFromMainThread();
}

bool vstplugin::showWindow(bool bResetPosition) {
    ERect* prc = nullptr;
    this->dispatch(effEditGetRect, 0, 0, (void*) &prc);
    ivec2 defSize = { 0, 0 };
    if (prc) {
        defSize = {prc->right - prc->left, prc->bottom - prc->top};
    }
    this->openWindow(bResetPosition, defSize);
    return true;
}

String vstplugin::getInfo(std::vector<String>& list) {
    String out;

    char szBuf[256] = "";

    list.push_back(StringFormat("Filename %s", StringAsCStr(this->sName)));

    if (this->getNameString(szBuf)) {
        list.push_back(StringFormat("Name %s", szBuf));
    }
    list.push_back(StringFormat("Dir %s", StringAsCStr(this->sDir)));

    AEffect* handle = this->handle->aeffect;
    char sUID[5];
    int i;
    for (i = 0; i < 4; i++) {
        sUID[i] = ((char*) &handle->uniqueID)[3 - i];
        if (!sUID[i])
            sUID[i] = ' ';
    }
    sUID[i] = '\0';
    list.push_back(StringFormat("VstID: '%s' (%08X)", sUID, handle->uniqueID));
    list.push_back(StringFormat("Version %d", handle->version));
    list.push_back(StringFormat("initialDelay: %d", handle->initialDelay));

    list.push_back(StringFormat("%d outputs", handle->numOutputs));
    list.push_back(StringFormat("%d inputs", handle->numInputs));
    list.push_back(StringFormat("%d programs", handle->numPrograms));
    list.push_back(StringFormat("%d parameters", handle->numParams));

    list.push_back(StringFormat("Flags: %08X", handle->flags));
    if (handle->flags & effFlagsNoSoundInStop)
        out += "effFlagsNoSoundInStop\n";
    if (handle->flags & effFlagsIsSynth)
        out += "effFlagsIsSynth\n";
    if (handle->flags & effFlagsProgramChunks)
        out += "effFlagsProgramChunks\n";
    if (handle->flags & effFlagsCanReplacing)
        out += "effFlagsCanReplacing\n";
    if (handle->flags & effFlagsHasEditor)
        out += "effFlagsHasEditor\n";
    if (handle->flags & effFlagsCanDoubleReplacing)
        out += "effFlagsCanDoubleReplacing\n";
    const char* plug_features_array[] = {
        PlugCanDos::canDoSendVstEvents,
        PlugCanDos::canDoSendVstMidiEvent,
        PlugCanDos::canDoReceiveVstEvents,
        PlugCanDos::canDoReceiveVstMidiEvent,
        PlugCanDos::canDoReceiveVstTimeInfo,
        PlugCanDos::canDoOffline,
        PlugCanDos::canDoMidiProgramNames,
        PlugCanDos::canDoBypass,
    };
    for (const char* canDo : plug_features_array) {
        if (this->dispatch(effCanDo, 0, 0, (void*) canDo)) {
            list.push_back(StringFormat("effCanDo %s", canDo));
        }
    }

    return out;
}

handles_t::~handles_t() {
    hmodule = nullptr;// we no longer own
}
int64_t vstplugin::dispatch(
        int32_t opcode,
        int32_t index,
        int64_t value,
        void* ptr,
        float opt) {
    return vst_dispatch(this, this->handle->aeffect, opcode, index, value, ptr, opt);
}
void vstplugin::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    dbgassert(!isInSuspend);
    dbgassert(this->handle->aeffect);
    dbgassert(in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
    vst_process(this, this->handle->aeffect, in->buf, out->buf, numSamples);
}

void vstplugin::sendNotesOff() {
#ifdef VST_PLUGIN_TRACK_NOTES
    if (bCanReceiveMidi) {
        const auto& heldNotes = handle->heldNotes;
        VstEvent_t::ReallocVstEvents(&handle->midiEventsBuf, heldNotes.size() + 1);
        VstEvent_t* midiEventsBuf = handle->midiEventsBuf;
        midiEventsBuf->reset();
        for (const auto& notePitch : heldNotes) {
            noteevent_t evt = {notePitch, 0, 0, 0, false, false};
            midiEventsBuf->writeVstMidiEvt(evt, 0, format.blockSize);
        }
        dbgassert(midiEventsBuf->vstEvents->numEvents == (int32_t) heldNotes.size());
        midiEventsBuf->writeInstantOff();
        //TODO: decide if we should make a copy, plugin may manipulate data
        //VstEvent_t midiEventsBufTemp = *midiEventsBuf;
        this->midiEventsDispatched += handle->midiEventsBuf->vstEvents->numEvents;
        this->dispatch(effProcessEvents, 0, 0, handle->midiEventsBuf->vstEvents);
    }
#endif
    handle->heldNotes.clear();
}
void vstplugin::processMidi(midi_data_processing_t& midiEvents) {
    if (this->numMidiInputChannels > 0) {
        size_t numEvents = midiEvents.noteEvents->size()+midiEvents.ctrlEvents->size();
        if (numEvents) {
            const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, midiEvents.bpm100, format.sampleRate);
            VstEvent_t::ReallocVstEvents(&handle->midiEventsBuf, numEvents);
#ifdef VST_PLUGIN_TRACK_NOTES
            auto& heldNotes = handle->heldNotes;
#endif
            VstEvent_t* midiEventsBuf = handle->midiEventsBuf;
            for (auto& evt : *midiEvents.noteEvents) {
                if (this->numMidiInputChannels > evt.channel) {
                    midiEventsBuf->writeVstNoteEvent(evt, tickToSamples, format.blockSize);
#ifdef VST_PLUGIN_TRACK_NOTES
                    bool bContained = std::binary_search(std::begin(heldNotes), std::end(heldNotes), evt.pitch);
                    if (evt.isNoteOn && !bContained) {
                        insertSorted(heldNotes, evt.pitch);
                    } else if (!evt.isNoteOn && bContained) {
                        removeEntry(heldNotes, evt.pitch);
                    }
#endif
                }
            }
            for (auto& evt : *midiEvents.ctrlEvents) {
                auto midiChannel = int32_t(evt.message & 0x0F);
                if (this->numMidiInputChannels > midiChannel) {
                    auto offsetInBlock = math::floordS32((evt.tick - midiEvents.tickLatencyCompensated) * tickToSamples);
                    if (offsetInBlock < 0 || offsetInBlock >= format.blockSize) {
                        log_lf(Log::L_WARN, "VST: ctrl event out of range: %d\n", offsetInBlock);
                        continue;
                    }
                    midiEventsBuf->writeMidiMessage(evt.message, offsetInBlock);
                }
            }
            this->midiEventsDispatched += handle->midiEventsBuf->vstEvents->numEvents;
            this->dispatch(effProcessEvents, 0, 0, handle->midiEventsBuf->vstEvents);
        }
    }
}

FUNC_NOINLINE
void vst_onException(vstplugin* plugin)
{
    log_lf(Log::L_ERROR, "segfault/fatal exception\n");
    if (!plugin->isBypass()) {
        plugin->setParamValue(PARAM_ENABLE, 0, FLG_PAR_UPDATE_NOSTORE);
        log_lf(Log::L_ERROR, "segfault/fatal exception on %s\n", StringAsCStr(plugin->getName()));
    }
}
vstplugin::vstplugin(handles_t* _handle, int32_t globalId, IHostCallback* hostcallback, String _sDir, String sName, int32_t _moduleId, int32_t _bugfixFlags)
    : effectbase(std::move(sName), globalId, hostcallback),
      handle(_handle),
      internalModuleId(_moduleId),
      sDir(std::move(_sDir)),
      bugfixFlags(_bugfixFlags)
{
    bSupportsWindowResize = _handle->axEffect != nullptr;
}
