#pragma once
#include "automation.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
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
        virtual const automation_channel_ref& getChannelRef() const = 0;
    };

    class guictr_edit_modulation_slot : public guictr_base {
        automatable_t* paramAutomatable = nullptr;
        int32_t paramIdx = 0;
        int32_t modulationIndex = 0;
        guibutton btnSourceName;
        gui_numberinput_float fieldMinVal;
        gui_numberinput_float fieldMaxVal;
        guidropdown_generic<String> fieldMode;
        guibutton btnRemove;
        public:
        guictr_edit_modulation_slot()
            : guictr_base(),
            fieldMinVal(nullptr),
            fieldMaxVal(nullptr)
        {
            margin  = 0;
            padding = 2;
            this->guiType = gui_type::CTR_TYPE_EDIT_MODULATION;
            add(&btnSourceName);
            add(&fieldMaxVal);
            add(&fieldMinVal);
            add(&fieldMode);
            add(&btnRemove);
            fieldMaxVal.setLabel("Max");
            fieldMinVal.setLabel("Min");
            fieldMode.setLabel("Mode");
            btnRemove.setLabel("Remove");
            btnRemove.setText("Remove");
            btnSourceName.setText("Source");
            btnSourceName.setLabel("Source");
        }
        ~guictr_edit_modulation_slot() override {
            removeGuis();
        }
        void setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::automation_channel_ref _ref, int32_t modulationIndex);

        void renderBackground(NVGcontext* vg) override {
            drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
        }
        GuiConstant::constant_t getGuiConstantHeight() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        void layout() override;
        void determineSize(ivec2& prefSize) override;
        void buttonClicked(guibase* _button) override {
            if (parent) {
                parent->buttonClicked(_button);
            }
        }
    };

    class guictr_edit_modulation : public guictxtmenu_base {
        automatable_t* paramAutomatable = nullptr;
        int32_t paramIdx = 0;
        guibutton btnAddModulation;
        std::vector<guictr_edit_modulation_slot*> slots;
        const Host::PluginManager* host = nullptr;
        public:
        guictr_edit_modulation()
            : guictxtmenu_base()
        {
            this->guiType = gui_type::CTR_TYPE_EDIT_MODULATION;
            add(&btnAddModulation);
            btnAddModulation.setLabel("Add Modulation");
            btnAddModulation.setText("Add");
            padding = 2;
        }
        ~guictr_edit_modulation() override {
            removeGuis();
        }
        void setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::automation_channel_ref _ref);
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

    class guictr_dragged_modulation_src : 
        public guitooltip<guictr_dragged_modulation_src>, public DAW::UI::IDraggedModulationSource {
        DAW::automation_channel_ref ref;
    public:
        guictr_dragged_modulation_src() : guitooltip<guictr_dragged_modulation_src>(this) {
            this->guiType = gui_type::CTR_TYPE_MODULATION_DRAGGED;
            pos = { 0, 0 };
            setDragRendered(true);
        }
        void setChannelRef(const DAW::automation_channel_ref& _ref) {
            ref = _ref;
        }
        const DAW::automation_channel_ref& getChannelRef() const override {
            return ref;
        }
        ~guictr_dragged_modulation_src() override = default;
        bool isDragMoveable() override {
            return true;
        }
        void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
        void handleDraggedRelease(MouseEvent& evt) override;
        void handleDraggedMove(MouseEvent& evt) override;
        void dragMoveOn(guibase* target, ivec2 mousepos) override;
        void dragReleaseOn(guibase* target, ivec2 mousepos) override;
    };

    class guibutton_modulate : public guibutton, public IDraggedModulationSource {
        const DAW::automation_channel_ref ref;
        guictr_dragged_modulation_src dragged;
        bool hasDragged        = false;
        public:
        guibutton_modulate(DAW::automation_channel_ref ref) : guibutton(), ref(ref) {
            this->guiType = gui_type::CTR_TYPE_MODULATION_BUTTON;
            drawFn   = drawTextureSymbol;
            drawParm = ICON_MODULATION;
            dragged.setParent(this);
        }
        const DAW::automation_channel_ref& getChannelRef() const override {
            return ref;
        }
        void setControl(BaseCtrl* parentCtrl) override {
            guibase::setControl(parentCtrl);
            dragged.setControl(parentCtrl);
        }
        void handleDraggedMove(MouseEvent& evt) override;
        void handleDraggedRelease(MouseEvent& evt) override;
    };
}
