#include "modulation.h"
#include "assert_dbg.h"
#include "host/automation/automation.h"
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
#include "host/daw/mainctrl.h"
#include "host/plugin/internal/internal-plugin.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_util.h"
#include "str_util.h"
#include "logging.h"
#include "window.h"
#include <array>
#include <functional>
#include <nanovg_min.h>
#include <utility>

namespace DAW::UI::Modulation {
    class ctxt_endpoint : public ctxtmenu_entry_track_io {
    public:
        ctxt_endpoint(int32_t _id, const String& name) : ctxtmenu_entry_track_io(_id, name) {
        }
        virtual modulation_channel_ref getEndpoint() = 0;
    };
class ctxt_modchannel final : public ctxt_endpoint {
    public:
        const modulation_channel_ref endpoint;

        ctxt_modchannel(int32_t _id, const String& name, modulation_channel_ref _endpoint)
            : ctxt_endpoint(_id, name), endpoint(_endpoint) {
        }
        void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
            ctxtmenu_entry_track_io::render(ctxtSize, vg, idx, mouse);
        }

        bool isBus() override {
            return false;
        }

        modulation_channel_ref getEndpoint() override {
            return endpoint;
        }
    };
    class ctxt_bus : public ctxtmenu_entry_track_io {
    public:
        const bus_type busType;
        const String busName;

        ctxt_bus(int32_t _id, const String& name, bus_type bustype)
            : ctxtmenu_entry_track_io(_id, name),
            busType(bustype),
            busName(name) {
        }

        bool isBus() override {
            return true;
        }
    };
class ctxt_bus_track final : public ctxt_bus {
        const audio_stage_ref_t busStage;

    public:
        ctxt_bus_track(int32_t _id, const String& name, audio_stage_ref_t _stageBus)
            : ctxt_bus(_id, name, bus_type::internal), busStage(_stageBus) {
        }
        audio_stage_ref_t getStageRef() {
            return busStage;
        }
    };

    /* top select menu */
