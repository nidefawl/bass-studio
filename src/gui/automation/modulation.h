#pragma once
#include "host/automation/automation.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/tooltip/tooltip.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "renderresources.h"

namespace DAW::Host {
    class PluginManager;
}

namespace DAW::UI {
    class IDraggedModulationSource {
    public:
        virtual ~IDraggedModulationSource() = default;
        virtual const modulation_channel_ref& getChannelRef() const = 0;
    };
    class IModulateable {
    public:
        virtual ~IModulateable() = default;
        virtual void getAutomationRef(automatable_t*& at, int32_t& paramIdx) const = 0;
    };
}
namespace DAW::UI::Modulation {
class guictr_edit_modulation_slot final : public guictr_base {
        automatable_t* paramAutomatable = nullptr;
        int32_t paramIdx = 0;
        int32_t slotIdx = 0;
        guibutton btnSourceName;
        gui_numberinput_float fieldMinVal;
        gui_numberinput_float fieldMaxVal;
        guidropdown_generic<String> fieldMode;
        guibuttonstate btnClamp;
        guibutton btnRemove;
    public:
        guictr_edit_modulation_slot();
        ~guictr_edit_modulation_slot() override {
            removeGuis();
        }
        void setSlotIndex(int32_t idx) {
            slotIdx = idx;
        }
        void setModulationSource(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::modulation_channel_ref* _ref, int32_t modulationIndex);
        void setParamAndAutomation(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, int32_t type);
        void renderBackground(NVGcontext* vg) override {
            drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
        }
        GuiConstant::constant_t getGuiConstantHeight() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        void layout() override;
        void determineSize(ivec2& prefSize) override;
        void buttonClicked(guibase* _button) override;
    };

class guictr_edit_modulation final : public guictxtmenu_base {
        automatable_t* paramAutomatable = nullptr;
        int32_t paramIdx = 0;
        guibutton btnAddModulation;
        std::vector<guictr_edit_modulation_slot*> slots;
        const Host::PluginManager* host = nullptr;
        public:
        guictr_edit_modulation()
            : guictxtmenu_base()
        {
            setGuiType(gui_type::CTR_TYPE_EDIT_MODULATION);
            add(&btnAddModulation);
            btnAddModulation.setLabel("Add Modulation");
            btnAddModulation.setText("Add");
            padding = 2;
        }
        ~guictr_edit_modulation() override {
            remove(&btnAddModulation);
            destroyGuis();
        }
        void setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx);
        void updateSlots();

        void renderBackground(NVGcontext* vg) override {
            drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
        }
        GuiConstant::constant_t getGuiConstantHeight() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        GuiConstant::constant_t getGuiConstantTitlebar() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        void render(NVGcontext* vg) override;

        void layout() override;
        void determineSize(ivec2& prefSize) override;

        void buttonClicked(guibase* _button) override;
    };

class gui_dragged_modulation final : 
        public guitooltip<gui_dragged_modulation>, public IDraggedModulationSource {
        DAW::modulation_channel_ref ref{};
        automatable_param_ref_t previewParamRef{};
        modulation_scaling_t previewScaling{};
    public:
        gui_dragged_modulation() : guitooltip<gui_dragged_modulation>(this) {
            setGuiType(gui_type::CTR_TYPE_MODULATION_DRAGGED);
            pos = { 0, 0 };
            setDragRendered(true);
        }
        void setChannelRef(const DAW::modulation_channel_ref& _ref) {
            ref = _ref;
        }
        const DAW::modulation_channel_ref& getChannelRef() const override {
            return ref;
        }
        ~gui_dragged_modulation() override = default;
        void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
        void handleDraggedRelease(MouseEvent& evt) override;
        void handleDraggedMove(MouseEvent& evt) override;
        void dragMoveOn(guibase* target, ivec2 mousepos) override;
        void dragReleaseOn(guibase* target, ivec2 mousepos) override;
    };

class guibutton_modulate final : public guibutton, public IDraggedModulationSource {
        const DAW::modulation_channel_ref ref;
        gui_dragged_modulation dragged;
        bool hasDragged        = false;
        public:
        explicit guibutton_modulate(DAW::modulation_channel_ref ref) : guibutton(), ref(ref) {
            setGuiType(gui_type::CTR_TYPE_MODULATION_BUTTON);
            drawFn   = drawTextureSymbol;
            drawParm = ICON_MODULATION;
            dragged.setParent(this);
        }
        const DAW::modulation_channel_ref& getChannelRef() const override {
            return ref;
        }
        void setControl(BaseCtrl* parentCtrl) override {
            guibase::setControl(parentCtrl);
            dragged.setControl(parentCtrl);
        }
        void handleDraggedBegin(MouseEvent& evt) override;
        void handleDraggedMove(MouseEvent& evt) override;
        void handleDraggedRelease(MouseEvent& evt) override;
        void render(NVGcontext* vg) override;
    };
}

template<>
void guitooltip<DAW::UI::Modulation::gui_dragged_modulation>::setContent();