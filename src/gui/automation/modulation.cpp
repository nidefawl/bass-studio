#include "modulation.h"
#include "assert_dbg.h"
#include "automation.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/tooltip/tooltip.h"
#include "gui/views/controls.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "host/host_pluginmanager.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_util.h"
#include "str_util.h"
#include "logging.h"
#include "window.h"
#include <array>
#include <utility>

namespace DAW::UI {
    class ctxtmenu_modulation_endpoint : public ctxtmenu_entry_track_io {
    public:
        ctxtmenu_modulation_endpoint(int32_t _id, const String& name) : ctxtmenu_entry_track_io(_id, name) {
        }
        virtual automatable_param_ref_t getEndpoint() = 0;
    };
    class ctxtmenu_entry_stage_channel : public ctxtmenu_modulation_endpoint {
    public:
        const automatable_param_ref_t endpoint;

        ctxtmenu_entry_stage_channel(int32_t _id, const String& name, automatable_param_ref_t _endpoint)
            : ctxtmenu_modulation_endpoint(_id, name), endpoint(_endpoint) {
        }
        void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
            ctxtmenu_entry_track_io::render(ctxtSize, vg, idx, mouse);
        }

        bool isBus() override {
            return false;
        }

        automatable_param_ref_t getEndpoint() override {
            return endpoint;
        }
    };
    class ctxtmenu_entry_modulation_bus : public ctxtmenu_entry_track_io {
    public:
        const bus_type busType;
        const String busName;

        ctxtmenu_entry_modulation_bus(int32_t _id, const String& name, bus_type bustype)
            : ctxtmenu_entry_track_io(_id, name),
            busType(bustype),
            busName(name) {
        }

        bool isBus() override {
            return true;
        }
    };
    class ctxtmenu_entry_modulation_bus_internal : public ctxtmenu_entry_modulation_bus {
        const audio_stage_ref_t busStage;

    public:
        ctxtmenu_entry_modulation_bus_internal(int32_t _id, const String& name, audio_stage_ref_t _stageBus)
            : ctxtmenu_entry_modulation_bus(_id, name, bus_type::internal), busStage(_stageBus) {
        }
        audio_stage_ref_t getStageRef() {
            return busStage;
        }
    };

    /* top select menu */
    class guictxtmenu_modulation : public guictxtmenu {

    public:
        guictxtmenu_modulation(DawCtrl * _dawCtrl)
        {
            this->dawCtrl = _dawCtrl;
            auto proj = _dawCtrl->getDaw()->getProject();
            int32_t idx = 0;
            for (auto* track : proj->getTracksFlatVec()) {
                addEntry(new ctxtmenu_entry_modulation_bus_internal(idx++, track->name, track->audio->toRef()));
            }
        }
        guictxtmenu_modulation(DawCtrl * _dawCtrl, audio_stage_ref_t _track, int lvl)
        {
            this->lvl = lvl;
            this->dawCtrl = _dawCtrl;
            auto const stage = _dawCtrl->getDaw()->getPluginManager()->getAudioStage(_track);
            if (stage) {
                int32_t idx = 0;
                for (auto* effect : stage->effects) {
                    addEntry(new ctxtmenu_entry_stage_channel(idx++, effect->getAutomatableName(), effect->toRef()));
                }
            }
        }

        void addEntry(ctxtmenu_entry* entry) = delete;
        void addEntry(ctxtmenu_entry_track_io* entry) {
            guictxtmenu::addEntry(entry);
        }

        void clickedElement(ctxtmenu_entry* e, int _id) override {
            auto const ctxtEndpointEntry = static_cast<ctxtmenu_entry_track_io*>(e);
            if (ctxtEndpointEntry->isBus()) {
                return;
            }
            dbgassert(dynamic_cast<ctxtmenu_modulation_endpoint*>(e));
            auto const entry = static_cast<ctxtmenu_modulation_endpoint*>(e);
        }


        guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override {
            guictxtmenu* popup = nullptr;
            auto entry = dynamic_cast<ctxtmenu_entry_modulation_bus*>(e);
            if (entry) {
                if (entry->busType == bus_type::internal) {
                    auto stageEntry = dynamic_cast<ctxtmenu_entry_modulation_bus_internal*>(entry);
                    dbgassert(stageEntry);
                    if (stageEntry) {
                        popup = new guictxtmenu_modulation(dawCtrl, stageEntry->getStageRef(), lvl);
                    }
                }
            }
            return popup;
        }
    };

    void guictr_dragged_modulation_src::handleDraggedRelease(MouseEvent& evt) {
        dawCtrl->objectDragRelease(this, evt);
    }

    void guictr_dragged_modulation_src::handleDraggedMove(MouseEvent& evt) {
        dawCtrl->objectDragMove(this, evt);
    }

    void guictr_dragged_modulation_src::dragMoveOn(guibase* target, ivec2 mousepos) {
        target->modulationDragMove(this, toControlsObjectSpace(mousepos, target));
    }

    void guictr_dragged_modulation_src::dragReleaseOn(guibase* target, ivec2 mousepos) {
        target->modulationDragRelease(this, toControlsObjectSpace(mousepos, target));
    }

    void guictr_dragged_modulation_src::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
        mousepos -= pos;
        mousepos += ivec2(20, 20);
        nvgTranslate(vg, mousepos.x, mousepos.y);
        auto iconS = ivec2(fontSize);
        NVGcolor color = theme->getColor(GuiColor::COL_KNOB_MODULATED);
        NVGcolor color2 = theme->getColor(GuiColor::COL_LABEL_ACTIVE);
        ivec2 bgCenter = pos + ivec2(0, size.y/2) - ivec2(iconS.x+INSET_TABLE, iconS.y/2);

        drawBackground(vg, theme, pos-ivec2(iconS.x+INSET_TABLE*2, 0), math::maxvec2(size, iconS)+ivec2(iconS.x+INSET_TABLE*2, 0), 0, false);
        drawTextureSymbol(vg, bgCenter, iconS, color, ICON_MODULATION, -1); 
        setFont(vg, fontSize, color2, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent() - ivec2(INSET_TABLE << 1), fontSize);
        if (textField.isVisible()) {
            textField.render(vg);
        }
    }

    void guibutton_modulate::handleDraggedMove(MouseEvent& evt) {
        hasDragged = false;
        if (!hasDragged) {
            dragged.setChannelRef(ref);
            dragged.setLabel(StringFormat("Modulation Macro %d", ref.paramIdxDst));
            dragged.pos = {};
            dragged.layout();
            dbgassert(dragged.isDragRendered());
            parentCtrl->setDragged(&dragged);
            hasDragged = true;
        }
        dawCtrl->objectDragMove(&dragged, evt);
    }

    void guibutton_modulate::handleDraggedRelease(MouseEvent& evt) {
        dbgassert(dragged.isDragRendered());
        if (hasDragged) {
            dawCtrl->objectDragRelease(&dragged, evt);
            return;
        }
        if (parent)
            parent->buttonClicked(this);
    }

    void guictr_edit_modulation::buttonClicked(guibase* _button) {
        if (_button ==  &btnAddModulation) {
            auto* popup = new guictxtmenu_modulation(dawCtrl);
            popup->size = btnAddModulation.size;
            popup->setFontSize(dawCtrl->getTheme()->getFloat(GuiConstant::CONST_FONT_SIZE_CONTEXT_MENU));
            popup->size.x = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
            parentCtrl->openAppMenu(0, popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
            return;
        }
        if (_button->id >= 16) {
            int32_t modulationIndex = _button->id - 16;
            if (paramAutomatable) {
                auto lock = dawCtrl->lockPlayThread();
                paramAutomatable->removeModulation(modulationIndex);
                updateSlots();
                parentCtrl->relayout();
            }
            // closeContextMenu();
        }
    }

    void guictr_edit_modulation::determineSize(ivec2& prefSize) {
        const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
        auto innerHeight                = TRACK_HEIGHT_STEP;
        for (auto* slot : slots) {
            ivec2 tmpSize = prefSize;
            slot->determineSize(tmpSize);
            innerHeight += tmpSize.y;
        }
        innerHeight += TRACK_HEIGHT_STEP;
        auto padding2 = paddingBR(padding) + paddingTL(padding);
        innerHeight += padding2.y;
        prefSize = ivec2(prefSize.x, innerHeight + TRACK_HEIGHT_STEP);
    }

    void guictr_edit_modulation::layout() {
        const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
        auto padding                    = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        auto cs                         = getSizeContent();
        auto w                          = cs.x - padding * 2;
        auto posSlots                   = ivec2(0, theme->get(getGuiConstantTitlebar()));
        for (auto slot : slots) {
            slot->size = ivec2(cs.x, TRACK_HEIGHT_STEP);
            slot->pos  = posSlots;
            posSlots.y += slot->size.y;
        }
        this->btnAddModulation.size = ivec2(w, TRACK_HEIGHT_STEP);
        this->btnAddModulation.pos  = ivec2(padding, posSlots.y + padding);
        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void guictr_edit_modulation::render(NVGcontext* vg) {
        nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
        nvgTranslate(vg, pos.x, pos.y);
        renderFrameBase(vg);
        int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : 0;
        if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
        renderTitleBar(vg, size, getLabel(), getGuiConstantTitlebar(), 0, flags, true);
        renderFrameOutline(vg);
        ivec2 posInset = getPosContent();
        nvgTranslate(vg, posInset.x - pos.x, posInset.y - pos.y);
        nvgTranslateZ(vg, -4.0f);
        for (guibase* gui : guis) {
            nvgSave(vg);
            gui->render(vg);
            nvgRestore(vg);
        }
    }

    void guictr_edit_modulation::updateSlots() {
        dbgassert(host);
        String text = "Modulation: ";
        text += paramAutomatable->getAutomatableName();
        text += " ";
        text += paramAutomatable->getParamName(paramIdx);
        setLabel(text);
        auto isModulated = paramAutomatable->isParamModulated(paramIdx);
        if (isModulated) {
            auto& inputs   = paramAutomatable->getModulations(paramIdx);
            auto numInputs = inputs.size();
            while (slots.size() > numInputs) {
                remove(slots.back());
                delete slots.back();
                slots.pop_back();
            }
            while (slots.size() < numInputs) {
                auto slot = new guictr_edit_modulation_slot();
                add(slot);
                slots.push_back(slot);
            }
            for (size_t i = 0; i < numInputs; ++i) {
                slots[i]->setAutomationRef(host, paramAutomatable, paramIdx, *inputs[i], i);
            }
        } else {
            while (slots.size() > 0) {
                remove(slots.back());
                delete slots.back();
                slots.pop_back();
            }
        }
    }

    void guictr_edit_modulation_slot::setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::modulation_channel_ref _ref, int32_t modulationIndex) {
        dbgassert(host);
        dbgassert(_paramAutomatable);
        btnRemove.id           = 16 + modulationIndex;
        this->modulationIndex  = modulationIndex;
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
        auto& modChannels      = _paramAutomatable->getModulations(_paramIdx);
        auto& modChannelRef    = *modChannels[modulationIndex];
        fieldMinVal.setRef(&modChannelRef.scale.min);
        fieldMaxVal.setRef(&modChannelRef.scale.max);
        auto channel = DAW::ResolveModulationChannel(host, modChannelRef);
        String channelName;
        if (channel) {
            channelName = channel->getName();
        }
        String srcName = paramAutomatable->getAutomatableName();
        srcName += " ";
        srcName += paramAutomatable->getParamName(_paramIdx);
        if (!channelName.empty()) {
            btnRemove.setTooltipText("Remove Modulation: " + channelName);
        } else {
            btnRemove.setTooltipText("Remove Modulation");
        }
        setLabel(srcName);
        btnSourceName.setText(channelName);
        btnClamp.setStateRef(&modChannelRef.scale.bClamp);
        fieldMode.setOptions({ "Replace", "Add", "Multiply", "Bypass" });
        fieldMode.setSelectedIndex(math::clamp(static_cast<int32_t>(modChannelRef.scale.mode), 0, fieldMode.getLastIndex()));
        fieldMode.setCallback([pMode = &modChannelRef.scale.mode](int idx, String& s) -> String {
            *pMode = static_cast<DAW::ModulationMode>(idx);
            return s;
        });
    }

    void guictr_edit_modulation_slot::layout() {
        auto padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        auto cs = getSizeContent();
        std::array<float, 6> scales{ 0.2f, 0.15f, 0.15f, 0.2f, 0.15f, 0.15f };
        guibase* prevGui = nullptr;
        size_t idx = 0;
        float w = cs.x - (scales.size() - 1) * padding;
        for (auto gui : guis) {
            gui->pos = {};
            if (prevGui) {
                gui->pos.x = prevGui->pos.x + prevGui->size.x + padding;
            }
            dbgassert(idx < scales.size());
            gui->size = { math::max(math::roundfS32(w * scales[idx++]), 16), cs.y };
            prevGui = gui;
        }
        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void guictr_edit_modulation_slot::determineSize(ivec2& prefSize) {
        const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
        auto padding2 = paddingBR(padding) + paddingTL(padding);
        prefSize                        = ivec2(prefSize.x, TRACK_HEIGHT_STEP + padding2.y);
    }

    void guictr_edit_modulation::setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::modulation_channel_ref _ref) {
        this->host             = host;
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
        updateSlots();
    }

    bool IsHiglightedModulation(const guibase* gui, automatable_t* at, int32_t paramIdx) {
        if (!at) {
            return false;
        }
        auto dawCtrl = gui->dawCtrl;
        if (dawCtrl) {
            auto dragged = dawCtrl->getDraggedModulation();
            if (dragged) {
                return dawCtrl->guiOver != gui;
            }
            auto focused = dawCtrl->getFocusedModulation();
            if (focused) {
                auto ref = focused->getChannelRef();
                if (at && at->isParamConnectedTo(paramIdx, ref))
                    return true;
            }
        }
        return false;
    }

    guictr_edit_modulation_slot::guictr_edit_modulation_slot()
        : guictr_base(),
          fieldMinVal(nullptr),
          fieldMaxVal(nullptr) {
        margin        = 0;
        padding       = 2;
        this->guiType = gui_type::CTR_TYPE_EDIT_MODULATION;
        add(&btnSourceName);
        add(&fieldMinVal);
        add(&fieldMaxVal);
        add(&fieldMode);
        add(&btnClamp);
        add(&btnRemove);
        fieldMaxVal.setLabel("Max");
        fieldMinVal.setLabel("Min");
        fieldMode.setLabel("Mode");
        btnRemove.setText("Remove");
        btnSourceName.setText("Source");
        btnClamp.setText("Clamp");
        btnClamp.setButtonColor(GuiColor::COL_BASE_BG_FRAME_HIGHLIGHT);
    }
    void guictr_edit_modulation_slot::buttonClicked(guibase* _button) {
        if (&btnClamp == _button) {
            auto* b = btnClamp.getStateRef();
            if (b) {
                *b = !*b;
            }
            return;
        }
        if (parent) {
            parent->buttonClicked(_button);
        }
    }
}// namespace DAW::UI

