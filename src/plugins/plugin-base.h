#pragma once
#include <memory>
#include <vector>
#include <cmath>
#include "host/plugin/vst_plugin.h"
#include "plugin.h"
#include <vstsdk-plugin-2.4/audioeffect.h>
#include <vstsdk-plugin-2.4/audioeffectx.h>

#define PLUGIN_VENDOR_NAME "MichaelH"

class PluginViewContainers;
class BasePluginVST2 : public AudioEffectX {
protected:
    vstplugin* hostSidePlugin = nullptr;
    std::vector<std::shared_ptr<PluginViewContainers>> views;

public:
    BasePluginVST2(audioMasterCallback audioMaster,
                   const char* pluginUIDStr,
                   VstInt32 numPrograms,
                   VstInt32 numParams,
                   VstInt32 numInputs,
                   VstInt32 numOutputs);
    ~BasePluginVST2() override = default;

    void createEditorWindow(std::shared_ptr<PluginViewContainers> view);

    void open() override;     ///< Called when plug-in is initialized
    void close() override;    ///< Called when plug-in will be released
    void suspend() override {}///< Called when plug-in is switched to off
    void resume() override {} ///< Called when plug-in is switched to on

    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override = 0;
    virtual std::shared_ptr<PluginViewContainers> createView()                             = 0;


    bool getInputProperties(VstInt32 index, VstPinProperties* properties) override;
    bool getOutputProperties(VstInt32 index, VstPinProperties* properties) override;
    bool getVendorString(char* text) override;

    void setHostSideHandle(vstplugin* plugin) {
        this->hostSidePlugin = plugin;
    }
    vstplugin* getHostSideHandle() {
        return this->hostSidePlugin;
    }

protected:
    bool issetprogram = false;
};
