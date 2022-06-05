#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <memory>
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-host-2.4/aeffectx.h>
#include "config.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"

#include "basectrl.h"
#include "platform.h"
#include "../plugin.h"
#include "info-plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"
#include "midi-defs.h"
#include "../synth/IPlugMidi.h"

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginHostInfo::createPlugin(audioMaster);
}
#endif


namespace PluginHostInfo {
    const char* const PLUGIN_EFFECT_NAME = "HostInfo";
    const char* const PLUGIN_UID = "INFO";
    const char* const PLUGIN_PRODUCT_NAME = "HostInfo VST2.4";
    void timeInfoToStrings(VstTimeInfo* timeinfo, std::vector<String>& strings) {
        strings.push_back(StringFormat("samplePos %.4f", timeinfo->samplePos));
        strings.push_back(StringFormat("sampleRate %.3f", timeinfo->sampleRate));
        strings.push_back(StringFormat("nanoSeconds %.2f", timeinfo->nanoSeconds));
        strings.push_back(StringFormat("ppqPos %.5f", timeinfo->ppqPos));
        strings.push_back(StringFormat("tempo %.4f", timeinfo->tempo));
        strings.push_back(StringFormat("barStartPos %.4f", timeinfo->barStartPos));
        strings.push_back(StringFormat("cycleStartPos %.4f", timeinfo->cycleStartPos));
        strings.push_back(StringFormat("cycleEndPos %.4f", timeinfo->cycleEndPos));
        strings.push_back(StringFormat("timeSigNumerator %d", timeinfo->timeSigNumerator));
        strings.push_back(StringFormat("timeSigDenominator %d", timeinfo->timeSigDenominator));
        strings.push_back(StringFormat("smpteOffset %d", timeinfo->smpteOffset));
        strings.push_back(StringFormat("smpteFrameRate %d", timeinfo->smpteFrameRate));
        strings.push_back(StringFormat("samplesToNextClock %d", timeinfo->samplesToNextClock));
        strings.push_back(StringFormat("flags %08X", timeinfo->flags));
    }

    using StdThreadLock = std::lock_guard<std::recursive_mutex>;
    struct PluginVST2_HostInfo_impl_t {

        std::vector<uint8_t> dataPlugin;
        std::vector<uint8_t> dataPreset;
        std::recursive_mutex mutex;
        IMidiQueue midiQueue;
        std::vector<int> heldNotes;
        int32_t invalidNoteMessages = 0;
        std::recursive_mutex& getMutex() {
            return mutex;
        }
        PluginVST2_HostInfo_impl_t() = default;

        void processMidiBlockEnd(int sampleFrames) {
            midiQueue.Flush(sampleFrames);
            if (!midiQueue.Empty()) {
                dbgassert(0);
            }
        }
        void processMidiSamplePos(int sample, int logVerbosity) {
            while (!midiQueue.Empty()) {
                auto message = midiQueue.Peek();
                if (message.mOffset > sample)
                    break;

                auto status   = message.StatusMsg();
                auto ctrl     = message.ControlChangeIdx();
                auto note     = message.NoteNumber();
                auto velocity = pow(message.Velocity() * .0078125, 1.25);

                if (status == IMidiMsg::kNoteOn && velocity == 0)
                    status = IMidiMsg::kNoteOff;

                switch (status) {
                    case IMidiMsg::kNoteOff:
                        if (!removeEntry(heldNotes, note)) {
                            invalidNoteMessages++;
                            log_printf("Sample %03d: Note %s OFF msg, but NOT held\n", message.mOffset, noteName(note));
                        } else {
                            if (logVerbosity > 3)
                                log_printf("Sample %03d: %s OFF\n", message.mOffset, noteName(note));
                        }
                        break;
                    case IMidiMsg::kNoteOn:
                        if (stl_contains(heldNotes, note)) {
                            invalidNoteMessages++;
                            log_printf("Sample %03d: Note %s ON msg, but ALREADY held\n", message.mOffset, noteName(note));
                        } else {
                            if (logVerbosity > 3)
                                log_printf("Sample %03d: %s ON\n", message.mOffset, noteName(note));
                        }
                        heldNotes.push_back(note);
                        break;
                    case IMidiMsg::kPitchWheel: {
                        break;
                    }
                    case IMidiMsg::kControlChange: {
                        switch (ctrl) {
                            case IMidiMsg::kAllNotesOff:
                                //heldNotes.clear();
                                break;
                            default:
                                break;
                        }
                        break;
                    }
                    default:
                        log_printf("Unhandled midi msg %d\n", (int32_t) status);
                        break;
                }
                midiQueue.Remove();
            }
        }
        void ProcessMidiMsg(IMidiMsg& msg) {
            midiQueue.Add(msg);
        }
    };

    PluginVST2_HostInfo::PluginVST2_HostInfo(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs), impl(new PluginVST2_HostInfo_impl_t()) {
        programsAreChunks(true);
        isSynth(true);
    }


    VstIntPtr PluginVST2_HostInfo::dispatcher (VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
    {
        if (opcode != effEditIdle)
            log_printf("dispatch %d %d %012zX %012zX %f\n", opcode, index, static_cast<uint64_t>(value), reinterpret_cast<uint64_t>(ptr), opt);
        return AudioEffectX::dispatcher(opcode, index, value, ptr, opt);
    }

    void PluginVST2_HostInfo::open() {
        BasePluginVST2::open();
        // if (getLogVerbosity() > 0)
        {
            VstIntPtr version = audioMaster (&cEffect, audioMasterVersion, 0, 0, nullptr, 0.0f);
            log_printf("audioMasterVersion %zd\n", version);
            char buf[128]{};
            if (audioMaster (&cEffect, audioMasterGetVendorString, 0, 0, &buf, 0.0f)) {
                buf[127] = 0;
                log_printf("audioMasterGetVendorString %s\n", buf);
            }
            buf[0] = 0;
            if (audioMaster (&cEffect, audioMasterGetProductString, 0, 0, &buf, 0.0f)) {
                buf[127] = 0;
                log_printf("audioMasterGetProductString %s\n", buf);
            }
        }
    }
    void PluginVST2_HostInfo::close() {
        BasePluginVST2::close();
    }


    PluginVST2_HostInfo::~PluginVST2_HostInfo() {
        delete impl;
    }

    void PluginVST2_HostInfo::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_HostInfo::setProgramName(char* name) {
    }

    void PluginVST2_HostInfo::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_HostInfo::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kLogVerbosity:
            case kLogBlocksProcessed:
                vst_strncpy(label, "", PLUGIN_PARAM_STR_MAX_LEN);
                return;
            default:
                vst_strncpy(label, "", PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_HostInfo::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kLogVerbosity: {
                snprintf(text, PLUGIN_PARAM_STR_MAX_LEN, "%d", getLogVerbosity());
                break;
            }
            case kLogBlocksProcessed: {
                snprintf(text, PLUGIN_PARAM_STR_MAX_LEN, "%d", getLogBlocks());
                break;
            }
        }
    }

    void PluginVST2_HostInfo::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kLogVerbosity:
                vst_strncpy(label, "Log Verbosity", PLUGIN_PARAM_STR_MAX_LEN);
                return;
            case kLogBlocksProcessed:
                vst_strncpy(label, "Log Blocks", PLUGIN_PARAM_STR_MAX_LEN);
                return;
        }
    }

    void PluginVST2_HostInfo::setParameter(VstInt32 index, float value) {
        Program* ap = current();
        switch (index) {
            case kLogVerbosity:
                ap->logVerbosity = value;
                break;
            case kLogBlocksProcessed:
                ap->logBlocks = value;
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

    float PluginVST2_HostInfo::getParameter(VstInt32 index) {
        Program* ap = current();
        float value = 0;
        switch (index) {
            case kLogVerbosity:
                value = ap->logVerbosity;
                break;
            case kLogBlocksProcessed:
                value = ap->logBlocks;
                break;
        }
        return value;
    }
    /* Return parameter properties */
    bool PluginVST2_HostInfo::getParameterProperties(VstInt32 index, VstParameterProperties* p) {

        if (!p)
            return false;
        memset(p, 0, sizeof(VstParameterProperties));
        if (index == kLogVerbosity) {
            safe_strcpy(p->label, "Logging Verbosity");
            safe_strcpy(p->shortLabel, "Log Verbosity");
            return true;
        }
        if (index == kLogBlocksProcessed) {
            safe_strcpy(p->label, "Log Blocks");
            safe_strcpy(p->shortLabel, "Log Blocks");
            return true;
        }
        return false;
    }

    bool PluginVST2_HostInfo::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", PLUGIN_PROGRAM_STR_MAX_LEN);
            return true;
        }
        return false;
    }

    bool PluginVST2_HostInfo::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_HostInfo::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_HostInfo::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    VstInt32 PluginVST2_HostInfo::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_HostInfo::processEvents(VstEvents* events) {
        assert(events);
        if (events) {
            StdThreadLock lock(impl->getMutex());
            int32_t len = events->numEvents;
            if (events->numEvents && getLogVerbosity() > 6)
                log_printf("events->numEvents %d\n", events->numEvents);
            for (int i = 0; i < len; i++) {
                auto pEvent = events->events[i];
                if (pEvent->type == VstEventTypes::kVstMidiType) {
                    if (getLogVerbosity() > 6)
                        log_printf("pEvent->type kVstMidiType\n");
                    VstMidiEvent* pME = (VstMidiEvent*) pEvent;
                    IMidiMsg msg(pME->deltaFrames, pME->midiData[0], pME->midiData[1], pME->midiData[2]);
                    impl->ProcessMidiMsg(msg);
                    /*log_printf("event[%d].type %d\n", i, pME->type);
                    log_printf("event[%d].byteSize %d\n", i, pME->byteSize);
                    log_printf("event[%d].deltaFrames %d\n", i, pME->deltaFrames);
                    log_printf("event[%d].flags %d\n", i, pME->flags);
                    log_printf("event[%d].noteLength %d\n", i, pME->noteLength);
                    log_printf("event[%d].noteOffset %d\n", i, pME->noteOffset);
                    log_printf("event[%d].midiData %02X%02X%02X%02X\n", i,
                    (unsigned)pME->midiData[0], (unsigned)pME->midiData[1], (unsigned)pME->midiData[2], (unsigned)pME->midiData[3]);
                    log_printf("event[%d].detune %d\n", i, (unsigned)pME->detune);
                    log_printf("event[%d].noteOffVelocity %d\n", i, (unsigned)pME->noteOffVelocity);
                    log_printf("event[%d].reserved1 %d\n", i, (unsigned)pME->reserved1);
                    log_printf("event[%d].reserved2 %d\n", i, (unsigned)pME->reserved2);*/
                } else {
                    if (getLogVerbosity() > 7)
                        log_printf("pEvent->type %d\n", pEvent->type);
                }
            }
        }
        return 1;
    }
    VstInt32 PluginVST2_HostInfo::canDo(char* text) {
        //if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
        //    return 1;
        if (!strcmp(text, PlugCanDos::canDoReceiveVstMidiEvent))
            return 1;
        if (!strcmp(text, PlugCanDos::canDoReceiveVstTimeInfo))
            return 1;
        if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    ///< Host stores plug-in state. Returns the size in bytes of the chunk (plug-in allocates the data array)
    VstInt32 PluginVST2_HostInfo::getChunk(void** data, bool isPreset) {
        if (getLogVerbosity() > 2)
            log_printf("getChunk isPreset = %d: PTR %08zX\n", isPreset, (uint64_t) (data));
        if (isPreset) {
            impl->dataPreset.resize(1000);
            std::fill(impl->dataPreset.begin(), impl->dataPreset.end(), 0xAA);
            *data = impl->dataPreset.data();
            return impl->dataPreset.size();
        } else {
            impl->dataPlugin.resize(2000);
            std::fill(impl->dataPlugin.begin(), impl->dataPlugin.end(), 0x11);
            *data = impl->dataPlugin.data();
            return impl->dataPlugin.size();
        }
    }

    ///< Host restores plug-in state
    VstInt32 PluginVST2_HostInfo::setChunk(void* data, VstInt32 byteSize, bool isPreset) {
        if (getLogVerbosity() > 2)
            log_printf("setChunk size %d, isPreset = %d: PTR %08zX\n", byteSize, isPreset, reinterpret_cast<uint64_t>(data));
        if (isPreset && byteSize == 1000) {
            impl->dataPreset.resize(byteSize);
            memcpy(impl->dataPreset.data(), data, byteSize);
            return byteSize;
        } else if (!isPreset && byteSize == 2000) {
            impl->dataPreset.resize(byteSize);
            memcpy(impl->dataPreset.data(), data, byteSize);
            return byteSize;
        } else {
            log_printf("mismatch :( \n");
        }

        return 0;
    }

    void PluginVST2_HostInfo::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        //numCalls++;
        if (getLogBlocks() > 1) {
            std::vector<String> strings;
            String str;
            str = StringFormat("Blocksize %d", getBlockSize());
            strings.push_back(str);
            str = StringFormat("Samplerate %.0f", getSampleRate());
            strings.push_back(str);
            int flags = 0;
            for (int i = 8; i < 16; i++) {
                flags |= (1 << i);
            }
            log_printf("Process block %d\n", sampleFrames);
            VstTimeInfo* timeinfo = getTimeInfo(flags);
            if (!timeinfo) {
                log_printf("getTimeInfo() == nullptr\n");
                assert(timeinfo);
                timeInfoToStrings(timeinfo, strings);
            }
            for (String& str2 : strings) {
                log_printf("%s\n", StringAsCStr(str2));
            }
        }

        dbgassert(sampleFrames <= blockSize);
        if (!issetprogram && sampleFrames <= blockSize) {

            for (int s = 0; s < sampleFrames; s++) {
                impl->processMidiSamplePos(s, getLogVerbosity());
            }

            if (this->getAeffect()->numOutputs == 1) {
                memcpy(outputs[0], inputs[0], sizeof(float) * sampleFrames);
            } else if (this->getAeffect()->numOutputs == 2) {
                memcpy(outputs[0], inputs[0], sizeof(float) * sampleFrames);
                memcpy(outputs[1], inputs[1], sizeof(float) * sampleFrames);
            }
            impl->processMidiBlockEnd(sampleFrames);
        }
    }

    PluginVST2_HostInfo_impl_t* getImpl(PluginVST2_HostInfo* plugin) {
        return plugin->impl;
    }

    Program::Program() : ProgramParameters() {
        vst_strncpy(name, "Init", PLUGIN_PROGRAM_STR_MAX_LEN);
    }

}// namespace PluginHostInfo

namespace PluginHostInfo {


    class guicontainer_plugin_HostInfo : public guictr_base {
        PluginVST2_HostInfo* const plugin;
        guiknob_pluginparam knobParam0;

    public:
        explicit guicontainer_plugin_HostInfo(PluginVST2_HostInfo* plugin)
            : guictr_base(),
            plugin(plugin),
            knobParam0(PARAM_OFFSET_EXTERNAL + kLogVerbosity, kLogVerbosity)
        {
            setBackgroundRendered(true);
            padding = 4;
            margin  = 4;
            add(&knobParam0);
        }
        ~guicontainer_plugin_HostInfo() override {
            remove(&knobParam0);
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
                case kLogVerbosity:
                    return &knobParam0;
            }
            return nullptr;
        }

        void onSetParameter(int32_t index, float value) {
#if BUILD_EXTERNAL_PLUGIN
            guiknob_pluginparam* knob = getKnobFromParameter(index);
            if (knob) {
                knob->setValueInit(value);
            }
#endif
        }
        void onGuiOpen() {
#if BUILD_VSTHOST
            knobParam0.setEffectInstance(plugin->getHostSideHandle());
#endif
#if BUILD_EXTERNAL_PLUGIN
            knobParam0.setAudioEffect(plugin);
#endif
        }
        void onGuiClose() {
#if BUILD_VSTHOST
            knobParam0.setEffectInstance(nullptr);
#endif
#if BUILD_EXTERNAL_PLUGIN
            knobParam0.setAudioEffect(nullptr);
#endif
        }

        void render(NVGcontext* vg) override {
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
            PluginVST2_HostInfo_impl_t* curEffectImpl = getImpl(plugin);
            if (!curEffectImpl) {
                dbgassert(0);
                return;
            }

            std::vector<String> strings;
            String str;
            str = StringFormat("Blocksize %d", this->plugin->getBlockSize());
            strings.push_back(str);
            str = StringFormat("Samplerate %.0f", this->plugin->getSampleRate());
            strings.push_back(str);
            AudioEffectX* effx = dynamic_cast<AudioEffectX*>(this->plugin);
            int flags          = 0;
            for (int i = 8; i < 16; i++) {
                flags |= (1 << i);
            }
            VstTimeInfo* timeinfo = effx->getTimeInfo(flags);
            assert(timeinfo);
            PluginHostInfo::timeInfoToStrings(timeinfo, strings);
            setFont(vg, 16, THEMECOL_TEXT, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
            float lineh;
            nvgTextMetrics(vg, NULL, NULL, &lineh);


            int y = INSET_CTR_SPACING;
            int x = this->knobParam0.right() + INSET_CTR_SPACING;
            {
                StdThreadLock lock(curEffectImpl->getMutex());
                std::vector<int> heldNotes = curEffectImpl->heldNotes;//TODO: not threadsafe
                String s                   = "Held notes: ";
                for (int i : heldNotes) {
                    s += String(noteName(i)) + ",";
                    if (s.length() > 32) {
                        nvgText(vg, x, y, StringAsCStr(s), NULL);
                        s = "";
                        y += lineh;
                    }
                }
                if (heldNotes.empty())
                    s += "<empty>";
                if (s.length() > 0) {
                    nvgText(vg, x, y, StringAsCStr(s), NULL);
                    s = "";
                    y += lineh;
                }
            }

            nvgText(vg, x, y, StringAsCStr(StringFormat("Invalid notes: %d", curEffectImpl->invalidNoteMessages)), NULL);
            y += lineh;

            for (String& s : strings) {
                nvgText(vg, x, y, StringAsCStr(s), NULL);
                y += lineh;
            }


            for (guibase* gui : guis) {
                nvgSave(vg);
                gui->render(vg);
                nvgRestore(vg);
            }
        }

        void layout() override {
            const int inset = 4;
            knobParam0.size = ivec2(64, 90);
            knobParam0.pos  = ivec2(inset);
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
    };

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_HostInfo(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_HostInfo::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guicontainer_plugin_HostInfo, PluginVST2_HostInfo>>(this, 280, 360);
        this->views.push_back(view);
        return view;
    }
}// namespace PluginHostInfo
