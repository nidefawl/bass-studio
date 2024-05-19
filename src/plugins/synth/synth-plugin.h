#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <mutex>
#include "../plugin-base.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>
#include "synth-types.hpp"
#include "synth-param.hpp"

namespace PluginSynth {
    extern const std::array<const char*, 14> stringsSettings;

    class PluginVST2_Synth;

    struct snapshot_t;
    class SynthImplUnison;

    class PluginVST2_Synth final : public BasePluginVST2 {
    public:
        using ThreadLock = std::lock_guard<std::recursive_mutex>;
        explicit PluginVST2_Synth(audioMasterCallback audioMaster);
        ~PluginVST2_Synth() override;
    
        // internal API
        std::shared_ptr<PluginViewContainer> createViewCtrVst2() override;
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;

        void addPropertiesParameterTooltip(Table::tbl& table, int idx) override;

        void notifyUiChanges();
        void onPresetLoaded();
        void settingChanged(int32_t setting, float value);

        int32_t loadPreset(const String& path);
        SynthImplUnison* getSynth();

#ifdef DISPATCHER_DEBUG_TRACE
        VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif// DEBUG
        std::recursive_mutex& getMutex() {
            return mutex;
        }


        // VST2 API
        void setSampleRate(float sampleRate) override;
        void setBlockSize(VstInt32 blockSize) override;
        VstInt32 processEvents(VstEvents* events) override;///< Called when new MIDI events come in
        void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;
        VstInt32 getChunk (void** data, bool isPreset = false) override;
	    VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset = false) override;

        bool beginSetProgram() override {
            this->issetprogram = true;
            return false;
        }///< Called before a program is loaded

        bool endSetProgram() override {
            this->issetprogram = false;
            return false;
        }///< Called after a program was loaded


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
        void getUiSnapshot(snapshot_t& snapshot);
        void setUiSnapshot(snapshot_t& snapshot);
    private:

        /* TODO: release lastProgramChunks after several seconds */
        std::vector<std::shared_ptr<std::vector<std::byte>>> lastProgramChunks;
        std::recursive_mutex mutex;
        SynthImplUnison* const impl;
        std::vector<SynthParamBase*>& vecParams;
    };
    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginSynth
