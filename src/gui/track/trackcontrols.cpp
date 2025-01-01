#include "trackcontrols.h"

#include "assert_dbg.h"
#include "fileio.h"
#include "gui/automation/modulation.h"
#include "gui/container/container.h"
#include "gui/controls/filebrowser.hpp"
#include "host/audio_config.h"
#include "host/clip/clip.h"
#include "config.h"
#include "gui/views/pluginlist.h"
#include "guiglobals.h"
#include "host/daw_channel.h"
#include "host/plugin/base/base-plugin.h"
#include "keyboard.h"
#include "logging.h"
#include "math/seq_math.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/vst/vstplugin.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "platform.h"
#include "saferef.h"
#include "seq_util.h"
#include "theme.h"
#include "tls.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/controls/textfield.h"
#include "gui/controls/button.h"
#include "event.h"
#include "renderresources.h"
#include "trackautomation.h"
#include "trackcontent.h"
#include "gui/dropdown/dropdown.h"
#include "gui/dropdown/dropdown_generic.h"
#include "dsp_util.h"
#include "str_util.h"
#include "gui/meter/guimeter.h"
#include "color_util.h"
#include "host/automation/automation.h"
#include "gui/automation/automatable.h"
#include "subtrack.h"
#include "host/meter/meter.h"
#include "gui/tooltip/tooltip.h"
#include "appsettings.h"
#include "host/audiohost/audio_host.h"
#include "host/project/projectcontroller.h"
#include "gui/meter/guimeter.h"
#include "host/track/trackctr_types.h"
#include "gui/plugin/pluginctr.h"
#include "host/host_pluginmanager.h"
#include "window.h"

using namespace DAW::AudioIO;
using DAW::bus_type;
using DAW::stage_bufferpoint;
using DAW::channel_ref_t;
using DAW::midichannel_ref_t;

namespace DAW {
    void OpenFloatingTextInput(AppCtrl* ctrl, ivec2 popupPos, ivec2 popupSize, const String& initialStr, const std::function<bool(const String& str)>& callback) {
        const int titleHeight = ctrl->getTheme()->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

        auto const field = new gui_textfield();
        field->size      = popupSize;
        field->size.y    = titleHeight;
        field->pos       = { 0, 0 };
        field->setFontSize(titleHeight);
        field->setReturnCommits(true);

        auto const ctxtMenu = new guictxtmenu_base();
        ctxtMenu->size      = field->size;
        ctxtMenu->add(field);
        ctxtMenu->layout();
        ctxtMenu->canTakeInputFocus = true;
        ctxtMenu->maxHeight         = field->size.y;
        dbgassert(!ctxtMenu->isBackgroundRendered());
        ctxtMenu->setBackgroundRendered(false);
        field->setEndEditCallback(callback);
        ctrl->openOverlayGui(ctxtMenu, popupPos, WINDOW_POS_RELATIVE | WINDOW_IS_BORDERLESS | WINDOW_IS_FOCUSED);
        // m_trackentry is not valid here
        field->setValue(initialStr);
        field->setSelectionRange(-1, -1);
        field->parentCtrl->focusGui(field);
    }
    void OpenRenameTrackPopup(DawCtrl* ctrl, track_gui_entry_t* trackentry) {
        auto cb = [trackEntry = trackentry](const String& str) {
            trackEntry->track->name = str;
            return false;
        };
        guibase* title = nullptr;
        if (trackentry->trackControls) {
            title = trackentry->trackControls->getTitle();
        }
        if (trackentry->trackMixerTitle) {
            title = trackentry->trackMixerTitle;
        }
        if (!title) {
            return;
        }
        auto popupPos    = title->toScreenSpace(ivec2(0));
        OpenFloatingTextInput(ctrl, popupPos, title->size, trackentry->track->name, cb);
    }
    bool OpenRenameAbsoluteFilePopup(AppCtrl* ctrl, ivec2 popupPos, ivec2 popupSize, const String& pathAbs, std::function<bool(const String& str)> callback) {
        if (pathAbs.empty()) {
            return false;
        }
        String pathOnly, nameOnly, ext;
        SplitPath(pathAbs, &pathOnly, &nameOnly, &ext);
        App::Platform::sanitizePathToDirectory(pathOnly);
        auto cb = [=](const String& newFileName) {
            if (newFileName.empty()) {
                return false;
            }
            if (pathOnly.empty()) {
                return false;
            }
            // strip file extension
            String newFileNameNoExt;
            SplitPath(newFileName, nullptr, &newFileNameNoExt, nullptr);
            if (newFileNameNoExt.empty()) {
                return false;
            }
            String newPath = pathOnly + newFileNameNoExt + "." + ext;
            if (FileExists(newPath)) {
                return false;
            }
            if (!MoveAbsoluteFile(pathAbs, newPath)) {
                return false;
            }
            callback(newPath);
            return false;
        };
        OpenFloatingTextInput(ctrl, popupPos, popupSize, nameOnly, cb);
        return true;
    }
}
int trackHeight(track_gui_entry_t* const m_trackentry) {
    int trackheight = m_trackentry->layout.height;
    for (auto t2 : m_trackentry->subtracks) {
        trackheight += t2->height;
    }
    return trackheight;
}
bool addTrHeight(track_gui_entry_t* const m_trackentry, int32_t offset) {

    bool changed  = false;
    int maxHeight = m_trackentry->subtracks.size() ? 4 : TRACK_MAX_HEIGHT;
    if (offset > 0 && m_trackentry->layout.height < maxHeight) {
        m_trackentry->layout.height++;
        return true;
    }
    for (auto t2 : m_trackentry->subtracks) {
        int32_t nHeight = math::min(TRACK_MAX_HEIGHT_SUB, math::max(TRACK_MIN_HEIGHT_SUB, t2->height + offset));
        changed         = nHeight != t2->height;
        t2->height      = nHeight;
    }
    if (offset < 0 && !changed) {
        int32_t nHeight = math::min(TRACK_MAX_HEIGHT_SUB, math::max(2, m_trackentry->layout.height + offset));
        changed |= nHeight != m_trackentry->layout.height;
        m_trackentry->layout.height = nHeight;
    }
    return changed;
}
template<typename T, int minHeight = TRACK_MIN_HEIGHT_SUB, int maxHeight = TRACK_MAX_HEIGHT_SUB>
void resize(track_gui_entry_t* const m_trackentry, T* al, int32_t mouseDragDist, int32_t heightStep) {

    if (m_trackentry->track->type < TRACK_TYPE_MIDI) {
        //resize content-lane on bottom-sticked tracks
        int32_t adjustedHeightSteps = math::min(128, math::max(1, (mouseDragDist) / heightStep));
        if (!m_trackentry->subtracks.empty()) {
            int32_t curHeightSteps = trackHeight(m_trackentry);
            int32_t distSteps      = adjustedHeightSteps - al->layout.height;
            if (distSteps && curHeightSteps != al->layout.height) {
                while (distSteps) {
                    int32_t distStepsBef = distSteps;
                    for (auto t2 : m_trackentry->subtracks) {
                        if (distSteps > 0 && t2->height > TRACK_MIN_HEIGHT_SUB && al->layout.height < maxHeight) {
                            al->layout.height++;
                            t2->height--;
                            distSteps--;
                        }
                        if (distSteps < 0 && t2->height < TRACK_MAX_HEIGHT_SUB && al->layout.height > minHeight) {
                            al->layout.height--;
                            t2->height++;
                            distSteps++;
                        }
                        if (!distSteps) {
                            break;
                        }
                    }
                    if (distStepsBef == distSteps) {
                        break;
                    }
                }
            }
        }
    } else {

        int32_t totalHeightSteps = math::min(maxHeight, math::max(minHeight, (mouseDragDist) / heightStep));
        al->layout.height        = totalHeightSteps;
    }
}

class gui_subtrack_waveview;

class ctxtmenu_entry_bus : public ctxtmenu_entry_track_io {
public:
    const bus_type busType;
    const String busName;
    const audio_channel_ref_t stageEndpoint;

    ctxtmenu_entry_bus(int32_t _id, const String& name, bus_type bustype, audio_channel_ref_t _stageEndpoint)
        : ctxtmenu_entry_track_io(_id, name),
          busType(bustype),
          busName(name),
          stageEndpoint(_stageEndpoint) {
    }

    bool isBus() override {
        return true;
    }
};
class ctxtmenu_entry_bus_external final : public ctxtmenu_entry_bus {
public:
    ctxtmenu_entry_bus_external(int32_t _id, const String& name, audio_channel_ref_t _stageEndpoint)
        : ctxtmenu_entry_bus(_id, name, bus_type::external, _stageEndpoint) {
    }
};
class ctxtmenu_entry_bus_internal final : public ctxtmenu_entry_bus {
    const audio_stage_ref_t busStage;

public:
    ctxtmenu_entry_bus_internal(int32_t _id, const String& name, audio_stage_ref_t _stageBus, audio_channel_ref_t _stageEndpoint)
        : ctxtmenu_entry_bus(_id, name, bus_type::internal, _stageEndpoint), busStage(_stageBus) {
    }
    audio_stage_ref_t getStageRef() {
        return busStage;
    }
};


class ctxtmenu_entry_endpoint : public ctxtmenu_entry_track_io {
public:
    ctxtmenu_entry_endpoint(int32_t _id, const String& name) : ctxtmenu_entry_track_io(_id, name) {
    }
    virtual channel_ref_t getEndpoint() = 0;
};

class ctxtmenu_entry_external_channel final : public ctxtmenu_entry_endpoint {
public:
    const io_cfg_channel channel;
    const stage_bufferpoint isInput;

    explicit ctxtmenu_entry_external_channel(int32_t _id, const io_cfg_channel& _channel, stage_bufferpoint _isInput)
        : ctxtmenu_entry_endpoint(_id, _channel.name),
          channel(_channel),
          isInput(_isInput) {
    }
    explicit ctxtmenu_entry_external_channel(int32_t _id, const String& name, stage_bufferpoint _isInput)
        : ctxtmenu_entry_endpoint(_id, name), channel(), isInput(_isInput) {
    }
    void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
        ctxtmenu_entry_track_io::render(ctxtSize, vg, idx, mouse);

        if (channel.idx > -1) {
            // auto* stream = audiohost::getInstance()->getStream(0);
            // if (stream) {
                // ivec2 sizeMeter{ height - 2, height - 2 };
                // renderMeterAt(vg, theme, { width - sizeMeter.x + 1, y + 1 }, sizeMeter, &rmsMtr);
            // }
        }
    }
    bool isBus() override {
        return false;
    }
    channel_ref_t getEndpoint() override {
        if (isInput == stage_bufferpoint::INPUT)
            return DAW::ChannelAudioInput(channel.idx,
                                          channel.offset,
                                          "External " + getExternalIOName(channel.type, channel.idx, isInput),
                                          channel.type);
        else
            return DAW::ChannelAudioOutput(channel.idx,
                                           channel.offset,
                                           "External " + getExternalIOName(channel.type, channel.idx, isInput),
                                           channel.type);
    }
};
class ctxtmenu_entry_stage_channel final : public ctxtmenu_entry_endpoint {
public:
    const audio_channel_ref_t endpoint;
    channelnum_t dstChannelOffset;

    ctxtmenu_entry_stage_channel(int32_t _id, const String& name, audio_channel_ref_t _endpoint, channelnum_t _dstChannelOffset = 0)
        : ctxtmenu_entry_endpoint(_id, name), endpoint(_endpoint), dstChannelOffset(_dstChannelOffset) {
    }
    void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
        ctxtmenu_entry_track_io::render(ctxtSize, vg, idx, mouse);
        auto daw = DawInstance::get();
        audio_stage_t* stage = daw->getPluginManager()->getAudioStage(endpoint.stageRef);
        if (stage) {
 
        }
    }

    bool isBus() override {
        return false;
    }

    channel_ref_t getEndpoint() override {
        auto daw = DawInstance::get();
        audio_stage_t* stage = daw->getPluginManager()->getAudioStage(endpoint.stageRef);
        if (stage) {
            track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
            dbgassert(trImpl);
            if (trImpl) {
                return DAW::ChannelStage(trImpl, endpoint.buffer, 0, dstChannelOffset);
            }
        }
        return DAW::ChannelNone();
    }
};
class ctxtmenu_entry_default_channel final : public ctxtmenu_entry_endpoint {
public:
    ctxtmenu_entry_default_channel(int32_t _id, const String& name)
        : ctxtmenu_entry_endpoint(_id, name) {
    }

    bool isBus() override {
        return false;
    }

    channel_ref_t getEndpoint() override {
        return DAW::ChannelDefaultNone();
    }
};

/* top select menu */
class guidropdown_select_bus_ctxt final : public guictxtmenu {
    const audio_stage_ref_t busStage;
    const audio_channel_ref_t stageEndpoint;

public:
    guidropdown_select_bus_ctxt(DawCtrl * _dawCtrl, audio_stage_ref_t _busStage, audio_channel_ref_t _dstStage)
        : busStage(_busStage),
          stageEndpoint(_dstStage)
    {
        this->dawCtrl = _dawCtrl;
        int32_t idx = 0;
        if (_dstStage.buffer != stage_bufferpoint::INPUT) {
            addEntry(new ctxtmenu_entry_stage_channel(idx++, "Input 0", audio_channel_ref_t{ _busStage, stage_bufferpoint::INPUT }, 0));
            addEntry(new ctxtmenu_entry_stage_channel(idx++, "Input 1", audio_channel_ref_t{ _busStage, stage_bufferpoint::INPUT }, 2));
        } else {
            addEntry(new ctxtmenu_entry_stage_channel(idx++, "Output", audio_channel_ref_t{ _busStage, stage_bufferpoint::OUTPUT_POST }, 0));
        }
        /* auto host = _dawCtrl->getDaw()->getPluginManager();
        auto stage = host->getAudioStage(_busStage);
        if (stage) {
            auto track =  stage->getTrack();
            if (track) {
                auto& childTracks = track->children;
                for (track_t* childTrack : childTracks) {
                    dbgassert(childTrack->audio);
                    addEntry(new ctxtmenu_entry_bus_internal(idx, childTrack->name, childTrack->audio->toRef(), stageEndpoint));
                    idx++;
                }
            }
        } */
    }
    guidropdown_select_bus_ctxt(DawCtrl * _dawCtrl, const io_cfg_tracks& cfg, audio_channel_ref_t _dstStage)
        : busStage(AudioStageRefNULL()),
          stageEndpoint(_dstStage) 
    {
        this->dawCtrl = _dawCtrl;
        int32_t idx = 0;
        auto& list  = stageEndpoint.buffer == stage_bufferpoint::INPUT ? cfg.input : cfg.output;
        for (auto& channel : list) {
            addEntry(new ctxtmenu_entry_external_channel(idx, channel, _dstStage.buffer));
            idx++;
        }
    }

    explicit guidropdown_select_bus_ctxt(DawCtrl * _dawCtrl, audio_channel_ref_t _stageEndpoint, int lvl = 0)
        : busStage(AudioStageRefNULL()),
          stageEndpoint(_stageEndpoint)
    {
        this->dawCtrl = _dawCtrl;
        int32_t idx      = 0;
        String inputName = stageEndpoint.buffer == stage_bufferpoint::INPUT ? "External input" : "External output";
        addEntry(new ctxtmenu_entry_stage_channel(idx++, "None", AudioChannelRefNULL()));
        addEntry(new ctxtmenu_entry_default_channel(idx++, "Default"));
        addEntry(new ctxtmenu_splitter());
        addEntry(new ctxtmenu_entry_bus_external(idx++, inputName, stageEndpoint));
        addEntry(new ctxtmenu_splitter());

        project_t* project = dawCtrl->getDaw()->getProject();
        dbgassert(project);
        if (project) {
            auto& tracks = project->trackList;
            for (track_t* track : tracks) {
                dbgassert(track->audio);
                addEntry(new ctxtmenu_entry_bus_internal(idx, track->name, track->audio->toRef(), stageEndpoint));
                idx++;
            }
        }
    }

    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        auto const ctxtEndpointEntry = static_cast<ctxtmenu_entry_track_io*>(e);
        if (ctxtEndpointEntry->isBus()) {
            return false;
        }
        dbgassert(dynamic_cast<ctxtmenu_entry_endpoint*>(e));
        auto const entry = static_cast<ctxtmenu_entry_endpoint*>(e);
        auto const stage = dawCtrl->getDaw()->getPluginManager()->getAudioStage(stageEndpoint.stageRef);
        if (!stage)
            return true;
        auto const trImpl = dynamic_cast<track_impl_t*>(stage);
        dbgassert(trImpl);
        if (!assert_expr(trImpl))
            return true;
        if (stageEndpoint.buffer == stage_bufferpoint::INPUT) {
            trImpl->inputChannel = entry->getEndpoint();
        } else {
            trImpl->outputChannel = entry->getEndpoint();
        }
        dawCtrl->closeAllContextMenus();
        return true;
    }


    guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override {
        guictxtmenu* popup = nullptr;
        auto entry = dynamic_cast<ctxtmenu_entry_bus*>(e);
        if (entry) {
            if (entry->busType == bus_type::internal) {
                auto stageEntry = dynamic_cast<ctxtmenu_entry_bus_internal*>(entry);
                dbgassert(stageEntry);
                if (stageEntry) {
                    popup = new guidropdown_select_bus_ctxt(dawCtrl, stageEntry->getStageRef(), stageEndpoint);
                }
            }
            if (entry->busType == bus_type::external) {
                auto& settings = daw_tls::getSettings();
                auto& cfg = settings.iosettings.getChannelConfig(settings.iosettings.device_api);
                popup     = new guidropdown_select_bus_ctxt(dawCtrl, cfg, stageEndpoint);
            }
        }
        return popup;
    }
};

class guidropdown_select_bus final : public guidropdownbase {
    track_t* const track;
    const bool isInput;

public:
    guidropdown_select_bus(track_gui_entry_t* _entry, const bool _isInput) : guidropdownbase(), track(_entry->track), isInput(_isInput) {
    }
    String getString() override {
        track_impl_t* trImpl = track->audio;
        dbgassert(trImpl);
        if (trImpl) {
            auto& channel      = isInput ? trImpl->inputChannel : trImpl->outputChannel;
            project_t* project = dawCtrl->getDaw()->getProject();
            dbgassert(project);
            if (project) {
                if (channel.type == DAW::stage_type::INPUT_DEFAULT) {
                    DAW::channel_ref_t out;
                    if (DAW::resolveDefaultConnection(dawCtrl->getDaw()->getPluginManager(), project, trImpl, isInput, out)) {
                        return out.name;
                    }
                    return "Default";
                }
            }
            return channel.name;
        }
        return "<Invalid Track>";
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        track_impl_t* trImpl = track->audio;
        dbgassert(trImpl);
        if (!trImpl)
            return;
        auto stageBufferPoint = isInput ? stage_bufferpoint::INPUT : stage_bufferpoint::OUTPUT_POST;
        auto* popup = new guidropdown_select_bus_ctxt(dawCtrl, audio_channel_ref_t{ trImpl->toRef(),  stageBufferPoint});
        popup->size             = size;
        popup->setFontSize(size.y);
        popup->size.x = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
        auto posScreen = parentCtrl->toScreenSpace(toScreenSpace({0, size.y}) + ivec2(0, 1));
        this->dawCtrl->openAppMenu(0, popup, posScreen, WINDOW_IS_BORDERLESS | WINDOW_POS_ABSOLUTE);
    }
};

class ctxtmenu_entry_midi_endpoint : public ctxtmenu_entry_track_io {
public:
    ctxtmenu_entry_midi_endpoint(int32_t _id, const String& name) : ctxtmenu_entry_track_io(_id, name) {
    }
    virtual midichannel_ref_t getEndpoint() = 0;
};
class ctxtmenu_entry_external_midi_channel final : public ctxtmenu_entry_midi_endpoint {
public:
    const midi_channel channel;
    const stage_bufferpoint isInput;

    explicit ctxtmenu_entry_external_midi_channel(int32_t _id, const midi_channel& _channel, stage_bufferpoint _isInput)
        : ctxtmenu_entry_midi_endpoint(_id, _channel.deviceName),
          channel(_channel),
          isInput(_isInput) {
    }
    bool isBus() override {
        return false;
    }
    midichannel_ref_t getEndpoint() override {
        return DAW::MidiChannelExternal(channel.idx,
                                      channel.deviceName);
    }
};
class ctxtmenu_entry_stage_midi_channel final : public ctxtmenu_entry_midi_endpoint {
public:
    const audio_channel_ref_t endpoint;

    ctxtmenu_entry_stage_midi_channel(int32_t _id, const String& name, audio_channel_ref_t _endpoint)
        : ctxtmenu_entry_midi_endpoint(_id, name), endpoint(_endpoint) {
    }

    bool isBus() override {
        return false;
    }

    midichannel_ref_t getEndpoint() override {
        audio_stage_t* stage = DawInstance::get()->getPluginManager()->getAudioStage(endpoint.stageRef);
        if (stage) {
            return DAW::MidiChannelStage(stage, endpoint.buffer);
        }
        return DAW::MidiChannelNone();
    }
};
class ctxtmenu_entry_default_midi_channel final : public ctxtmenu_entry_midi_endpoint {
public:
    ctxtmenu_entry_default_midi_channel(int32_t _id, const String& name)
        : ctxtmenu_entry_midi_endpoint(_id, name) {
    }

    bool isBus() override {
        return false;
    }

    midichannel_ref_t getEndpoint() override {
        return DAW::MidiChannelDefault();
    }
};

class guidropdown_select_midi_ctxt final : public guictxtmenu {
    const audio_stage_ref_t busStage;
    const audio_channel_ref_t stageEndpoint;

public:
    guidropdown_select_midi_ctxt(DawCtrl * _dawCtrl, audio_stage_ref_t _busStage, audio_channel_ref_t _dstStage)
        : busStage(_busStage),
          stageEndpoint(_dstStage)
    {
        this->dawCtrl = _dawCtrl;
        int32_t idx = 0;
        addEntry(new ctxtmenu_entry_stage_midi_channel(idx++, "Pre", audio_channel_ref_t{ _busStage, stage_bufferpoint::INPUT }));
        addEntry(new ctxtmenu_entry_stage_midi_channel(idx++, "Post", audio_channel_ref_t{ _busStage, stage_bufferpoint::OUTPUT_POST }));
        // audio_stage_t* stage = dawCtrl->getDaw()->getPluginManager()->getAudioStage(stageEndpoint.stageRef);
        // if (stage) {
        //     track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
        //     dbgassert(trImpl);
        //     if (trImpl) {
        //         dbgassert(trImpl->getTrack());
        //         auto& childTracks = trImpl->getTrack()->children;
        //         for (track_t* childTrack : childTracks) {
        //             dbgassert(childTrack->audio);
        //             addEntry(new ctxtmenu_entry_bus_internal(idx, childTrack->name, childTrack->audio->toRef(), stageEndpoint));
        //             idx++;
        //         }
        //     }
        // }
    }
    guidropdown_select_midi_ctxt(DawCtrl * _dawCtrl, const app_iomidiconfig& midiCfg, audio_channel_ref_t _dstStage)
        : busStage(AudioStageRefNULL()),
          stageEndpoint(_dstStage) 
    {
        this->dawCtrl = _dawCtrl;
        auto& list  = stageEndpoint.buffer == stage_bufferpoint::INPUT ? midiCfg.inputs : midiCfg.outputs;
        String labelAll = stageEndpoint.buffer == stage_bufferpoint::INPUT ? "All Inputs" : "All Outputs";
        addEntry(new ctxtmenu_entry_external_midi_channel(0, {255, labelAll, {}}, _dstStage.buffer));
        int32_t idx = 0;
        for (auto& channel : list) {
            addEntry(new ctxtmenu_entry_external_midi_channel(idx + 1, {idx, channel.deviceName, {}}, _dstStage.buffer));
            idx++;
        }
    }

    explicit guidropdown_select_midi_ctxt(DawCtrl * _dawCtrl, audio_channel_ref_t _stageEndpoint, int lvl = 0)
        : busStage(AudioStageRefNULL()),
          stageEndpoint(_stageEndpoint)
    {
        this->dawCtrl = _dawCtrl;
        int32_t idx      = 0;
        String inputName = stageEndpoint.buffer == stage_bufferpoint::INPUT ? "External input" : "External output";
        addEntry(new ctxtmenu_entry_stage_midi_channel(0, "None", AudioChannelRefNULL()));
        addEntry(new ctxtmenu_entry_default_midi_channel(1, "Default"));
        addEntry(new ctxtmenu_entry_default_midi_channel(3, "Custom"));
        addEntry(new ctxtmenu_splitter());
        addEntry(new ctxtmenu_entry_bus_external(2, inputName, stageEndpoint));
        addEntry(new ctxtmenu_splitter());
        project_t* project = dawCtrl->getDaw()->getProject();
        dbgassert(project);
        if (project) {
            auto& tracks = project->trackList;
            for (track_t* track : tracks) {
                dbgassert(track->audio);
                addEntry(new ctxtmenu_entry_bus_internal(idx, track->name, track->audio->toRef(), stageEndpoint));
                idx++;
            }
        }
    }

    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (e->id == 3) {
            dawCtrl->closeAllContextMenus();
            return true;
        }
        auto const ctxtEndpointEntry = static_cast<ctxtmenu_entry_track_io*>(e);
        if (ctxtEndpointEntry->isBus()) {
            return false;
        }
        dbgassert(dynamic_cast<ctxtmenu_entry_midi_endpoint*>(e));
        auto const entry = static_cast<ctxtmenu_entry_midi_endpoint*>(e);
        auto const stage = dawCtrl->getDaw()->getPluginManager()->getAudioStage(stageEndpoint.stageRef);
        if (!stage)
            return true;
        auto const trImpl = dynamic_cast<track_impl_t*>(stage);
        dbgassert(trImpl);
        if (!assert_expr(trImpl))
            return true;
        if (stageEndpoint.buffer == stage_bufferpoint::INPUT) {
            auto ep = entry->getEndpoint();
            auto lock = dawCtrl->lockPlayThread();
            trImpl->midiInputChannels.clear();
            trImpl->midiInputChannels.push_back(ep);
        } else {
            // trImpl->outputChannel = entry->getEndpoint();
        }
        dawCtrl->closeAllContextMenus();
        return true;
    }

    guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override {
        guictxtmenu* popup = nullptr;
        auto entry         = dynamic_cast<ctxtmenu_entry_bus*>(e);
        if (entry) {
            if (entry->busType == bus_type::internal) {
                auto stageEntry = dynamic_cast<ctxtmenu_entry_bus_internal*>(entry);
                dbgassert(stageEntry);
                if (stageEntry) {
                    popup = new guidropdown_select_midi_ctxt(dawCtrl, stageEntry->getStageRef(), stageEndpoint);
                }
            }
            if (entry->busType == bus_type::external) {
                auto& settings     = daw_tls::getSettings();
                auto& midiSettings = settings.iosettings.getIOConfigMidi("stdmidi");
                popup              = new guidropdown_select_midi_ctxt(dawCtrl, midiSettings, stageEndpoint);
            }
        }
        return popup;
    }
};

class guidropdown_select_midi_input final : public guidropdownbase {
    track_t* const track;
    const bool isInput;

public:
    guidropdown_select_midi_input(track_gui_entry_t* _entry, const bool _isInput) : guidropdownbase(), track(_entry->track), isInput(_isInput) {
    }
    String getString() override {
        track_impl_t* trImpl = track->audio;
        dbgassert(trImpl);
        if (trImpl && trImpl->midiInputChannels.size()) {
            return trImpl->midiInputChannels.front().name;
        }
        return "<Invalid Track>";
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        track_impl_t* trImpl = track->audio;
        dbgassert(trImpl);
        if (!trImpl)
            return;
        auto stageBufferPoint = isInput ? stage_bufferpoint::INPUT : stage_bufferpoint::OUTPUT_POST;
        auto* popup = new guidropdown_select_midi_ctxt(dawCtrl, audio_channel_ref_t{ trImpl->toRef(),  stageBufferPoint});
        popup->size             = size;
        popup->setFontSize(size.y);
        popup->size.x = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
        this->dawCtrl->openAppMenu(0, popup, toScreenSpace({0, size.y}) + ivec2(0, 1), WINDOW_IS_BORDERLESS | WINDOW_POS_RELATIVE);
    }
};
class gui_trackcontrols_io final : public guictr_base {
    guidropdown_select_bus selectOutput;
    guidropdown_select_bus selectInput;
    guidropdown_select_midi_input selectMidiInput;

public:
    explicit gui_trackcontrols_io(track_gui_entry_t* _entry)
        : guictr_base(),
          selectOutput(_entry, false),
          selectInput(_entry, true),
          selectMidiInput(_entry, true) {
        add(&selectOutput);
        add(&selectInput);
        add(&selectMidiInput);
        selectInput.setLabel("Audio In");
        selectOutput.setLabel("Audio Out");
        selectMidiInput.setLabel("Midi In");
        padding = 0;
    }
    ~gui_trackcontrols_io() override {
        remove(&selectMidiInput);
        remove(&selectInput);
        remove(&selectOutput);
    }
    void layout() override {
        const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);

        int32_t inset      = CONST_PADDING_TRACK_CONTROLS;
        selectOutput.pos    = ivec2(inset, inset);
        selectInput.pos   = ivec2(inset, TRACK_HEIGHT_STEP + inset);
        selectMidiInput.pos   = ivec2(inset, (TRACK_HEIGHT_STEP + inset)*2);
        selectInput.size   = getSizeContent() - ivec2(inset * 2);
        selectInput.size.y = TRACK_HEIGHT_STEP - inset * 2;
        selectOutput.size  = selectInput.size;
        selectMidiInput.size  = selectInput.size;
        for (auto gui : guis) {
            gui->layout();
        }
    }

    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto gui : guis) {
            if (gui->isVisible()) {
                gui->render(vg);
            }
        }
    }
};
class gui_trackcontrols_mixer final : public guictr_base {
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;
    DAW::rmsmeter m_subMeter;
    gui_trackmeter m_guiMeter;

public:
    gui_slider_gain trackGain;
    gui_slider_pan trackPanning;
    guibutton_trackbypass btnBypass;
    guibutton_track_solo btnSolo;
    guibutton_track_record_arm btnRecord;
    guibutton btnActivate;
    std::vector<gui_slider_gain*> sendGains;
    std::vector<gui_slider_pan*> sendPans;
    explicit gui_trackcontrols_mixer(track_gui_entry_t* _entry)
        : guictr_base(),
          m_track(_entry->track),
          m_trackentry(_entry),
          m_subMeter(_entry->track->audio->meter.getSubChannelMeter(0, 2)),
          m_guiMeter(&m_subMeter),
          btnBypass(_entry),
          btnSolo(_entry) ,
          btnRecord(_entry) {
        (void) m_trackentry;
        trackGain.setAutomationRef(&m_track->audio->mixer, PARAM_TRACK_GAIN);
        trackPanning.setAutomationRef(&m_track->audio->mixer, PARAM_TRACK_PAN);
        padding            = 0;
        btnBypass.drawFn   = drawTextureSymbol;
        btnBypass.drawParm = ICON_BYPASS;
        btnBypass.setFlag(FLG_RENDER_BUTTON_WITH_LED, true);
        btnActivate.drawFn   = drawTextureSymbol;
        btnActivate.drawParm = ICON_EFFECT;
        trackGain.setLabel("Gain Level");
        trackPanning.setLabel("Pan");
        btnActivate.setLabel("Load plugins");
        add(&btnBypass);
        add(&btnSolo);
        add(&btnRecord);
        add(&btnActivate);
        add(&trackGain);
        add(&trackPanning);
        add(&m_guiMeter);
        if (m_track->type != TRACK_TYPE_MASTER && m_track->type != TRACK_TYPE_RETURN) {
            sendGains.resize(MAX_SEND_CHANNELS);
            sendPans.resize(MAX_SEND_CHANNELS);
            for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
                sendGains[i] = new gui_slider_gain();
                sendGains[i]->setVisible(false);
                sendGains[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND_GAIN + i);
                sendGains[i]->setLabel(StringFormat("Send %d", i + 1));
                sendGains[i]->setFlag(FLG_RENDER_LABEL, true);
                sendPans[i] = new gui_slider_pan();
                sendPans[i]->setVisible(false);
                sendPans[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND_PAN + i);
                sendPans[i]->setLabel("Pan");
                add(sendGains[i]);
                add(sendPans[i]);
            }
        }
    }
    ~gui_trackcontrols_mixer() override {
        for (auto* sendGainCtrl : sendGains) {
            remove(sendGainCtrl);
            delete sendGainCtrl;
        }
        for (auto* sendPanCtrl : sendPans) {
            remove(sendPanCtrl);
            delete sendPanCtrl;
        }
        remove(&m_guiMeter);
        remove(&trackPanning);
        remove(&trackGain);
        remove(&btnActivate);
        remove(&btnRecord);
        remove(&btnSolo);
        remove(&btnBypass);
    }
    void buttonClicked(guibase* button) override {
        auto const daw = dawCtrl->getDaw();
        ThreadLock lock = daw->lockPlayThread();
        if (&btnSolo == button) {
            bool isSolo = (m_track->audio->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE;
            if (!isShift(parentCtrl->lastMouseEvent.kbmods)) {
                daw->unsoloAll();
            }
            daw->setSoloState(m_track->audio->toRef(), !isSolo);
        }
        if (&btnRecord == button) {
            bool isArmed = (m_track->audio->flags & audiostageflags_t::RECORD_ARMED) != audiostageflags_t::NONE;
            daw->setTrackArmed(m_track->audio->toRef(), !isArmed);
        }
        if (&btnBypass == button) {
            track_params_t& trackParams = m_track->audio->mixer;
            auto fNew = float(!trackParams.isEnabled());
            auto flags = FLG_PAR_UPDATE_FINISH | FLG_PAR_UPDATE_USER;
            trackParams.setParamEdit(PARAM_ENABLE, fNew, flags);
        }
        if (&btnActivate == button) {
            auto pluginMgr = daw->getPluginManager();
            std::vector<effectbase*> effects;
            m_track->audio->getDeferredEffects(effects);
            for (auto effect : effects) {
                pluginMgr->activateDeferred(effect, 0);
            }
            daw->onPluginsChanged();
            onChildLayoutChanged(button);

#ifndef NDEBUG
            log_printf("deferredEffects post activateDeferred on track %s: %zu\n", m_track->szName, m_track->audio->deferredEffects.size());
#endif
        }
    }
    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            if (gui->isVisible()) {
                gui->onTick(ctrl);
            }
        }
    }
    void layout() override {

        std::vector<effectbase*> effects;
        dbgassert(m_track->audio);
        m_track->audio->getDeferredEffects(effects);
        int nDefEffects = CtrSize(effects);
        btnActivate.setEnabled(nDefEffects > 0);
        auto str = nDefEffects > 9 ? "9+" : (StringFormat("%d", nDefEffects));
        // btnActivate.setText(str);
        btnActivate.setLabel("Load "+str+" deferred plugins");
        btnActivate.setVisible(nDefEffects > 0);

        const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
        const int32_t mW = theme->get(GuiConstant::CONST_METER_WIDTH);
        const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

        int32_t inset = CONST_PADDING_TRACK_CONTROLS;
        int32_t i2    = inset * 2;
        m_guiMeter.size = ivec2(mW - i2, size.y - i2);
        m_guiMeter.pos  = ivec2(size.x - mW + inset, inset);


        int32_t heightInner = TRACK_HEIGHT_STEP - i2;
        int32_t csX      = size.x - mW;
        int32_t rowY = 0;
        int32_t nButtons = btnActivate.isVisible() ? 3 : 2;
        btnBypass.size   = ivec2(csX - inset * 4 - heightInner * nButtons, heightInner);
        btnBypass.pos    = ivec2(inset, inset + rowY);
        btnSolo.pos      = ivec2(csX - inset - heightInner, inset + rowY);
        btnSolo.size     = ivec2(heightInner, heightInner);
        btnRecord.pos    = ivec2(csX - inset * 2 - heightInner * 2, inset + rowY);
        btnRecord.size   = ivec2(heightInner, heightInner);
        btnActivate.pos    = ivec2(csX - inset * 2 - heightInner * 3, inset + rowY);
        btnActivate.size   = ivec2(heightInner, heightInner);
        rowY += TRACK_HEIGHT_STEP;
        
        int32_t sendGainWidth = csX*4/5 - inset;
        trackGain.size       = ivec2(sendGainWidth, heightInner);
        trackGain.pos        = ivec2(inset, rowY + inset);
        trackPanning.size        = ivec2(csX - sendGainWidth - i2 - inset, heightInner);
        trackPanning.pos         = ivec2(i2 + sendGainWidth, rowY + inset);

        rowY += TRACK_HEIGHT_STEP;
        if (!sendGains.empty()) {
            const int32_t HEIGHT_SEND_GAIN = TRACK_HEIGHT_STEP;
            const int32_t SEND_PER_ROW     = 1;

            project_t* project = dawCtrl->getDaw()->getProject();
            dbgassert(project);
            int32_t numReturnChannels = project->trackReturnCtr.size();
            int pos                   = 0;
            ivec2 sendPos = { inset, inset + rowY };
            for (int32_t i = 0; i < numReturnChannels; ++i) {
                if (pos >= SEND_PER_ROW) {
                    pos = 0;
                    sendPos.x = inset;
                    sendPos.y += HEIGHT_SEND_GAIN;
                }
                sendGains[i]->size = ivec2(sendGainWidth, heightInner);
                sendGains[i]->pos  = sendPos;
                sendPos.x += sendGainWidth + inset;
                sendPans[i]->size = ivec2(csX - sendGainWidth - i2 - inset, heightInner);
                sendPans[i]->pos  = sendPos;
                sendPos.x += csX - sendGainWidth - i2 + inset;
                ++pos;
                // dbgassert(sendGains[i]->size.x > 0 && sendGains[i]->size.y > 0);
                // dbgassert(sendPans[i]->size.x > 0 && sendPans[i]->size.y > 0);
            }
            for (auto sendGainCtrl : sendGains) {
                auto idx = sendGainCtrl->getParamIdx() - PARAM_OFFSET_SEND_GAIN;
                sendGainCtrl->setVisible(idx < numReturnChannels);
            }
            for (auto sendPanCtrl : sendPans) {
                auto idx = sendPanCtrl->getParamIdx() - PARAM_OFFSET_SEND_PAN;
                sendPanCtrl->setVisible(idx < numReturnChannels);
            }
        }
        for (auto gui : guis) {
            gui->layout();
        }
    }

    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto gui : guis) {
            if (gui->isVisible()) {
                gui->render(vg);
            }
        }
    }
};


class guidropdown_popup_sel_automation_device final : public guictxtmenu {
    track_gui_entry_t* const m_trackentry;

public:
    explicit guidropdown_popup_sel_automation_device(DawCtrl* _dawCtrl, track_gui_entry_t* const trackentry)
        : m_trackentry(trackentry)
    {
        this->dawCtrl = _dawCtrl;
        this->size.x   = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        std::vector<automatable_t*> targets;
        trackentry->track->getStage()->getAutomatableTrackTargets(targets);
        int32_t idx = 0;
        addEntry(new ctxtmenu_entry("None", idx));
        idx++;
        for (auto t : targets) {
            addEntry(new ctxtmenu_entry(t->getAutomatableName(), idx));
            idx++;
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        std::vector<automatable_t*> targets;
        auto* trImpl = m_trackentry->track->getStage();
        trImpl->getAutomatableTrackTargets(targets);
        if (_id == 0) {
            m_trackentry->state.selectedAutomationCtr = nullptr;
        } else {
            _id--;
            if (_id >= 0 && _id < (int) targets.size()) {
                auto* atDevice                              = targets[_id];
                int32_t numParams                           = atDevice->getNumParameters();
                m_trackentry->state.selectedAutomationCtr   = atDevice;
                m_trackentry->state.selectedAutomationParam = numParams ? 0 : -1;
            }
        }
        dawCtrl->updateVisibleTrackContents();
        closeContextMenu();
        return true;
    }
};
class guidropdown_popup_sel_automation_param final : public guictxtmenu {
    track_gui_entry_t* const m_trackentry;
public:
    class ctxt_menu_entry_param final : public ctxtmenu_entry {
        bool m_automated;
    public:
        ctxt_menu_entry_param(int32_t _id, const String& name, bool automated)
            : ctxtmenu_entry(name, _id),
            m_automated(automated)
        {
            if (m_automated) {
                setIcon(&RenderResources::imgIcons[ICON_AUTOMATION], GuiColor::COL_AUTOMATED);
            }
        }
        ~ctxt_menu_entry_param() override = default;
        void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
            ctxtmenu_entry::render(ctxtSize, vg, idx, mouse);
            
        }
    };
    explicit guidropdown_popup_sel_automation_param(DawCtrl* _dawCtrl, track_gui_entry_t* const trackentry) : m_trackentry(trackentry) {
        this->dawCtrl  = _dawCtrl;
        this->size.x   = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;

        automatable_t* autom = m_trackentry->state.selectedAutomationCtr;
        addEntry(new ctxtmenu_entry("None", 0));
        if (autom) {
            std::vector<automatable_param_t*> paramsAutomated;
            std::vector<automatable_param_t*> paramsRest;
            autom->getSortedParamsSeperate(paramsAutomated, paramsRest, true);
            std::for_each(paramsAutomated.cbegin(), paramsAutomated.cend(), [this](const auto* param) {
                addEntry(new ctxt_menu_entry_param(1 + param->idx, param->name, true));
            });
            std::for_each(paramsRest.cbegin(), paramsRest.cend(), [this](const auto* param) {
                addEntry(new ctxt_menu_entry_param(1 + param->idx, param->name, false));
            });
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        m_trackentry->state.selectedAutomationParam = -1;
        if (_id > 0) {
            automatable_t* autom = m_trackentry->state.selectedAutomationCtr;
            if (autom) {
                const int32_t paramIdx = _id - 1;
                dbgassert(autom->getParam(paramIdx));
                m_trackentry->state.selectedAutomationParam = paramIdx;
            }
        }
        dawCtrl->getDaw()->updateVisibleTrackContents();
        closeContextMenu();
        return true;
    }
};
class guidropdown_automation_device final : public guidropdownbase {
    track_gui_entry_t* const m_trackentry;

public:
    explicit guidropdown_automation_device(track_gui_entry_t* const trackentry)
        : guidropdownbase(),
          m_trackentry(trackentry) {
    }
    String getString() override {
        automatable_t* automatable = m_trackentry->state.selectedAutomationCtr;
        return !automatable ? "None" : automatable->getAutomatableName();
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        guictxtmenu_base* popup = new guidropdown_popup_sel_automation_device(dawCtrl, m_trackentry);
        m_trackentry->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
};
class guidropdown_automation_param final : public guidropdownbase {
    track_gui_entry_t* const m_trackentry;

public:
    explicit guidropdown_automation_param(track_gui_entry_t* const trackentry)
        : guidropdownbase(),
          m_trackentry(trackentry) {
    }
    String getString() override {
        automatable_t* automatable = m_trackentry->state.selectedAutomationCtr;
        int32_t paramIdx           = m_trackentry->state.selectedAutomationParam;
        return !automatable || paramIdx < 0 ? "None" : automatable->getParamName(paramIdx);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        guictxtmenu_base* popup = new guidropdown_popup_sel_automation_param(dawCtrl, m_trackentry);
        popup->size.x           = 250;
        m_trackentry->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
};
class gui_trackcontrols_title final : public guictr_base {

    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;
    guidropdown_automation_device automationSelectDevice;
    guidropdown_automation_param automationSelectParam;
    guibuttontoggle hideTrack;
    guibuttontoggle hideAutomation;
    guibuttontoggle addAutomationLane;
    DragModeTrack dragMode = DragModeTrack::DRAG_TRACK_NONE;

public:
    explicit gui_trackcontrols_title(track_gui_entry_t* _entry)
        : guictr_base(),
          m_track(_entry->track),
          m_trackentry(_entry),
          automationSelectDevice(_entry),
          automationSelectParam(_entry) {
        setGuiType(gui_type::CTR_TYPE_TRACK_TITLE);
        setCanMouseHit(true);
        hideTrack.setRadius(12);
        hideAutomation.setRadius(10);
        addAutomationLane.setRadius(10);

        hideTrack.setStateRef(&_entry->layout.hideTrack);
        hideAutomation.setStateRef(&_entry->layout.hideSubtracks);
        padding                = 0;
        hideTrack.getIcon      = [e = _entry] { return e->layout.hideTrack ? ICON_ARR_RIGHT : ICON_ARR_DOWN; };
        hideAutomation.getIcon = [e = _entry] { return e->layout.hideSubtracks ? ICON_ARR_RIGHT : ICON_ARR_DOWN; };
        addAutomationLane.icon = ICON_PLUS;
        add(&hideTrack);
    }
    ~gui_trackcontrols_title() override {
        removeUNCHECKED(&hideAutomation);
        removeUNCHECKED(&hideTrack);
        removeUNCHECKED(&addAutomationLane);
        removeUNCHECKED(&automationSelectParam);
        removeUNCHECKED(&automationSelectDevice);
    }
    track_gui_entry_t* getTrackEntry() {
        return m_trackentry;
    }
    const track_gui_entry_t* getTrackEntry() const {
        return m_trackentry;
    }
    void layout() override {
        const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
        const int titleHeight             = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        //TODO: this is not optimal!
        removeUNCHECKED(&automationSelectParam);
        removeUNCHECKED(&automationSelectDevice);
        removeUNCHECKED(&hideAutomation);
        const int buttonRadius = (TRACK_HEIGHT_STEP - INSET_TRACK_CONTENT * 2) / 2;
        hideTrack.setRadius(buttonRadius);
        hideAutomation.setRadius(buttonRadius - 2);
        addAutomationLane.setRadius(buttonRadius - 2);
        int32_t inset    = CONST_PADDING_TRACK_CONTROLS;
        int32_t i2       = inset * 2;
        int32_t h        = TRACK_HEIGHT_STEP - i2;
        int32_t insetBtn = (TRACK_HEIGHT_STEP - hideAutomation.size.y) / 2;

        const int32_t hideTrIns = (titleHeight - hideTrack.size.y) / 2;
        hideTrack.pos           = ivec2(hideTrIns, hideTrIns);

        automationSelectParam.size  = ivec2(size.x - i2, h);
        automationSelectDevice.size = ivec2(size.x - i2, h);


        int32_t yCtrls = 0;
        int32_t hCtrls = size.y - TRACK_HEIGHT_STEP;
        if (hCtrls >= TRACK_HEIGHT_STEP * 3) {
            yCtrls += TRACK_HEIGHT_STEP;
            addUNCHECKED(&automationSelectDevice);
            addUNCHECKED(&automationSelectParam);
            addUNCHECKED(&hideAutomation);
            addUNCHECKED(&addAutomationLane);
            automationSelectDevice.pos = ivec2(inset, yCtrls + inset);
            automationSelectParam.pos  = ivec2(inset, yCtrls + TRACK_HEIGHT_STEP + inset);
            hideAutomation.pos         = ivec2(inset, size.y - TRACK_HEIGHT_STEP + insetBtn);
            addAutomationLane.pos      = ivec2(size.x - inset - addAutomationLane.size.x, size.y - TRACK_HEIGHT_STEP + insetBtn);
        } else if (hCtrls >= TRACK_HEIGHT_STEP * 2) {
            yCtrls += TRACK_HEIGHT_STEP;
            addUNCHECKED(&automationSelectParam);
            addUNCHECKED(&hideAutomation);
            addUNCHECKED(&addAutomationLane);
            automationSelectParam.pos = ivec2(inset, yCtrls + inset);
            hideAutomation.pos        = ivec2(inset, yCtrls + TRACK_HEIGHT_STEP + insetBtn);
            addAutomationLane.pos     = ivec2(size.x - inset - addAutomationLane.size.x, yCtrls + TRACK_HEIGHT_STEP + insetBtn);
        } else if (hCtrls >= TRACK_HEIGHT_STEP) {
            yCtrls += TRACK_HEIGHT_STEP;
            addUNCHECKED(&automationSelectParam);
            automationSelectParam.pos = ivec2(inset, yCtrls + inset);
        }
        for (auto g : guis) {
            g->layout();
        }
    }
    bool isResize(ivec2 mpos) {
        return mpos.x >= left() && mpos.x < right() && mpos.y >= bottom() - DRAG_RANGE/2 && mpos.y < bottom() + DRAG_RANGE/2;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (isResize(mpos)) {
            evt.requestFocus(this);
            if (evt.type <= MouseHitType::MOUSE_RIGHT)
                evt.requestCursor(CURSOR_RESIZE_V);
            return true;
        }
        if (contains(mpos)) {
            ivec2 local = this->toContainerSpace(mpos);
            for (guibase* gui : guis) {
                if (gui->isVisible() && gui->mouseHitTest(local, evt)) {
                    return true;
                }
            }
            evt.requestFocus(this);
            return true;// always need to return true if contained, parent has z-order
        }
        return false;
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        dragMode = DragModeTrack::DRAG_TRACK_NONE;
        if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
            const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
            if (evt.relMousepos.y < titleHeight)
                DAW::OpenRenameTrackPopup(dawCtrl, m_trackentry);
            return;
        }
        dawCtrl->setSelectedTrack(m_track);
        if (isResize(evt.relMousepos + this->pos)) {
            dragMode = DragModeTrack::DRAG_TRACK_RESIZE;
        }
    }

    void handleDraggedMove(MouseEvent& evt) override {
        if (dragMode == DragModeTrack::DRAG_TRACK_RESIZE) {
            int32_t mouseDragDist = evt.relMousepos.y;
            int32_t heightStep    = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
            resize<track_gui_entry_t, TRACK_MIN_HEIGHT, TRACK_MAX_HEIGHT>(m_trackentry, m_trackentry, mouseDragDist, heightStep);
            parent->onChildLayoutChanged(this);
            dawCtrl->updateVisibleTrackContents();
        } else {
            parentCtrl->objectDragMove(this, evt);
        }
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        if (dragMode == DragModeTrack::DRAG_TRACK_RESIZE) {

        } else {
            parentCtrl->objectDragRelease(this, evt);
        }
        dragMode = DragModeTrack::DRAG_TRACK_NONE;
    }
    void buttonClicked(guibase* button) override {
        if (button == &hideTrack) {
            m_trackentry->layout.hideTrack = !m_trackentry->layout.hideTrack;
        }
        if (button == &hideAutomation) {
            m_trackentry->layout.hideSubtracks = !m_trackentry->layout.hideSubtracks;
        }
        if (button == &addAutomationLane) {
            m_trackentry->layout.hideTrack     = false;
            m_trackentry->layout.hideSubtracks = false;
        }
        if (button == &addAutomationLane) {
            automatable_t* autom = m_trackentry->state.selectedAutomationCtr;
            int32_t param        = m_trackentry->state.selectedAutomationParam;
            if (autom && param > -1) {
                m_trackentry->parent->addAutomationLane(m_trackentry, autom, param, true);
            }
        }
        updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
        m_trackentry->parentCtrl->updateVisibleTrackContents();
    }
    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }
        NVGcolor color = rgbToNvg(m_track->rgb);
        const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        const int rectHeight  = math::min(titleHeight, size.y);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, size.x, rectHeight);
        nvgFillColor(vg, color);
        nvgFill(vg);

        if (dawCtrl->getSelectedTrack() == m_track) {
            NVGcolor color2 = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK_TITLE);
            int right       = hideTrack.right() + (hideTrack.pos.x) /*inset*/;
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, right, rectHeight);
            nvgFillColor(vg, color2);
            nvgFill(vg);
        }

        renderTextLabel(vg,
                        vec2(hideTrack.right() + INSET_TITLE * 2.0f, titleHeight / 2.0f),
                        vec2(size.x-INSET_TITLE*4.0, titleHeight),
                        m_track->name,
                        theme,
                        titleHeight,
                        getContrastFontColorNvg(color),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (auto g : guis) {
            g->render(vg);
        }
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
        target->trackEntryDragMove(this->m_trackentry, toControlsObjectSpace(mousepos, target));
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
        target->trackEntryDragRelease(this->m_trackentry, toControlsObjectSpace(mousepos, target));
    }
    void handleRightClick(MouseEvent& evt) override {
        parent->handleRightClick(evt);
    }
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
        if (this->m_track->audio) {
            auto tooltip = new guitooltip<gui_trackcontrols_title>(this);
            return tooltip;
        }
        return nullptr;
    }
};

class gui_track_subtrack_controls final : public guictr_base {
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;

public:
    gui_track_subtrack* const subtrack;

private:
    guibuttontoggle removeLane;
    DragModeTrack dragMode = DragModeTrack::DRAG_TRACK_NONE;

public:
    gui_track_subtrack_controls(track_gui_entry_t* _entry, gui_track_subtrack* _subtrack)
        : guictr_base(),
          m_track(_entry->track),
          m_trackentry(_entry),
          subtrack(_subtrack) {
        removeLane.setRadius(10);
        padding         = 0;
        removeLane.icon = ICON_MINUS;
        add(&removeLane);
    }
    ~gui_track_subtrack_controls() override {
        remove(&removeLane);
    }
    void layout() override {
        const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        const int buttonRadius          = (TRACK_HEIGHT_STEP - INSET_TRACK_CONTENT * 2) / 2;
        removeLane.setRadius(buttonRadius - 2);
        int32_t insetBtn = (TRACK_HEIGHT_STEP - removeLane.size.y) / 2;
        removeLane.pos    = ivec2(size.x - removeLane.size.x - insetBtn, insetBtn);
    }
    void buttonClicked(guibase* button) override {
        if (button == &removeLane) {
            DAW::Cursor& cursor = m_trackentry->parentCtrl->getCursor();
            int32_t laneIdx     = this->subtrack->idx;
            if (cursor.inSubTrack(m_trackentry->idx, laneIdx)) {
                cursor.fixCursorSubRange(m_trackentry->subtracks.size() - 1);
            }
            auto daw = dawCtrl->getDaw();
            m_trackentry->parent->removeSubtrack(m_trackentry, subtrack);
            // TODO: fix simliar segfaults: this instance of gui_track_subtrack_mixer is deleted here, but we still access its memberdawCtrl 
            // dawCtrl->getDaw()->updateVisibleTrackContents();
            daw->updateVisibleTrackContents();
        }
    }
    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }

        for (auto g : guis) {
            g->render(vg);
        }

        subtrack->renderMixerInfo(vg, getPosContent(), getSizeContent());
    }
    bool isResize(ivec2 mpos) {
        int32_t resizeTopOrBottom = bottom();
        return mpos.x >= left() && mpos.x < right() && mpos.y >= resizeTopOrBottom - DRAG_RANGE/2 && mpos.y < resizeTopOrBottom + DRAG_RANGE/2;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (isResize(mpos) && this->subtrack != this->m_trackentry->subtracks.back()) {
            evt.requestFocus(this);
            if (evt.type <= MouseHitType::MOUSE_RIGHT)
                evt.requestCursor(CURSOR_RESIZE_V);
            return true;
        }
        if (contains(mpos)) {
            ivec2 local = this->toContainerSpace(mpos);
            for (guibase* gui : guis) {
                if (gui->isVisible() && gui->mouseHitTest(local, evt)) {
                    return true;
                }
            }
            evt.requestFocus(this);
            return true;// always need to return true if contained, parent has z-order
        }
        return false;
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        dawCtrl->setSelectedTrack(m_track);
        if (isResize(evt.relMousepos + this->pos)) {
            dragMode = DragModeTrack::DRAG_TRACK_RESIZE;
        }
    }

    void handleDraggedMove(MouseEvent& evt) override {
        const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        if (dragMode == DragModeTrack::DRAG_TRACK_RESIZE) {
            int32_t mouseDragDist = evt.relMousepos.y;
            int32_t totalHeightSteps = math::min(128, math::max(1, (mouseDragDist) / TRACK_HEIGHT_STEP));
            int32_t distSteps      = totalHeightSteps - subtrack->height;
            if (distSteps && totalHeightSteps != subtrack->height) {
                subtrack->height = totalHeightSteps;
                parent->onChildLayoutChanged(this);
                dawCtrl->updateVisibleTrackContents();
            }
        }
    }
    void handleRightClick(MouseEvent& evt) override {
        if (subtrack->at) {
            parentCtrl->openContextMenu(new guictxtmenu_at_param(dawCtrl, subtrack->at, subtrack->param), evt.mousepos);
        }
    }
};
gui_track_control::gui_track_control(track_gui_entry_t* _entry, scaled_grid& _grid)
    : gui_track_content_base(_entry, _grid),
      title(new gui_trackcontrols_title(_entry)),
      mixer(new gui_trackcontrols_mixer(_entry)),
      io(new gui_trackcontrols_io(_entry)) {
    add(title);
    add(mixer);
    add(io);
    padding = 0;
}
gui_track_control::~gui_track_control() {
    for (gui_track_subtrack_controls* ctrl : automationLaneControls) {
        remove(ctrl);
        delete ctrl;
    }
    remove(io);
    remove(mixer);
    remove(title);
    delete mixer;
    delete io;
    delete title;
}
void gui_track_control::addSubtrackMixer(track_gui_entry_t* entry, gui_track_subtrack* al) {
    gui_track_subtrack_controls* al_ctrl = new gui_track_subtrack_controls(entry, al);
    automationLaneControls.push_back(al_ctrl);
    add(al_ctrl);
}
void gui_track_control::removeSubtrackMixer(gui_track_subtrack* al) {
    auto& ctrls = automationLaneControls;
    auto it     = std::find_if(ctrls.begin(), ctrls.end(), [al](const gui_track_subtrack_controls* ref) {
        return ref->subtrack == al;
        });
    dbgassert(it != ctrls.end());
    remove(*it);
    delete (*it);
    ctrls.erase(it);
}
void gui_track_control::removeAllAutomationLanes(automatable_t* at, int32_t paramIdx) {
    auto& ctrls = automationLaneControls;
    auto it     = std::remove_if(ctrls.begin(), ctrls.end(), [this, at, paramIdx](gui_track_subtrack_controls* ref) {
        if ((at == NULL || ref->subtrack->at == at) && (paramIdx < 0 || ref->subtrack->param == paramIdx)) {
            remove(ref);
            delete ref;
            return true;
        }
        return false;
        });
    ctrls.erase(it, ctrls.end());
}
void gui_track_control::removeAllAutomationLanes(automatable_t* at) {
    removeAllAutomationLanes(at, -1);
}
void gui_track_control::removeAllSubtracks() {
    for (auto at : automationLaneControls) {
        remove(at);
        delete at;
    }
    automationLaneControls.clear();
}
void gui_track_control::renderGroupHandle(NVGcontext* vg) {
    auto lvl = m_track->getChildLvl();
    auto p   = m_track->parent;
    while (p) {
        dbgassert(lvl);

        ivec2 inset{ 2, 0 };
        nvgBeginPath(vg);
        nvgRect(vg, lvl * 8 - 8 + inset.x, pos.y + inset.y, 8 - inset.x * 2, size.y - inset.y * 2);
        nvgFillColor(vg, rgbToNvg(p->rgb));
        nvgFill(vg);
        p = p->parent;
        lvl--;
    }
}
void gui_track_control::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    auto bgColor = theme->getColor(GuiColor::COL_BG_BRT);
    if (dawCtrl->getSelectedTrack() == m_track) {
        bgColor = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK);
    }

    auto bgImage = theme->getBackgroundImage(GuiBackgroundImage::BG_TRACK_MIXER_1);
    if (bgImage) {
        bgImage->render(this, vg);
        bgColor.a *= 0.5f;
    }

    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, size.x, size.y);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    if (safeRefGet(dawCtrl->getDragDropTarget().target) == this) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, size.x, size.y);
        nvgFillColor(vg, rgbaToNvg(0x3fdddd33));
        nvgFill(vg);
    }

    for (guibase* g : guis) {
        //content
        nvgSave(vg);
        g->render(vg);
        nvgRestore(vg);
    }
    nvgBeginPath(vg);
    nvgMoveTo(vg, title->right(), 0);
    nvgLineTo(vg, title->right(), size.y);
    if (io->isVisible()) {
        nvgMoveTo(vg, io->right(), 0);
        nvgLineTo(vg, io->right(), size.y);
    }
    for (gui_track_subtrack_controls* g : automationLaneControls) {
        nvgMoveTo(vg, g->left(), g->top() - TRACK_HEIGHT_SPACING_HALF);
        nvgLineTo(vg, g->right(), g->top() - TRACK_HEIGHT_SPACING_HALF);
    }
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
}

bool canResizeTitleBar(const track_gui_entry_t* const m_trackentry) {
    return !m_trackentry->isHidden() && !m_trackentry->layout.hideSubtracks && m_trackentry->subtracks.size();
}

bool gui_track_control::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    ivec2 local    = this->toContainerSpace(mpos);
    bool contained = contains(mpos);
    if (contained) {
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_HOVER) {
            evt.requestFocus(this);
            return true;
        }
        for (guibase* gui : guis) {
            if (gui->isVisible() && gui->mouseHitTest(local, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) return false;
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_FILE) return false;
        evt.requestFocus(this);
    }
    if (evt.type <= MouseHitType::MOUSE_RIGHT) {
        guibase* g = nullptr;
        if (m_track->type < TRACK_TYPE_MIDI) {
            if (isResize(mpos)) {
                g = this;
            } else if (canResizeTitleBar(m_trackentry) && title->isResize(local)) {
                g = title;
            }
        } else {
            if (canResizeTitleBar(m_trackentry) && title->isResize(local)) {
                g = title;
            } else if (isResize(mpos)) {
                g = this;
            }
        }
        if (g) {
            evt.requestFocus(g);
            evt.requestCursor(CURSOR_RESIZE_V);
            return true;
        }
    }
    return contained;// always need to return true if contained, parent has z-order
}

void gui_track_control::handleDragDropHover(MouseHitEvt& mouseHit) {
    dawCtrl->setSelectedTrackEntry(m_trackentry);
}

gui_track_content_base::gui_track_content_base(track_gui_entry_t* _entry, scaled_grid& _grid)
    : m_grid(_grid), m_track(_entry->track), m_trackentry(_entry) {
    setGuiType(gui_type::CTR_TYPE_TRACKCONTENT);
}

void gui_track_control::handleDraggedBegin(MouseEvent& evt) {
    dawCtrl->setSelectedTrack(m_track);
    if (isResize(evt.relMousepos + this->pos)) {
        dragMode = DragModeTrack::DRAG_TRACK_RESIZE;
    }
}

void gui_track_control::handleDraggedRelease(MouseEvent& evt) {
    dragMode = DragModeTrack::DRAG_TRACK_NONE;
}

bool gui_track_control::isResize(ivec2 mpos) {
    int32_t resizeTopOrBottom = m_track->type < TRACK_TYPE_MIDI ? top() : bottom();
    return mpos.y >= resizeTopOrBottom - DRAG_RANGE/2 && mpos.y < resizeTopOrBottom + DRAG_RANGE/2;
}

guibase* gui_track_control::getTitle() {
    return title;
}
String gui_track_control::getLabel() const {
    return m_trackentry->track->name;
}

void gui_track_control::layout() {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    const int32_t TRACK_IO_WIDTH    = theme->get(GuiConstant::CONST_TRACK_IO_WIDTH);
    const int32_t TRACK_MIXER_WIDTH = theme->get(GuiConstant::CONST_MIXER_WIDTH);
    int32_t titleW                  = size.x - TRACK_MIXER_WIDTH;
    if (io->isVisible()) {
        titleW -= TRACK_IO_WIDTH;
    }
    mixer->size = ivec2(TRACK_MIXER_WIDTH - TRACK_HEIGHT_SPACING, size.y);
    int32_t trH = m_trackentry->isHidden() ? 1 : m_trackentry->layout.height;
    title->size = ivec2(titleW - TRACK_HEIGHT_SPACING, trH * TRACK_HEIGHT_STEP);
    title->pos  = ivec2(TRACK_HEIGHT_SPACING_HALF, 0);
    mixer->pos  = ivec2(size.x - TRACK_MIXER_WIDTH + TRACK_HEIGHT_SPACING_HALF, 0);

    io->size = ivec2(TRACK_IO_WIDTH - TRACK_HEIGHT_SPACING, size.y);
    io->pos  = ivec2(size.x - TRACK_MIXER_WIDTH - TRACK_IO_WIDTH + TRACK_HEIGHT_SPACING_HALF, 0);
    for (gui_track_subtrack_controls* ctrl : automationLaneControls) {
        ctrl->pos  = ivec2(title->pos.x, ctrl->subtrack->pos.y - pos.y);
        ctrl->size = ivec2(title->size.x, ctrl->subtrack->size.y);
    }
    for (guibase* g : guis) {
        g->layout();
    }
}

namespace DAW {

gui_track_drop_position_t GetTrackSlotFromCoord(guictr_tracks* parent, const ivec2 _pos, bool bIncludeBeforeAfter) {
    const int dropMaxDistance = 32;

    using drop_type = gui_track_drop_position_t::drop_type;

    auto& trackList      = parent->guiMgr.getTracksVisibleFlat();
    int minDistDragPoint = std::numeric_limits<int32_t>::max();
    gui_track_drop_position_t minSlot{ 0, nullptr, drop_type::none, { 0, 0 } };
    const auto itcBegin = trackList.crbegin();
    const auto itcEnd   = trackList.crend();


    if (bIncludeBeforeAfter) {
        auto checkDropPoint = [](int32_t minY, int32_t maxY, int mouseY) -> int32_t {
            if (mouseY >= minY && mouseY < maxY) {
                return math::abs(minY + (maxY - minY) / 2 - mouseY);
            }
            return -1;
        };

        for (auto it = itcBegin; it != itcEnd; it++) {
            int32_t slotIdx = static_cast<int32_t>(itcEnd - it - 1);
            track_gui_entry_t* trackEntry = *it;

            auto* gui = trackEntry->trackControls;
            auto* gui2 = trackEntry->trackControls;
            dbgassert(gui->isVisible());
            auto distDragPoint = checkDropPoint(gui->pos.y - dropMaxDistance, gui->pos.y + dropMaxDistance, _pos.y);
            if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
                minDistDragPoint = distDragPoint;
                minSlot          = { slotIdx, trackEntry->track, drop_type::track_before, { gui->pos.x, gui->pos.y } };
            }
            if (trackEntry->track->children.empty()) {
                distDragPoint = checkDropPoint(gui->pos.y + gui->size.y - dropMaxDistance, gui->pos.y + gui->size.y + dropMaxDistance, _pos.y);
                if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
                    minDistDragPoint = distDragPoint;
                    minSlot          = { slotIdx, trackEntry->track, drop_type::track_after, { gui->pos.x, gui->pos.y + gui->size.y } };
                }
            }
            distDragPoint = checkDropPoint(gui2->pos.y + dropMaxDistance, gui2->pos.y + gui2->size.y - dropMaxDistance, _pos.y);
            if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
                minDistDragPoint = distDragPoint;
                minSlot          = { slotIdx, trackEntry->track, drop_type::track_on, { gui2->pos.x, gui2->pos.y + gui2->size.y / 2 } };
            }
        }
    } else {
        auto checkDropPoint = [](int32_t minY, int32_t maxY, int mouseY) -> int32_t {
            return math::abs(minY + (maxY - minY) / 2 - mouseY);
        };

        // check if we're below last top track and before first bottom track
        auto& tracksTop = parent->guiMgr.getTracksTopFlat();
        auto& tracksBottom = parent->guiMgr.getTracksBottomFlat();
        if (tracksTop.empty() && tracksBottom.empty()) {
            return minSlot;
        }
        auto lastTopTrack = tracksTop.back();
        auto firstBottomTrack = tracksBottom.front();
        if (lastTopTrack && !firstBottomTrack && _pos.y > lastTopTrack->trackControls->pos.y + lastTopTrack->trackControls->size.y) {
            return minSlot;
        }
        if (!lastTopTrack && firstBottomTrack && _pos.y < firstBottomTrack->trackControls->pos.y) {
            return minSlot;
        }
        if (lastTopTrack && firstBottomTrack && _pos.y > lastTopTrack->trackControls->pos.y + lastTopTrack->trackControls->size.y && _pos.y < firstBottomTrack->trackControls->pos.y) {
            return minSlot;
        }

        for (auto it = itcBegin; it != itcEnd; it++) {
            int32_t slotIdx = static_cast<int32_t>(itcEnd - it - 1);
            track_gui_entry_t* trackEntry = *it;
            auto* gui = trackEntry->trackControls;
            dbgassert(gui->isVisible());
            auto distDragPoint = checkDropPoint(gui->pos.y, gui->pos.y + gui->size.y, _pos.y);
            if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
                minDistDragPoint = distDragPoint;
                minSlot          = { slotIdx, trackEntry->track, drop_type::track_on, { gui->pos.x, gui->pos.y + gui->size.y / 2 } };
            }
        }
    }
    return minSlot;
}

void SetDragDropTrackInidicatorFromMousePos(guictr_tracks* parent, ivec2 mousepos, const String& clipboardName, bool bIncludeBeforeAfter) {
    using drop_type = gui_track_drop_position_t::drop_type;
    parent->dawCtrl->getDragDropTarget().reset();
    gui_track_drop_position_t slot = GetTrackSlotFromCoord(parent, mousepos, bIncludeBeforeAfter);

    dbgassert(slot.droptype == drop_type::none || slot.droppedTrack);
    int32_t treeIdx = 0;
    track_t* targetTrack = slot.droppedTrack;
    switch (slot.droptype) {
        case drop_type::none:
            if (!bIncludeBeforeAfter) {
                GetTrackSlotFromCoord(parent, mousepos, bIncludeBeforeAfter);
            }
            treeIdx = -2;
            break;
        case drop_type::track_on:
            //insert into slot.droppedTrack at end
            treeIdx     = !slot.droppedTrack->children.empty() ? slot.droppedTrack->children.back()->childIdxTree : 0;
            break;
        case drop_type::track_before:
            //insert into slot.droppedTrack->parent before slot.droppedTrack
            treeIdx     = slot.droppedTrack->childIdxTree;

            break;
        case drop_type::track_after:
            //insert into slot.droppedTrack->parent after slot.droppedTrack
            treeIdx     = slot.droppedTrack->childIdxTree + 1;
            break;
        default:
            dbgassert(0);
            return;
    }

    dragdrop_target_indicator_t target;
    guibase* dropTarget = parent;
    String trNameDest = "";
    track_gui_entry_t* entry{};
    ivec2 dropPos{};
    ivec2 dropSize{};
    if (targetTrack && parent->getTrackEntry(targetTrack, &entry)) {
        dropTarget = entry->trackControls;
        trNameDest = entry->track->name;
        dropPos    = entry->trackControls->pos;
        dropSize   = entry->trackControls->size;
    }
    switch (slot.droptype) {
        case drop_type::track_on:
            target = { dragdrop_target_indicator_t::target_area, treeIdx, dropTarget->toRef(), dropPos + ivec2(0, dropSize.y / 2), "Move '" + clipboardName + "' to '" + trNameDest + "'" };
            break;
        case drop_type::track_before:
            target = { dragdrop_target_indicator_t::target_line, treeIdx, dropTarget->toRef(), dropPos + ivec2(0, 2), "Move '" + clipboardName + "' here" };
            break;
        case drop_type::track_after:
            target = { dragdrop_target_indicator_t::target_line, treeIdx, dropTarget->toRef(), dropPos + ivec2(0, dropSize.y - 2), "Move '" + clipboardName + "' here" };
            break;
        case drop_type::none:
            // new track
            target = { dragdrop_target_indicator_t::target_area, treeIdx, parent->toRef(), parent->pos + parent->size / 2, "Insert " + clipboardName + " on new track" };
            break;
        default:
            dbgassert(0);
            return;
    }
    parent->dawCtrl->getDragDropTarget() = target;
}

void MoveTrackToSlot(DawInstance* daw, track_t* track, gui_track_drop_position_t slot) {
    using drop_type = gui_track_drop_position_t::drop_type;
    int32_t treeIdx = 0;
    track_t* targetTrack = nullptr;
    dbgassert(slot.droptype == drop_type::none || slot.droppedTrack);
    switch (slot.droptype) {
        case drop_type::none:
            return;
        case drop_type::track_on:
            //insert into slot.droppedTrack at end
            targetTrack = slot.droppedTrack;
            treeIdx     = !slot.droppedTrack->children.empty() ? slot.droppedTrack->children.back()->childIdxTree : 0;
            break;
        case drop_type::track_before:
            //insert into slot.droppedTrack->parent before slot.droppedTrack
            targetTrack = slot.droppedTrack->parent;
            treeIdx     = slot.droppedTrack->childIdxTree;
            break;
        case drop_type::track_after:
            //insert into slot.droppedTrack->parent after slot.droppedTrack
            targetTrack = slot.droppedTrack->parent;
            {
                int idx = slot.droppedTrack->childIdxTree + 1;
                // auto* p = slot.droppedTrack->parent;
                // while (p && idx == CtrSize(p->track->children)) {
                //     idx = p->track->childIdxTree + 1;
                //     p   = getParentOf(p);
                // }
                // targetTrack = p;
                treeIdx     = idx;
            }
            break;
        default:
            dbgassert(0);
            return;
    }
    if (TRACKTYPE_TO_CTR(slot.droppedTrack->type) != TRACKTYPE_TO_CTR(track->type)) {
        log_printf("Cannot move there\n");
        return;
    }
    track_tree_pos_t treePos{};
    treePos.treeIdx      = treeIdx;
    treePos.parent       = targetTrack;
    treePos.trackTypeCtr = TRACKTYPE_TO_CTR(track->type);
    std::vector<track_t*> selectedTracks;
    selectedTracks.push_back(track);
    ThreadLock lock  = daw->lockPlayThread();
    bool failed      = !daw->getTracks().moveTracks(selectedTracks, treePos);
    String strTarget = "<root>";
    if (treePos.parent) {
        strTarget = treePos.parent->name;
    }
    log_printf("Moving %zu tracks to %s[%d] %s\n", selectedTracks.size(), StringAsCStr(strTarget), treePos.treeIdx, failed ? "Failed" : "Success");

    daw->onPluginsChanged();
    daw->updateVisibleTrackContents();
    //TODO: edithistory entry
}

void InsertTrackContainerOnTrack(DawInstance* daw, trackcontainer_snapshot_t* ctr, const gui_track_drop_position_t& slot) {
    auto* pluginMgr = daw->getPluginManager();
    for (track_snapshot_t& ts : ctr->tracks) {
        DAW::assignFreeStageIdsTrackSnapshot(pluginMgr, ts);
        ts.trackLoaded = new track_t(ts);
        daw->addTrackImpl(-1, ts.trackLoaded, FLG_TRK_CHANGE_USER, loadTrackIdSnapshot(ts.stageIds));
    }

    // pre load plugins
    for (track_snapshot_t& ts : ctr->tracks) {
        ts.trackLoaded->loadSnapshot(daw->getHost(), ts);
    }

    for (track_snapshot_t& ts : ctr->tracks) {
        DAW::MoveTrackToSlot(daw, ts.trackLoaded, slot);
    }

    // TODO: make this a async blocking progress
    /* for (track_snapshot_t& ts : ctr->tracks) {
        log_printf("track '%s' loading %zu plugins\n", StringAsCStr(ts.trackLoaded->name), ts.data.pluginSnapshots.size());
        std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
        for (auto effect: effects) {
            pluginMgr->activateDeferred(effect, 0);
        }
    } */
    daw->onPluginsChanged();
    daw->updateVisibleTrackContents();
}

} // namespace DAW

void gui_track_control::handleDraggedMove(MouseEvent& evt) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    if (dragMode == DragModeTrack::DRAG_TRACK_RESIZE) {
        auto trackCtr = m_trackentry->parent;
        double scrollPixelOffset = trackCtr->getScrollOffsetPixels();
        int32_t mouseDragDist = evt.relMousepos.y;
        bool resizeTop        = m_track->type < TRACK_TYPE_MIDI;
        if (resizeTop) {
            mouseDragDist = -evt.relMousepos.y + size.y;
        }
        int32_t totalHeightSteps = math::min(128, math::max(1, (mouseDragDist) / TRACK_HEIGHT_STEP));
        if (m_trackentry->layout.hideTrack && totalHeightSteps > TRACK_MIN_HEIGHT) {
            m_trackentry->layout.hideTrack = false;
            updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
        }
        int nChanged = 0;
        while (totalHeightSteps < trackHeight(m_trackentry) && addTrHeight(m_trackentry, -1)) {
            nChanged++;
        }
        while (totalHeightSteps > trackHeight(m_trackentry) && addTrHeight(m_trackentry, 1)) {
            nChanged++;
        }
        if (!nChanged && m_trackentry->layout.height == TRACK_MIN_HEIGHT && totalHeightSteps == TRACK_MIN_HEIGHT) {
            m_trackentry->layout.hideTrack = true;
            updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
        }
        parent->onChildLayoutChanged(this);
        dawCtrl->updateVisibleTrackContents();
        if (nChanged) {
            trackCtr->scrollToPixelOffset(scrollPixelOffset);
        }
    }
}

bool guictxtmenu_track::clickedElement(ctxtmenu_entry* e, int _id) {
    if (e->commandtype != GlobalCommandType::CMD_NONE) {
        auto ctxt = DAW::UI::CommandContext{ e->commandtype };
        closeContextMenu();
        if (m_trackentry->parent->handleEditorCommand(ctxt)) {
            return true;
        }
        dawCtrl->handleGlobalCommand(ctxt);
        return true;
    }
    auto const daw    = dawCtrl->getDaw();
    ThreadLock lock   = daw->lockPlayThread();
    track_t* const tr = m_trackentry->track;
    if (_id >= cmdPickColor->id) {
        _id -= cmdPickColor->id;
        if (tr) {
            tr->rgb           = colorPalette[_id];
            bool bUpdateClips = isShift(parentCtrl->lastMouseEvent.kbmods);
            if (bUpdateClips) {
                for (auto& clip : tr->getClips().getClips()) {
                    clip->rgb = tr->rgb;
                }
            }
        }
    } else if (_id == cmdReactivateAutomation->id) {
        if (tr) {
            std::vector<automatable_t*> targets;
            tr->audio->getAutomatableTrackTargets(targets);
            for (automatable_t* atl : targets) {
                atl->visitAutomatedParams([](auto& param) {
                    param.src.active = true;
                });
            }
        }
    } else if (_id == cmdShowAllAutomation->id) {
        gui_track_automationlane* gtr_at = nullptr;
        if (tr) {
            m_trackentry->layout.hideTrack     = false;
            m_trackentry->layout.hideSubtracks = false;
            updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
            auto trCtr = m_trackentry->parent;
            trCtr->removeAllSubtracks(m_trackentry);
            std::vector<automatable_t*> targets;
            tr->audio->getAutomatableTrackTargets(targets);
            for (automatable_t* atl : targets) {
                std::vector<int32_t> automated;
                atl->getAutomated(automated);
                for (int32_t param : automated) {
                    gtr_at = trCtr->addAutomationLane(m_trackentry, atl, param, true);
                }
            }
        }
        if (gtr_at) {
            dawCtrl->updateVisibleTrackContents();
            m_trackentry->parent->scrollTo(gtr_at);
        }
    } else if (_id == cmdDuplicateTrack->id) {
        if (tr) {
            track_t* newTrack = daw->createNewTrack(tr->type);
            String strNewName = StringFormat("%s copy", StringAsCStr(tr->name));
            track_snapshot_t trSnap(tr, tracksnapshot_store_opts_t::All());
            DAW::assignFreeStageIdsTrackSnapshot(daw->getPluginManager(), trSnap);

            // trSnap.stageIds.inputStageId = -1;
            *newTrack = trSnap;
            daw->addTrackImpl(tr->localIdxFlat + 1, newTrack, FLG_TRK_CHANGE_USER, loadTrackIdSnapshot(trSnap.stageIds));
            track_gui_entry_t* entry{};
            if (m_trackentry->parent->getTrackEntry(tr, &entry)) {
                auto pos = DAW::gui_track_drop_position_t{
                    .slot         = tr->localIdxFlat + 1,
                    .droppedTrack = m_trackentry->track,
                    .droptype     = DAW::gui_track_drop_position_t::drop_type::track_after,
                    .pos          = { 0, 0 }
                };
                if (m_trackentry->trackControls) {
                    pos.pos = m_trackentry->trackControls->getLeftBottom();
                }
                DAW::MoveTrackToSlot(daw, newTrack, pos);
                newTrack->loadSnapshot(daw->getHost(), trSnap);
                newTrack->name = DAW::MakeUniqueTrackName(dawCtrl->getDaw()->getProject(), strNewName);
                //ensure unique IDs
                daw->onPluginsChanged();
                dbgassert(daw->getPluginManager()->validateIds());
                entry->parent->layout();
                daw->updateVisibleTrackContents();
                entry->parent->scrollTo(entry->trackContent);
            }
        }
    } else if (_id == cmdDeleteTrack->id) {
        daw->removeTrackId(m_trackentry->track->projectIdx);
        daw->updateVisibleTrackContents();
    } else if (_id == cmdAddChildMidiTrack->id) {
        auto trackCtr     = m_trackentry->parent;
        track_t* newTrack = daw->createNewTrack(tr->type);
        tr->addChild(newTrack);
        daw->addTrackImpl(0, newTrack, FLG_TRK_CHANGE_USER);
        newTrack->name = DAW::MakeUniqueTrackName(dawCtrl->getDaw()->getProject(), tr->name);
        daw->updateVisibleTrackContents();
        track_gui_entry_t* entry{};
        if (trackCtr->getTrackEntry(newTrack, &entry)) {
            trackCtr->scrollTo(entry->trackContent);
        }
    } else if (_id == cmdRenameTrack->id) {
        DAW::OpenRenameTrackPopup(dawCtrl, m_trackentry);
        return true;
    } else if (_id == cmdShowWaveform->id) {
        auto trackCtr = m_trackentry->parent;
        bool isShown  = (tr->audio->flags & audiostageflags_t::CONVERT_OUTPUT) != audiostageflags_t::NONE;
        if (isShown) {
            tr->audio->flags &= ~(audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::RECORD_OUTPUT);
            std::vector<gui_track_subtrack*> subtracksVecCopy = m_trackentry->subtracks;
            for (auto subtrack : subtracksVecCopy) {
                if (subtrack->subtrackType() == gui_track_subtrack::SUBTRACK_TYPE_WAVE) {
                    trackCtr->removeSubtrack(m_trackentry, subtrack);
                }
            }
        } else {
            tr->audio->flags |= audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::RECORD_OUTPUT;
            auto gui = makeGuiSubtrack(m_trackentry, m_trackentry->parent->getGrid(), gui_track_subtrack::SUBTRACK_TYPE_WAVE);
            trackCtr->addSubTrack(m_trackentry, gui, true);
        }
        daw->updateVisibleTrackContents();
    }
    closeContextMenu();
    return true;
}

guictxtmenu_track::guictxtmenu_track(DawCtrl* _dawCtrl, track_gui_entry_t* const trackentry)
    : guictxtmenu(),
      m_trackentry(trackentry) {
    this->size.x    = 260;
    this->maxHeight = 0;
    this->dawCtrl   = _dawCtrl;
    addEntry(cmdDuplicateTrack = new ctxtmenu_entry("Duplicate Track", 1));
    addEntry(cmdRenameTrack = new ctxtmenu_entry("Rename Track", 6));
    addEntry(cmdDeleteTrack = new ctxtmenu_entry("Delete Track", 2));
    addEntry(new ctxtmenu_splitter());
    addEntry(cmdShowAllAutomation = new ctxtmenu_entry("Show all automation", 0));
    addEntry(cmdShowWaveform = new ctxtmenu_entry("Show waveform", 5));
    addEntry(cmdReactivateAutomation = new ctxtmenu_entry("Reactivate all automation", 7));
    addEntry(new ctxtmenu_splitter());
    addEntry(cmdPickColor = new ctxtmenu_color_select("Pick Color", 100));
    _dawCtrl->setSelectedTrack(m_trackentry->track);
    addEntry(new ctxtmenu_splitter());
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_EXPORT_TRACK));
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_IMPORT_TRACK));
    addEntry(new ctxtmenu_splitter());
    addEntry(cmdAddChildMidiTrack = new ctxtmenu_entry("Add child MIDI Track", 4));
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_INSERT_MIDI_TRACK));
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_INSERT_AUDIO_TRACK));
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_INSERT_RETURN_TRACK));
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_INSERT_MASTER_TRACK));
}


void gui_track_control::handleRightClick(MouseEvent& evt) {
    m_trackentry->parentCtrl->openContextMenu(new guictxtmenu_track(dawCtrl, this->m_trackentry), evt.mousepos);
}

template<>
void guitooltip<gui_trackcontrols_title>::setContent() {
    auto guiPtr = getInstanceOrNull();
    if (!guiPtr) {
        return;
    }
    auto trackEntry = guiPtr->getTrackEntry();
    if (!trackEntry) {
        return;
    }
    auto ptr = trackEntry->track;
    if (!ptr) {
        return;
    }
    using Table::table_entry_t;
    using Table::tbl;
    using Table::tbl_row_t;
    using Table::tblfloat;
    using Table::tblint;
    using Table::tblstr;
    using Table::tblString;
    table.tableWidth = 250;
    {
        table.rows.push_back({ { tblstr{ "track" }, tblString{ ptr->name } } });
        auto audio = ptr->audio;
        table.rows.push_back({ { tblstr{ "stageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.stageId) } } });
        table.rows.push_back({ { tblstr{ "inputStageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.inputStageId) } } });
        table.rows.push_back({ { tblstr{ "outputStageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.outputStageId) } } });
        table.rows.push_back({ { tblstr{ "outputPostStageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.outputPostStageId) } } });
        table.rows.push_back({ { tblstr{ "latency input " }, tblint{ audio->getInputLatency() } } });
        table.rows.push_back({ { tblstr{ "latency intern" }, tblint{ audio->getInternalLatencyCustom() } } });
        table.rows.push_back({ { tblstr{ "latency output" }, tblint{ audio->getOutputLatency() } } });
        table.rows.push_back({ { tblstr{ "sampleRate" }, tblint{ audio->sampleFormat.sampleRate } } });
    }
}

namespace DAW {
    guictr_base* createTrackControlsIO(track_gui_entry_t* _entry) {
        return new gui_trackcontrols_io(_entry);
    }
} // namespace DAW
