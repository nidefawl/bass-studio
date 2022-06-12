#include "dialog_io.h"
#include "appsettings.h"
#include "gui/controls/button.h"
#include "dialog.h"
#include "gui/controls/inputfield.h"
#include "gui/dropdown/dropdown.h"
#include "gui/meter/guimeter.h"
#include "gui/controls/textfield.h"
#include "gui/container/container.h"
#include "gui/container/scrollcontainer.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "host/audio_host.h"
#include "host/mainctrl.h"
#include "host/midi_host.h"
#include "host/vst_host.h"
#include "gui/controls/list.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "meter.h"
#include "platform.h"
#include "renderresources.h"
#include "seq_util.h"
#include "str_util.h"
#include "gui/views/controls.h"
#include "tls.h"
#include "types.h"
#include <array>
#include <cstdint>
#include <utility>
#include <portaudio.h>
#include <portmidi.h>
#ifdef _WIN32
#include "platform/win/windowsize.h"
#endif
#ifdef __linux__
#include "platform/linux/windowsize.h"
#endif


namespace DAW::DialogSettings {

constexpr int ID_BTN_CLOSE    = 1;

class guidropdown_setting_options_t;
class guidropdown_setting_options_ctxt_t : public guictxtmenu {
    guidropdown_setting_options_t* parent;
    std::vector<String> strings;

public:
    explicit guidropdown_setting_options_ctxt_t(guidropdown_setting_options_t* _parent);
    void clicked(int _id) override;
};
class guidropdown_setting_options_t : public guidropdownbase {
public:
    std::vector<String> options;
    std::function<void(int)> cbOnOptionSelected;
    std::function<String()> fnGetCurrentVal;
    std::function<int32_t()> fnGetCurrentIdx;

public:
    int32_t getSelectIndex() override { return fnGetCurrentIdx ? fnGetCurrentIdx() : -1; }
    int32_t getLastIndex()  override { return CtrSize(options); }
    void setSelectedIndex(int32_t idx)  override { clicked(idx); }
    String getString() override { return fnGetCurrentVal ? fnGetCurrentVal() : "<null>"; }
    void handleDraggedRelease(MouseEvent& evt) override {
        if (options.empty()) return;
        guictxtmenu_base* popup = new guidropdown_setting_options_ctxt_t(this);
        popup->dawCtrl = dawCtrl;
        popup->size = size;
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
        : gui_list_entry(),
          deviceAPI(std::move(_deviceAPI)),
          deviceName(std::move(_deviceName)),
          nChannels(_nChannels),
          isInput(_isInput)
    {
        icon = -1;
    }
    String getText() override { return deviceName; }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {}
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
    void handleDraggedBegin(MouseEvent& evt) override { toggle(); }
    app_ioaudioconfig& getCnf() { return daw_tls::getSettings().iosettings.getConfig(deviceAPI); }
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

        renderText(vg,
                   vec2(x, rowHeight * 0.5f),
                   vec2(size),
                   getText(),
                   rowHeight);

        ivec2 sizeIcon = ivec2(inner.y - 4);
        ivec2 posIcon  = {inner.x - (int)spacing - sizeIcon.y, (inner.y - sizeIcon.y) / 2};
        bool enbl = enabled();

        renderTextLabel(vg,
                        vec2(posIcon.x - 4, rowHeight * 0.5f),
                        vec2(size),
                        StringFormat("%d CH", nChannels),
                        theme,
                        rowHeight,
                        theme->getColor(enbl ? GuiColor::COL_ON : GuiColor::COL_OFF),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);


        RenderResources::NvgImageTexture& image = RenderResources::imgIcons[ICON_SPEAKER];
        nvgTranslate(vg, posIcon.x, posIcon.y);
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, sizeIcon.x, sizeIcon.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
            nvgFill(vg);
            nvgStrokeColor(vg, theme->getBgStrokeColor(parent->getFlags()));
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
            nvgStroke(vg);
            if (enbl) {
                drawIcon(vg, sizeIcon, &image, 2);
            }
        nvgTranslate(vg, -posIcon.x, -posIcon.y);
        nvgTranslate(vg, -pos.x, -pos.y);
    }
};

class guictr_input_channel : public guictr_base {
    std::shared_ptr<audiohost::HostIOStream::IOChannel> ioChannel;
    const bool isInput;
    std::shared_ptr<guibase> guimeter;
    guibutton btnTrackType;

public:
    guictr_input_channel(std::shared_ptr<audiohost::HostIOStream::IOChannel>& _ioChannel, bool _isInput)
        : ioChannel(_ioChannel), isInput(_isInput) {
        add(&btnTrackType);
        btnTrackType.setText(DAW::AudioIO::getTrackTypeStr(_ioChannel->type));
        guimeter = std::make_shared<gui_trackmeter>(&_ioChannel->meter);
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

    bool setScissorTransformContainer(NVGcontext* vg) override {
        ivec2 sizeInset = getSizeContent();
        if (sizeInset.y <= 0 || sizeInset.x <= 0) {
            return false;
        }
        nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
        nvgTranslate(vg, pos.x, pos.y);
        return true;
    }

    void render(NVGcontext* vg) override {
        if (!setScissorTransformContainer(vg)) {
            return;
        }
        renderFrameBase(vg);
        int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : 0;
        if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
        renderTitleBar(vg, size, this->label, GuiConstant::CONST_SMALL_LABEL_HEIGHT, size.y, flags, false);
        renderFrameOutline(vg);
        ivec2 posInset  = getPosContent();
        nvgTranslate(vg, posInset.x-pos.x, posInset.y-pos.y);
        nvgTranslateZ(vg, -4.0f);
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
        using namespace DAW::AudioIO;
        if (gui == &btnTrackType) {
            log_printf("Switch track type\n");
            auto& settings = daw_tls::getSettings();
            auto& cnf = settings.iosettings.getChannelConfig(settings.iosettings.device_api);
            io_cfg_tracks newConfig = cnf;
            auto& list    = isInput ? cnf.input : cnf.output;
            auto& newList = isInput ? newConfig.input : newConfig.output;
            newList.clear();

            const channel_pairing type = getNextTrackType(ioChannel->type);
            const auto nChannelsPrev = getNumChannelsFromTrackType(ioChannel->type);
            const auto nChannels     = getNumChannelsFromTrackType(type);
            const auto base          = (ioChannel->channelOffset / nChannels);
            const auto begin         = base * nChannels;
            const auto end           = begin + nChannels;

            auto ratio = math::max(1, nChannelsPrev / nChannels);
            for (channelnum_t c = 0; c < ratio; c++) {
                io_cfg_channel channels;
                channels.idx    = 0;
                channels.type   = type;
                channels.offset = (base + c) * nChannels;
                newList.push_back(channels);
            }
            for (auto& existChannelCnf : list) {
                auto existChCnfChannels = getNumChannelsFromTrackType(existChannelCnf.type);
                if (existChannelCnf.offset >= end || existChannelCnf.offset + existChCnfChannels <= begin) {
                    newList.push_back(existChannelCnf);
                }
            }
            std::sort(newList.begin(), newList.end(),
                      [](const io_cfg_channel& entryA, const io_cfg_channel& entryB) {
                          return entryA.offset < entryB.offset;
                      });
            while (true) {
                // Find unassigned channels
                channelnum_t endPrevChannel = 0;
                auto it = std::find_if(newList.begin(), newList.end(),
                              [&endPrevChannel](const io_cfg_channel& entryA) {
                                if (entryA.offset > endPrevChannel)
                                    return true;
                                endPrevChannel = entryA.offset + getNumChannelsFromTrackType(entryA.type);
                                return false;
                              });
                if (it == newList.end()) {
                    break;
                }

                channelnum_t endChannel = it->offset;
                int freeChannels = endChannel - endPrevChannel;
                log_printf("found %u unassigned channels %u to %u\n", freeChannels, endPrevChannel, endChannel);

                // create tracks for unassigned channels
                while (freeChannels > 0) {
                    const channel_pairing type2 = getTrackTypeFromNumChannels(static_cast<channelnum_t>(freeChannels));
                    io_cfg_channel channel2;
                    channel2.idx       = -1;
                    channel2.type      = type2;
                    channel2.offset    = endPrevChannel;
                    auto nChannels2 = getNumChannelsFromTrackType(channel2.type);
                    endPrevChannel += nChannels2;
                    freeChannels -= nChannels2;
                    newList.push_back(channel2);
                    log_printf("add track %s channels %u to %u\n", StringAsCStr(channel2.name), channel2.offset,
                               channel2.offset + nChannels2);
                }

                // make sure we did not assign too many channels
                dbgassert(freeChannels >= 0);
                std::sort(newList.begin(), newList.end(),
                          [](const io_cfg_channel& entryA, const io_cfg_channel& entryB) {
                              return entryA.offset < entryB.offset;
                          });
            }

            int32_t idx = 0;
            for (io_cfg_channel& entry : newList) {
                entry.idx  = idx++;
                entry.name = getTrackName(entry.type, entry.idx, isInput);
            }


            // find maximum channel idx used
            channelnum_t maxChannel = 0;
            for (auto& ch : newList) {
                auto chCount  = getNumChannelsFromTrackType(ch.type);
                auto chEndIdx = ch.offset + chCount;
                maxChannel = math::max<channelnum_t>(maxChannel, chEndIdx);
            }

            std::vector<int32_t> vChannelIdc(maxChannel);
            std::fill(vChannelIdc.begin(), vChannelIdc.end(), -1);

            // make sure we have no channel double assignment
            bool foundDblAssignment = false;
            for (auto& ch : newList) {
                auto chCount = getNumChannelsFromTrackType(ch.type);
                for (int j = 0; j < chCount; j++) {
                    foundDblAssignment |= vChannelIdc[j + ch.offset] != -1;
                    dbgassert (!foundDblAssignment);
                    vChannelIdc[j + ch.offset] = ch.idx;
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
                log_printf("Invalid channel configuration\n");
            } else {
                cnf = newConfig;
                dawCtrl->getDaw()->configureSampleRate();
            }
        }
    }

    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            gui->onTick(ctrl);
        }
    }
};
class guictr_input_meters : public guictr_base, public gui_scrollcontainer  {
    std::vector<std::shared_ptr<guictr_input_channel>> guiMeters;
    const bool isInput;
    int32_t prevStream = 0;
    gui_scrollbar scrollbar;
    int scrollOffset  = 0;
    int contentWidth = 0;
    bool hasScrollbar = false;

public:
    explicit guictr_input_meters(const bool _isInput)
        : isInput(_isInput),
        scrollbar(0, 0.0f, *this)
    {
        setCanMouseHit(true);
        scrollbar.setParent(this);
        margin /= 2;
        padding /= 2;
    }

    ~guictr_input_meters() override {
        removeGuis();
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        auto* stream = dawCtrl->getDaw()->getAudioHost()->getStream(0);
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
    void updateChannels(audiohost::HostIOStream* stream) {
        for (auto& gui : guiMeters) {
            remove(gui.get());
        }
        guiMeters.clear();
        if (stream) {

            auto& ioChannels = isInput ? stream->channelsInput : stream->channelsOutput;
            String devName = isInput ? stream->inputName : stream->outputName;

            while (guiMeters.size() < ioChannels.size()) {
                auto idx         = guiMeters.size();
                String trackName = audiohost::HostIOStream::getTrackName(ioChannels[idx].get(), isInput);
                auto p           = std::make_shared<guictr_input_channel>(ioChannels[idx], isInput);
                p->setLabel(trackName);
                guiMeters.push_back(p);
                add(p.get());
                p->dawCtrl = this->dawCtrl;
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
        dbgassert(this->dawCtrl);
        for (guibase* gui : guis) {
            gui->onTick(ctrl);
        }
        auto* stream = dawCtrl->getDaw()->getAudioHost()->getStream(0);
        if ((prevStream && !stream) || (stream && prevStream != stream->streamId)) {
            updateChannels(stream);
        }
    }

    void setPositions(ivec2 cs, ivec2 offset) {
        auto maxChannels = math::max<channelnum_t>(guiMeters.size(), 6);
        auto nMeters     = math::max<channelnum_t>(1, maxChannels);
        ivec2 meterSize  = { math::clamp(math::max(8, (cs.x) / nMeters), 96, 256), cs.y };
        ivec2 meterPos   = { 0, 0 };
        if (hasScrollbar) {
            meterSize.y -= gui_scrollbar::defaultW + 2;
        }
        for (auto& meter : guiMeters) {
            meter->pos  = meterPos + offset;
            meter->size = meterSize;
            meterPos.x += meterSize.x;
        }
    }

    void layout() override {
        auto cs = getSizeContent();
        setPositions(cs, {0, 0});
        if (!guiMeters.empty())
            contentWidth = guiMeters.back()->right();
        else contentWidth = 0;
        hasScrollbar = contentWidth >= cs.x;
        scrollbar.setVisible(hasScrollbar);
        guis.erase(std::remove_if(guis.begin(), guis.end(), [bar=&scrollbar](const guibase* x) { return x == bar;}), guis.end());
        if (hasScrollbar) {
            guis.insert(guis.begin(), &scrollbar);
            scrollbar.parent = this;
            scrollbar.size = ivec2(cs.x - 2, gui_scrollbar::defaultW - 2);
            scrollbar.pos  = ivec2(1, cs.y - gui_scrollbar::defaultW);
            scrollOffsetChanged(1, scrollbar.scrollOffset);
        }
        for (auto& gui : guis) {
            if (gui == &scrollbar)
                continue;
            gui->layout();
        }
    }

    ivec2 getScrollTotalSize() const override {
        return {contentWidth, getSizeContent().y};
    }

    ivec2 getScrollViewSize() const override {
        return getSizeContent();
    }

    void scrollOffsetChanged(int dir, float offset) override {
        auto cs = getSizeContent();
        if (hasScrollbar) {
            this->scrollOffset = -offset * (contentWidth - size.x);
        } else {
            this->scrollOffset = 0;
        }
        setPositions(cs, {this->scrollOffset, 0});
    };

    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override {
        return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        scrollbar.setControl(parentCtrl);
    }
};
class guidialog_audio_io : public setting_dialog {
    DawInstance* const daw;
    appsettings& settings;
    guibutton_audioengine* audioEngineOn;
    guidropdownbase* audioBlockSize;
    guidropdownbase* audioSampleRate;
    guidropdownbase* audioInternalBlockSize;
    guidropdownbase* audioInternalSampleRate;
    guidropdownbase* selectAPI;
    guidropdownbase* asioDevice;
    gui_list* deviceListInput;
    gui_list* deviceListOutput;
    guictr_input_meters metersInput;
    guictr_input_meters metersOutput;

public:
    void onDialogShow() override { updateOptions(); }
    void updateOptions() {
        auto& settings = daw_tls::getSettings();
        auto audiohost = daw->getAudioHost();
        if (audiohost->initPa()) {
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
        delete deviceListOutput;
        delete deviceListInput;
        delete asioDevice;
        delete selectAPI;
        delete audioInternalSampleRate;
        delete audioInternalBlockSize;
        delete audioSampleRate;
        delete audioBlockSize;
        delete audioEngineOn;
    }

    guidialog_audio_io(DawInstance* _daw)
        : setting_dialog(),
          daw(_daw),
          settings(daw_tls::getSettings()),
          audioEngineOn(new guibutton_audioengine{}),
          deviceListInput(new gui_list()),
          deviceListOutput(new gui_list()),
          metersInput(true),
          metersOutput(false)
    {
        using namespace DAW::AudioIO;

        dawCtrl = daw->getMainControl();
        dbgassert(dawCtrl);
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

        for (auto sr : ExtSamplerates) {
            extSampleRate->options.push_back(StringFormat("%u", sr));
        }
        for (auto sr : IntSamplerates) {
            intSampleRate->options.push_back(StringFormat("%u", sr));
        }
        extSampleRate->cbOnOptionSelected = [this](int option) {
            if (option >= 0 && option < CtrSize(ExtSamplerates)) {
                settings.iosettings.samplerate = ExtSamplerates[option];
                daw->configureSampleRate();
            }
        };
        extSampleRate->fnGetCurrentVal = [this]() -> String {
            return StringFormat("%u", settings.iosettings.samplerate);
        };
        extSampleRate->fnGetCurrentIdx = [this]() -> uint32_t {
            return indexOfCtr(ExtSamplerates, settings.iosettings.samplerate);
        };
        intSampleRate->cbOnOptionSelected = [this](int option) {
            if (option >= 0 && option < CtrSize(IntSamplerates)) {
                settings.iosettings.internalSamplerate = IntSamplerates[option];
                daw->configureSampleRate();
            }
        };
        intSampleRate->fnGetCurrentVal = [this]() -> String {
            return StringFormat("%u", settings.iosettings.internalSamplerate);
        };
        intSampleRate->fnGetCurrentIdx = [this]() -> uint32_t {
            return indexOfCtr(IntSamplerates, settings.iosettings.internalSamplerate);
        };
        static constexpr blocksize_t BLOCK_SIZE_BITS = 10;
        for (auto i = 0U; i < BLOCK_SIZE_BITS; i++) {
            blocksize_t blockSize = 1U << (4U + i);
            extBlockSize->options.push_back(StringFormat("%u", blockSize));
            intBlockSize->options.push_back(StringFormat("%u", blockSize));
        }
        extBlockSize->cbOnOptionSelected = [this](int option) {
            if (option >= 0 && option < BLOCK_SIZE_BITS) {
                int blockSize = 1 << (4 + option);
                settings.iosettings.blocksize = blockSize;
                daw->configureSampleRate();
            }
        };
        extBlockSize->fnGetCurrentVal = [this]() -> String { return StringFormat("%u", settings.iosettings.blocksize); };
        intBlockSize->cbOnOptionSelected = [this](int option) {
            if (option >= 0 && option < BLOCK_SIZE_BITS) {
                int blockSize = 1 << (4 + option);
                settings.iosettings.internalBlocksize = blockSize;
                daw->configureSampleRate();
            }
        };
        intBlockSize->fnGetCurrentVal = [this]() -> String {
            return StringFormat("%u", settings.iosettings.internalBlocksize);
        };

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
            if (option >= 0 && option < CtrSize(api->options)) {
                settings.iosettings.device_api = api->options[option];
                updateOptions();
                daw->configureSampleRate();
            }
        };
        api->fnGetCurrentVal     = [this]() -> String { return settings.iosettings.device_api; };
        asio->cbOnOptionSelected = [this, asio](int option) {
            if (option >= 0 && option < CtrSize(asio->options)) {
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
                daw->configureSampleRate();
            }
        };
        asio->fnGetCurrentVal = [this]() -> String {
            if (settings.iosettings.asioConfig.deviceName.length()) {
                return settings.iosettings.asioConfig.deviceName;
            }
            return "None";
        };

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
        extBlockSize->setLabel("External Blocksize");
        extSampleRate->setLabel("External Samplerate");
        intBlockSize->setLabel("Internal Blocksize");
        intSampleRate->setLabel("Internal Samplerate");
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
        for (auto g : guis) {
            g->dawCtrl = this->dawCtrl;
        }
        dbgassert(metersInput.dawCtrl);
        dbgassert(metersOutput.dawCtrl);
    }

    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            gui->onTick(ctrl);
        }
    }

    void layout() override {
        const ivec2 cs = getSizeContent();

        int32_t inset  = 5;
        const int32_t height = theme->get(GuiConstant::CONST_ROW_HEIGHT);

        audioEngineOn->size           = ivec2(cs.x - inset * 2, height);
        audioEngineOn->pos            = ivec2(inset, inset);
        audioInternalBlockSize->size  = ivec2(cs.x - inset * 2, height);
        audioInternalBlockSize->pos   = ivec2(inset, audioEngineOn->bottom() + inset);
        audioInternalSampleRate->size = ivec2(cs.x - inset * 2, height);
        audioInternalSampleRate->pos  = ivec2(inset, audioInternalBlockSize->bottom() + inset);
        audioBlockSize->size          = ivec2(cs.x - inset * 2, height);
        audioBlockSize->pos           = ivec2(inset, inset + audioInternalSampleRate->bottom());
        audioSampleRate->size         = ivec2(cs.x - inset * 2, height);
        audioSampleRate->pos          = ivec2(inset, audioBlockSize->bottom() + inset);
        selectAPI->size               = ivec2(cs.x - inset * 2, height);
        selectAPI->pos                = ivec2(inset, audioSampleRate->bottom() + inset);
        int32_t h                     = (cs.y - inset) - (selectAPI->bottom() + inset);
        int32_t h1                    = math::max((int)(h * 0.2), 120);

        guibase* pNextGui = selectAPI;
        asioDevice->size  = ivec2(cs.x - inset * 2, height);
        asioDevice->pos   = ivec2(inset, selectAPI->bottom() + inset);
        if (asioDevice->isVisible()) {
            pNextGui = asioDevice;
        }
        int topPadding = 5;
        int leftPadding = 0;

        deviceListInput->pos  = ivec2(leftPadding, pNextGui->bottom() + topPadding);
        deviceListInput->size = ivec2((cs.x) - topPadding * 2, h1);
        if (deviceListInput->isVisible()) {
            pNextGui = deviceListInput;
        }
        deviceListOutput->pos  = ivec2(leftPadding, pNextGui->bottom() + topPadding);
        deviceListOutput->size = ivec2((cs.x) - topPadding * 2, math::min(cs.y - deviceListOutput->pos.y, h1));
        if (deviceListOutput->isVisible()) {
            pNextGui = deviceListOutput;
        }
        topPadding += 4;
        h                = (cs.y - topPadding) - (pNextGui->bottom() + topPadding);
        int32_t h2       = math::max((int)(h * 0.5), 120);
        metersInput.pos  = ivec2(leftPadding, pNextGui->bottom() + topPadding);
        metersInput.size = ivec2((cs.x) - topPadding * 2, h2);
        if (metersInput.isVisible()) {
            pNextGui = &metersInput;
        }
        metersOutput.pos  = ivec2(leftPadding, pNextGui->bottom() + topPadding);
        metersOutput.size = ivec2((cs.x) - topPadding * 2, h2);
        if (metersOutput.isVisible()) {
            pNextGui = &metersOutput;
        }
        for (auto gui : guis) {
            gui->layout();
        }
        deviceListInput->setRowHeight(height);
        deviceListOutput->setRowHeight(height);
    }

    void buttonClicked(guibase* button) override {
        if (button == this->audioEngineOn) {
            settings.startEngine = !settings.startEngine;
            daw->configureSampleRate();
            return;
        }
        if ((button->id & 0x0F) == 0xF) {
            daw->configureSampleRate();
            return;
        }
        if (this->parent) {
            this->parent->buttonClicked(button);
        }
    }
};
class gui_listentry_mididevice : public gui_list_entry {
    appsettings& settings;
    String deviceAPI;
    String deviceName;
    const bool isInput;

public:
    gui_listentry_mididevice(String _deviceAPI, String _deviceName, bool _isInput)
        : gui_list_entry(),
          settings(daw_tls::getSettings()),
          deviceAPI(std::move(_deviceAPI)),
          deviceName(std::move(_deviceName)),
          isInput(_isInput)
    {
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
        auto it = std::find_if(c.begin(), c.end(), [devN = deviceName](const midi_channel& config) { return config.deviceName == devN; });
        return it != c.end();
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

        renderTextLabel(vg,
                        vec2(x, rowHeight * 0.5f),
                        vec2(size),
                        getText(),
                        theme,
                        rowHeight,
                        THEMECOL_TEXT,
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        ivec2 sizeIcon = ivec2(inner.y - 4);
        ivec2 posIcon  = {inner.x - (int)spacing - sizeIcon.y, (inner.y - sizeIcon.y) / 2};
        bool enbl = enabled();

        renderTextLabel(vg,
                        vec2(posIcon.x - 4, rowHeight * 0.5f),
                        vec2(size),
                        enbl ? "On" : "Off",
                        theme,
                        rowHeight,
                        theme->getColor(enbl ? GuiColor::COL_ON : GuiColor::COL_OFF),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);


        RenderResources::NvgImageTexture& image = RenderResources::imgIcons[ICON_MIDIPLUG];
        nvgTranslate(vg, posIcon.x, posIcon.y);
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, sizeIcon.x, sizeIcon.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
            nvgFill(vg);
            nvgStrokeColor(vg, theme->getBgStrokeColor(parent->getFlags()));
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
            nvgStroke(vg);
            if (enbl) {
                drawIcon(vg, sizeIcon, &image, 2);
            }
        nvgTranslate(vg, -posIcon.x, -posIcon.y);
        nvgTranslate(vg, -pos.x, -pos.y);


    }
};


class guidialog_midi_io : public setting_dialog {
    DawInstance* const daw;
    gui_list deviceListInput;
    gui_list deviceListOutput;

public:
    void onDialogShow() override { updateOptions(); }

    ~guidialog_midi_io() override {
        removeGuis();
    }

    guidialog_midi_io(DawInstance* _daw)
        : setting_dialog(),
          daw(_daw)
    {
        add(&deviceListInput);
        add(&deviceListOutput);
        deviceListInput.setLabel("Midi input device");
        deviceListOutput.setLabel("Midi output device");
        deviceListInput.setFlag(FLG_RENDER_LABEL, true);
        deviceListOutput.setFlag(FLG_RENDER_LABEL, true);
        setLabel("Midi I/O");
        updateOptions();
    }

    void layout() override {
        const ivec2 cs = getSizeContent();

        const int32_t inset  = 0;
        const int32_t height = theme->get(GuiConstant::CONST_ROW_HEIGHT);

        int32_t heightList     = math::max(230, cs.y * 2 / 5);

        deviceListInput.pos   = ivec2(inset);
        deviceListInput.size  = ivec2((cs.x) - inset * 2, heightList);
        deviceListOutput.pos  = ivec2(inset, deviceListInput.bottom() + inset);
        deviceListOutput.size = ivec2((cs.x) - inset * 2, math::min(cs.y - deviceListOutput.pos.y, heightList));

        for (auto gui : guis) {
            gui->layout();
        }

        deviceListInput.setRowHeight(height);
        deviceListOutput.setRowHeight(height);
    }
    void buttonClicked(guibase* button) override {
        if ((button->id & 0x0F) == 0xF) {
            daw->getMidiHost()->reopenAllConfiguredDevices(false);
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
            deviceListInput.setList(_newListIn);
            deviceListOutput.setList(_newListOut);
            deviceListInput.layout();
            deviceListOutput.layout();
        }
    };
};


class guidialog_settings_other : public setting_dialog {
    DawInstance* const daw;
    enum appsetting_type {
        VM_MODE,
        SHADER_RENDER_RESPONSIVENESS,
    };
    guictr_base listOptions;
    guidropdown_setting_options_t* autosave;
public:
    class gui_listentry_settings_other_bool : public gui_list_entry {
        appsetting_type type;
        String title;
        public:
        gui_listentry_settings_other_bool(appsetting_type type, String title)
            : gui_list_entry(),
            type(type),
            title(std::move(title))
        {
            icon = -1;
        }
        String getText() override { return title; }
        void dragMoveOn(guibase* target, ivec2 mousepos) override {}
        void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
        void handleDraggedBegin(MouseEvent& evt) override { toggle(); parent->buttonClicked(this); }
        bool enabled() {
            auto& settings = daw_tls::getSettings();
            switch (type) {
                case VM_MODE:
                    return settings.vmmode;
                case SHADER_RENDER_RESPONSIVENESS:
                    return settings.shaderDebug;
                default:
                    break;
            }
            return false;
        }
        void toggle() {
            auto& settings = daw_tls::getSettings();
            switch (type) {
                case VM_MODE:
                    settings.vmmode = !settings.vmmode;
                    break;
                case SHADER_RENDER_RESPONSIVENESS:
                    settings.shaderDebug = !settings.shaderDebug;
                    break;
                default:
                    break;
            }
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

            renderText(vg,
                    vec2(x, rowHeight * 0.5f),
                    vec2(size),
                    getText(),
                    rowHeight);

            ivec2 sizeIcon = ivec2(inner.y - 4);
            ivec2 posIcon  = {inner.x - (int)spacing - sizeIcon.y, (inner.y - sizeIcon.y) / 2};
            bool enbl = enabled();

            /* renderTextLabel(vg,
                            vec2(posIcon.x - 4, rowHeight * 0.5f),
                            vec2(size),
                            "info",
                            theme,
                            rowHeight,
                            theme->getColor(enbl ? GuiColor::COL_ON : GuiColor::COL_OFF),
                            NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE); */


            RenderResources::NvgImageTexture& image = RenderResources::imgIcons[ICON_X];
            nvgTranslate(vg, posIcon.x, posIcon.y);
                nvgBeginPath(vg);
                nvgRect(vg, 0, 0, sizeIcon.x, sizeIcon.y);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
                nvgFill(vg);
                nvgStrokeColor(vg, theme->getBgStrokeColor(parent->getFlags()));
                nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
                nvgStroke(vg);
                if (enbl) {
                    drawIcon(vg, sizeIcon, &image, 0);
                }
            nvgTranslate(vg, -posIcon.x, -posIcon.y);
            nvgTranslate(vg, -pos.x, -pos.y);
        }
    };
    void onDialogShow() override { buttonClicked(nullptr); }

    ~guidialog_settings_other() override {
        listOptions.destroyGuis();
        removeGuis();
    }
    guidialog_settings_other(DawInstance* _daw)
        : setting_dialog(),
          daw(_daw)
    {
        setCanMouseHit(true);
        listOptions.setCanMouseHit(true);
        listOptions.setBackgroundRendered(true);
        listOptions.setLabel("Other Settings");
        listOptions.setFlag(FLG_RENDER_LABEL, true);
        // setLabel("Other Settings");

        autosave  = new guidropdown_setting_options_t();
        autosave->setFlag(FLG_RENDER_LABEL, true);
        autosave->setLabel("Autosave");
        autosave->options.emplace_back("Disabled");
        autosave->options.emplace_back("5 Minutes");
        autosave->options.emplace_back("15 Minutes");
        autosave->options.emplace_back("30 Minutes");
        autosave->options.emplace_back("60 Minutes");
        autosave->options.emplace_back("2 Hours");
        autosave->options.emplace_back("3 Hours");
        autosave->cbOnOptionSelected = [](int option) {
            const int32_t delays[] = {0, 5, 15, 30, 60, 120, 180};
            auto& autosave = daw_tls::getSettings().autosave;
            autosave.tmSaveDelayMinutes = delays[math::clamp(option, 0, 6)];
        };
        autosave->fnGetCurrentVal = []() -> String {
            auto& autosave = daw_tls::getSettings().autosave;
            if (autosave.tmSaveDelayMinutes <= 0) {
                return "Disabled";
            }
            return std::to_string(autosave.tmSaveDelayMinutes) + " Minutes";
        };
        listOptions.add(new gui_listentry_settings_other_bool{VM_MODE, "Knobs: Disable raw mouse input (for VMs)"});
        listOptions.add(new gui_listentry_settings_other_bool{SHADER_RENDER_RESPONSIVENESS, "Visual: Animate UI responsiveness"});
        listOptions.add(autosave);
        add(&listOptions);
    }
    
    void layout() override {

        listOptions.pos   = {};
        listOptions.size  = getSizeContent();
        const auto cs = listOptions.getSizeContent();
        const auto height = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        const int32_t inset = 5;
        int32_t topPos = inset;
        for (auto g : listOptions.guis) {
            g->pos = ivec2(inset, topPos);
            g->size = ivec2(cs.x - inset * 2, height);
            topPos = g->bottom() + inset;
        }
        for (auto gui : guis) {
            gui->layout();
        }


    }
    void buttonClicked(guibase* button) override {
        if (this->parent && button) {
            this->parent->buttonClicked(button);
        }
    }
};
class guidialog_settings_plugins_vst2 : public guictr_base {
    DawInstance* const daw;
    appsettings& settings;
    guibutton scanNow;
    guibutton selectFolder;
    gui_textfield pathVstVal;
public:
    void onDialogShow() { updateOptions(); }

    ~guidialog_settings_plugins_vst2() override {
        removeGuis();
    }
    guidialog_settings_plugins_vst2(DawInstance* _daw)
        : guictr_base(),
          daw(_daw),
          settings(daw_tls::getSettings())
    {
        selectFolder.id = 0x10;
        selectFolder.setLabel("Select VST2 Plugin Directory");
        selectFolder.setText(selectFolder.getLabel());
        scanNow.id = 0x11;
        scanNow.setLabel("Scan VST2 Plugins");
        scanNow.setText(scanNow.getLabel());
        pathVstVal.setEditable(true);
        pathVstVal.setValue(settings.pluginsettings.pathVst2);
        add(&pathVstVal);
        add(&selectFolder);
        add(&scanNow);
        setLabel("VST2 Plugins");
        updateOptions();
    }
    void layout() override {
        ivec2 cs = getSizeContent();

        const int32_t inset   = 5;
        const int32_t height  = theme->get(GuiConstant::CONST_ROW_HEIGHT);

        pathVstVal.size   = ivec2(cs.x - inset * 2, height);
        pathVstVal.pos    = ivec2(inset);
        selectFolder.size = ivec2(cs.x - inset * 2, height);
        selectFolder.pos  = ivec2(inset, pathVstVal.bottom() + inset);
        scanNow.size      = ivec2(cs.x - inset * 2, height);
        scanNow.pos       = ivec2(inset, selectFolder.bottom() + inset);

        for (auto gui : guis) {
            gui->layout();
        }
    }
    void buttonClicked(guibase* button) override {
        if (button->id == 0x11) {
            auto plughost = daw->getHost();
            if (!plughost->isScanning()) {
                plughost->scanPlugins();
                scanNow.setText("Cancel Scanning");
            } else {
                plughost->checkScanner();
                plughost->stopScanner();
                scanNow.setText("Scan VST2 Plugins");
            }

            return;
        }
        if (button->id == 0x10) {
            // select folder
            String out   = DAW_PLATFORM_VST2_PATH_DEFAULT;
            String curre = settings.pluginsettings.pathVst2;
            App::Platform::sanitizePathToDirectory(curre);

            if (0 == browseForFolder(selectFolder.getLabel(), curre, out)) {
                settings.pluginsettings.pathVst2 = out;
                try {
                    saveSettings(settings);
                } catch (std::exception& e) {
                    log_lf(Log::L_ERROR, "Failed saving settings %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(SETTINGS_NAME)), e.what());
                }
            }
            return;
        }
        if (this->parent) {
            this->parent->buttonClicked(button);
        }
    }


    void updateOptions() {
        auto plughost = daw->getHost();
        if (!plughost->isScanning()) {
            scanNow.setText("Scan VST2 Plugins");
        } else {
            scanNow.setText("Cancel Scanning");
        }
    }
    void onTick(AppCtrl* appctrl) override {
        pathVstVal.setValue(settings.pluginsettings.pathVst2);
        updateOptions();
    }
};
class guidialog_settings_plugins : public setting_dialog {
    guidialog_settings_plugins_vst2 settings_vst2;
    public:
    void onDialogShow() override { settings_vst2.onDialogShow(); }

    ~guidialog_settings_plugins() override {
        removeGuis();
    }
    guidialog_settings_plugins(DawInstance* _daw)
        : setting_dialog(),
          settings_vst2(_daw)
    {
        add(&settings_vst2);
        settings_vst2.setBackgroundRendered(true);
        settings_vst2.setFlag(FLG_RENDER_LABEL, true);
        setLabel("Plugins");
    }

    void layout() override {
        const ivec2 cs = getSizeContent();
        settings_vst2.pos   = ivec2(0);
        settings_vst2.size  = cs;
        for (auto gui : guis) {
            gui->layout();
        }
    }
};

struct guidialog_settings::dialog_entry {
    guibuttonstate tabButton;
    setting_dialog* tabCtr;
    bool active = false;
    dialog_entry(setting_dialog* _ctr, String title) : tabButton(), tabCtr(_ctr) {
        tabButton.setText(title);
        tabButton.setButtonColor(GuiColor::COL_BASE_BG_FOCUSED);
        tabButton.setStateRef(&active);
    }
};
void guidialog_settings::init(DawInstance* daw) {
    ctrType = CTR_TYPE_SETTINGS;
    addEntry(new guidialog_audio_io(daw), "Audio I/O");
    addEntry(new guidialog_midi_io(daw), "Midi I/O");
    addEntry(new guidialog_settings_plugins(daw), "Plugins");
    addEntry(new guidialog_settings_other(daw), "Other");
    add(&btnClose);
    btnClose.id = ID_BTN_CLOSE;
    btnClose.setText("Close");
    setLabel("Settings");
    setActiveEntry(0);
}
// guidialog_settings::guidialog_settings(ivec2 _dialogSize, bool _resizeable) : guidialog_base(_dialogSize, _resizeable) {
//     init();
// }
guidialog_settings::guidialog_settings(DawInstance* daw)
    : guidialog_base(ivec2{640, 760}, true)
{
    init(daw);
}
void guidialog_settings::addEntry(setting_dialog* ctr, String title) {
    auto* entry = new guidialog_settings::dialog_entry{ctr, title};
    guictr_base::add(&entry->tabButton);
    this->entries.push_back(entry);
}
void guidialog_settings::setActiveEntry(int32_t idx) {
    if (idx >= 0 && idx < CtrSize(entries)) {
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
    ivec2 buttonPos = { inset, inset * 2 };
    int32_t buttonW = csW / 5;
    for (auto* entry : entries) {
        entry->tabButton.pos  = buttonPos;
        entry->tabButton.size = ivec2(buttonW, HEIGHT_DEFAULT_INPUT);
        entry->tabButton.layout();
        buttonPos.y += HEIGHT_DEFAULT_INPUT + inset;
    }
    ivec2 sizeContentTab = ivec2(csW - buttonW - inset * 2, csize.y - inset * 2);

    auto entry = this->activeEntry;
    if (entry) {
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

    switch (button->id) {
        case ID_BTN_CLOSE:
            closeContextMenu();
            break;
    }
}

} // namespace