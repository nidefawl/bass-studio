#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>


namespace PluginHostInfo {
#define MAX_VERBOSITY 16
#define MAX_LOG_BLOCKS 3
    class PluginVST2_HostInfo;

    enum {
        // Global
        kNumPrograms = 0,// wonder if that works
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };

    enum {
        kLogVerbosity       = 0,
        kLogBlocksProcessed = 1,
        kNumParams
    };


    class ProgramParameters {
    public:
        float logVerbosity = 0.0f;
        float logBlocks    = 0.0f;
    };

class Program final : public ProgramParameters {
        friend class PluginVST2_HostInfo;

    public:
        Program();
        ~Program() = default;

    private:
        char name[PLUGIN_PROGRAM_STR_MAX_LEN + 1]{ 0 };
    };

    struct PluginVST2_HostInfo_impl_t;
class PluginVST2_HostInfo final : public BasePluginVST2 {
        friend PluginVST2_HostInfo_impl_t* getImpl(PluginVST2_HostInfo*);

    protected:
        PluginVST2_HostInfo_impl_t* const impl;

    public:
        explicit PluginVST2_HostInfo(audioMasterCallback audioMaster);
        ~PluginVST2_HostInfo() override;

        bool getParameterProperties(VstInt32 index, VstParameterProperties* p) override;
        void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;
        VstInt32 processEvents(VstEvents* events) override;
        std::shared_ptr<PluginViewContainers> createViewCtrVst2() override;
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
        VstInt32 getChunk(void** data, bool isPreset = false) override;
        VstInt32 setChunk(void* data, VstInt32 byteSize, bool isPreset = false) override;


	    VstIntPtr dispatcher (VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) override;
        void open() override;
        void close() override;

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

    private:
        int getLogVerbosity() {
            return math::roundfS32(current()->logVerbosity * MAX_VERBOSITY);
        }
        int getLogBlocks() {
            return math::roundfS32(current()->logBlocks * MAX_LOG_BLOCKS);
        }
        Program singleProgram;
    };
    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginHostInfo
