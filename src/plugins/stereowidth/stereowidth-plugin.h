#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


namespace PluginStereoWidth {

    class PluginVST2_StereoWidth;

    enum {
        // Global
        kNumPrograms = 0,// wonder if that works
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };

    enum {
        kStereoWidth = 0,
        kGain        = 1,
        kNumParams
    };

    class ProgramParameters {
    public:
        float width;
        float gain;
    };

    class BaseVST2_ProgramStereoWidth : public ProgramParameters {
        friend class PluginVST2_StereoWidth;

    public:
        BaseVST2_ProgramStereoWidth();
        ~BaseVST2_ProgramStereoWidth() = default;

    private:
        char name[kVstMaxProgNameLen + 1]{ 0 };
    };

    class PluginVST2_StereoWidth : public BasePluginVST2 {

    public:
        explicit PluginVST2_StereoWidth(audioMasterCallback audioMaster);
        ~PluginVST2_StereoWidth() override = default;

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

        BaseVST2_ProgramStereoWidth* current() {
            return &singleProgram;
        }
#ifdef DISPATCHER_DEBUG_TRACE
        VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif// DEBUG

    private:
        BaseVST2_ProgramStereoWidth singleProgram;
        BaseVST2_ProgramStereoWidth paramsState;
        //BaseVST2_Program programs[kNumPrograms];
    };

    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginStereoWidth
