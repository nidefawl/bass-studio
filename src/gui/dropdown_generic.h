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
class guidropdown_generic: public guidropdownbase, public guidropdown_cb {
	std::vector<T> options;
	String current;
public:
	std::function<String(int, T&)> fnOptionSelected;
public:
	~guidropdown_generic() {

	}
	void setOptions(const std::vector<T>& options, String currentVal) {
		this->current = currentVal;
		this->options = options;
	}
	void handleDraggedRelease(MouseEvent& evt) override;
	String getString() override {
		return current;
	}
	void onOptionSelected(int _id) override;
	String optionToString(const T& ref);
};
