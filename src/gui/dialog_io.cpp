#include "dialog_io.h"
#include "appsettings.h"
#include "button.h"
#include "dialog.h"
#include "dropdown.h"
#include "gui/guimeter.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "guicontextmenu_base.h"
#include "host/audio_host.h"
#include "host/mainctrl.h"
#include "host/midi_host.h"
#include "host/vst_host.h"
#include "list.h"
#include "math/vec.h"
#include "meter.h"
#include "platform.h"
#include "renderresources.h"
#include "str_util.h"
#include "tempocontrols.h"
#include <array>
#include <utility>
#include <portaudio.h>
#include <portmidi.h>

using DAW::settings;

namespace {
    constexpr int ID_BTN_CLOSE    = 1;
    constexpr int TITLE_FONT_SIZE = 30;
    constexpr int TEXT_FONT_SIZE  = 20;
    constexpr int BTN_FONT_SIZE   = 16;
    constexpr int ROW_FONT_SIZE   = 18;
} // namespace
namespace GuiConstant {
    extern constant_t CONST_SMALL_LABEL_HEIGHT;
}
using meterType          = rmsmeter<16000>;
using guimeterTypeMono   = gui_trackmeter<16000, 1>;
using guimeterTypeStereo = gui_trackmeter<16000, 2>;
using guimeterType4Ch    = gui_trackmeter<16000, 4>;
using guimeterType6Ch    = gui_trackmeter<16000, 6>;

std::shared_ptr<guibase> getMeter(int32_t t, meterType* meter) {
    if (t < 2) return std::make_shared<guimeterTypeMono>(meter);

    if (t < 3) return std::make_shared<guimeterTypeStereo>(meter);

    if (t < 5) return std::make_shared<guimeterType4Ch>(meter);

    return std::make_shared<guimeterType6Ch>(meter);
}

class guidropdown_setting_options_t;
class guidropdown_setting_options_ctxt_t : public guictxtmenu {
    guidropdown_setting_options_t* parent;
    std::vector<String> strings;

public:
    guidropdown_setting_options_ctxt_t(guidropdown_setting_options_t* _parent);
    void clicked(int _id) override;
};
class guidropdown_setting_options_t : public guidropdownbase {
public:
    std::vector<String> options;
    std::function<void(int)> cbOnOptionSelected;
    std::function<String()> fnGetCurrentVal;
    std::function<uint32_t()> fnGetCurrentIdx;

public:
    uint32_t getSelectIndex() override { return fnGetCurrentIdx(); }
    uint32_t getLastIndex()  override { return options.size(); }
    void setSelectedIndex(uint32_t idx)  override { clicked(idx); }
    String getString() override { return fnGetCurrentVal ? fnGetCurrentVal() : "<null>"; }
    void handleDraggedRelease(MouseEvent& evt) override {
        if (options.empty()) return;
        guictxtmenu_base* popup = new guidropdown_setting_options_ctxt_t(this);
        popup->size             = size;
        popup->setFontSize(size.y);
        this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
    std::vector<String>& getOptions() { return options; }
    void clicked(uint32_t idx) {
        if (cbOnOptionSelected) {
            if (idx < options.size()) {
                cbOnOptionSelected((int32_t)idx);
            }
        }
    }
};


guidropdown_setting_options_ctxt_t::guidropdown_setting_options_ctxt_t(guidropdown_setting_options_t* _parent)
    : parent(_parent) {
    this->size.x                 = 120;
    this->fontSize               = FONT_SIZE_CTXT_SMALL;
    this->paddingV               = 0;
    std::vector<String>& options = parent->getOptions();

    int32_t idx = 0;
    for (const String& option : options) {
        addEntry(new ctxtmenu_entry(option, idx++));
    }
}
void guidropdown_setting_options_ctxt_t::clicked(int _id) {
    closeContextMenu();
    parent->clicked(_id);
}

class gui_listentry_audiodevice : public gui_list_entry {
    String deviceAPI;
    String deviceName;
    const int32_t nChannels;
    const bool isInput;

public:
    gui_listentry_audiodevice(String _deviceAPI, String _deviceName, int32_t _nChannels, bool _isInput)
        : gui_list_entry(), deviceAPI(_deviceAPI), deviceName(_deviceName), nChannels(_nChannels), isInput(_isInput) {
        icon = -1;
    }
    String getText() override { return deviceName; }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {}
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
    void handleDraggedBegin(MouseEvent& evt) override { toggle(); }
    app_ioaudioconfig& getCnf() { return settings.iosettings.getConfig(deviceAPI); }
    bool enabled() {
        const auto& c = getCnf();
        return isInput ? c.deviceNameInput == deviceName : c.deviceNameOutput == deviceName;
    }
    bool toggle() {
        auto& c     = getCnf();
        String& cnf = isInput ? c.deviceNameInput : c.deviceNameOutput;
        cnf         = enabled() ? "" : deviceName;
        if (parent && parent->parent) {
            parent->parent->buttonClicked(this);
        }
        return false;
    }
    void render(NVGcontext* vg) override {
        BaseCtrl* ctrl  = parentCtrl;
        float spacing   = INSET_TITLE;
        float x         = spacing;
        float rowHeight = size.y;
        if (icon > -1) {
            x += rowHeight + spacing;
        }
        ivec2 inner = size;
        if (ctrl->isCtrOrChildFocused(this)) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, inner.x, inner.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
            nvgFill(vg);
        }
        nvgTranslate(vg, pos.x, pos.y);
        if (icon > -1) {
            RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
            drawIcon(vg, inner, &image);
        }
        setFont(vg, (int)(ROW_FONT_SIZE), G_WHITE, G_TITLE_ALIGN);
        nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), nullptr);
        ivec2 sizeIcon = ivec2(inner.y - 4);
        ivec2 posIcon  = {inner.x - (int)spacing - sizeIcon.y, (inner.y - sizeIcon.y) / 2};

        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, posIcon.x - 4, rowHeight / 2, StringAsCStr(StringFormat("%d CH", nChannels)), nullptr);
        //auto* _entry = safeRefGet(ref);
        //if (_entry) {
        bool enbl = enabled();
        setFont(vg, (int)(ROW_FONT_SIZE), theme->getColor(enbl ? GuiColor::COL_ON : GuiColor::COL_OFF), G_TITLE_ALIGN);
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        RenderResources::NvgImageTexture& image = RenderResources::imgIcons[ICON_SPEAKER];
        nvgTranslate(vg, posIcon.x, posIcon.y);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, sizeIcon.x, sizeIcon.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
        nvgFill(vg);
        //nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GUI_STROKE));
        //nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
        nvgStrokeColor(vg, theme->getBgStrokeColor(parent->getFlags()));
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
        nvgStroke(vg);
        if (enbl) {
            drawIcon(vg, sizeIcon, &image, 2);
        }
        nvgTranslate(vg, -posIcon.x, -posIcon.y);
        //String str = enbl?"On":"Off";bypass
        //nvgText(vg, size.x-spacing, rowHeight / 2, StringAsCStr(str), NULL);
        //}
        //nvgBeginPath(vg);
        //int i2 = 4;
        //nvgRect(vg, i2, i2, size.x-i2*2, size.y-i2*2);
        //nvgFillColor(vg, rgbToNvg(0xFF11ff11));
        //nvgFill(vg);

        nvgTranslate(vg, -pos.x, -pos.y);
    }
};

// TODO: this should be member of DawInstance
void updateSrBs() {
    DawInstance* daw = DawInstance::get();
    bool b     = daw->isPlaying();
    if (b) {

        daw->stopPlaying();
    }
    daw->setAudioThreadState(playback_state::status_stop);
    daw->setAudioThreadState(playback_state::status_no_process);
    {

        ThreadLock lock  = MainCtrl::getPlayThread()->lockThread();
        vsthost* host    = daw->getHost();
        audiohost* ahost = audiohost::getInstance();
        ahost->stopAudio();
        host->setOutput(nullptr);
        if (settings.startEngine) {
            host->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(settings.iosettings.internalSamplerate),
                                                 settings.iosettings.internalBlocksize, sampleformat_bits_t::FLOAT_32});
            if (ahost->startAudio(settings.iosettings)) {
                host->setOutput(ahost);
            } else {
                //settings.startEngine = false;
            }
        }
    }
    if (settings.startEngine) {
        if (b) {
            daw->startPlaying();
        } else {
            daw->setAudioThreadState(playback_state::status_stop);
        }
    }
}

namespace AudioIO {
    tracktype getNextTrackType(tracktype type) {
        switch (type) {
            default:
            case MONO:
                return STEREO;
            case STEREO:
                return MONO;
//            case MULTI_CHANNEL_4:
//                return MULTI_CHANNEL_6;
//            case MULTI_CHANNEL_6:
//                return MONO;
        }
    }
    String getTrackTypeStr(tracktype type) {
        switch (type) {
            default:
            case MONO:
                return "MONO";
            case STEREO:
                return "STEREO";
            case MULTI_CHANNEL_4:
                return "4CH";
            case MULTI_CHANNEL_6:
                return "6CH";
        }
    }
} // namespace AudioIO
class guictr_input_channel : public guictr_base {
    std::shared_ptr<audiohost::audiostream::audiotrack> track;
    const bool isInput;
    std::shared_ptr<guibase> guimeter;
    guibutton btnTrackType;

public:
    guictr_input_channel(std::shared_ptr<audiohost::audiostream::audiotrack>& _track, bool _isInput)
        : track(_track), isInput(_isInput) {
        add(&btnTrackType);
        btnTrackType.setFontScale(0.3f);
        btnTrackType.setText(AudioIO::getTrackTypeStr(_track->type));
        int32_t nChannels = getNumChannelsFromTrackType(track->type);
        guimeter          = getMeter(nChannels, &track->meter);
        add(guimeter.get());
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setFlag(FLG_RENDER_LABEL, true);
        setFlagInternal(FLG_HAS_COLOR_BG);
        setCanMouseHit(true);
        setFlag(FLG_VERTICAL_LABEL, true);
        padding = 3;
    }

    ~guictr_input_channel() override {
        remove(guimeter.get());
        remove(&btnTrackType);
    }

    audiohost::audiostream::audiotrack* getTrack() { return track.get(); }

    void render(NVGcontext* vg) override {
        //if (isBackgroundRendered()) {
        //renderBackground(vg);
        //}
        if (!setScissorTransformContainer(vg)) {
            return;
        }
        renderFrameBase(vg);
        int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
        renderTitleBar(vg, getSizeContent(), this->label, GuiConstant::CONST_SMALL_LABEL_HEIGHT, getSizeContent().y, flags, false);
        renderFrameOutline(vg);
        for (auto c : guis) {
            if (c->isVisible()) {
                nvgSave(vg);
                c->render(vg);
                nvgRestore(vg);
            }
        }
    }

    void layout() override {
        const int32_t htt = theme->get(GuiConstant::CONST_SMALL_LABEL_HEIGHT);
        int32_t w         = getSizeContent().x - htt;
        int topH          = math::min(getSizeContent().y / 6, w / 3);
        guimeter->pos     = {htt, topH};
        guimeter->size    = math::maxvec2(ivec2(0), getSizeContent() - ivec2{htt, topH});
        btnTrackType.pos  = {htt, 0};
        btnTrackType.size = {guimeter->size.x, topH};
        for (guibase* gui : guis) {
            gui->setVisible(gui->size.x>5 && gui->size.y>5);
            gui->layout();
        }
    }

    void buttonClicked(guibase* gui) override {
        if (gui == &btnTrackType) {
            log_printf("Switch track type\n", 0);
            auto& cnf = settings.iosettings.getChannelConfig(settings.iosettings.device_api);
            AudioIO::io_cfg_tracks newConfig = cnf;
            auto& list    = isInput ? cnf.input : cnf.output;
            auto& newList = isInput ? newConfig.input : newConfig.output;
            newList.clear();

            const AudioIO::tracktype type = AudioIO::getNextTrackType(track->type);
            const int32_t nChannelsPrev   = AudioIO::getNumChannelsFromTrackType(track->type);
            const int32_t nChannels       = AudioIO::getNumChannelsFromTrackType(type);
            const int32_t base            = (track->channelOffset / nChannels);
            const int32_t begin           = base * nChannels;
            const int32_t end             = begin + nChannels;

            int32_t ratio = math::max(1, nChannelsPrev / nChannels);
            for (int32_t c = 0; c < ratio; c++) {
                AudioIO::io_cfg_channel channels;
                channels.idx           = 0;
                channels.type          = type;
                channels.channelOffset = (base + c) * nChannels;
                newList.push_back(channels);
            }
            for (auto& existChannelCnf : list) {
                int32_t existChCnfChannels = AudioIO::getNumChannelsFromTrackType(existChannelCnf.type);
                if (existChannelCnf.channelOffset >= end || existChannelCnf.channelOffset + existChCnfChannels <= begin) {
                    newList.push_back(existChannelCnf);
                }
            }
            std::sort(newList.begin(), newList.end(),
                      [](const AudioIO::io_cfg_channel& entryA, const AudioIO::io_cfg_channel& entryB) {
                          return entryA.channelOffset < entryB.channelOffset;
                      });
            while (true) {
                // Find unassigned channels
                int32_t endPrevChannel = 0;
                auto it = std::find_if(newList.begin(), newList.end(),
                              [&endPrevChannel](const AudioIO::io_cfg_channel& entryA) {
                                if (entryA.channelOffset > endPrevChannel)
                                    return true;
                                endPrevChannel = entryA.channelOffset + AudioIO::getNumChannelsFromTrackType(entryA.type);
                                return false;
                              });
                if (it == newList.end()) {
                    break;
                }

                int32_t endChannel = it->channelOffset;
                int32_t free       = endChannel - endPrevChannel;
                log_printf("found %d unassigned channels %d to %d\n", free, endPrevChannel, endChannel);

                // create tracks for unassigned channels
                while (free > 0) {
                    const AudioIO::tracktype type2 = AudioIO::getTrackTypeFromNumChannels(free);
                    AudioIO::io_cfg_channel channel2;
                    channel2.idx           = -1;
                    channel2.type          = type2;
                    channel2.channelOffset = endPrevChannel;
                    int32_t nChannels2     = AudioIO::getNumChannelsFromTrackType(channel2.type);
                    endPrevChannel += nChannels2;
                    free -= nChannels2;
                    newList.push_back(channel2);
                    log_printf("add track %s channels %d to %d\n", StringAsCStr(channel2.name), channel2.channelOffset,
                               channel2.channelOffset + nChannels2);
                }

                // make sure we did not assign too many channels
                dbgassert(free >= 0);
                std::sort(newList.begin(), newList.end(),
                          [](const AudioIO::io_cfg_channel& entryA, const AudioIO::io_cfg_channel& entryB) {
                              return entryA.channelOffset < entryB.channelOffset;
                          });
            }

            int32_t idx = 0;
            for (AudioIO::io_cfg_channel& entry : newList) {
                entry.idx  = idx++;
                entry.name = AudioIO::getTrackName(entry.type, entry.idx, isInput);
            }


            // find maximum channel idx used
            int32_t maxChannel = -1;
            for (auto& ch : newList) {
                auto chCount  = AudioIO::getNumChannelsFromTrackType(ch.type);
                auto chEndIdx = ch.channelOffset + chCount;
                maxChannel = math::max(maxChannel, chEndIdx);
            }

            std::vector<int32_t> vChannelIdc(maxChannel);
            std::fill(vChannelIdc.begin(), vChannelIdc.end(), -1);

            // make sure we have no channel double assignment
            bool foundDblAssignment = false;
            for (auto& ch : newList) {
                auto chCount = AudioIO::getNumChannelsFromTrackType(ch.type);
                for (int j = 0; j < chCount; j++) {
                    foundDblAssignment |= vChannelIdc[j + ch.channelOffset] != -1;
                    dbgassert (!foundDblAssignment);
                    vChannelIdc[j + ch.channelOffset] = ch.idx;
                }
            }

            // make sure we have no gaps
            int32_t numUnused = 0;
            for (auto& chIdx : vChannelIdc) {
                if (chIdx > -1) {
                    dbgassert(numUnused == 0);
                } else {
                    numUnused++;
                }
            }
            bool validConfig = !foundDblAssignment && numUnused == 0;
            if (!validConfig) {
                log_printf("Invalid channel configuration\n", 0);
            } else {
                cnf = newConfig;
                { updateSrBs(); }
            }
        }
    }

    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            gui->onTick(ctrl);
        }
    }
};
class guictr_input_meters : public guictr_base {
    std::vector<std::shared_ptr<guictr_input_channel>> guiMeters;
    const bool isInput;
    int32_t prevStream = 0;

    //
    //void effectbase::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    //meter.update(out, 1.0f);
    //}
public:
    explicit guictr_input_meters(const bool _isInput) : guictr_base(), isInput(_isInput) {}
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        auto* stream = audiohost::getInstance()->getStream(0);
        if (stream && prevStream && prevStream == stream->streamId) {
            for (auto c : guis) {
                if (c->isVisible()) {
                    nvgSave(vg);
                    c->render(vg);
                    nvgRestore(vg);
                }
            }
        }
    }
    void updateChannels() {
        auto* stream = audiohost::getInstance()->getStream(0);
        for (auto& gui : guiMeters) {
            remove(gui.get());
        }
        guiMeters.clear();
        if (stream) {

            auto& tracks   = isInput ? stream->tracksInput : stream->tracksOutput;
            String devName = isInput ? stream->inputName : stream->outputName;

            while (guiMeters.size() < tracks.size()) {
                auto idx         = guiMeters.size();
                String trackName = audiohost::audiostream::getTrackName(tracks[idx].get(), isInput);
                auto p           = std::make_shared<guictr_input_channel>(tracks[idx], isInput);
                p->setLabel(trackName);
                guiMeters.push_back(p);
                add(p.get());
            }
            String s = isInput ? "Input" : "Output";
            s += " Channels ";
            s += devName;
            setLabel(s);
        } else {
        }
        prevStream = stream ? stream->streamId : 0;
        layout();
    }
    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            gui->onTick(ctrl);
        }
        auto* stream = audiohost::getInstance()->getStream(0);
        if ((prevStream && !stream) || (stream && prevStream != stream->streamId)) {
            log_printf("on stream change %X -> %X\n", (int64_t)prevStream, (int64_t)stream);
            updateChannels();
        }
    }
    ~guictr_input_meters() override {
        for (auto& g : guiMeters) {
            remove(g.get());
        }
    }
    void layout() override {
        ivec2 cs            = getSizeContent();
        int32_t maxChannels = math::max<int32_t>(guiMeters.size(), 6);
        int32_t nMeters     = math::max<int32_t>(1, maxChannels);
        ivec2 meterSize     = {math::min(128, math::max(8, (cs.x) / nMeters)), cs.y - INSET_CTR_SPACING * 2};
        ivec2 meterPos      = {INSET_CTR_SPACING, INSET_CTR_SPACING};

        for (auto& meter : guiMeters) {
            meter->pos  = meterPos;
            meter->size = meterSize;
            meterPos.x += meterSize.x;
        }
        for (auto& gui : guis) {
            gui->layout();
        }
    }
};
class guidialog_audio_io : public setting_dialog {
    guidropdownbase* selectAPI;
    guidropdownbase* asioDevice;
    gui_list* deviceListInput;
    gui_list* deviceListOutput;
    guibutton_audioengine* audioEngineOn;
    guidropdownbase* audioBlockSize;
    guidropdownbase* audioSampleRate;
    guidropdownbase* audioInternalBlockSize;
    guidropdownbase* audioInternalSampleRate;
    guictr_input_meters metersInput;
    guictr_input_meters metersOutput;

public:
    void onDialogShow() override { updateOptions(); }
    void updateOptions() {

        if (audiohost::getInstance()->initPa()) {
            String deviceAPIName     = settings.iosettings.device_api;
            int apiCount             = Pa_GetHostApiCount();
            int deviceApiIdxSelected = -1;
            for (int i = 0; i < apiCount; i++) {
                const PaHostApiInfo* info = Pa_GetHostApiInfo(i);
                if (info) {
                    if (deviceApiIdxSelected < 0 || !strcmp(StringAsCStr(settings.iosettings.device_api), info->name)) {
                        deviceApiIdxSelected = i;
                        deviceAPIName        = info->name;
                    }
                }
            }
            std::vector<gui_list_entry*> _newListIn;
            std::vector<gui_list_entry*> _newListOut;
            int deviceCount = Pa_GetDeviceCount();
            for (int i = 0; i < deviceCount; i++) {
                const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
                if (info && info->hostApi == deviceApiIdxSelected && info->maxOutputChannels > 0) {
                    _newListOut.push_back(
                            new gui_listentry_audiodevice{deviceAPIName, info->name, info->maxOutputChannels, false});
                }
                if (info && info->hostApi == deviceApiIdxSelected && info->maxInputChannels > 0) {
                    _newListIn.push_back(
                            new gui_listentry_audiodevice{deviceAPIName, info->name, info->maxInputChannels, true});
                }
            }
            int idx = 0;
            for (auto* p : _newListIn) {
                p->id = 0x1f | (idx++ << 8);
            }
            idx = 0;
            for (auto* p : _newListOut) {
                p->id = 0x0f | (idx++ << 8);
            }
            deviceListInput->setList(_newListIn);
            deviceListOutput->setList(_newListOut);
            deviceListInput->layout();
            deviceListOutput->layout();
            this->asioDevice->setVisible(settings.iosettings.device_api == "ASIO");
            this->deviceListInput->setVisible(!this->asioDevice->isVisible());
            this->deviceListOutput->setVisible(!this->asioDevice->isVisible());
            if (this->parent && this->parentCtrl) {
                this->layout();
            }
        }
    }
    ~guidialog_audio_io() override {
        removeGuis();
        delete deviceListInput;
        delete deviceListOutput;
        delete selectAPI;
        delete asioDevice;
        delete audioBlockSize;
        delete audioEngineOn;
        delete audioSampleRate;
        delete audioInternalSampleRate;
        delete audioInternalBlockSize;
    }
    guidialog_audio_io() : setting_dialog(), metersInput(true), metersOutput(false) {
        this->deviceListInput  = new gui_list();
        this->deviceListOutput = new gui_list();
        this->audioEngineOn    = new guibutton_audioengine{};

        auto api  = new guidropdown_setting_options_t();
        auto asio = new guidropdown_setting_options_t();
        auto extBlockSize       = new guidropdown_setting_options_t();
        auto extSampleRate      = new guidropdown_setting_options_t();
        auto intBlockSize       = new guidropdown_setting_options_t();
        auto intSampleRate      = new guidropdown_setting_options_t();

        this->selectAPI               = api;
        this->asioDevice              = asio;
        this->audioBlockSize          = extBlockSize;
        this->audioSampleRate         = extSampleRate;
        this->audioInternalBlockSize  = intBlockSize;
        this->audioInternalSampleRate = intSampleRate;

        for (int i = 0; i < 4; i++) {
            extSampleRate->options.push_back(StringFormat("%d", AudioIO::ExtSamplerates[i]));
        }
        for (int i = 0; i < 4; i++) {
            intSampleRate->options.push_back(StringFormat("%d", AudioIO::IntSamplerates[i]));
        }
        extSampleRate->cbOnOptionSelected = [](int option) {
            if (option >= 0 && option < 4) {
                settings.iosettings.samplerate = AudioIO::ExtSamplerates[option];
                updateSrBs();
            }
        };
        extSampleRate->fnGetCurrentVal = []() -> String {
            return StringFormat("%d", settings.iosettings.samplerate);
        };
        extSampleRate->fnGetCurrentIdx = []() -> uint32_t {
            return indexOfCtr(AudioIO::ExtSamplerates, settings.iosettings.samplerate);
        };
        intSampleRate->cbOnOptionSelected = [](int option) {
            if (option >= 0 && option < 4) {
                settings.iosettings.internalSamplerate = AudioIO::IntSamplerates[option];
                updateSrBs();
            }
        };
        intSampleRate->fnGetCurrentVal = []() -> String {
            return StringFormat("%d", settings.iosettings.internalSamplerate);
        };
        intSampleRate->fnGetCurrentIdx = []() -> uint32_t {
            return indexOfCtr(AudioIO::IntSamplerates, settings.iosettings.internalSamplerate);
        };
        for (int i = 0; i < 10; i++) {
            int blockSize = 1 << (4 + i);
            extBlockSize->options.push_back(StringFormat("%d", blockSize));
            intBlockSize->options.push_back(StringFormat("%d", blockSize));
        }
        extBlockSize->cbOnOptionSelected = [](int option) {
            if (option >= 0 && option < 10) {
                int blockSize                 = 1 << (4 + option);
                settings.iosettings.blocksize = blockSize;
                updateSrBs();
            }
        };
        extBlockSize->fnGetCurrentVal = []() -> String { return StringFormat("%d", settings.iosettings.blocksize); };
        intBlockSize->cbOnOptionSelected = [](int option) {
            if (option >= 0 && option < 10) {
                int blockSize                         = 1 << (4 + option);
                settings.iosettings.internalBlocksize = blockSize;
                updateSrBs();
            }
        };
        intBlockSize->fnGetCurrentVal = []() -> String {
            return StringFormat("%d", settings.iosettings.internalBlocksize);
        };

        //

        dbgassert(audiohost::getInstance()->initPa());
        {
            int apiCnt     = Pa_GetHostApiCount();
            int apiIdxASIO = -1;
            for (int i = 0; i < apiCnt; i++) {
                auto info = Pa_GetHostApiInfo(i);
                if (info) {
                    if (info->type == PaHostApiTypeId::paASIO) {
                        apiIdxASIO = i;
                    } else {
                        if (settings.iosettings.device_api.empty()) {
                            settings.iosettings.device_api = info->name;
                        }
                    }
                    api->options.emplace_back(info->name);
                }
            }
            int devCount = Pa_GetDeviceCount();
            for (int i = 0; i < devCount; i++) {
                auto info = Pa_GetDeviceInfo(i);
                if (info && info->hostApi == apiIdxASIO) {
                    asio->options.emplace_back(info->name);
                }
            }
        }
        updateOptions();
        api->cbOnOptionSelected = [this, api](int option) {
            if (option >= 0 && option < api->options.size()) {
                settings.iosettings.device_api = api->options[option];
                updateOptions();
                updateSrBs();
            }
        };
        api->fnGetCurrentVal     = []() -> String { return settings.iosettings.device_api; };
        asio->cbOnOptionSelected = [asio](int option) {
            if (option >= 0 && option < asio->options.size()) {
                auto devOption               = asio->options[option];
                app_ioasioconfig& asioconfig = settings.iosettings.asioConfig;
                asioconfig.device_api        = "ASIO";
                asioconfig.deviceName        = devOption;
                if (asioconfig.outputs.empty() && asioconfig.inputs.empty()) {
                    io_channel channel;
                    channel.idx = 0;
                    channel.channels.push_back(0);
                    channel.channels.push_back(1);
                    asioconfig.outputs.push_back(channel);
                }
                updateSrBs();
            }
        };
        asio->fnGetCurrentVal = []() -> String {
            if (settings.iosettings.asioConfig.deviceName.length()) {
                return settings.iosettings.asioConfig.deviceName;
            }
            return "None";
        };

        selectAPI->setFontScale(0.77f);
        asioDevice->setFontScale(0.77f);
        //selectDevice->setFontScale(0.77f);
        deviceListInput->setRenderHR(true);
        deviceListInput->setRowMargin(ivec4(0, 1, 0, 1));
        deviceListOutput->setRenderHR(true);
        deviceListOutput->setRowMargin(ivec4(0, 1, 0, 1));
        deviceListInput->setBackgroundRendered(true);
        deviceListOutput->setBackgroundRendered(true);
        metersInput.setBackgroundRendered(true);
        metersOutput.setBackgroundRendered(true);
        metersInput.setBackgroundRenderedInset(false);
        metersOutput.setBackgroundRenderedInset(false);
        metersInput.setLabel("Input Channels");
        metersOutput.setLabel("Output Channels");
        selectAPI->setLabel("Audio API");
        asioDevice->setLabel("ASIO Device");
        audioEngineOn->setLabel("Audio Engine");
        extBlockSize->setLabel("Ext. Blocksize");
        extSampleRate->setLabel("Ext. Samplerate");
        intBlockSize->setLabel("Int. Blocksize");
        intSampleRate->setLabel("Int. Samplerate");
        deviceListInput->setLabel("Audio input device");
        deviceListOutput->setLabel("Audio output device");
        metersInput.setFlag(FLG_RENDER_LABEL, true);
        metersOutput.setFlag(FLG_RENDER_LABEL, true);
        selectAPI->setFlag(FLG_RENDER_LABEL, true);
        asioDevice->setFlag(FLG_RENDER_LABEL, true);
        extBlockSize->setFlag(FLG_RENDER_LABEL, true);
        extSampleRate->setFlag(FLG_RENDER_LABEL, true);
        intBlockSize->setFlag(FLG_RENDER_LABEL, true);
        intSampleRate->setFlag(FLG_RENDER_LABEL, true);
        deviceListInput->setFlag(FLG_RENDER_LABEL, true);
        deviceListOutput->setFlag(FLG_RENDER_LABEL, true);
        setLabel("Audio I/O");
        setBackgroundRendered(true);
        add(selectAPI);
        add(asioDevice);
        add(deviceListInput);
        add(deviceListOutput);
        add(audioEngineOn);
        add(extBlockSize);
        add(extSampleRate);
        add(intBlockSize);
        add(intSampleRate);
        add(&metersInput);
        add(&metersOutput);
    }
    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            gui->onTick(ctrl);
        }
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        float lineh;
        setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
        nvgTextMetrics(vg, nullptr, nullptr, &lineh);
        nvgText(vg, 5, this->audioEngineOn->bottom(), StringAsCStr(this->audioEngineOn->label), nullptr);
        nvgText(vg, 5, this->audioBlockSize->bottom(), StringAsCStr(this->audioBlockSize->label), nullptr);
        nvgText(vg, 5, this->audioSampleRate->bottom(), StringAsCStr(this->audioSampleRate->label), nullptr);
        nvgText(vg, 5, this->audioInternalBlockSize->bottom(), StringAsCStr(this->audioInternalBlockSize->label), nullptr);
        nvgText(vg, 5, this->audioInternalSampleRate->bottom(), StringAsCStr(this->audioInternalSampleRate->label), nullptr);
        nvgText(vg, 5, this->selectAPI->bottom(), StringAsCStr(this->selectAPI->label), nullptr);
        if (this->asioDevice->isVisible()) {
            nvgText(vg, 5, this->asioDevice->bottom(), StringAsCStr(this->asioDevice->label), nullptr);
        }
        //if (this->deviceListInput->isVisible()) {
        //nvgText(vg, 5, this->deviceListInput->top()-2, StringAsCStr(this->deviceListInput->label), nullptr);
        //}
        //if (this->deviceListOutput->isVisible()) {
        //nvgText(vg, 5, this->deviceListOutput->top()-2, StringAsCStr(this->deviceListOutput->label), nullptr);
        //}


        for (auto c : guis) {
            nvgSave(vg);
            //if (c == this->selectAPI) {
            //nvgIntersectScissor(vg, c->pos.x, c->pos.y, c->size.x, c->size.y);
            //}
            if (c->isVisible()) {
                c->render(vg);
            }
            nvgRestore(vg);
        }
        //if (!this->asioDevice->isVisible()) {
        //
        //auto stream = audiohost::getInstance()->getStream(0);
        //if (stream) {
        //int nChannels = stream->nInputChannels;
        //nvgText(vg, 5, this->deviceListInput->bottom()+TEXT_FONT_SIZE+2, StringAsCStr(StringFormat("%d
        //channels", nChannels)), nullptr);
        //
        //nChannels = stream->nOutputChannels;
        //nvgText(vg, 5, this->deviceListOutput->bottom()+TEXT_FONT_SIZE+2, StringAsCStr(StringFormat("%d
        //channels", nChannels)), nullptr);
        //}
        //}
    }
    void layout() override {
        ivec2 cs = getSizeContent();

        int32_t inset                 = 5;
        int32_t buttonW               = math::max(120, cs.x * 2 / 3);
        int32_t height                = 20;
        audioEngineOn->size           = ivec2(buttonW, height);
        audioEngineOn->pos            = ivec2(cs.x - inset * 2 - buttonW, inset);
        audioInternalBlockSize->size  = ivec2(buttonW, height);
        audioInternalBlockSize->pos   = ivec2(cs.x - inset * 2 - buttonW, audioEngineOn->bottom() + inset);
        audioInternalSampleRate->size = ivec2(buttonW, height);
        audioInternalSampleRate->pos  = ivec2(cs.x - inset * 2 - buttonW, audioInternalBlockSize->bottom() + inset);
        audioBlockSize->size          = ivec2(buttonW, height);
        audioBlockSize->pos           = ivec2(cs.x - inset * 2 - buttonW, inset + audioInternalSampleRate->bottom());
        audioSampleRate->size         = ivec2(buttonW, height);
        audioSampleRate->pos          = ivec2(cs.x - inset * 2 - buttonW, audioBlockSize->bottom() + inset);
        selectAPI->size               = ivec2(buttonW, height);
        selectAPI->pos                = ivec2(cs.x - inset * 2 - buttonW, audioSampleRate->bottom() + inset);
        int32_t h                     = (cs.y - inset) - (selectAPI->bottom() + inset);
        int32_t h1                    = math::max((int)(h * 0.2), 120);

        guibase* pNextGui = selectAPI;
        asioDevice->size  = ivec2(buttonW, height);
        asioDevice->pos   = ivec2(cs.x - inset * 2 - buttonW, selectAPI->bottom() + inset);
        if (asioDevice->isVisible()) {
            pNextGui = asioDevice;
        }
        deviceListInput->pos  = ivec2(inset, pNextGui->bottom() + inset);
        deviceListInput->size = ivec2((cs.x) - inset * 2, h1);
        if (deviceListInput->isVisible()) {
            pNextGui = deviceListInput;
        }
        deviceListOutput->pos  = ivec2(inset, pNextGui->bottom() + inset);
        deviceListOutput->size = ivec2((cs.x) - inset * 2, math::min(cs.y - deviceListOutput->pos.y, h1));
        if (deviceListOutput->isVisible()) {
            pNextGui = deviceListOutput;
        }
        h                = (cs.y - inset) - (pNextGui->bottom() + inset);
        int32_t h2       = math::max((int)(h * 0.5), 150);
        metersInput.pos  = ivec2(inset, pNextGui->bottom() + inset);
        metersInput.size = ivec2((cs.x) - inset * 2, h2);
        if (metersInput.isVisible()) {
            pNextGui = &metersInput;
        }
        metersOutput.pos  = ivec2(inset, pNextGui->bottom() + inset);
        metersOutput.size = ivec2((cs.x) - inset * 2, h2);
        if (metersOutput.isVisible()) {
            pNextGui = &metersOutput;
        }
        for (auto gui : guis) {
            gui->layout();
        }
        int rowHeight = 30;
        while (h1 < rowHeight * 6 && rowHeight > 8) {
            rowHeight -= 4;
        }
        deviceListInput->setRowHeight(rowHeight);
        deviceListOutput->setRowHeight(rowHeight);
    }

    void buttonClicked(guibase* button) override {
        if (button == this->audioEngineOn) {
            settings.startEngine = !settings.startEngine;
            updateSrBs();
            return;
        }
        if ((button->id & 0x0F) == 0xF) {
            updateSrBs();
            return;
        }
        if (this->parent) {
            this->parent->buttonClicked(button);
        }
    }
};
class gui_listentry_mididevice : public gui_list_entry {
    String deviceAPI;
    String deviceName;
    const bool isInput;

public:
    gui_listentry_mididevice(String _deviceAPI, String _deviceName, bool _isInput)
        : gui_list_entry(), deviceAPI(_deviceAPI), deviceName(_deviceName), isInput(_isInput) {
        icon = -1;
    }
    String getText() override { return deviceName; }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {}
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
    void handleDraggedBegin(MouseEvent& evt) override { toggle(); }
    std::vector<midi_channel>& getCnf() {
        return isInput ? settings.iosettings.getIOConfigMidi(deviceAPI).inputs
                       : settings.iosettings.getIOConfigMidi(deviceAPI).outputs;
    }
    bool enabled() {
        auto& c = getCnf();
        auto it = std::find_if(c.begin(), c.end(),
                               [devN = deviceName](const midi_channel& config) { return config.deviceName == devN; });
        if (it != c.end()) {
            return true;
        }
        return false;
    }
    bool toggle() {
        bool bEnbl = enabled();
        auto& c    = getCnf();
        erase_if(c, [devN = deviceName](const midi_channel& config) { return config.deviceName == devN; });
        if (!bEnbl) {
            midi_channel ch;
            ch.idx        = 0;
            ch.deviceName = deviceName;
            c.push_back(ch);
        }
        if (parent && parent->parent) {
            parent->parent->buttonClicked(this);
        }
        return false;
    }
    void render(NVGcontext* vg) override {
        BaseCtrl* ctrl  = parentCtrl;
        float spacing   = INSET_TITLE;
        float x         = spacing;
        float rowHeight = size.y;
        if (icon > -1) {
            x += rowHeight + spacing;
        }
        if (ctrl->isCtrOrChildFocused(this)) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
            nvgFill(vg);
        }
        nvgTranslate(vg, pos.x, pos.y);
        if (icon > -1) {
            RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
            drawIcon(vg, size, &image);
        }
        setFont(vg, (int)(ROW_FONT_SIZE), G_WHITE, G_TITLE_ALIGN);
        nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), nullptr);
        //auto* _entry = safeRefGet(ref);
        //if (_entry) {
        bool enbl = enabled();
        setFont(vg, (int)(ROW_FONT_SIZE), theme->getColor(enbl ? GuiColor::COL_ON : GuiColor::COL_OFF), G_TITLE_ALIGN);
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        String str = enbl ? "On" : "Off";
        nvgText(vg, size.x - spacing, rowHeight / 2, StringAsCStr(str), nullptr);
        //}
        //nvgBeginPath(vg);
        //int i2 = 4;
        //nvgRect(vg, i2, i2, size.x-i2*2, size.y-i2*2);
        //nvgFillColor(vg, rgbToNvg(0xFF11ff11));
        //nvgFill(vg);
        nvgTranslate(vg, -pos.x, -pos.y);
    }
};


class guidialog_midi_io : public setting_dialog {
    gui_list* deviceListInput;
    gui_list* deviceListOutput;

public:
    void onDialogShow() override { updateOptions(); }

    ~guidialog_midi_io() override {
        removeGuis();
        delete deviceListInput;
        delete deviceListOutput;
    }
    guidialog_midi_io() : setting_dialog() {
        deviceListInput  = new gui_list();
        deviceListOutput = new gui_list();

        setBackgroundRendered(true);
        add(deviceListInput);
        add(deviceListOutput);
        deviceListInput->setLabel("Midi input device");
        deviceListOutput->setLabel("Midi output device");
        setLabel("Midi I/O");
        updateOptions();
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        float lineh;
        setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
        nvgTextMetrics(vg, nullptr, nullptr, &lineh);
        nvgText(vg, 5, this->deviceListInput->top() - 2, StringAsCStr(this->deviceListInput->label), nullptr);
        nvgText(vg, 5, this->deviceListOutput->top() - 2, StringAsCStr(this->deviceListOutput->label), nullptr);

        for (auto c : guis) {
            nvgSave(vg);
            //if (c == this->selectAPI) {
            //nvgIntersectScissor(vg, c->pos.x, c->pos.y, c->size.x, c->size.y);
            //}
            c->render(vg);
            nvgRestore(vg);
        }
    }
    void layout() override {
        ivec2 cs = getSizeContent();

        int32_t inset          = 5;
        int32_t heightList     = math::max(230, cs.y * 2 / 5);
        deviceListInput->pos   = ivec2(inset, inset + (int32_t)(TEXT_FONT_SIZE * 1.2));
        deviceListInput->size  = ivec2((cs.x) - inset * 2, heightList);
        deviceListOutput->pos  = ivec2(inset, deviceListInput->bottom() + inset + (int32_t)(TEXT_FONT_SIZE * 1.2));
        deviceListOutput->size = ivec2((cs.x) - inset * 2, math::min(cs.y - deviceListOutput->pos.y, heightList));
        for (auto gui : guis) {
            gui->layout();
        }
    }
    void buttonClicked(guibase* button) override {
        if ((button->id & 0x0F) == 0xF) {
            midihost::getInstance()->reopenAllConfiguredDevices(false);
            //updateSrBs();
            return;
        }
        if (this->parent) {
            this->parent->buttonClicked(button);
        }
    }


    void updateOptions() {
        if (midihost::getInstance()->isInitialized()) {
            for (int i = 0; i < Pm_CountDevices(); i++) {
                const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
                if (info->input) log_printf("%d: %s, %s\n", i, info->interf, info->name);
            }
            printf("MIDI output devices:\n");
            for (int i = 0; i < Pm_CountDevices(); i++) {
                const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
                if (info->output) log_printf("%d: %s, %s\n", i, info->interf, info->name);
            }


            std::vector<gui_list_entry*> _newListIn;
            std::vector<gui_list_entry*> _newListOut;
            int deviceCount = Pm_CountDevices();
            for (int i = 0; i < deviceCount; i++) {
                const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
                if (info && info->output > 0) {
                    _newListOut.push_back(new gui_listentry_mididevice{"stdmidi", info->name, false});
                }
                if (info && info->input > 0) {
                    _newListIn.push_back(new gui_listentry_mididevice{"stdmidi", info->name, true});
                }
            }
            int idx = 0;
            for (auto* p : _newListIn) {
                p->id = 0x1f | (idx++ << 8);
            }
            idx = 0;
            for (auto* p : _newListOut) {
                p->id = 0x0f | (idx++ << 8);
            }
            deviceListInput->setList(_newListIn);
            deviceListOutput->setList(_newListOut);
            deviceListInput->layout();
            deviceListOutput->layout();
        }
    };
};

class guidialog_plugin_settings : public setting_dialog {
    guibutton* scanNow;
    guibutton* selectFolder;

public:
    void onDialogShow() override { updateOptions(); }

    ~guidialog_plugin_settings() override {
        removeGuis();
        delete scanNow;
        delete selectFolder;
    }
    guidialog_plugin_settings() : setting_dialog() {
        scanNow      = new guibutton();
        selectFolder = new guibutton();

        setBackgroundRendered(true);
        selectFolder->id = 0x10;
        selectFolder->setText(settings.pluginPath);
        selectFolder->setLabel("VST2 Plugin Path");
        scanNow->id = 0x11;
        scanNow->setText("Scan VST2 folder");
        scanNow->setLabel("Scan");
        add(selectFolder);
        add(scanNow);
        setLabel("Plugins");
        updateOptions();
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        float lineh;
        setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
        nvgTextMetrics(vg, nullptr, nullptr, &lineh);
        nvgText(vg, 5, this->selectFolder->bottom(), StringAsCStr(this->selectFolder->label), nullptr);
        nvgText(vg, 5, this->scanNow->bottom(), StringAsCStr(this->scanNow->label), nullptr);

        for (auto c : guis) {
            nvgSave(vg);
            //if (c == this->selectAPI) {
            //nvgIntersectScissor(vg, c->pos.x, c->pos.y, c->size.x, c->size.y);
            //}
            c->render(vg);
            nvgRestore(vg);
        }
    }
    void layout() override {
        ivec2 cs = getSizeContent();

        int32_t inset      = 5;
        int32_t buttonW    = math::max(120, cs.x * 2 / 3);
        int32_t height     = 20;
        selectFolder->size = ivec2(buttonW, height);
        selectFolder->pos  = ivec2(cs.x - inset * 2 - buttonW, inset);
        scanNow->size      = ivec2(buttonW, height);
        scanNow->pos       = ivec2(cs.x - inset * 2 - buttonW, selectFolder->bottom() + inset);

        for (auto gui : guis) {
            gui->layout();
        }
    }
    void buttonClicked(guibase* button) override {
        if (button->id == 0x11) {
            auto plughost = vsthost::getInstance();
            if (!plughost->isScanning()) {
                plughost->scanPlugins();
                scanNow->setText("Cancel Scanning");
            } else {
                plughost->checkScanner();
                plughost->stopScanner();
                scanNow->setText("Scan VST2 folder");
            }

            return;
        }
        if (button->id == 0x10) {
            selectFolder->setText(settings.pluginPath);
            // select folder
            String out   = "C:/plugins";
            String curre = settings.pluginPath;

            replaceString(curre, "/", "\\");
            if (0 == browseForFolder("Select VST2 Plugin Path", curre, out)) {
                settings.pluginPath = out;
                try {
                    saveSettings(settings);
                } catch (std::exception& e) {
                    getGlobalLogger()->logStr(StringFormat("Exception: %s\n", e.what()));
                }
            }
            selectFolder->setText(settings.pluginPath);
            return;
        }
        if (this->parent) {
            this->parent->buttonClicked(button);
        }
    }


    void updateOptions() {
        auto plughost = vsthost::getInstance();
        if (!plughost->isScanning()) {
            scanNow->setText("Scan VST2 folder");
        } else {
            scanNow->setText("Cancel Scanning");
        }
    };
};


struct guidialog_settings::dialog_entry {
    guibuttonstate tabButton;
    setting_dialog* tabCtr;
    bool active = false;
    dialog_entry(setting_dialog* _ctr, String title) : tabButton(), tabCtr(_ctr) {
        tabButton.setText(title);
        tabButton.setButtonColor(GuiColor::COL_BASE_BG_FOCUSED);
        tabButton.setStateRef(&active);
        tabButton.setFontScale(0.7f);
    }
};
void guidialog_settings::init() {
    ctrType = CTR_TYPE_SETTINGS;
    addEntry(new guidialog_audio_io(), "Audio I/O");
    addEntry(new guidialog_midi_io(), "Midi I/O");
    addEntry(new guidialog_plugin_settings(), "Plugins");
    add(&btnClose);
    btnClose.id = ID_BTN_CLOSE;
    btnClose.setText("Close");
    btnClose.setFontSize(BTN_FONT_SIZE);
    setLabel("Settings");
    setActiveEntry(0);
}
guidialog_settings::guidialog_settings(ivec2 _dialogSize, bool _resizeable) : guidialog_base(_dialogSize, _resizeable) {
    init();
}
guidialog_settings::guidialog_settings() : guidialog_base(ivec2{640, 760}, true) { init(); }
void guidialog_settings::addEntry(setting_dialog* ctr, String title) {
    auto* entry = new guidialog_settings::dialog_entry{ctr, title};
    guictr_base::add(&entry->tabButton);
    this->entries.push_back(entry);
}
void guidialog_settings::setActiveEntry(int32_t idx) {
    if (idx >= 0 && idx < entries.size()) {
        guidialog_settings::dialog_entry* entry = entries[idx];
        if (this->activeEntry) {
            this->activeEntry->active = false;
            this->removeUNCHECKED(this->activeEntry->tabCtr);
        }
        this->activeEntry         = entry;
        this->activeEntry->active = true;
        this->add(this->activeEntry->tabCtr);
        if (this->parentCtrl) {
            this->layout();
        }
        this->activeEntry->tabCtr->onDialogShow();
    }
}
guidialog_settings::~guidialog_settings() {
    for (auto* entry : entries) {
        remove(&entry->tabButton);
    }
    remove(&btnClose);
    // only this->activeEntry->tabCtr should be in this cointainer
    // at this point. And it must be a valid pointer
    dbgassert(guis.size() <= 1);
    removeGuis();
    for (auto* entry : entries) {
        delete entry->tabCtr;
        delete entry;
    }
}

void guidialog_settings::render(NVGcontext* vg) { guictxtmenu_base::render(vg); }
void guidialog_settings::layout() {
    int32_t inset = INSET_CTR_SPACING;

    ivec2 csize       = getSizeContent();
    int32_t closeSize = 32;
    btnClose.size     = ivec2(closeSize * 4, closeSize);
    btnClose.pos      = ivec2(csize.x - btnClose.size.x - inset, csize.y - btnClose.size.y);

    csize.y -= btnClose.size.y;

    int csW         = csize.x - inset * 2;
    ivec2 buttonPos = {inset, inset * 2};
    int32_t buttonW = csW / 5;
    for (auto* entry : entries) {
        entry->tabButton.pos  = buttonPos;
        entry->tabButton.size = ivec2(buttonW, HEIGHT_DEFAULT_INPUT);
        entry->tabButton.layout();
        buttonPos.y += HEIGHT_DEFAULT_INPUT + inset;
    }
    ivec2 sizeContentTab = ivec2(csW - buttonW - inset * 2, csize.y - inset * 2);
    for (auto* entry : entries) {
        entry->tabCtr->pos  = ivec2(buttonPos.x + buttonW + inset * 2, inset);
        entry->tabCtr->size = sizeContentTab;
        entry->tabCtr->determineSize(entry->tabCtr->size);
        entry->tabCtr->layout();
    }

    for (auto gui : guis) {
        gui->layout();
    }
}
void guidialog_settings::buttonClicked(guibase* button) {

    auto it = std::find_if(entries.begin(), entries.end(), [button](const guidialog_settings::dialog_entry* entry) {
        return &entry->tabButton == button;
    });
    if (it != entries.end()) {
        size_t pos = it - entries.begin();
        setActiveEntry((int32_t)pos);
    }
    //if (parent) {
    //parent->buttonClicked(button);
    //}
    switch (button->id) {
        case ID_BTN_CLOSE:
            closeContextMenu();
            break;
    }
}
