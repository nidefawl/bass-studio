#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "config.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_daw.h"

#include "basectrl.h"

#include "platform.h"

#include "latency-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"

#define PLUGIN_EFFECT_NAME "Latency"
#define PLUGIN_UID "LTCY"
#define PLUGIN_PRODUCT_NAME "Latency introducing plugin"
#define MAX_LATENCY (1024 * 16)

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginLatency::createPlugin(audioMaster);
}
#endif


namespace PluginLatency {

    PluginVST2_Latency::PluginVST2_Latency(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        setNewLatency(current()->latency);
    }

    void PluginVST2_Latency::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_Latency::setProgramName(char* name) {
    }

    void PluginVST2_Latency::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_Latency::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kLatency:
                vst_strncpy(label, "samples", kVstMaxParamStrLen);
                return;
            default:
                vst_strncpy(label, "", kVstMaxParamStrLen);
        }
    }

    void PluginVST2_Latency::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kLatency: {
                snprintf(text, kVstMaxParamStrLen, "%d", current()->latency);
                break;
            }
        }
    }

    void PluginVST2_Latency::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kLatency:
                vst_strncpy(label, "Latency", kVstMaxParamStrLen);
                return;
        }
    }

    void PluginVST2_Latency::setParameter(VstInt32 index, float value) {
        Program* ap = current();
        switch (index) {
            case kLatency:
                ap->latency = math::max(0, math::min(MAX_LATENCY, (int32_t) std::round(value * MAX_LATENCY)));
                setNewLatency(ap->latency);
                break;
        }
#if BUILD_VSTHOST
        for (auto& pviewctr : this->views) {
            if (pviewctr->isInUse()) {
                pviewctr->onSetParameter(index, value);
            }
        }
#else
        if (this->editor) {
            static_cast<pluginwindow*>(this->editor)->onSetParameter(index, value);
        }
#endif
    }

    float PluginVST2_Latency::getParameter(VstInt32 index) {
        Program* ap = current();
        float value = 0;
        switch (index) {
            case kLatency:
                value = std::max(0.0f, std::min(1.0f, ap->latency / (float) MAX_LATENCY));
                break;
        }
        return value;
    }

    bool PluginVST2_Latency::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", kVstMaxProgNameLen);
            return true;
        }
        return false;
    }

    bool PluginVST2_Latency::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_Latency::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_Latency::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    void PluginVST2_Latency::setNewLatency(int32_t nSamplesLatency) {
        this->newLatency     = nSamplesLatency;
        this->latencyChanged = true;
    }

    VstInt32 PluginVST2_Latency::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_Latency::canDo(char* text) {
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    void PluginVST2_Latency::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        if (this->latencyChanged) {
            this->latencyChanged             = false;
            this->curLatency                 = this->newLatency;
            this->getAeffect()->initialDelay = this->curLatency;
        }
        dbgassert(this->curLatency >= 0 && this->curLatency <= (1 << 20));
        int32_t nChannels = this->getAeffect()->numOutputs;
        if (!this->delayLine) {
            this->delayLine = std::make_unique<DelayLine>();
        }
        AudioBlock inputBlock(inputs, nChannels, sampleFrames);
        AudioBlock outputBlock(outputs, nChannels, sampleFrames);
        delayAudio(this->delayLine.get(), &inputBlock, &outputBlock, this->curLatency);
    }


    Program::Program() : ProgramParameters() {
        vst_strncpy(name, "Init", kVstMaxProgNameLen);
        latency = 1024;
    }

}

namespace PluginLatency {


    class guicontainer_plugin_latency : public guictr_base {
        PluginVST2_Latency* const plugin;
        guiknob_pluginparam knoblatency;

    public:
        explicit guicontainer_plugin_latency(PluginVST2_Latency* plugin)
            : guictr_base(),
            plugin(plugin),
            knoblatency(PARAM_OFFSET_EXTERNAL + kLatency, kLatency) {
            setBackgroundRendered(true);
            padding = 4;
            margin  = 4;
            add(&knoblatency);
        }
        ~guicontainer_plugin_latency() override {
            remove(&knoblatency);
        }

        guiknob_pluginparam* getKnobFromParameter(int32_t index) {
            switch (index) {
                case kLatency:
                    return &knoblatency;
            }
            return nullptr;
        }
        void onSetParameter(int32_t index, float value) {
#if BUILD_EXTERNAL_PLUGIN
            guiknob_pluginparam* knob = getKnobFromParameter(index);
            if (knob) {
                knob->setValueInit(value);
            }
#endif
        }
        void onGuiOpen() {
#if BUILD_VSTHOST
            knoblatency.setEffectInstance(plugin->getHostSideHandle());
#endif
        }
        void onGuiClose() {
        }

        void render(NVGcontext* vg) override {
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }

            for (guibase* gui : guis) {
                nvgSave(vg);
                gui->render(vg);
                nvgRestore(vg);
            }
        }
        void layout() override {
            ivec2 cs           = getSizeContent();
            const int inset    = 4;
            const int knobSize = math::max(32, (cs.x - inset * 3) / 2);
            knoblatency.size   = ivec2(knobSize, cs.y - inset * 2);
            knoblatency.size   = ivec2(knobSize, cs.y - inset * 2);
            knoblatency.pos    = ivec2(inset);
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
        bool handleKeyInput(KeyEvent& event) override {
            if (event.type != KeyEventType::K_RELEASE) {
            }
            return false;
        }
        void buttonClicked(guibase* button) override {
        }
    };

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_Latency(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_Latency::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guicontainer_plugin_latency, PluginVST2_Latency>>(this);
        this->views.push_back(view);
        return view;
    }
}