namespace DAW {
    void OpenModulationEditor(DawCtrl* dawCtrl, ivec2 mousePos, automatable_t* atl, int32_t paramIdx, DAW::modulation_channel_ref ref) {
        dawCtrl->closeAllContextMenus();
        auto ctxtMenu = new DAW::UI::guictr_edit_modulation();
        ctxtMenu->size = {520, 420};
        ctxtMenu->pos = {0, 0};
        ctxtMenu->canTakeInputFocus = true;
        ctxtMenu->maxHeight = -1;
        ctxtMenu->setAutomationRef(dawCtrl->getDaw()->getPluginManager(), atl, paramIdx, ref);
        dawCtrl->openOverlayGui(ctxtMenu, mousePos, WINDOW_POS_RELATIVE | WINDOW_IS_RESIZABLE | WINDOW_IS_BORDERLESS);
    }
} // namespace DAW

template<>
void guitooltip<DAW::UI::guictr_dragged_modulation_src>::setContent() {
    table.tableWidth = 140;
    auto cell = Table::tblString{ptr->getTooltipText()};
    if (table.strW) {
        table.tableWidth = table.strW->getStringWidth(cell.str);
    }
    Table::tbl_row_t row{{std::move(cell)}};
    table.rows.push_back(std::move(row));
}