#pragma once
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/colorpick.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/textfield.h"
#include "gui/dropdown/dropdown_generic.h"
#include "guibackgroundimage.h"
#include "renderresources.h"

class guictr_set_background : public guictr_base {
    class guictr_load_image : public guictr_base {
        public:
        gui_textfield pathVal;
        guibutton btnLoadImage;
        guictr_load_image() : guictr_base() {
            padding = 2;
            margin = 0;
            add(&pathVal);
            add(&btnLoadImage);
            btnLoadImage.setLabel("Load Image");
            btnLoadImage.setText(btnLoadImage.getLabel());
        }
        ~guictr_load_image() override {
            removeGuis();
        }
        void layout() override {
            ivec2 cs = getSizeContent();
            int w = cs.x / 3;
            pathVal.size = ivec2(cs.x - w, cs.y);
            pathVal.pos  = ivec2(0, 0);
            btnLoadImage.size = ivec2(w, cs.y);
            btnLoadImage.pos  = ivec2(pathVal.right(), 0);
            guictr_base::layout();
        }
    };
    class guictr_3buttons : public guictr_base {
        public:
        std::array<guibutton, 3> btnPos;
        guictr_3buttons() : guictr_base() {
            padding = 4;
            margin = 2;
            for (int i = 0; i < 3; i++) {
                btnPos[i].setLabel("X");
                btnPos[i].setText(btnPos[i].getLabel());
                add(&btnPos[i]);
            }
            setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        }
        ~guictr_3buttons() override {
            removeGuis();
        }
        void buttonClicked(guibase* button) override {
            parent->buttonClicked(button);
        }
    };
    class guictr_scale : public guictr_base {
        public:
        vec2 scale;
        gui_numberinput_float scaleX;
        gui_numberinput_float scaleY;
        guidropdown_generic<String> dropdownAbsoluteScale;
        guictr_scale() : guictr_base(), scale(1.0f), scaleX(&scale.x), scaleY(&scale.y) {
            padding = 2;
            margin = 0;
            add(&dropdownAbsoluteScale);
            add(&scaleX);
            add(&scaleY);
            scaleX.setLabel("Scale X");
            scaleY.setLabel("Scale Y");
            dropdownAbsoluteScale.setOptions({ "Relative", "Absolute" });
            dropdownAbsoluteScale.setSelectedIndex(0);
            dropdownAbsoluteScale.setLabel("Scale Mode");
            setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        }
        ~guictr_scale() override {
            removeGuis();
        }
    };
    public:
    container_background_image bgImage;
    guictr_load_image selectImagePath;
    guidropdown_generic<String> dropdownLayout;
    guictr_scale scale;
    guibutton btnClearImage;
    std::array<guictr_3buttons, 3> btnPos;
    gui_color_select colorSelect;
    std::function<void(const container_background_image&)> fnEditBackground;
    guictr_set_background() : guictr_base() {
        padding = 2;
        margin = 0;
        setCanMouseHit(true);
        selectImagePath.pathVal.id = 0x10;
        selectImagePath.btnLoadImage.id = 0x11;
        btnClearImage.id = 0x12;
        btnClearImage.setLabel("Clear Image");
        btnClearImage.setText(btnClearImage.getLabel());
        dropdownLayout.setOptions({ "Fixed Position", "Fill", "Contain", "Cover", "Repeat" });
        dropdownLayout.id = 0x13;
        dropdownLayout.setSelectedIndex(0);
        dropdownLayout.setLabel("Layout Mode");
        dropdownLayout.setCallback([this](int idx, String& sel) {
            bgImage.layout = static_cast<container_background_image::layout_t>(idx);
            fnEditBackground(bgImage);
            updateButtonStates();
            return sel;
        });
        scale.scaleX.fnValueEditChanged = [this](gui_numberinput_field_base* _field, float val) {
            bgImage.scale.x = val;
            fnEditBackground(bgImage);
        };
        scale.scaleY.fnValueEditChanged = [this](gui_numberinput_field_base* _field, float val) {
            bgImage.scale.y = val;
            fnEditBackground(bgImage);
        };
        scale.dropdownAbsoluteScale.setCallback([this](int idx, String& sel) {
            bgImage.scaleAbsolute = idx == 1;
            fnEditBackground(bgImage);
            return sel;
        });
        add(&selectImagePath);
        add(&dropdownLayout);
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                btnPos[y].btnPos[x].id = 0x20 + y * 3 + x;
                btnPos[y].btnPos[x].setButtonColor(GuiColor::COL_NOTE_SELECTED);
            }
            add(&btnPos[y]);
        }
        add(&scale);
        add(&colorSelect);
        colorSelect.setText("Color");
        colorSelect.id = 0x14;
        add(&btnClearImage);
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
    }
    void updateButtonStates() {
        if (bgImage.layout == container_background_image::layout_t::position) {
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 3; ++x) {
                    btnPos[y].btnPos[x].setEnabled(true);
                    btnPos[y].btnPos[x].setFlag(FLG_HAS_COLOR_BG, false);
                }
            }
            if (bgImage.verticalPos >= 0 && bgImage.verticalPos < 3 && bgImage.horizontalPos >= 0 && bgImage.horizontalPos < 3) {
                btnPos[bgImage.verticalPos].btnPos[bgImage.horizontalPos].setFlag(FLG_HAS_COLOR_BG, true);
            }
        } else {
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 3; ++x) {
                    btnPos[y].btnPos[x].setEnabled(false);
                    btnPos[y].btnPos[x].setFlag(FLG_HAS_COLOR_BG, false);
                }
            }
        }
    }
    void determineSize(ivec2& prefSize) override {
        prefSize.y = 7*(32 + padding);
    }
    ~guictr_set_background() override {
        removeGuis();
    }
    void setEditBackground(container_background_image& bg) {
        bgImage = bg;
        selectImagePath.pathVal.setValue(bg.path);
        dropdownLayout.setSelectedIndex(static_cast<int>(bg.layout));
        scale.scale = bg.scale;
        scale.dropdownAbsoluteScale.setSelectedIndex(bg.scaleAbsolute ? 1 : 0);
        updateButtonStates();
    }
    void buttonClicked(guibase* button) override {
        if (button->id == 0x11) {
            String path;
            String lastProjectDirectory = selectImagePath.pathVal.value();
            if (promptUserFilePath(parentCtrl->window, 0, FILE_TYPES_IMAGES, path, lastProjectDirectory)) {
                bgImage.path = path;
            }
        }
        if (button->id == 0x12) {
            selectImagePath.pathVal.setValue("");
            bgImage.path = "";
        }
        if (button->id == 0x14) {
            bgImage.rgba = colorSelect.getColor();
        }
        for (int i = 0; i < 9; i++) {
            if (button->id == 0x20 + i) {
                bgImage.horizontalPos = static_cast<container_background_image::position_t>(i % 3);
                bgImage.verticalPos = static_cast<container_background_image::position_t>(i / 3);
            }
        }
        fnEditBackground(bgImage);
        updateButtonStates();
    }
};