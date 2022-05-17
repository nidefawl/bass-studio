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

#include "../plugin.h"
#include "bitcrush-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"

#define PLUGIN_EFFECT_NAME "Samplecrush"
#define PLUGIN_UID "SMPC"
#define PLUGIN_PRODUCT_NAME "Samplecrush plugin"

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginBitcrush::createPlugin(audioMaster);
}
#endif


namespace PluginBitcrush {

    PluginVST2_Bitcrush::PluginVST2_Bitcrush(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        setNewBitcrushLvl(current()->bitcrush);
    }

    void PluginVST2_Bitcrush::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_Bitcrush::setProgramName(char* name) {
    }

    void PluginVST2_Bitcrush::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_Bitcrush::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kBitcrush:
                vst_strncpy(label, "samples", kVstMaxParamStrLen);
                return;
            default:
                vst_strncpy(label, "", kVstMaxParamStrLen);
        }
    }

    void PluginVST2_Bitcrush::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kBitcrush: {
                int nPow2 = (1 << current()->bitcrush);
                snprintf(text, kVstMaxParamStrLen, "%d", nPow2);
                break;
            }
        }
    }

    void PluginVST2_Bitcrush::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kBitcrush:
                vst_strncpy(label, "Bitcrush", kVstMaxParamStrLen);
                return;
        }
    }

    void PluginVST2_Bitcrush::setParameter(VstInt32 index, float value) {
        Program* ap = current();
        switch (index) {
            case kBitcrush:
                ap->bitcrush = math::max(BITCRUSH_BITS_MIN, math::min(BITCRUSH_BITS_MAX, (int32_t) std::round(value * (BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN) + BITCRUSH_BITS_MIN)));
                setNewBitcrushLvl(ap->bitcrush);
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

    float PluginVST2_Bitcrush::getParameter(VstInt32 index) {
        Program* ap = current();
        float value = 0;
        switch (index) {
            case kBitcrush:
                value = std::max(0.0f, std::min(1.0f, (ap->bitcrush - BITCRUSH_BITS_MIN) / (float) (BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN)));
                break;
        }
        return value;
    }

    bool PluginVST2_Bitcrush::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", kVstMaxProgNameLen);
            return true;
        }
        return false;
    }

    bool PluginVST2_Bitcrush::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_Bitcrush::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_Bitcrush::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }
    void PluginVST2_Bitcrush::setNewBitcrushLvl(int32_t nSamplesBitcrush) {
        this->newBitcrush        = nSamplesBitcrush;
        this->bitcrushLvlChanged = true;
    }

    VstInt32 PluginVST2_Bitcrush::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_Bitcrush::canDo(char* text) {
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    static void processSampleCrush(float** inputs, float** outputs, VstInt32 sampleFrames, const int sampleCrushLevel) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        int steps   = 1 << (sampleCrushLevel);
        if (steps <= 1) {
            for (int a = 0; a < sampleFrames; a++) {
                (*out1++) = (*in1++) < 0 ? -1 : 1;
                (*out2++) = (*in2++) < 0 ? -1 : 1;
            }
        } else {
            for (int a = 0; a < sampleFrames; a += steps) {
                float accL = 0;
                float accR = 0;

                for (int b = 0; b < steps; b++) {
                    accL += (*in1++);
                    accR += (*in2++);
                }
                for (int b = 0; b < steps; b++) {
                    (*out1++) = (accL) < 0 ? -1 : 1;
                    (*out2++) = (accR) < 0 ? -1 : 1;
                }
            }
        }
    }
    void PluginVST2_Bitcrush::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        if (this->bitcrushLvlChanged) {
            this->bitcrushLvlChanged         = false;
            this->curBitcrush                = this->newBitcrush;
            this->getAeffect()->initialDelay = this->curBitcrush;
        }
        dbgassert(this->curBitcrush >= BITCRUSH_BITS_MIN && this->curBitcrush <= (BITCRUSH_BITS_MAX));

        if (this->getAeffect()->numOutputs == 2) {
            Program* ap = current();
            processSampleCrush(inputs, outputs, sampleFrames, ap->bitcrush);
        }
    }


    Program::Program() : ProgramParameters() {
        vst_strncpy(name, "Init", kVstMaxProgNameLen);
        bitcrush = BITCRUSH_BITS_MAX;
    }

}// namespace PluginBitcrush

