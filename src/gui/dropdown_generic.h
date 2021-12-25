#pragma once
#include "dropdown.h"
#include "str_util.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"


class guidropdown_cb {
public:
    virtual ~guidropdown_cb() {
    }
    virtual void onOptionSelected(int _id) = 0;
};

template<typename T>
class guidropdown_generic : public guidropdownbase, public guidropdown_cb {
    std::vector<T> options;
    String current;

public:
    std::function<String(int, T&)> fnOptionSelected;

public:
    ~guidropdown_generic() {
    }
    void setOptions(const std::vector<T>& vecOptions, String strSelectedVal) {
        this->current = strSelectedVal;
        this->options = vecOptions;
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        std::vector<String> strOptions;
        strOptions.reserve(options.size());
        for (auto& option : options) {
            strOptions.push_back(optionToString(option));
        }
        auto* popup   = createContextMenu(std::move(strOptions));
        popup->size   = size;
        int fontScale = math::round((this->fontSize > 0 ? this->fontSize : size.y) * fFontScale);
        popup->setFontSize(fontScale);
        this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
    String getString() override {
        return current;
    }
    void onOptionSelected(int _id) override {
        if (_id >= 0) {
            auto& option = options[_id];
            if (fnOptionSelected) {
                current = fnOptionSelected(_id, option);
            } else {
                current = optionToString(option);
            }
        }
    }
    guictxtmenu_base* createContextMenu(std::vector<String>&& strOptions);
    String optionToString(const T& ref);
};
