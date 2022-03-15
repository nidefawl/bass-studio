#include "dropdown_generic.h"
#include "str_util.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"


class guidropdown_generic_ctxt : public guictxtmenu {
    guidropdown_cb* const parent;
    std::vector<String> options;

public:
    guidropdown_generic_ctxt(guidropdown_cb* _parent, std::vector<String>&& _options) : parent(_parent), options(_options) {
        this->size.x   = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        int32_t idx    = 0;
        for (auto str : options) {
            addEntry(new ctxtmenu_entry(str, idx));
            idx++;
        }
    }
    void clicked(int _id) override {
        closeContextMenu();
        if (_id >= 0 && _id < options.size()) {
            parent->onOptionSelected(_id);
        }
    }
};
/* TODO: find a way to have this generic, while having definition of guidropdown_generic_ctxt only in this cpp file */
template<>
guictxtmenu_base* guidropdown_generic<String>::createContextMenu(std::vector<String>&& strOptions) {
    return new guidropdown_generic_ctxt(this, std::move(strOptions));
}
template<>
String guidropdown_generic<String>::optionToString(const String& ref) {
    return ref;
}

template<>
guictxtmenu_base* guidropdown_generic<int32_t>::createContextMenu(std::vector<String>&& strOptions) {
    return new guidropdown_generic_ctxt(this, std::move(strOptions));
}
template<>
String guidropdown_generic<int32_t>::optionToString(const int32_t& ref) {
    return std::to_string(ref);
}
