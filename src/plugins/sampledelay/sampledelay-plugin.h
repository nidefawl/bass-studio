#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>


namespace PluginSampleDelay {
    static constexpr samplecount_t MIN_DELAY = -16*1024;
    static constexpr samplecount_t MAX_DELAY = 16*1024;
    class PluginVST2_SampleDelay;

    enum {
        // Global
        kNumPrograms = 0,// wonder if that works
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };

    enum {
        kSampleDelay = 0,
        kNumParams
    };

    class ProgramParameters {
    public:
        float delay;
    };

    class BaseVST2_ProgramSampleDelay : public ProgramParameters {
        friend class PluginVST2_SampleDelay;

    public:
        BaseVST2_ProgramSampleDelay();
        ~BaseVST2_ProgramSampleDelay() = default;

    private:
        char name[PLUGIN_PROGRAM_STR_MAX_LEN + 1]{ 0 };
    };

    class PluginVST2_SampleDelay : public BasePluginVST2 {

    public:
        explicit PluginVST2_SampleDelay(audioMasterCallback audioMaster);
        ~PluginVST2_SampleDelay() override = default;

        // internal API
        std::shared_ptr<PluginViewContainers> createView() override;
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;

        // VST2 API
        void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;

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

        BaseVST2_ProgramSampleDelay* current() {
            return &singleProgram;
        }
#ifdef DISPATCHER_DEBUG_TRACE
        VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif// DEBUG

    private:
        void processStereo(float** inputs, AudioBlock& out, VstInt32 sampleFrames, const float filterCoeff, BaseVST2_ProgramSampleDelay& params, const BaseVST2_ProgramSampleDelay nextParams);
        BaseVST2_ProgramSampleDelay singleProgram;
        BaseVST2_ProgramSampleDelay paramsState;
        std::unique_ptr<DelayLine> delayLine = nullptr;
        std::unique_ptr<DelayLine> delayLine2 = nullptr;
    };

    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginSampleDelay
