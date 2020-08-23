#include "guicontainer_dnd_layout.h"


static const int32_t dropIndicatorWidth = 8;
//int controlH = HEIGHT_DEFAULT_INPUT;
bool guictr_layout::setOverlayPos(g_layout_ctr_drop_area* area, const dock_pos dockPos) {
	ivec2 relPos = ivec2(0);
	ivec2 relSize = size;
	switch (dockPos) {
	case dock_pos::LEFT:
		relPos = ivec2(-dropIndicatorWidth/2, 0);
		relSize = ivec2(dropIndicatorWidth, size.y);
		break;
	case dock_pos::RIGHT:
		relPos = ivec2(size.x-dropIndicatorWidth/2, 0);
		relSize = ivec2(dropIndicatorWidth, size.y);
		break;
	case dock_pos::TOP:
		relPos = ivec2(0, -dropIndicatorWidth/2);
		relSize = ivec2(size.x, dropIndicatorWidth);
		break;
	case dock_pos::BOTTOM:
		relPos = ivec2(0, size.y-dropIndicatorWidth/2);
		relSize = ivec2(size.x, dropIndicatorWidth);
		break;
	case dock_pos::CENTER:
		relPos = ivec2(dropIndicatorWidth/2, dropIndicatorWidth/2);
		relSize = ivec2(size.x-dropIndicatorWidth, size.y-dropIndicatorWidth);
		break;
	case dock_pos::STACK:
		dbgassert(0);
		return false;
	default:
		return false;
//		layoutGui->validPreview = false;
//		layoutGui->boxes.clear();
//		return false;
	}
	area->dockPos = dockPos;
	area->dockPosOffset = 0;
	area->pos = toScreenSpace(relPos - paddingTL(padding));
	area->size = math::maxvec2(ivec2(relSize), ivec2(10, 10));
	return true;
}
bool guictr_layout::setOverlayPosForTab(g_layout_ctr_drop_area* area, const dock_pos dockPos, const int32_t dockOffset, const bool rightSideHandle) {
	ivec2 relPos = ivec2(0);
	ivec2 relSize = size;
	dbgassert(dockPos == dock_pos::STACK);
	if (dockOffset > -1 && dockOffset <= entries.size())
	{
		uint32_t dockIndex = dockOffset >= entries.size() ? entries.size() - 1U : static_cast<uint32_t>(dockOffset);
		const std::shared_ptr<guictr_layout_entry>& entry = entries[dockIndex];
		const guibase* entryHandle = entry.get()->getHandle();
		if (entryHandle) {
			if (rightSideHandle) {
				relPos = paddingTL(padding) + entryHandle->pos + ivec2(entryHandle->size.x - dropIndicatorWidth/2, 0);
			} else {
				relPos = paddingTL(padding) + entryHandle->pos + ivec2(-dropIndicatorWidth/2, 0);
			}
			relSize = ivec2(dropIndicatorWidth, entryHandle->size.y);
		} else {
			// not expected to be called, backup code path
			relPos = paddingTL(padding) + entry.get()->getGui()->pos - ivec2(FONT_SIZE_CTXT_SMALL+dropIndicatorWidth/2, 0);
			relSize = ivec2(dropIndicatorWidth, FONT_SIZE_CTXT_SMALL);
		}
		area->dockPos = dockPos;
		area->dockPosOffset = dockOffset;
		area->pos = toScreenSpace(relPos - paddingTL(padding));
		area->size = math::maxvec2(ivec2(relSize), ivec2(10, 10));
		return true;
	}
	return false;
}
g_layout_ctr_drop_area* guictr_layout::makeDropArea(int32_t idx) {
	auto& vec = dragdropContainerAreaHelpers;
	while (vec.size() <= idx) {
		vec.push_back(std::make_shared<g_layout_ctr_drop_area>(this));
	}
	return vec[idx].get();
}
void guictr_layout::getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& vecHandles) {
	if (!this->entries.size()) {
		setOverlayPos(makeDropArea(0), dock_pos::CENTER);
		vecHandles.push_back(dragdropContainerAreaHelpers[0]);
	} else {
		int32_t areaOffset = 0;
//		switch (this->ctrLayout) {
//			case container_layout::SPLIT_H:
//				makeOverlay(makeDropArea(0), dock_pos::TOP);
//				makeOverlay(makeDropArea(1), dock_pos::BOTTOM);
//				vecHandles.push_back(dragdropContainerAreaHelpers[0]);
//				vecHandles.push_back(dragdropContainerAreaHelpers[1]);
//				break;
//			case container_layout::SPLIT_V:
//				makeOverlay(makeDropArea(0), dock_pos::LEFT);
//				makeOverlay(makeDropArea(1), dock_pos::RIGHT);
//				vecHandles.push_back(dragdropContainerAreaHelpers[0]);
//				vecHandles.push_back(dragdropContainerAreaHelpers[1]);
//				break;
//			case container_layout::SOLE:
//				makeOverlay(makeDropArea(0), dock_pos::LEFT);
//				makeOverlay(makeDropArea(1), dock_pos::RIGHT);
//				makeOverlay(makeDropArea(2), dock_pos::TOP);
//				makeOverlay(makeDropArea(3), dock_pos::BOTTOM);
//				for (int i = 0; i < 4; i++) {
//					vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
//				}
//				break;
//			case container_layout::TABBED:
//				break;
//		}
		for (int i = 0; i < 4; i++) {
			setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT)+i));
			vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
			areaOffset++;
		}
		for (int i = 0; i < entries.size(); i++) {
			if (entries[i]->hasHandle) {
				setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, false);
				vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
				areaOffset++;
				if (i+1 == entries.size() || this->ctrLayout != container_layout::TABBED) {
					setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, true);
					vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
					areaOffset++;
				}

			}
		}
	}
}

void guictr_layout::layout() {
	ivec2 cs = getSizeContent();
	int32_t entryIdx = 0;
	int padding = cs != size ? 0 : 4;
	vec2 segSizeF = vec2(cs);
	vec2 axis = vec2(0);
	int32_t nEntries = math::max<int32_t>(1, entries.size());
	bool showHandles = true;
	switch (this->ctrLayout) {
	case container_layout::TABBED:
		showHandles = true;
		break;
	case container_layout::SOLE:
		break;
	case container_layout::SPLIT_V:
		segSizeF = vec2(cs.x / (float)nEntries, cs.y);
		axis.x = 1.0f;
		break;
	case container_layout::SPLIT_H:
		segSizeF = vec2(cs.x, cs.y / (float)nEntries);
		axis.y = 1.0f;
		break;
	}
	if (this->ctrLayout == container_layout::TABBED) {
		int paddingHandle = 1;
		int controlHeight = FONT_SIZE_CTXT;
		vec2 handleInset(6);
		vec2 segSizeF = vec2((vec2(cs)-handleInset*2.0f).x / (float)nEntries, (float)controlHeight);
		axis.x = 1;
		entryIdx = 0;
		for (auto &entry : entries) {
			auto *gui = entry->getGui();
			auto *guiHandle = entry->getHandle();
			gui->pos = ivec2(padding) + ivec2(0, controlHeight);
			gui->size = math::maxvec2(cs - ivec2(padding * 2) - ivec2(0, controlHeight), ivec2(4, 4));
			if (guiHandle && showHandles) {
				guiHandle->pos = ivec2(handleInset + (float)entryIdx * segSizeF * vec2(1, 0) + vec2(paddingHandle));
				guiHandle->size = ivec2(math::maxvec2f(vec2(segSizeF.x, controlHeight), vec2(4, 4)) - vec2(paddingHandle*2));
			}
			entryIdx++;
		}
	} else {

		for (auto &entry : entries) {
			int paddingHandle = padding;
			int controlHeight = entry->hasHandle ? FONT_SIZE_CTXT_SMALL : 0;
			auto *gui = entry->getGui();
			gui->pos = ivec2(vec2((float)entryIdx * segSizeF * axis) + vec2(0, controlHeight) + vec2(padding));
			gui->size = ivec2(math::maxvec2f(segSizeF - vec2(padding * 2) - vec2(0, controlHeight), vec2(4, 4)));
			auto *guiHandle = entry->getHandle();
			if (guiHandle && entry->hasHandle) {
				guiHandle->pos = ivec2(vec2((float)entryIdx * segSizeF * axis) + vec2(paddingHandle));
				guiHandle->size = ivec2(vec2(segSizeF.x, controlHeight) - vec2(paddingHandle*2));
			}
			entryIdx++;
		}
	}
	for (auto *gui : guis) {
		gui->determineSize(gui->size);
		//TODO: do not layout invisible (tabbed) entries
		gui->layout();
	}
}

void guictr_layout_entry_handle::handleDraggedBegin(MouseEvent &evt){
	hasDragged = false;
	if (!hasClicked) {
		hasClicked = true;
		parent->buttonClicked(this);
	}
}
void guictr_layout_entry_handle::handleDraggedMove(MouseEvent &evt) {
	dbgassert(!hasDragged);
	float fDist = math::distvec2(ivec2(0, 0), *evt.dragDistance);
	if (!hasDragged && (fDist > 2.0f)) {

		parentCtrl->dragContainerBegin(evt, parentCtr);
		hasDragged = true;
	} else if (!hasDragged) {
//		parent->buttonClicked(this);
	}
}
void guictr_layout_entry_handle::handleDraggedRelease(MouseEvent& evt) {
	if (parent)
		parent->buttonClicked(this);
}
void guictr_layout_entry_handle::render(NVGcontext* vg) {
		int32_t stateFlags = getStateFlags();
		nvgBeginPath(vg);
	//	nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, 3.0f);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, theme->getBgStrokeColor(stateFlags));
		nvgFill(vg);
//		renderWidgetBorder(vg, fl);
//		renderButtonLabel(vg, fl);
        String str = parentCtr->getGui()->label;
		if (drawFn || str.length()) {
			nvgSave(vg);
			setScissorTransform(vg);

			ivec2 renderPos(0);
			if (str.length() > 0) {
                int fontScale = 12;
                //math::round((this->fontSize > 0 ? this->fontSize : math::min(size.y, size.x)) * fFontScale);
				GuiColor::constant_t c = (stateFlags & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;

				//			nvgDawText(vg, this, pos, size, str, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				renderCenteredMultilineText(vg, theme, str, fontScale, c, renderPos, size);

			}

			if (drawFn) {
				drawFn(vg, renderPos, size, getBackgroundColor(getStateFlags()), drawParm, isEnabled());
			}
			nvgRestore(vg);
		}
	}

void i_ctr_drop_area::render(NVGcontext* vg) {
	nvgBeginPath(vg);
//	nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, 3.0f);
	nvgRect(vg, pos.x, pos.y, size.x, size.y);
	auto handleColor = rgbaToNvg(0x7f00ff00);
	switch (this->dockPos) {
	case dock_pos::STACK:
		handleColor = rgbaToNvg(0x7fffff00);
		break;
	}
	nvgFillColor(vg, handleColor);
	nvgFill(vg);
}


bool guictr_layout::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase *gui : handles) {
			if (!gui->isVisible())
				continue;

			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		for (guibase *gui : guis) {
			if (!gui->isVisible())
				continue;

			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			evt.requestFocus(this);
			return true;
		}
		if (canMouseHit()) {
			evt.requestFocus(this);
			return true;
		}
	}
	return false;
}

bool guictr_layout::getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) {
	auto it = std::find_if(this->entries.begin(), this->entries.end(), [ctr](auto& e) {
		return ctr == e.get();
	}
	);
	if (it == entries.end()) {
		//throw applogicexception(StringFormat("%s - attempt to remove non-present element", StringAsCStr(getClassName())));
		dbgassert(0);
		return false;
	}
	out = *it;
	if (!remove) {
		return true;
	}
	entries.erase(it);
	guictr_base::remove(ctr->getGui());
	auto *guiHandle = ctr->getHandle();
	if (guiHandle) {
		removeEntry(handles, guiHandle);
		guictr_base::remove(guiHandle);
	}
	ctr->parentLayoutContainer = nullptr;
	return true;
}

void guictr_layout::addEntry(std::shared_ptr<guictr_layout_entry> ctr, int32_t posOffset) {
	auto it = std::find(entries.begin(), entries.end(), ctr);
	if (it != entries.end()) {
		throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
	}
	auto insertPos =
			posOffset == -1 ?
					entries.begin() : (posOffset == -2 || posOffset >= entries.size() ? entries.end() : (entries.begin() + posOffset));
	entries.insert(insertPos, ctr);
	auto guiCtr = ctr->getGui();
	ctr->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
	guictr_base::add(guiCtr);
	auto* guiHandle = ctr->getHandle();
	if (guiHandle && ctr->hasHandle) {
		guictr_base::add(guiHandle);
		handles.push_back(guiHandle);
	}
	ctr->parentLayoutContainer = this;
}

bool guictr_layout::placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) {
//	auto itHelper = std::find_if(dragdropContainerAreaHelpers.begin(), dragdropContainerAreaHelpers.end(),
//			[area](std::shared_ptr<g_layout_ctr_drop_area>& e) {
//				return e.get() == area;
//			}
//	);
//	if (itHelper == dragdropContainerAreaHelpers.end()) {
//		throw applogicexception("pointer area does not point to a member of this container");
//	}
	// prevent dropping into self
	guibase *parent = this;
	while (parent) {
		if (parent == ctr.get()->getGui()) {
			return false;
		}
		parent = parent->parent;
	}
//	std::shared_ptr<g_layout_ctr_drop_area> &dropArea = *itHelper;
	dock_pos dockPos = area->dockPos;
	int32_t dockPosOffset = area->dockPosOffset;
	auto updatedCtrLayout = dock_pos_to_container_layout(dockPos);
	if (entries.size() > 1 && updatedCtrLayout != ctrLayout) {
		// introduce new layer
		log_printf("Introduce new layer\n", 0);
		//			std::make_shared<guictr_layout> ctrLayoutNewLayer;
		//			this->getContainerRef(ctr, out, remove)
		switch (dockPos) {
		case dock_pos::LEFT:
			// set container layout to SPLIT_V, make this container right entry, entry to drop left
			break;
		case dock_pos::RIGHT:
			break;
		case dock_pos::TOP:
			break;
		case dock_pos::BOTTOM:
			break;
		}
		return false;
	}
	auto it = std::find(entries.begin(), entries.end(), ctr);
	if (it != entries.end()) {
		throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
	}
	std::shared_ptr<guictr_layout_entry> out;
	if (ctr->parentLayoutContainer != nullptr) {
		auto prevParent = ctr->parentLayoutContainer;
		//undock from current container
		if (!ctr->getContainerRef(out, true)) {
			return false;
		}
		prevParent->postContentChanged();
	}
	//		hasRemovedContainer = true;
	//		dbgassert(ctrContent2);
	//		dbgassert(ctrContent == ctrContent2);
	bool updatedVisible = false;
	if (ctrLayout != updatedCtrLayout) {
		log_printf("Container layout changed to %d\n", static_cast<int>(updatedCtrLayout));
		setLayout(updatedCtrLayout);
		updatedVisible = true;
	}
	if (dockPos == dock_pos::STACK) {
		if (dockPosOffset <= -1) {
			entries.insert(entries.begin(), ctr);
		} else if (dockPosOffset >= entries.size()) {
			entries.insert(entries.end(), ctr);
		} else {
			entries.insert(entries.begin() + dockPosOffset, ctr);
		}
	} else if (dockPos == dock_pos::RIGHT || dockPos == dock_pos::BOTTOM) {
		entries.insert(entries.end(), ctr);
	} else {
		entries.insert(entries.begin(), ctr);
	}

	//		ctr->getGui()->setFlag(FLG_RENDER_LABEL, true);
	auto guiCtr = ctr->getGui();
	ctr->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
	guictr_base::add(guiCtr);
	auto* guiHandle = ctr->getHandle();

	if (guiHandle && ctr->hasHandle) {
		guictr_base::add(guiHandle);
		handles.push_back(guiHandle);
	}
	ctr->parentLayoutContainer = this;
	if (!updatedVisible) {
		updateVisible();
	}
	layout();
	return true;
}

std::shared_ptr<guictr_layout_entry> guictr_layout::replaceContainerWith(guictr_base* ctr,
		std::shared_ptr<guictr_layout> newContainer) {
	std::shared_ptr<guictr_layout> retCtr;
	auto it = std::find_if(entries.begin(), entries.end(), [ctr](std::shared_ptr<guictr_layout_entry>& e) {
		return e->getGui() == ctr;
	});
	if (it == entries.end()) {
		throw applogicexception(StringFormat("%s - attempt to remove missing ctr", StringAsCStr(getClassName())));
	}


	std::shared_ptr<guictr_layout_entry> entry = *it; // copy for return
	guictr_base::remove(entry->getGui());
	auto *guiHandle = entry->getHandle();
	if (guiHandle) {
		removeEntry(handles, guiHandle);
		guictr_base::remove(guiHandle);
	}
	entry->parentLayoutContainer = nullptr;
	entries.erase(it);

	std::shared_ptr<guictr_layout_entry> entry1 = std::make_shared<my_test_ctr>(newContainer);
	newContainer->setLabel("");
	int32_t posOffset = it - entries.begin();
	auto insertPos = entries.begin() + posOffset;

	entries.insert(insertPos, entry1);
	auto guiCtr = entry1->getGui();
	entry1->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
	guictr_base::add(guiCtr);
	guiHandle = entry1->getHandle();
	if (guiHandle && entry1->hasHandle) {
		guictr_base::add(guiHandle);
		handles.push_back(guiHandle);
	}
	entry1->parentLayoutContainer = this;

	return entry;
}
