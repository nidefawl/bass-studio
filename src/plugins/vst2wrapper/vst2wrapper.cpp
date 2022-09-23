#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "assert_dbg.h"
#include "config.h"
#include "host/plugin/empty.h"
#include "modules.h"
#include "plugins/gain/gain-plugin.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/samplecrush/samplecrush-plugin.h"
#include "plugins/sampledelay/sampledelay-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "plugins/info/info-plugin.h"
#include "plugins/synth/synth-plugin.h"
#include "seq_util.h"
#include "types.h"
#include "automation.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"
#include "samplerate.h"
#include "math/seq_math.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/internal_plugin.h"
#include "plugins/plugin-ui.h"
#include "plugins/gain/gain-plugin.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"
#include "basectrl.h"
#include "platform.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "audioblock.h"
#include "vst2wrapper.h"
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-plugin-2.4/audioeffectx.h>


namespace PluginWrapper {
    const char* const PLUGIN_EFFECT_NAME = "Gain";
    const char* const PLUGIN_UID = "NMHG";
    const char* const PLUGIN_PRODUCT_NAME = "Gain plugin";
    static constexpr int32_t PARAM_OFFSET = 1;
    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }

    class guictr_effectbase_vst2;
    class PluginInternalVST2 : public BasePluginVST2 {
        internalplugin* const effect;
    public:
        explicit PluginInternalVST2(audioMasterCallback audioMaster, internalplugin* effect)
            : BasePluginVST2(audioMaster, PLUGIN_UID), effect(effect) {
            effect->setSampleFormat(sampleformat_t{ 44100, 512, sampleformat_bits_t::FLOAT_32 });
            effect->initBuffers();
            effect->initMeters();
            effect->bIsEnabled = true;
            uint32_t numPrograms = 0;
            effect->getNumberOfPrograms(numPrograms);
            uint32_t numParams = 0;
            effect->visitParams([&numParams](auto& mapEntry){
                if (mapEntry.first >= PARAM_OFFSET) {
                    ++numParams;
                }
            });
            this->numPrograms = this->cEffect.numPrograms = static_cast<VstInt32>(numPrograms);
            this->numParams = this->cEffect.numParams = static_cast<VstInt32>(numParams);
            this->setNumInputs(effect->blockInputs->channels);
            this->setNumOutputs(effect->blockOutputs->channels);
            setInitialDelay(static_cast<VstInt32>(effect->getPluginLatency()));
        }

        ~PluginInternalVST2() override = default;

        // VST2 API
        bool beginSetProgram() override {
            this->issetprogram = true;
            return false;
        }

        bool endSetProgram() override {
            this->issetprogram = false;
            return false;
        }

        VstPlugCategory getPlugCategory() override {
            return kPlugCategEffect;
        }

        void setProgram(VstInt32 program) override {
            if (program < 0 || program >= this->numPrograms)
                return;
            curProgram = program;
        }

        void setProgramName(char* name) override {
        }

        void getProgramName(char* name) override {
            if (name) *name = 0;
        }

        void getParameterLabel(VstInt32 index, char* label) override {
            index += PARAM_OFFSET;
            if (!assert_expr(effect->getParam(index) && label))
                return;
            *label = 0;
            auto name = effect->getParamValueDisplay(index);
            safe_str_to_buf(label, kVstMaxParamStrLen, name.unit);
        }

        void getParameterDisplay(VstInt32 index, char* text) override {
            index += PARAM_OFFSET;
            if (!assert_expr(effect->getParam(index) && text))
                return;
            *text = 0;
            auto name = effect->getParamValueDisplay(index);
            safe_str_to_buf(text, kVstMaxParamStrLen, name.value);
        }

        void getParameterName(VstInt32 index, char* label) override {
            index += PARAM_OFFSET;
            if (!assert_expr(effect->getParam(index) && label))
                return;
            *label = 0;
            auto name = effect->getParamName(index);
            safe_str_to_buf(label, kVstMaxParamStrLen, name);
        }

        void setParameter(VstInt32 index, float value) override {
            index += PARAM_OFFSET;
            auto param = effect->getParam(index);
            if (!assert_expr(param))
                return;
            auto preVal = param->value;
            auto flags = FLG_PAR_UPDATE_USER;
            effect->setParamValue(index, value, flags);
            effect->postSetParameter(index, preVal, effect->getParamValue(index), flags);
        }

        float getParameter(VstInt32 index) override {
            index += PARAM_OFFSET;
            auto param = effect->getParam(index);
            if (!assert_expr(param))
                return 0.0f;
            return param->value;
        }

        bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) override {
            return false;
        }

        bool getEffectName(char* name) override {
            vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
            return true;
        }

        bool getVendorString(char* text) override {
            vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
            return true;
        }

        bool getProductString(char* text) override {
            vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
            return true;
        }

        VstInt32 getVendorVersion() override {
            return 1;
        }

        VstInt32 canDo(char* text) override {
            //if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
            //    return 1;
            if (!strcmp(text, PlugCanDos::canDoReceiveVstMidiEvent))
                return 1;
            if (!strcmp(text, PlugCanDos::canDoReceiveVstTimeInfo))
                return 1;
            if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
                return 1;
            return -1;// explicitly can't do; 0 => don't know
        }

        void suspend() override {
            effect->onDisable();
        }

        void resume() override {
            setInitialDelay(static_cast<VstInt32>(effect->getPluginLatency()));
            effect->onEnable();
        }

        void setSampleRate(float sampleRate) override {
            BasePluginVST2::setSampleRate(sampleRate);
            auto sr = static_cast<samplerate_t>(sampleRate);
            auto format = effect->getSampleFormat();
            if (sr != format.sampleRate) {
                format.sampleRate = sr;
                effect->setSampleFormat(format);
                effect->initBuffers();
                effect->initMeters();
            }
        }

        void setBlockSize(VstInt32 blockSize) override {
            BasePluginVST2::setBlockSize(blockSize);
            auto bs = static_cast<blocksize_t>(blockSize);
            auto format = effect->getSampleFormat();
            if (bs != format.blockSize) {
                format.blockSize = bs;
                effect->setSampleFormat(format);
                effect->initBuffers();
                effect->initMeters();
            }
        }

        void processReplacing(float** inputs, float** outputs, VstInt32 numSamples) override {
            if (issetprogram)
                return;
            if (!assert_expr(numSamples <= blockSize)) {
                return;
            }
            double samplePos = 0;
            double tick = 0;
            playback_state state = playback_state::status_stop;
            VstTimeInfo* timeinfo = getTimeInfo(VstTimeInfoFlags::kVstTransportChanged | VstTimeInfoFlags::kVstPpqPosValid);
            if (timeinfo) {
                samplePos = timeinfo->samplePos;
            }
            if (timeinfo && (timeinfo->flags & VstTimeInfoFlags::kVstPpqPosValid)) {
                tick = timeinfo->ppqPos * TICKS_QUARTER;
            }
            if (timeinfo && (timeinfo->flags & VstTimeInfoFlags::kVstTransportPlaying)) {
                state = playback_state::status_playback;
            }
            dbgassert(this->getAeffect()->numInputs == effect->blockInputs->channels);
            dbgassert(this->getAeffect()->numOutputs == effect->blockOutputs->channels);
            AudioBlock inputBlock(inputs, this->getAeffect()->numInputs, numSamples);
            AudioBlock outputBlock(outputs, this->getAeffect()->numOutputs, numSamples);
            effect->blockInputs->copyFrom(&inputBlock);
            effect->process(effect->blockInputs, effect->blockOutputs, tick, samplePos, numSamples, state);
            outputBlock.copyFrom(effect->blockOutputs);
            effect->postProcess(&outputBlock, numSamples, true);
        }
        // internal API
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
            // I don't think this will be called
            // This API part is mainly used for internal plugins that rely on VST2 API
            dbgassert(0);
            return BasePluginVST2::convertParamValueDisplay(idx, displayValue);
        }

        std::shared_ptr<PluginViewContainers> createViewCtrVst2() override;

        void sendParameterUpdateToHost(int32_t index, float value, int32_t flags) {
            if (index >= PARAM_OFFSET) {
                if (assert_expr(audioMaster)) {
                    audioMaster (&cEffect, audioMasterAutomate, index - PARAM_OFFSET, 0, nullptr, value);	// value is in opt
                }
            }
        }
    };
    class guictr_effectbase_vst2 : public guictr_base {
        PluginWrapper::PluginInternalVST2* const vstInstance;
        effectbase* const module;
        std::vector<guiknob_pluginparam*> knobs;
        gui_textfield editfield;
        void init() {
            setLayoutMode(LAYOUT_HORIZONTAL);
            editfield.setFlag(FLG_NO_LAYOUT, true);
            setBackgroundRendered(true);
            setCanMouseHit(true);
            padding = 4;
            margin  = 2;
            editfield.setVisible(false);
            editfield.setAlignment(gui_textfield::Alignment::Center);
            editfield.setReturnCommits(true);
        }
    public:
        explicit guictr_effectbase_vst2(PluginWrapper::PluginInternalVST2* _vstInstance, effectbase* module)
        : guictr_base(), vstInstance(_vstInstance), module(module)
        {
            init();
            std::vector<automatable_param_t*> paramsSorted;
            module->getSortedParams(paramsSorted);
            erase_if(paramsSorted, [](const automatable_param_t* p) {
                return p->idx == PARAM_ENABLE;
            });
            knobs.reserve(paramsSorted.size());
            for (automatable_param_t* param : paramsSorted) {
                knobs.push_back(new guiknob_pluginparam(param->idx, param->idx, guiknob::knobtype::SLIDER_LABELED));
                add(knobs.back());
            };
            add(&editfield);
        }
        ~guictr_effectbase_vst2() override {
            removeGuis();
        }
        void layoutEntries(ivec2 dir) override {
            guictr_base::layoutEntries(dir);
        }
        void buttonClicked(guibase* button) override {
            auto param = dynamic_cast<guiknob_pluginparam*>(button);
            if (param && module) {
                auto paramIdx = param->getParamIdx();
                auto paramValue = module->getParamValueDisplay(paramIdx);
                editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                    auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{str, paramValue.unit});
                    if (paramConverted.success) {
                        module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
                        if (param->fnValueEditChanged)
                            param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                    return true;
                };
                auto layout = param->getLayout();
                editfield.pos = layout.pValue;
                editfield.size = layout.sValue;
                editfield.setVisible(true);
                editfield.layout();
                editfield.setValue(paramValue.value);
                editfield.setSelectionRange(-1, -1);
                editfield.setFontSize(layout.valueHeight*layout.fontScaleValue);
                parentCtrl->focusGui(&editfield);
                return;
            }
            guictr_base::buttonClicked(button);
        }

        void onGuiOpen() {
            for (auto knob : knobs) {
                knob->setEffectInstance(module);
                knob->fnSetValue = [module=this->module, paramIdx=knob->getParamIdx()](float value, int flags) {
                    if (module) {
                        //TODO: lock external VST2 instances
                        // ThreadLock lock     = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                        automation_t* param = module->getRegisteredAutomation(paramIdx);
                        if (param) {
                            param->active = false;
                        }
                        module->setParamValue(paramIdx, value, flags);
                    }
                };
                knob->fnValueEditBegin = [vstInstance=this->vstInstance, paramIdx=knob->getParamIdx()](float preVal, float val) {
                    if (paramIdx >= PARAM_OFFSET) {
                        vstInstance->beginEdit(paramIdx - PARAM_OFFSET);
                    }
                };
                knob->fnValueEditChanged = [knob, vstInstance=this->vstInstance, paramIdx=knob->getParamIdx()](float preVal, float val) {
                    knob->setValueInit(val);
                    if (paramIdx >= PARAM_OFFSET) {
                        vstInstance->setParameterAutomated(paramIdx - PARAM_OFFSET, val);
                    }
                };
                knob->fnValueEditFinish = [vstInstance=this->vstInstance, paramIdx=knob->getParamIdx()](float preVal, float val) {
                    if (paramIdx >= PARAM_OFFSET) {
                        vstInstance->endEdit(paramIdx - PARAM_OFFSET);
                    }
                };
            }
        }

        void onGuiClose() {
            for (auto knob : knobs) {
                knob->setEffectInstance(nullptr);
            }
        }

        guiknob_pluginparam* getKnobFromParameter(int32_t index) {
            auto it = std::find_if(knobs.begin(), knobs.end(), [index](guiknob_pluginparam* knob) {
                return knob->getParamIdx() == index;
            });
            return it != knobs.end() ? *it : nullptr;
        }

        void onSetParameter(int32_t index, float value) {
            auto knob = getKnobFromParameter(index);
            if (knob) {
                knob->setValueInit(value);
            }
        }
        void getSizeScale(int& w, int& h) const {
            w = math::max(20, 100 * CtrSize(knobs));
            h = 300;
        }
    };

    template<typename PluginGUI, typename Plugin>
    class VST2PluginViewContainer : public PluginViewContainers {
        PluginGUI ctr_main;
        uint32_t width;
        uint32_t height;
    public:
        explicit VST2PluginViewContainer(PluginWrapper::PluginInternalVST2* _vstInstance, Plugin* _effectBaseInstance, uint32_t _width = 320, uint32_t _height = 320)
            : ctr_main(_vstInstance, _effectBaseInstance), width(_width), height(_height) {
        }
        ~VST2PluginViewContainer() override = default;
        PluginGUI& getPluginUI() {
            return ctr_main;
        }
        const PluginGUI& getPluginUI() const {
            return ctr_main;
        }
        void layout(int32_t winW, int32_t winH) override {
            ctr_main.pos  = { 0, 0 };
            ctr_main.size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) override {
            v.push_back(&ctr_main);
        }
        void onGuiOpen() override {
            ctr_main.onGuiOpen();
        }
        void onGuiClose() override {
            ctr_main.onGuiClose();
        }
        void onSetParameter(int32_t index, float value) override {
            ctr_main.onSetParameter(index, value);
        }
        void getFixedSize(int32_t* w, int32_t* h) override {
            ctr_main.getSizeScale(*w, *h);
        }
    };

    std::shared_ptr<PluginViewContainers> PluginInternalVST2::createViewCtrVst2() {
        auto view = std::make_shared<VST2PluginViewContainer<guictr_effectbase_vst2, effectbase>>(this, effect, 50, 150);
        this->effect->views.push_back(view);
        this->views.push_back(view);
        return view;
    }
}// namespace PluginWrapper