namespace PluginBitcrush {


    class guicontainer_plugin_latency : public guictr_base {
        vstplugin* vstHostSide = nullptr;
        AudioEffect* curEffect = nullptr;
        guiknob_pluginparam knoblatency;

    public:
        guicontainer_plugin_latency()
            : guictr_base(), knoblatency(PARAM_OFFSET_EXTERNAL + kBitcrush, kBitcrush) {
            setBackgroundRendered(true);
            padding = 4;
            margin  = 4;
            add(&knoblatency);
        }
        ~guicontainer_plugin_latency() override {
            remove(&knoblatency);
        }
        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            if (this->contains(mpos)) {
                ivec2 localMouse = this->toContainerSpace(mpos);
                for (guibase* gui : guis) {
                    if (gui->mouseHitTest(localMouse, evt)) {
                        return true;
                    }
                }
                if (evt.type == MouseHitType::MOUSE_LEFT) {
                    evt.requestFocus(this);
                    return true;
                }
            }
            return false;
        }

        guiknob_pluginparam* getKnobFromParameter(int32_t index) {
            switch (index) {
                case kBitcrush:
                    return &knoblatency;
            }
            return nullptr;
        }
        void onSetParameter(int32_t index, float value) {
            guiknob_pluginparam* knob = getKnobFromParameter(index);
            if (knob && curEffect) {
                knob->setValueInit(value);
                knob->setDisplayValueFromEffect();
            }
        }
        void onGuiOpen(AudioEffect* eff) {
            this->curEffect = eff;
            knoblatency.setAudioEffect(eff);
        }
        void onGuiClose(AudioEffect* eff) {
            this->curEffect = nullptr;
        }
        void setVSTPlugin(vstplugin* _vstHostSide) {
            this->vstHostSide = _vstHostSide;
#if BUILD_VSTHOST
            knoblatency.setEffectInstance(_vstHostSide);
#endif
        }
        void onTick(AppCtrl* ctrl) override {
            for (guibase* gui : guis) {
                gui->onTick(ctrl);
            }
        }
        void prerender(NVGcontext* vg) override {
            for (guibase* gui : guis) {
                gui->prerender(vg);
            }
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


    class ViewContainers_Plugin_Bitcrush : public PluginViewContainersImpl {
    public:
        guicontainer_plugin_latency ctr_main;
        ViewContainers_Plugin_Bitcrush() : PluginViewContainersImpl(220, 150) {
        }
        ~ViewContainers_Plugin_Bitcrush() override = default;

        void layout(int32_t winW, int32_t winH) override {
            ctr_main.pos  = { 0, 0 };
            ctr_main.size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) override {
            v.push_back(&ctr_main);
        }
        void onGuiOpen(AudioEffect* eff) override {
            ctr_main.onGuiOpen(eff);
        }
        void onGuiClose(AudioEffect* eff) override {
            ctr_main.onGuiClose(eff);
        }
        void onSetParameter(int32_t index, float value) override {
            ctr_main.onSetParameter(index, value);
        }
        void getFixedSize(int32_t* w, int32_t* h) override {
            *w = this->width;
            *h = this->height;
        }
        void setVSTPlugin(vstplugin* hostsideplugin) override {
            ctr_main.setVSTPlugin(hostsideplugin);
        }
    };


    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_Bitcrush(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_Bitcrush::createView() {
        std::shared_ptr<PluginViewContainers> view = std::make_shared<ViewContainers_Plugin_Bitcrush>();
        this->views.push_back(view);
        return view;
    }
}// namespace PluginBitcrush
