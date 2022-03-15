#pragma once
#include <memory>
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


class DelayLine;
namespace PluginLatency {

    class PluginVST2_Latency;

    enum {
        // Global
        kNumPrograms = 0,// wonder if that works
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };

    enum {
        kLatency = 0,
        kNumParams
    };


    class ProgramParameters {
    public:
        int32_t latency = 0;
    };

    class Program : public ProgramParameters {
        friend class PluginVST2_Latency;

    public:
        Program();
        ~Program() = default;

    private:
        char name[kVstMaxProgNameLen + 1]{ 0 };
    };


    class PluginVST2_Latency : public BasePluginVST2 {

    public:
        explicit PluginVST2_Latency(audioMasterCallback audioMaster);
        ~PluginVST2_Latency() override = default;

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

        Program* current() {
            return &singleProgram;
        }

#ifdef DISPATCHER_DEBUG_TRACE
        VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif// DEBUG

    private:
        void setNewLatency(int32_t nSamplesLatency);
        Program singleProgram;
        std::unique_ptr<DelayLine> delayLine = nullptr;
        int32_t curLatency   = 0;
        int32_t newLatency   = 0;
        std::atomic<bool> latencyChanged{ false };
        //BaseVST2_Program programs[kNumPrograms];
    };
    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginLatency
