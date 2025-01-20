#pragma once
#include "dropdown.hpp"
#include "seq_util.hpp"
#include "str_util.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "gui/contextmenu/contextmenu.hpp"


class guidropdown_cb {
public:
    virtual ~guidropdown_cb() = default;
    virtual void onOptionSelected(int _id) = 0;
};

template<typename T>
class guidropdown_generic final : public guidropdownbase, public guidropdown_cb {
    std::vector<T> options;
    String current;

public:
    std::function<String(int, T&)> fnOptionSelected;
    void setCallback(std::function<String(int, T&)> _fnOptionSelected) {
        fnOptionSelected = std::move(_fnOptionSelected);
    }
public:
    ~guidropdown_generic() override = default;
    void setOptions(const std::vector<T>& vecOptions) {
        this->options = vecOptions;
    }
    int32_t getLastIndex() override {
        return CtrSize(options) -1;
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        std::vector<String> strOptions;
        strOptions.reserve(options.size());
        for (auto& option : options) {
            strOptions.push_back(optionToString(option));
        }
        auto* popup   = createContextMenu(std::move(strOptions));
        popup->size   = size;
        auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
        popup->setFontSize(fontSizeScaled);
        this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
    String getString() override {
        return current;
    }
    void onOptionSelected(int _id) override {
        if (_id >= 0) {
            auto& option = options.at(_id);
            if (fnOptionSelected) {
                current = fnOptionSelected(_id, option);
            } else {
                current = optionToString(option);
            }
        }
    }
    void setSelectedIndex(int32_t idx) override {
        if (idx >= 0 && idx < CtrSize(options)) {
            auto& option = options.at(idx);
            current = optionToString(option);
        } else {
            current = StringFormat("<Invalid %d>", idx);
        }
    }
    guictxtmenu_base* createContextMenu(std::vector<String>&& strOptions);
    String optionToString(const T& ref);
    void setCurrentString(const String& str) {
        current = str;
    }
};

class guidropdown_generic_ctxt final : public guictxtmenu {
    guidropdown_cb* const parent;
    std::vector<String> options;

public:
    guidropdown_generic_ctxt(guidropdown_cb* _parent, std::vector<String>&& _options) : parent(_parent), options(_options) {
        this->size.x   = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        int32_t idx    = 0;
        for (auto& str : options) {
            if (str == "-") {
                addEntry(new ctxtmenu_splitter());
            } else {
                addEntry(new ctxtmenu_entry(str, idx));
            }
            idx++;
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        closeContextMenu();
        if (_id >= 0 && _id < CtrSize(options)) {
            parent->onOptionSelected(_id);
        }
        return true;
    }
};