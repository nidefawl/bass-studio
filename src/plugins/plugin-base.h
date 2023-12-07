#pragma once
#include <memory>
#include <vector>
#include <cmath>
#include "host/plugin/vst/vstplugin.h"
#include "plugin.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>
#include "gui/table/table_fwd.h"

#define PLUGIN_VENDOR_NAME "MichaelH"
#define PLUGIN_PARAM_STR_MAX_LEN 128
#define PLUGIN_PROGRAM_STR_MAX_LEN 128

class PluginViewContainer;
class BasePluginVST2 : public AudioEffectX {
protected:
    vstplugin* hostSidePlugin = nullptr;
    std::vector<std::shared_ptr<PluginViewContainer>> views;
    virtual std::shared_ptr<PluginViewContainer> createViewCtrVst2() = 0;

public:
    BasePluginVST2(audioMasterCallback audioMaster,
                   uint32_t pluginUniqueID,
                   VstInt32 numPrograms,
                   VstInt32 numParams,
                   VstInt32 numInputs,
                   VstInt32 numOutputs);
    BasePluginVST2(audioMasterCallback audioMaster,
                   uint32_t pluginUniqueID);
    ~BasePluginVST2() override = default;

    void createEditorWindow(std::shared_ptr<PluginViewContainer> view);

    void open() override;     ///< Called when plug-in is initialized
    void close() override;    ///< Called when plug-in will be released
    void suspend() override {}///< Called when plug-in is switched to off
    void resume() override {} ///< Called when plug-in is switched to on

    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override = 0;


    bool getInputProperties(VstInt32 index, VstPinProperties* properties) override;
    bool getOutputProperties(VstInt32 index, VstPinProperties* properties) override;
    bool getVendorString(char* text) override;

    // internal API
    virtual std::shared_ptr<PluginViewContainer> getViewCtrVst2(int32_t uiId);
    virtual std::shared_ptr<PluginViewContainer> openViewCtrVst2(int32_t uiId);
    virtual param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue);
    virtual void addPropertiesParameterTooltip(Table::tbl& table, int idx);
    virtual void onWindowResize(ivec2 size);
    void setHostSideHandle(vstplugin* plugin) {
        this->hostSidePlugin = plugin;
    }
    vstplugin* getHostSideHandle() {
        return this->hostSidePlugin;
    }

protected:
    bool issetprogram = false;
};
