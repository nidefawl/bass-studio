#include "dropdown_generic.hpp"
#include "str_util.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "gui/contextmenu/contextmenu.hpp"

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