class guictxtmenu_modulation final : public guictxtmenu {
    public:
        std::function<void(const DAW::modulation_channel_ref&)> fnCallback;
        explicit guictxtmenu_modulation(DawCtrl * _dawCtrl)
        {
            this->dawCtrl = _dawCtrl;
            auto proj = _dawCtrl->getDaw()->getProject();
            int32_t idx = 0;
            for (auto* track : proj->getTracksFlatVec()) {
                addEntry(new ctxt_bus_track(idx++, track->name, track->audio->toRef()));
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
                    if (effect->hasAutomationModulationOutput()) {
                        auto effMod = static_cast<internal_modulator*>(effect);
                        auto& channelsSrc = effMod->getModulationOutputChannelDesc();
                        auto ref = effMod->toRef();
                        for (auto& channel : channelsSrc) {
                            modulation_channel_ref modChanRef;
                            modChanRef.refSrc = ref;
                            modChanRef.refSrc.type = AUTOMATABLE_MODULATOR_OUTPUT;
                            modChanRef.refSrc.paramIdx = channel.offset;
                            modChanRef.paramIdxDst = -1;
                            modChanRef.scale = { 0.0f, 1.0f };
                            auto name = effect->getAutomatableName() + " (" + channel.name + ")";
                            addEntry(new ctxt_modchannel(idx++, name, modChanRef));
                        }
                    }
                }
            }
        }

        void addEntry(ctxtmenu_entry* entry) = delete;
        void addEntry(ctxtmenu_entry_track_io* entry) {
            guictxtmenu::addEntry(entry);
        }

        bool clickedElement(ctxtmenu_entry* e, int _id) override {
            auto const ctxtEndpointEntry = static_cast<ctxtmenu_entry_track_io*>(e);
            if (ctxtEndpointEntry->isBus()) {
                return false;
            }
            dbgassert(dynamic_cast<ctxt_endpoint*>(e));
            auto const entry = static_cast<ctxt_endpoint*>(e);
            if (fnCallback)
                fnCallback(entry->getEndpoint());
            return true;
        }


        guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override {
            guictxtmenu_modulation* popup = nullptr;
            auto entry = dynamic_cast<ctxt_bus*>(e);
            if (entry) {
                if (entry->busType == bus_type::internal) {
                    auto stageEntry = dynamic_cast<ctxt_bus_track*>(entry);
                    dbgassert(stageEntry);
                    if (stageEntry) {
                        popup = new guictxtmenu_modulation(dawCtrl, stageEntry->getStageRef(), lvl);
                        popup->fnCallback = fnCallback;
                    }
                }
            }
            return popup;
        }
    };


    void gui_dragged_modulation::handleDraggedRelease(MouseEvent& evt) {
        dawCtrl->objectDragRelease(this, evt);
    }

    void gui_dragged_modulation::handleDraggedMove(MouseEvent& evt) {
        dawCtrl->objectDragMove(this, evt);
    }

    void gui_dragged_modulation::dragMoveOn(guibase* target, ivec2 mousepos) {
        automatable_param_ref_t newRef = {.type = AUTOMATABLE_NONE, .paramIdx = -1};
        automatable_t* newAt = nullptr;
        if (target->getGuiType() == gui_type::GUI_TYPE_KNOB || target->getGuiType() == gui_type::GUI_TYPE_SLIDER_TEXTFIELD) {
            auto knob = dynamic_cast<DAW::UI::IModulateable*>(target);
            int32_t paramIdx = -1;
            knob->getAutomationRef(newAt, paramIdx);
            if (newAt) {
                newRef = newAt->toRef();
                newRef.paramIdx = paramIdx;
            }
        }
        modulation_scaling_t scale = {.min = -0.25f, .max = 0.25f, .mode = ModulationMode::ADD};
        if (isShift(dawCtrl->lastMouseEvent.kbmods)) {
            scale.mode = ModulationMode::MUL;
            scale.min = 0.0f;
            scale.max = 1.0f;
        }
        if (newRef != previewParamRef || scale != previewScaling) {
            auto daw = dawCtrl->getDaw();
            auto lock = daw->lockPlayThread();
            auto prevAt = resolveAutomatableRefDevice(daw->getHost(), previewParamRef);
            if (prevAt) {
                auto refCopy = getChannelRef();
                refCopy.paramIdxDst = previewParamRef.paramIdx;
                DAW::DisonnectModulation(prevAt, refCopy);
            }
            previewParamRef = newRef;
            previewScaling = scale;
            if (newAt) {
                DAW::ConnectModulationInputChannel(newAt, newRef.paramIdx, getChannelRef(), scale, true);
            }
        }
    }

    void gui_dragged_modulation::dragReleaseOn(guibase* target, ivec2 mousepos) {
        if (previewParamRef.type != AUTOMATABLE_NONE) {
            auto daw = dawCtrl->getDaw();
            auto prevAt = resolveAutomatableRefDevice(daw->getHost(), previewParamRef);
            if (prevAt) {
                auto lock = daw->lockPlayThread();
                auto refCopy = getChannelRef();
                refCopy.paramIdxDst = previewParamRef.paramIdx;
                DAW::DisonnectModulation(prevAt, refCopy);
            }
            previewParamRef = {};
        }
        if (target->getGuiType() == gui_type::GUI_TYPE_KNOB || target->getGuiType() == gui_type::GUI_TYPE_SLIDER_TEXTFIELD) {
            auto knob = dynamic_cast<DAW::UI::IModulateable*>(target);
            automatable_t* at;
            int32_t paramIdx;
            knob->getAutomationRef(at, paramIdx);
            if (at) {
                auto lock = dawCtrl->lockPlayThread();
                modulation_scaling_t scale = {.min = -0.25f, .max = 0.25f, .mode = ModulationMode::ADD};
                if (isShift(dawCtrl->lastMouseEvent.kbmods)) {
                    scale.mode = ModulationMode::MUL;
                    scale.min = 0.0f;
                    scale.max = 1.0f;
                }
                DAW::ConnectModulationInputChannel(at, paramIdx, getChannelRef(), scale, false);
            }
        }
        if (target->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
            auto btn = dynamic_cast<DAW::UI::Modulation::guibutton_modulate*>(target);
            if (dawCtrl->getEditModulation() == btn) {
                dawCtrl->setEditModulation({});
            } else {
                dawCtrl->setEditModulation(toRef());
            }
        }
    }

    void gui_dragged_modulation::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
        mousepos -= pos;
        mousepos += ivec2(20, 20);
        nvgTranslate(vg, mousepos.x, mousepos.y);
        auto iconS = ivec2(math::roundfS32(fontSize));
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

    void guibutton_modulate::render(NVGcontext* vg) {
        if (dawCtrl && dawCtrl->getIsContainerRenderPass() && dawCtrl->getEditModulation() == this) {
            DawCtrl::ui_modulation_targets_t t;
            nvgSaveState(vg, &t.state);
            t.target = toRef();
            dawCtrl->getUIModulationTargets().push_back(t);
        }
        guibutton::render(vg);
    }

    void guibutton_modulate::handleDraggedBegin(MouseEvent& evt) {
        guibutton::handleDraggedBegin(evt);
        hasDragged = false;
        // dawCtrl->setEditModulation(toRef());
    }

    void guibutton_modulate::handleDraggedMove(MouseEvent& evt) {
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
        dawCtrl->objectDragRelease(&dragged, evt);
        if (hasDragged) {
            return;
        }
        if (parent)
            parent->buttonClicked(this);
    }

    void guictr_edit_modulation::buttonClicked(guibase* _button) {
        if (_button ==  &btnAddModulation) {
            auto* popup = new DAW::UI::Modulation::guictxtmenu_modulation(dawCtrl);
            popup->fnCallback = [this](const DAW::modulation_channel_ref& ref) -> void {
                if (dawCtrl && paramAutomatable){
                    auto lock = dawCtrl->lockPlayThread();
                    DAW::ConnectModulationInputChannel(paramAutomatable, paramIdx, ref, {}, false);
                    updateSlots();
                    layout();
                    parentCtrl->relayout();
                }
            };
            popup->size = btnAddModulation.size;
            popup->setFontSize(dawCtrl->getTheme()->getFloat(GuiConstant::CONST_FONT_SIZE_CONTEXT_MENU));
            popup->size.x = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
            parentCtrl->openAppMenu(0, popup, toScreenSpace(btnAddModulation.getLeftBottom()) + ivec2(0, 1), WINDOW_IS_BORDERLESS | WINDOW_POS_RELATIVE);
            return;
        }
        if (_button->id >= 16) {
            int32_t modulationIndex = _button->id - 16;
            if (paramAutomatable) {
                auto lock = dawCtrl->lockPlayThread();
                paramAutomatable->removeModulation(paramIdx, modulationIndex);
                updateSlots();
                parentCtrl->relayout();
            }
            // closeContextMenu();
        }
    }

    void guictr_edit_modulation::determineSize(ivec2& prefSize) {
        const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
        auto padding                    = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        auto innerHeight                = TRACK_HEIGHT_STEP + padding;
        for (auto* slot : slots) {
            ivec2 tmpSize = prefSize;
            slot->determineSize(tmpSize);
            innerHeight += tmpSize.y;
        }
        innerHeight += TRACK_HEIGHT_STEP + padding;
        auto padding2 = paddingBR(this->padding) + paddingTL(this->padding);
        innerHeight += padding2.y;
        prefSize = ivec2(prefSize.x, innerHeight);
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
        btnAddModulation.size = ivec2(w*0.5, TRACK_HEIGHT_STEP);
        btnAddModulation.pos  = ivec2(padding+(w-btnAddModulation.size.x)*0.5, posSlots.y + padding);
        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void guictr_edit_modulation::render(NVGcontext* vg) {
        if (!isRenderableSizeAndContext(vg))
            return;
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
        auto pModulations = paramAutomatable->getModulations(paramIdx);
        size_t numInputs = 2;
        if (pModulations) {
            numInputs += pModulations->size();
        }
        while (slots.size() > numInputs) {
            remove(slots.back());
            delete slots.back();
            slots.pop_back();
        }
        while (slots.size() < numInputs) {
            auto slot = new guictr_edit_modulation_slot();
            slot->setSlotIndex(CtrSize(slots));
            add(slot);
            slots.push_back(slot);
        }
        slots[0]->setParamAndAutomation(host, paramAutomatable, paramIdx, 0);
        slots[1]->setParamAndAutomation(host, paramAutomatable, paramIdx, 1);
        for (size_t i = 2; pModulations && i < numInputs; ++i) {
            auto modulationIndex = static_cast<int32_t>(i) - 2;
            slots[i]->setModulationSource(host, paramAutomatable, paramIdx, (*pModulations)[modulationIndex], modulationIndex);
        }
    }
    void guictr_edit_modulation_slot::setParamAndAutomation(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, int type) {
        dbgassert(host);
        dbgassert(_paramAutomatable);
        btnRemove.id           = 0;
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
        // auto& modChannelRef    = *_stableRef;
        auto param = paramAutomatable->getParam(paramIdx);
        if (!assert_expr(param)) {
            return;
        }
        auto& scale = type == 0 ? param->getParameterScale() : param->getAutomationScale();
        btnRemove.setEnabled(false);
        btnRemove.setVisible(false);
        if (type == 1) {
            btnSourceName.setText("Automation");
            fieldMinVal.setRef(&scale.min);
            fieldMaxVal.setRef(&scale.max);
            btnClamp.setStateRef(&scale.bClamp);
            fieldMode.setOptions({ "Replace", "Add", "Multiply", "Bypass" });
            fieldMode.setCallback([&scale](int idx, String& s) -> String {
                scale.mode = static_cast<DAW::ModulationMode>(idx);
                return s;
            });
        } else {
            btnSourceName.setText("Parameter Value");
            fieldMode.setOptions({ "Replace" });
            fieldMinVal.setEnabled(false);
            fieldMaxVal.setEnabled(false);
            fieldMinVal.setVisible(false);
            fieldMaxVal.setVisible(false);
            fieldMode.setEnabled(false);
  
        }
        fieldMode.setSelectedIndex(math::clamp(static_cast<int32_t>(scale.mode), 0, fieldMode.getLastIndex()));
        String srcName = paramAutomatable->getAutomatableName();
        srcName += " ";
        srcName += paramAutomatable->getParamName(paramIdx);
        setLabel(srcName);
    }

    void guictr_edit_modulation_slot::setModulationSource(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::modulation_channel_ref* _stableRef, int32_t modulationIndex) {
        dbgassert(host);
        dbgassert(_paramAutomatable);
        dbgassert(_stableRef->refSrc.type == AUTOMATABLE_MODULATOR_OUTPUT);
        btnRemove.id           = 16 + modulationIndex;
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
        // auto& modChannelRef    = *_stableRef;
        fieldMinVal.setRef(&_stableRef->scale.min);
        fieldMaxVal.setRef(&_stableRef->scale.max);
        String channelName;
        if (_stableRef->refSrc.type == AUTOMATABLE_MODULATOR_OUTPUT) {
            auto channel = DAW::ResolveModulationChannel(host, *_stableRef);
            if (channel) {
                channelName = channel->getName();
            }
        }
        String srcName = paramAutomatable->getAutomatableName();
        srcName += " ";
        srcName += paramAutomatable->getParamName(paramIdx);
        if (!channelName.empty()) {
            btnRemove.setTooltipText("Remove Modulation: " + channelName);
        } else {
            btnRemove.setTooltipText("Remove Modulation");
        }
        setLabel(srcName);
        btnSourceName.setText(channelName);
        btnClamp.setStateRef(&_stableRef->scale.bClamp);
        fieldMode.setOptions({ "Replace", "Add", "Multiply", "Bypass" });
        fieldMode.setSelectedIndex(math::clamp(static_cast<int32_t>(_stableRef->scale.mode), 0, fieldMode.getLastIndex()));
        fieldMode.setCallback([_stableRef](int idx, String& s) -> String {
            _stableRef->scale.mode = static_cast<DAW::ModulationMode>(idx);
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

    void guictr_edit_modulation::setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->host             = host;
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
        updateSlots();
    }

    bool IsEditModulation(const guibase* gui, automatable_t* at, int32_t paramIdx) {
        if (!at || !gui) {
            return false;
        }
        auto dawCtrl = gui->dawCtrl;
        if (dawCtrl) {
            auto editing = dawCtrl->getEditModulation();
            if (editing) {
                auto ref = editing->getChannelRef();
                if (at && at->isParamConnectedTo(paramIdx, ref))
                    return true;
            }
        }
        return false;
    }

    bool IsHiglightedModulation(const guibase* gui, automatable_t* at, int32_t paramIdx) {
        if (!at || !gui) {
            return false;
        }
        auto dawCtrl = gui->dawCtrl;
        if (dawCtrl) {
            auto dragged = dawCtrl->getDraggedModulation();
            if (dragged) {
                return dawCtrl->getGuiOverRef() != gui->toRef();
            }
            auto focused = dawCtrl->getFocusedModulation();
            if (focused) {
                auto ref = focused->getChannelRef();
                if (at && at->isParamConnectedTo(paramIdx, ref))
                    return true;
            }
            auto editing = dawCtrl->getEditModulation();
            if (editing) {
                auto ref = editing->getChannelRef();
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
        setGuiType(gui_type::CTR_TYPE_EDIT_MODULATION);
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
    void OpenModulationEditor(BaseCtrl* parentCtrl, ivec2 mousePos, automatable_t* atl, int32_t paramIdx) {
        parentCtrl->closeAllContextMenus();
        auto dawCtrl = parentCtrl->getDawCtrl();
        if (!assert_expr(dawCtrl)) {
            return;
        }
        auto ctxtMenu = new DAW::UI::Modulation::guictr_edit_modulation();
        ctxtMenu->size = {520, 420};
        ctxtMenu->pos = {0, 0};
        ctxtMenu->canTakeInputFocus = true;
        ctxtMenu->maxHeight = -1;
        ctxtMenu->setAutomationRef(dawCtrl->getDaw()->getPluginManager(), atl, paramIdx);
        dawCtrl->openOverlayGui(ctxtMenu, mousePos, WINDOW_POS_RELATIVE | WINDOW_IS_RESIZABLE | WINDOW_IS_BORDERLESS);
    }
} // namespace DAW

template<>
void guitooltip<DAW::UI::Modulation::gui_dragged_modulation>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    table.tableWidth = 140;
    auto cell = Table::tblString{ptr->getTooltipText()};
    if (table.strW) {
        table.tableWidth = table.strW->getStringWidth(cell.str);
    }
    Table::tbl_row_t row{{std::move(cell)}};
    table.rows.push_back(std::move(row));
}