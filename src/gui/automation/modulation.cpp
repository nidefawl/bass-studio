#include "modulation.h"
#include "assert_dbg.h"
#include "automation.h"
#include "gui/gui.h"
#include "gui/container/container.h"
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
#include <array>
#include <utility>

namespace DAW::UI {

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
        log_printf("guictr_dragged_modulation_src drag on %s\n", StringAsCStr(target->getClassName()));
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
            dragged.setLabel(StringFormat("Modulation Macro %d", ref.idx));
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
        if (_button->id >= 16) {
            int32_t modulationIndex = _button->id - 16;
            if (paramAutomatable) {
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

    void guictr_edit_modulation_slot::setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::automation_channel_ref _ref, int32_t modulationIndex) {
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
        fieldMode.setOptions({ "Replace", "Add", "Multiply" });
        fieldMode.setSelectedIndex(math::clamp(static_cast<int32_t>(modChannelRef.scale.mode), 0, fieldMode.getLastIndex()));
        fieldMode.setCallback([pMode = &modChannelRef.scale.mode](int idx, String& s) -> String {
            *pMode = static_cast<DAW::ModulationMode>(idx);
            return s;
        });
    }

    void guictr_edit_modulation_slot::layout() {
        const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());

        auto padding       = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        auto cs            = getSizeContent();
        auto srcNameW      = 0.3f * cs.x;
        auto w             = (cs.x - srcNameW) - padding * 2;
        btnSourceName.size = { srcNameW, TRACK_HEIGHT_STEP };
        btnSourceName.pos  = { padding, 0 };
        fieldMinVal.size   = ivec2(w * 0.25 - padding, TRACK_HEIGHT_STEP);
        fieldMinVal.pos    = ivec2(padding + srcNameW, 0);
        fieldMaxVal.size   = ivec2(w * 0.25 - padding, TRACK_HEIGHT_STEP);
        fieldMaxVal.pos    = ivec2(fieldMinVal.right()+padding, fieldMinVal.top());
        fieldMode.size     = ivec2(w * 0.3 - padding, TRACK_HEIGHT_STEP);
        fieldMode.pos      = ivec2(fieldMaxVal.right()+padding, fieldMaxVal.top());
        btnRemove.size     = ivec2(w * 0.2, TRACK_HEIGHT_STEP);
        btnRemove.pos      = ivec2(fieldMode.right()+padding, fieldMode.top());

        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void guictr_edit_modulation_slot::determineSize(ivec2& prefSize) {
        const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
        auto padding2                   = paddingBR(padding) + paddingTL(padding);
        prefSize                        = ivec2(prefSize.x, TRACK_HEIGHT_STEP + padding2.y);
    }

    void guictr_edit_modulation::setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::automation_channel_ref _ref) {
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

}// namespace DAW::UI

namespace DAW {
    void OpenModulationEditor(DawCtrl* dawCtrl, ivec2 mousePos, automatable_t* atl, int32_t paramIdx, DAW::automation_channel_ref ref) {
        dawCtrl->closeAllContextMenus();
        auto ctxtMenu = new DAW::UI::guictr_edit_modulation();
        ctxtMenu->size = {420, 420};
        ctxtMenu->pos = {0, 0};
        ctxtMenu->canTakeInputFocus = true;
        ctxtMenu->maxHeight = -1;
        ctxtMenu->setAutomationRef(dawCtrl->getDaw()->getPluginManager(), atl, paramIdx, ref);
        dawCtrl->openContextMenu(ctxtMenu, mousePos);
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
