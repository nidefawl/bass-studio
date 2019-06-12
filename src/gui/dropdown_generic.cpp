#include "dropdown_generic.h"
#include "str_util.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"


class guidropdown_generic_ctxt : public guictxtmenu {
	guidropdown_cb* const parent;
	std::vector<String> options;
public:
	guidropdown_generic_ctxt(guidropdown_cb* _parent, std::vector<String>&& _options) : parent(_parent), options(_options) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		int32_t idx = 0;
		for (auto str : options) {
//			my_printf("added %s\n", StringAsCStr(str));
			addEntry(new ctxtmenu_entry(str, idx));
			idx++;
		}
	}
	void clicked(int _id) {
		closeContextMenu();
		if (_id >= 0 && _id < options.size()) {
			parent->onOptionSelected(_id);
		}
	}
};

template<>
String guidropdown_generic<String>::optionToString(const String& ref) {
	return ref;
}
template<>
void guidropdown_generic<String>::onOptionSelected(int _id) {
	if (_id >= 0) {
		auto& option = options[_id];
		if (fnOptionSelected) {
			current = fnOptionSelected(_id, option);
		} else {
			current = optionToString(option);
		}
	}
}
template<>
void guidropdown_generic<String>::handleDraggedRelease(MouseEvent& evt) {
	std::vector<String> strOptions;
	strOptions.reserve(options.size());
	for (auto& option : options) {
		strOptions.push_back(optionToString(option));
	}
	guictxtmenu_base *popup = new guidropdown_generic_ctxt(this, std::move(strOptions));
	popup->size = size;
	popup->setFontSize(size.y);
	this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
}
