#pragma once

#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "rand.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

namespace PluginTestAdv {

    class GuiAdvPluginVST2;

    enum {
        // Global
        kNumPrograms = 1,
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };

    enum {
        kNoiseVolume = 0,
        kNumParams   = 1
    };

    class ProgramParameters {
    public:
        float latency;
        float noiseVolume;
        bool reportLatency;
    };

    class BaseVST2_Program : public ProgramParameters {
        friend class GuiAdvPluginVST2;

    public:
        BaseVST2_Program();
        ~BaseVST2_Program() = default;

    private:
        char name[kVstMaxProgNameLen + 1]{0};
    };

    class GuiAdvPluginVST2 : public BasePluginVST2 {

    public:
        explicit GuiAdvPluginVST2(audioMasterCallback audioMaster);
        ~GuiAdvPluginVST2() override = default;

        void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;
        std::shared_ptr<PluginViewContainers> createView() override;

        void setProgram(VstInt32 program) override;
        void setProgramName(char* name) override;
        void getProgramName(char* name) override;
        bool beginSetProgram() override {
            this->issetprogram = true;
            return false;
        }///< Called before a program is loaded
        bool endSetProgram() override {
            this->issetprogram = false;
            return false;
        }///< Called after a program was loaded
        bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) override;

        void setParameter(VstInt32 index, float value) override;
        float getParameter(VstInt32 index) override;
        void getParameterLabel(VstInt32 index, char* label) override;
        void getParameterDisplay(VstInt32 index, char* text) override;
        void getParameterName(VstInt32 index, char* text) override;

        bool getEffectName(char* name) override;
        bool getVendorString(char* text) override;
        bool getProductString(char* text) override;
        VstPlugCategory getPlugCategory() override {
            return kPlugCategEffect;
        }
        VstInt32 getVendorVersion() override;
        VstInt32 canDo(char* text) override;

        BaseVST2_Program* current() {
            return &(curProgram >= 0 && curProgram < kNumPrograms ? programs[curProgram] : programs[0]);
        }

#ifdef DISPATCHER_DEBUG_TRACE
        VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif// DEBUG

    private:
        BaseVST2_Program programs[kNumPrograms];
        seq_rand rnd;
    };
    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}

