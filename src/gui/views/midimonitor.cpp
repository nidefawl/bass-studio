#include <algorithm>
#include <nanovg.h>
#include <utility>
#include "color_util.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/tooltip/tooltip.h"
#include "gui/controls/button.h"
#include "gui/controls/list.h"
#include "gui/controls/textfield.h"
#include "host/midi_host.h"
#include "midi-msg.h"
#include "plugins/synth/IPlugMidi.h"
#include "renderresources.h"
#include "platform.h"
#include "host/plugin/base_plugin.h"
#include "host/mainctrl.h"
#include "host/host.h"
#include "host/audio_host.h"

namespace {


class gui_midi_inspect_entry : public gui_list_entry {
    MidiIOEvent evt;
public:
    MidiIOEvent& getEvt() {
        return evt;
    }
    explicit gui_midi_inspect_entry(MidiIOEvent& evt) : gui_list_entry(), evt(evt) {
        icon = ICON_MIDIPLUG;
        setLabel(IMidiMsg::FromU32(evt.message).ToString());
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
    }
    String getText() override {
        return label;
    }

    void render(NVGcontext* vg) override {
        BaseCtrl* ctrl  = parentCtrl;
        float spacing   = INSET_TITLE;
        float x         = spacing;
        float rowHeight = size.y;
        bool focused = ctrl->isCtrOrChildFocused(this);
        if (focused || selected) {
            auto color = theme->getColor(focused ? GuiColor::COL_BG_DRKER : GuiColor::COL_BG_DRK);
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
        nvgTranslate(vg, pos.x, pos.y);
        if (icon > -1) {
            RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
            drawIcon(vg, size, &image);
            x += rowHeight + spacing;
        }

        // const float percWidth = size.x / 4;
        float xText = size.x - spacing;
        // auto* _entry = safeRefGet(ref);
        // if (_entry) {
        //     host_stats_reducted_t stats{};
        //     auto host = dawCtrl->getDaw()->getHost();
        //     host->getShortStats(stats);
        //     float fPercentLoad = stats.timePerBlock_usec <= 0 ? 0 : _entry->procStats.timeTrackProcessPlugins * 100.0f / stats.timePerBlock_usec;
        //     String str = StringFormat("%.2f%%", fPercentLoad);
        //     float x2 = size.x - spacing;
        //     xText = x2 - renderText(vg, vec2(x2, rowHeight / 2), vec2(size.x*0.8, size.y), str, 0, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        //     if (size.x > rowHeight*10) {
        //         str = StringFormat("%zdµs", _entry->procStats.timeTrackProcessPlugins);
        //         x2 = size.x - percWidth - spacing;
        //          xText = x2 - renderText(vg, vec2(x2, rowHeight / 2), vec2(size.x*0.3, size.y), str, 0, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        //     }
        // }
        renderText(vg, vec2(x, rowHeight*0.5f), vec2(xText-x, size.y), getText());
        nvgTranslate(vg, -pos.x, -pos.y);
    }
//     guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
//         auto tooltip  = new gui_test(ref);
//         auto* graph   = &tooltip->getGraph();
//         graph->size   = { 256, 128 };
//         graph->pos    = { 0, 0 };
//         tooltip->size = graph->size;
//         tooltip->layout();
// //        tooltip->canTakeInputFocus = true;
//         tooltip->maxHeight = graph->size.y;
//         return tooltip;
//     }
};

class gui_midi_inspect_ctr : public guictr_base {
public:
    gui_list list;
    gui_midi_inspect_ctr() : guictr_base() {
        setBackgroundRendered(true);
        list.padding = 0;
        list.setBackgroundRendered(false);
        list.setRowHeight(14);
        add(&list);
    }
    ~gui_midi_inspect_ctr() override {
        removeGuis();
    }
    void layout() override {
        auto cs           = getSizeContent();
        list.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        list.pos            = { 0, 0 };
        list.size           = cs;
        for (auto* g : guis) {
            g->layout();
        }
    }
};
class gui_midi_inspect : public guictr_vert_layout {
    std::vector<gui_midi_inspect_entry*> listEntriesMessages;
    gui_midi_inspect_ctr listCtr;
    guibutton btnClear;
    String curquery     = "";
    int64_t tmLastUpdate = 0;

public:
    gui_midi_inspect() : guictr_vert_layout(1) {
        setGuiType(CTR_TYPE_MIDI_MONITOR);
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
        addElement({0.9f, &listCtr});
        addElement({-20, &btnClear});
        btnClear.setText("Clear");
    }
    ~gui_midi_inspect() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        update();
    }
    void onRemove() override {
        auto daw = dawCtrl->getDaw();
        auto lock = daw->lockPlayThread();
        auto midiHost = daw->getMidiHost();
        midiHost->setInspection(true, false);
        guictr_vert_layout::onRemove();
    }
    void update() {
        auto daw = dawCtrl->getDaw();
        ThreadLock lock = daw->getPlayThread()->tryLockThread();
        auto tmNow = getTimeMillis();
        if (!lock.isLocked()) {
            // force lock and update if 5 seconds have elapsed
            if (tmNow - tmLastUpdate > 5000) {
                lock = daw->lockPlayThread();
            } else {
                return;
            }
        }
        tmLastUpdate = tmNow;

        auto midiHost = daw->getMidiHost();
        midiHost->setInspection(true, true);
        auto vecData = midiHost->getInspectionInputMessages();
        for (auto& midiMsg : vecData) {
            auto it = std::find_if(listEntriesMessages.begin(), listEntriesMessages.end(),
                                    [&midiMsg](gui_midi_inspect_entry* p) {
                                        return p->getEvt().timestamp == midiMsg.timestamp && p->getEvt().message == midiMsg.message;
                                    });
            if (it == listEntriesMessages.end()) {
                gui_midi_inspect_entry* g = nullptr;
                g = new gui_midi_inspect_entry(midiMsg);
                listEntriesMessages.insert(listEntriesMessages.begin(), g);
            }
        }
        btnClear.setLabel("Clear");
        btnClear.setEnabled(!listEntriesMessages.empty());
        std::vector<gui_list_entry*> newList;
        newList.insert(newList.begin(), listEntriesMessages.begin(), listEntriesMessages.end());
        listCtr.list.setList(newList);
        listCtr.list.sort([](gui_list_entry* a, gui_list_entry* b) {
            return static_cast<gui_midi_inspect_entry*>(a)->getEvt().timestamp > static_cast<gui_midi_inspect_entry*>(b)->getEvt().timestamp;
        });
        layout();
    }
    void buttonClicked(guibase* button) override {
        if (&btnClear == button) {
            listCtr.list.setList({});
            listEntriesMessages.clear();
            update();
        }
    }
};

} // namespace
 
guictr_base* makeGuiMidiInspect() {
    return new gui_midi_inspect();
}