#ifdef BUILD_EXTERNAL_VST2_PLUGIN
class vst2_wrapper_host_callback : public i_host_callback {
    audioMasterCallback const host;
    PluginWrapper::PluginInternalVST2* vstInstance = nullptr;
    public:
    explicit vst2_wrapper_host_callback(audioMasterCallback _host)
    : host(_host) {
    }
    void setVstInstance(PluginWrapper::PluginInternalVST2* _vstInstance) {
        vstInstance = _vstInstance;
    }
    void onLatencyChanged(effectbase* effect) override {
        vstInstance->setInitialDelay(static_cast<VstInt32>(effect->getPluginLatency()));
        vstInstance->ioChanged();
    }
    void onParametersChanged(effectbase* effect, int32_t idx, float val, int flags, int stage) override {
        (void) effect;
        (void) host;
    }
    void onIOConfigChanged(effectbase* effect) override {
        (void) effect;
        (void) host;
    }
};

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    auto* hostcallback = new vst2_wrapper_host_callback(audioMaster); // TODO: handler leaks
    internalplugin* eff = nullptr;
    switch (BUILD_EXTERNAL_VST2_PLUGIN) {
        case PLUGIN_TYPE_GAIN:
            eff = new module_gain(0, hostcallback);
            break;
        case PLUGIN_TYPE_EMPTY:
            eff = new module_empty(0, hostcallback);
            break;
        case PLUGIN_TYPE_LATENCY:
            eff = new PluginLatency::module_latency(0, hostcallback);
            break;
        case PLUGIN_TYPE_SAMPLE_DELAY:
            eff = new PluginSampleDelay::module_sampledelay(0, hostcallback);
            break;
        case PLUGIN_TYPE_SAMPLE_CRUSH:
            eff = new PluginSampleCrush::module_samplecrush(0, hostcallback);
            break;
        case PLUGIN_TYPE_STEREO_WIDTH:
            eff = new PluginStereoWidth::module_stereowidth(0, hostcallback);
            break;
        case PLUG_INT_SYNTH:
        {
            delete hostcallback;
            return PluginSynth::createPlugin(audioMaster);
        }
        case PLUG_INT_HOSTINFO:
        {
            delete hostcallback;
            return PluginHostInfo::createPlugin(audioMaster);
        }
        default:
            break;
    }
    dbgassert(eff);
    if (!eff)
        return nullptr;
    auto vstInstance = new PluginWrapper::PluginInternalVST2(audioMaster, eff);
    hostcallback->setVstInstance(vstInstance);
    return vstInstance;
}
#endif