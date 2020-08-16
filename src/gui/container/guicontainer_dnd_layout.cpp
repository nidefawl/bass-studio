#include "guicontainer_dnd_layout.h"


static const int32_t dropIndicatorWidth = 20;
int controlH = HEIGHT_DEFAULT_INPUT;
bool guictr_layout::makeOverlay(g_layout_ctr_drop_area* area, const dock_pos dockPos, const int32_t dockOffset = -1) {
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
		if (dockOffset > -1 && dockOffset <= entries.size())
		{
			uint32_t dockIndex = dockOffset >= entries.size() ? entries.size() - 1U : static_cast<uint32_t>(dockOffset);
			const std::shared_ptr<guictr_layout_entry>& entry = entries[dockIndex];
			const guibase* entryHandle = entry.get()->getHandle();

			if (dockOffset >= entries.size()) {
				relPos = paddingTL(padding) + entryHandle->pos + ivec2(entryHandle->size.x - dropIndicatorWidth/2, 0);
			} else {
				relPos = paddingTL(padding) + entryHandle->pos + ivec2(-dropIndicatorWidth/2, 0);
			}
			relSize = ivec2(dropIndicatorWidth, entryHandle->size.y);
			break;
		}
		return false;
	default:
		return false;
//		layoutGui->validPreview = false;
//		layoutGui->boxes.clear();
//		return false;
	}
	area->dockPos = dockPos;
	area->dockPosOffset = dockOffset;
	area->pos = toScreenSpace(relPos - paddingTL(padding));
	area->size = math::maxvec2(ivec2(relSize), ivec2(10, 10));
	return true;
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
		makeOverlay(makeDropArea(0), dock_pos::CENTER);
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
			makeOverlay(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT)+i));
			vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
			areaOffset++;
		}
		for (int i = 0; i <= entries.size(); i++) {
			makeOverlay(makeDropArea(areaOffset), dock_pos::STACK, i);
			vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
			areaOffset++;
		}
	}
}

void guictr_layout::layout() {
	ivec2 cs = getSizeContent();
	int32_t entryIdx = 0;
	int padding = cs != size ? 0 : 4;
	int paddingHandle = 1;
	//		int margin = CTR_SPACING;
	ivec2 segSize = cs;
	ivec2 axis = ivec2(0, 0);
	int32_t nEntries = math::max<int32_t>(1, entries.size());
	switch (this->ctrLayout) {
	case container_layout::TABBED:
		break;
	case container_layout::SOLE:
		break;
	case container_layout::SPLIT_V:
		segSize = ivec2(cs.x / nEntries, cs.y);
		axis.x = 1;
		break;
	case container_layout::SPLIT_H:
		segSize = ivec2(cs.x, cs.y / nEntries);
		axis.y = 1;
		break;
	}
	if (this->ctrLayout == container_layout::TABBED) {
		segSize = ivec2(cs.x / nEntries, controlH);
		axis.x = 1;
		entryIdx = 0;
		for (auto &entry : entries) {
			auto *gui = entry->getGui();
			auto *guiHandle = entry->getHandle();
			gui->pos = ivec2(padding) + ivec2(0, controlH);
			gui->size = math::maxvec2(cs - ivec2(padding * 2) - ivec2(0, controlH), ivec2(4, 4));
			if (guiHandle) {
				guiHandle->pos = entryIdx * segSize * axis + ivec2(paddingHandle);
				guiHandle->size = math::maxvec2(ivec2(segSize.x, controlH), ivec2(4, 4))-ivec2(paddingHandle*2);
			}
			entryIdx++;
		}
	} else {

		for (auto &entry : entries) {
			auto *gui = entry->getGui();
			gui->pos = entryIdx * segSize * axis + ivec2(padding) + ivec2(0, controlH);
			gui->size = math::maxvec2(segSize - ivec2(padding * 2) - ivec2(0, controlH), ivec2(4, 4));
			auto *guiHandle = entry->getHandle();
			if (guiHandle) {
				guiHandle->pos = gui->pos + ivec2(controlH, -controlH);
				guiHandle->size = ivec2(gui->size.x - controlH * 2, controlH);
			}
			entryIdx++;
		}
	}
	for (auto *gui : guis) {
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
	auto handleColor = rgbToNvg(0xff00ff00);
	switch (this->dockPos) {
	case dock_pos::STACK:
		handleColor = rgbToNvg(0xffffff00);
		break;
	}
	nvgFillColor(vg, handleColor);
	nvgFill(vg);
}
