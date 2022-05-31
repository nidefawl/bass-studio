#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

#define BITCRUSH_BITS_MIN 0
#define BITCRUSH_BITS_MAX 4

class DelayLine;
namespace PluginBitcrush {

    class PluginVST2_Bitcrush;

    enum {
        // Global
        kNumPrograms = 0,// wonder if that works
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };

    enum {
        kSamples = 0,
        kNumParams
    };


    class ProgramParameters {
    public:
        int32_t bitcrush = 0;
    };

    class Program : public ProgramParameters {
        friend class PluginVST2_Bitcrush;

    public:
        Program();
        ~Program() = default;

    private:
        char name[kVstMaxProgNameLen + 1]{ 0 };
    };


    class PluginVST2_Bitcrush : public BasePluginVST2 {

    public:
        explicit PluginVST2_Bitcrush(audioMasterCallback audioMaster);
        ~PluginVST2_Bitcrush() override = default;

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
        void setNewBitcrushLvl(int32_t nbitcrushLvlh);
        Program singleProgram;
        int32_t curBitcrush = 0;
        int32_t newBitcrush = 0;
        std::atomic<bool> bitcrushLvlChanged{ false };
        //BaseVST2_Program programs[kNumPrograms];
    };
    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginBitcrush
