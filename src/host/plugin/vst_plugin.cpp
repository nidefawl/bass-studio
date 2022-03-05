#include <algorithm>
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-host-2.4/aeffectx.h>
#include "vst_plugin.h"

#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "logging.h"
#include "audioblock.h"
#include "snapshot.h"
#include "vst_plugin_handles.h"
#include "track.h"
#include "track_impl.h"
#include "host/vst_host.h"
#include "host/vst_window.h"
#include "gui/guiplugin.h"
#include "gui/pluginctr.h"
#include "gui/pluginviewcontainers.h"
#include "plugins/plugin-base.h"
#include "host/mainctrl.h"
#include "host/history.h"


float vst_getParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx);
void vst_setParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx, float value);
void vst_process(vstplugin* plugin, AEffect* aeffect, float** bufIn, float** bufOut, int32_t numSamples);
int64_t vst_dispatch(vstplugin* plugin,
                  AEffect* aeffect,
                  int32_t opcode,
                  int32_t index,
                  int64_t value,
                  void* ptr,
                  float opt);

bool vstplugin::onResize(vst_window*, ivec2) {
    return true;
}

bool vstplugin::updateWindow() {
    if (this->window != nullptr) {
        this->window->updateWindow();
        return true;
    }
    return false;
}

ivec2 vstplugin::constrainSize(vst_window*, ivec2& size) {
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
    return size;
}

bool vstplugin::onClose() {
    if (this->window != nullptr && bEditOpen) {
        this->dispatch(effEditClose);
    }
    bEditOpen = false;
    return true;
}

void vstplugin::onWindowDestroy() {
    this->window = nullptr;
}

void vstplugin::resume() {
    if (!bIsPostInit) {
        bIsPostInit = true;
        postLoad();
    }
    if (!isInSuspend) {
        return;
    }
    isInSuspend = false;
    this->dispatch(effMainsChanged, 0, true);
    this->dispatch(effStartProcess);
}

void vstplugin::sleep() {
    if (isInSuspend) {
        return;
    }
    isInSuspend = true;
    this->dispatch(effStopProcess);
    this->dispatch(effMainsChanged, 0, false);
}

void vstplugin::printNames() {
    char buf[256];
    printf("Name: %s\n", StringAsCStr(sName));
    if (this->dispatch(effGetVendorString, 0, 0, (void*) buf) && buf[0] != 0) {
        printf("effGetVendorString: %s\n", buf);
    }
    if (this->dispatch(effGetProductString, 0, 0, (void*) buf) && buf[0] != 0) {
        printf("effGetProductString: %s\n", buf);
    }
    if (this->dispatch(effGetEffectName, 0, 0, (void*) buf) && buf[0] != 0) {
        printf("effGetEffectName: %s\n", buf);
    }
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
    if (this->window != nullptr) {
        ERect* prc = nullptr;
        this->dispatch(effEditGetRect, 0, 0, (void*) &prc);
        if (prc) {
            this->window->resize({ prc->right - prc->left, prc->bottom - prc->top });
            return true;
        }
    }
    return false;
}

bool vstplugin::onShow(vst_window* _window) {
    if (this->window == _window) {
        bEditOpen = true;
        this->dispatch(effEditOpen, 0, 0, (void*) _window->getHWND());
        updateWindowSize();
        this->updateWindow();
    }
    return true;
}

void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();

void vstplugin::unload(vsthost* host, int flags) {
    effectbase::unload(host, flags);
    dbgassert(this->bIsSetup);
    if (this->window) {
        this->window->close();
    }
    if (this->window) {
        this->window->destroy();
    }
    AppWndProc_enableBlockReentrant();
    this->dispatch(effClose);
    AppWndProc_disableBlockReentrant();
    this->bIsSetup = false;
    log_printf("UNLOAD %s\n", StringAsCStr(this->sName));
}
void vstplugin::configureIOChannels() {
    const auto inputCount = this->handle->aeffect->numInputs;
    const auto outputCount = this->handle->aeffect->numOutputs;
    for (int side = 0; side < 2; ++side) {
        const auto channelCountSide = side == 0 ? inputCount : outputCount;
        auto& channelDescs          = side == 0 ? inputChannelsDesc : outputChannelsDesc;
        const char* sideName        = side == 0 ? "Input" : "Output";
        const auto dispatchOpCode   = side == 0 ? effGetInputProperties : effGetOutputProperties;
        channelDescs.clear();
        int32_t pinIndex = 0;
        VstPinProperties pin{};
        if (pinIndex < channelCountSide && this->dispatch(dispatchOpCode, pinIndex, 0, &pin)) {
            int32_t channelOffset = 0;
            do {
                const String pinName = pin.label;
                // const String pinAttr = StringFormat("Active %d, Stereo %d, UseSpeaker %d",
                //                             (pin.flags&VstPinPropertiesFlags::kVstPinIsActive)!=0,
                //                             (pin.flags&VstPinPropertiesFlags::kVstPinIsStereo)!=0,
                //                             (pin.flags&VstPinPropertiesFlags::kVstPinUseSpeaker)!=0);
                // outputNames.push_back(StringFormat("%s (%s)", pin.label, StringAsCStr(pinAttr)));
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
                for (int i = 0; i < channelCountSide;) {
                    if (i + 1 < channelCountSide) {
                        channelDescs.emplace_back(DAW::channel_desc{i, 2, StringFormat("Stereo %s #", sideName, i)});
                        i += 2;
                    } else {
                        channelDescs.emplace_back(DAW::channel_desc{i, 1, StringFormat("Mono %s #", sideName, i)});
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

    this->blockInputs  = new AudioBlock(math::max(2, inputCount), format.blockSize);
    this->blockOutputs = new AudioBlock(math::max(2, outputCount), format.blockSize);
}
void vstplugin::load(vsthost* host) {
    effectbase::load(host);
    dbgassert(!this->bIsSetup);
    auto aeffect = handle->aeffect;

    aeffect->resvd2     = 0;
    this->vstVersion    = dispatch(effGetVstVersion);
    this->uId           = aeffect->uniqueID;
    this->vendorVersion = dispatch(effGetVendorVersion);

    this->dispatch(effIdentify, 0, 0, nullptr, 0);
    this->dispatch(effOpen);
    this->dispatch(effStopProcess);
    this->dispatch(effMainsChanged, 0, false);

    this->dispatch(effSetSampleRate, 0, 0, nullptr, (float) format.sampleRate);
    this->dispatch(effSetBlockSize, 0, format.blockSize, nullptr, 0);

    configureIOChannels();

    this->pluginCategory  = this->dispatch(effGetPlugCategory);
    this->isSynth         = (handle->aeffect->flags & effFlagsIsSynth) != 0;
    this->bCanReceiveMidi = this->isSynth || this->dispatch(effCanDo, 0, 0, (void*) PlugCanDos::canDoReceiveVstMidiEvent) > 0;


    char buf[1024];
    vst_param_category fallbackCat = { 0, 0, "Parameters" };
    for (int i = 0; i < aeffect->numParams; i++) {
        int32_t paramIdentifier    = PARAM_OFFSET_EXTERNAL + i;
        automatable_param_t* param = registerParam(paramIdentifier);
        param->internalIdx         = i;
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
            if (param->flags & ParamSupportsDisplayIndex) {
                param->displayIndex = properties.displayIndex;
            }
        } else {
            param->flags = 0;
            fallbackCat.nParams++;
        }
        //TODO: wrap getParameter call in exception handler
        param->value = handle->aeffect->getParameter(handle->aeffect, param->internalIdx);
    }
    paramsCategories.push_back(fallbackCat);

    bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
    this->bIsSetup = true;
    if (aeffect->numParams)
        getRegisteredAutomation(65536);
}
void vstplugin::postLoad() {
    this->recvProgramListUpdate();
    this->recvProgramNameUpdate();
    visitParams([](auto& mapEntry) {
        automatable_param_t& param = mapEntry.second;
        param.paramValueState |= PARAM_FLAG_DIRTY;
        param.paramDisplayValState |= PARAM_FLAG_DIRTY;
    });
}

namespace {

    void createSnapshot(plugin_snapshot_t& ps, vstplugin* plugin, const tracksnapshot_store_opts_t& opts) {
        ps.version           = 9;
        ps.slot              = 0;
        ps.projectGlobalId   = plugin->projectGlobalId;
        ps.enabled           = plugin->bIsEnabled;
        ps.ioChannels.input  = plugin->inputChannelsDesc;
        ps.ioChannels.output = plugin->outputChannelsDesc;
        if (plugin->internalModuleId >= 0) {
            ps.pluginType = PLUGIN_TYPE_INTERNAL_EFFECT;
            ps.uId        = static_cast<uint32_t>(plugin->internalModuleId);
        } else {
            ps.pluginType    = PLUGIN_TYPE_VST;
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
                    log_lf(Log::L_DEBUG, "Plugin %s: Save data1[%d]\n", StringAsCStr(plugin->sName), pluginDataSize);
                }
            }
            if (storePluginPresetWithSnapshot) {
                void* pluginData2       = nullptr;
                int32_t pluginDataSize2 = plugin->dispatch(effGetChunk, 1, 0, &pluginData2, 0);
                if (pluginDataSize2 > 0 && pluginData2) {
                    auto* ptrData = reinterpret_cast<uint8_t*>(pluginData2);
                    ps.dataChunk2.reserve(pluginDataSize2);
                    ps.dataChunk2.assign(ptrData, ptrData + pluginDataSize2);
                    log_lf(Log::L_DEBUG, "Plugin %s: Save data2[%d]\n", StringAsCStr(plugin->sName), pluginDataSize2);
                }
            }
        }
        if (opts.storePluginPreset) {
            auto numParamsReserve = math::min<int32_t>(150, plugin->getNumParameters());
            ps.params.reserve(numParamsReserve);
            plugin->visitParams([&ps, vstplugin = plugin, usesBinaryChunks](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                if (param.inUse || !usesBinaryChunks) {
                    float curValue = param.value;
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
    info.numElements = this->handle->aeffect->numPrograms;
    info.pluginVersion = pluginSnapshot.vendorVersion;
    info.pluginUniqueID = pluginSnapshot.uId;
    this->dispatch(effBeginLoadBank, 0, 0, (void*)&info);
    this->dispatch(effBeginSetProgram);
    if ((this->getFlagsVST() & effFlagsProgramChunks) != 0) {
        if (!pluginSnapshot.dataChunk.empty()) {
            auto& localMem = this->handle->dataChunkLocalMemory;
            localMem       = pluginSnapshot.dataChunk;
            log_lf(Log::L_DEBUG, "Plugin %s: Load data1[%d]\n", StringAsCStr(this->sName), localMem.size());
            this->dispatch(effSetChunk, 0, (int64_t) localMem.size(), (void*) localMem.data());
        }
        if (loadPluginPresetWithSnapshot && !pluginSnapshot.dataChunk2.empty()) {
            log_lf(Log::L_DEBUG, "Plugin %s: Load data2[%d]\n", StringAsCStr(this->sName), pluginSnapshot.dataChunk2.size());
            this->dispatch(effSetChunk, 1, (int64_t) pluginSnapshot.dataChunk2.size(), (void*) pluginSnapshot.dataChunk2.data());
        }
    } else {
        this->dispatch(effSetProgram, 0, programIdx);
        if (!pluginSnapshot.currentProgramName.empty()) {
            this->dispatch(effSetProgramName, 0, 0, (void*)StringAsCStr(pluginSnapshot.currentProgramName));
        }
        loadEffectParamsFromSnapshot(pluginSnapshot, this);
    }
    this->dispatch(effSetProgram, 0, programIdx);
    this->dispatch(effEndSetProgram);
    this->dispatch(effSetProgram, 0, programIdx);
    this->bIsLoadingProgram = false;
}

void vstplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    createSnapshot(ps, this, opts);
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

guiplugin* vstplugin::makeGui() {

    dbgassert(handle->hmodule || handle->axEffect);
    if (!handle->gui) {
        std::shared_ptr<guipluginview> view;
        //auto vstPluginView = std::make_unique<guivstplugin>(this);
        //view = vstPluginView;
        //handle->gui = vstPluginView;
        handle->gui = std::make_unique<guivstplugin>(this);
        handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
        if (handle->axEffect) {//only provided by internal vst2 instance (not a DLL)
            guiplugin* pGuiPlugin = handle->gui.get();
            auto* pGuiVstPlugin   = dynamic_cast<guivstplugin*>(pGuiPlugin);
            dbgassert(pGuiVstPlugin);
            auto* baseVst2 = dynamic_cast<BasePluginVST2*>(handle->axEffect);
            dbgassert(baseVst2);
            auto viewCtr = baseVst2->createView();
            if (viewCtr && baseVst2 && pGuiVstPlugin) {
                pGuiVstPlugin->viewCtr = viewCtr;
                viewCtr->addTo(pGuiVstPlugin->viewCtrs);
                viewCtr->onGuiOpen(handle->axEffect);
                viewCtr->setVSTPlugin(this);
            }
        }
    }

    return handle->gui.get();
}

vstplugin::~vstplugin() {
    delete blockInputs;
    delete blockOutputs;
    delete handle;
}

guiplugin* vstplugin::getGui() {
    return handle->gui.get();
}

int32_t vstplugin::getPluginLatency() {
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

float vstplugin::getParamValue(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->internalIdx >= 0) {
        if (param->paramValueState & PARAM_FLAG_DIRTY) {
            param->value = vst_getParameter(this, handle->aeffect, param->internalIdx);
            param->paramValueState = PARAM_FLAG_SET;
        }
    }
    return param->value;
}

String vstplugin::getParamValueDisplay(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->internalIdx >= 0) {
        if (param->paramDisplayValState & PARAM_FLAG_DIRTY) {
            recvParamDisplayValueUpdate(param->internalIdx);
        }
        if (param->paramDisplayValState & PARAM_FLAG_SET) {
            if (param->unit.empty())
                return param->paramDisplayValStr;
            return param->paramDisplayValStr + " " + param->unit;
        }
    }
    return effectbase::getParamValueDisplay(idx);
}

void vstplugin::setParamValue(int32_t idx, float val, int flags) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    param->value = val;
    if (param->idx == PARAM_ENABLE) {
        updateOnEnableParam(param, this->bIsEnabled, val > 0, flags);
    } else {
        if (!(flags & FLG_PAR_UPDATE_NOSTORE) && !(flags & FLG_PAR_UPDATE_AUTOMATED)) {
            param->inUse = true;
        }
        if (param->internalIdx >= 0) {
            vst_setParameter(this, handle->aeffect, param->internalIdx, val);
            param->paramDisplayValState |= PARAM_FLAG_DIRTY;
            param->paramValueState = PARAM_FLAG_SET;
        }
    }
}

void vstplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    if (!(flags & FLG_PAR_UPDATE_USER)) {
        return;
    }
    dbgassert(this->trackImpl->getTrack());
    track_t* track                = this->trackImpl->getTrack();
    automationlane_snapshot_t ref = toRef();
    parameter_ref_t p             = { track->projectIdx, ref.type, this->projectGlobalId, idx };
    DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}

bool vstplugin::setCurrentProgram(uint32_t idx) {
    if (idx < this->programNames.size()) {
        dispatch(effSetProgram, 0, idx, nullptr, 0);
        visitParams([](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
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

void vstplugin::recvParamDisplayValueUpdate(int32_t internalIdx) {
    automatable_param_t* param = getEffectParam(internalIdx);
    dbgassert(param && param->internalIdx >= 0);
    param->paramDisplayValState &= ~PARAM_FLAG_DIRTY;
    char buf[128]{};
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
    char buf[128];
    for (int i = 0; i < handle->aeffect->numPrograms; i++) {
        memset(buf, 0, sizeof(buf));
        if (this->dispatch(effGetProgramNameIndexed, i, 0, buf)) {
            this->programNames.emplace_back(buf);
        }
    }
}

void vstplugin::recvProgramNameUpdate() {
    if (bIsLoadingProgram) {
        return;
    }
    char buf[128];
    memset(buf, 0, sizeof(buf));
    auto curProgram = dispatch(effGetProgram);
    if (curProgram >= 0 && dispatch(effGetProgramNameIndexed, curProgram, 0, buf)) {
        if (programNames.size() < curProgram && curProgram+1 < (4096)) {
            programNames.resize(curProgram+1);
        }
        if (curProgram >= 0 && curProgram < programNames.size()) {
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

automationlane_snapshot_t vstplugin::toRef() const {
    automationlane_snapshot_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}

void vstplugin::onEnable() {
    //TODO: check current thread, check if playthread is locked
    resume();
}

void vstplugin::onDisable() {
    //TODO: check current thread, check if playthread is locked
    sleep();
    vsthost::getInstance()->sendNotesOff(this);
}

bool vstplugin::close() {
    if (this->window != nullptr) {
        this->window->close();
    }
    return true;
}

bool vstplugin::show() {
    if (this->window == nullptr && (handle->aeffect->flags & effFlagsHasEditor)) {
        ERect* prc = nullptr;
        this->dispatch(effEditGetRect, 0, 0, (void*) &prc);
        ivec2 size = { 160, 120 };
        if (prc) {
            size = { prc->right - prc->left, prc->bottom - prc->top };
        }
        if (size.x <= 0) size.x = 160;
        if (size.y <= 0) size.y = 120;
        this->window = vst_window::make(this, this->sName, size, false);
    }
    if (this->window != nullptr) {
        this->window->show();
    }
    return false;
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
    list.push_back(StringFormat("VstID: '%s' (%08lXH)", sUID, handle->uniqueID));
    list.push_back(StringFormat("Version %d", handle->version));
    list.push_back(StringFormat("initialDelay: %d", handle->initialDelay));

    list.push_back(StringFormat("%d outputs", handle->numOutputs));
    list.push_back(StringFormat("%d inputs", handle->numInputs));
    list.push_back(StringFormat("%d programs", handle->numPrograms));
    list.push_back(StringFormat("%d parameters", handle->numParams));

    list.push_back(StringFormat("Flags: %08lXH", handle->flags));
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
void vstplugin::process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) {
    dbgassert(!isInSuspend);
    dbgassert(getTrackLink()->sampleFormat == this->format && in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
    vst_process(this, this->handle->aeffect, in->buf, out->buf, numSamples);
}

extern "C" void vst_onException(vstplugin* plugin)
{
    log_printf("segfault/fatal exception\n");
    if (!plugin->isBypass()) {
        plugin->setParamValue(PARAM_ENABLE, 0, FLG_PAR_UPDATE_NOSTORE);
        log_printf("segfault/fatal exception on %s\n", StringAsCStr(plugin->getName()));
    }
}