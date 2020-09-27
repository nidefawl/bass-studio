#include "guicontainer_dnd_layout.h"
#include "basectrl.h"
#include "../host/mainctrl.h"
#include "gui/container/guicontainer_layout_types.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>


static const int32_t dropIndicatorWidth = 8;

bool guictr_layout::setOverlayPos(i_ctr_drop_area* area, const dock_pos dockPos, ivec2 overlayPos, ivec2 overlaySize, int32_t dockPosOfffset, int32_t childContainerIndex)
{
    ivec2 relPos = overlayPos;
    ivec2 relSize = overlaySize;
	switch (dockPos) {
	case dock_pos::LEFT:
        relPos += ivec2(-dropIndicatorWidth / 2, 0);
		relSize = ivec2(overlaySize.x/3+dropIndicatorWidth, overlaySize.y);
		break;
	case dock_pos::RIGHT:
        relPos += ivec2(overlaySize.x * 2 / 3 - dropIndicatorWidth / 2, 0);
		relSize = ivec2(overlaySize.x/3+dropIndicatorWidth, overlaySize.y);
		break;
	case dock_pos::TOP:
        relPos += ivec2(0, -dropIndicatorWidth / 2);
		relSize = ivec2(overlaySize.x, overlaySize.y/3+dropIndicatorWidth);
		break;
	case dock_pos::BOTTOM:
        relPos += ivec2(0, overlaySize.y * 2 / 3 - dropIndicatorWidth / 2);
		relSize = ivec2(overlaySize.x, overlaySize.y/3+dropIndicatorWidth);
		break;
	case dock_pos::CENTER:
		relPos += ivec2(dropIndicatorWidth/2, dropIndicatorWidth/2);
		relSize = ivec2(overlaySize.x-dropIndicatorWidth, overlaySize.y-dropIndicatorWidth);
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
	area->childContainerIndex = childContainerIndex;
	area->pos = toScreenSpace(relPos - paddingTL(padding));
	area->size = math::maxvec2(ivec2(relSize), ivec2(10, 10));
	area->label = "DockPos "+std::to_string(static_cast<int32_t>(area->dockPos))+" of "+StringFormat("%8X", reinterpret_cast<uint64_t>(this));
	return true;
}
bool guictr_layout::setOverlayPosForTab(i_ctr_drop_area* area, const dock_pos dockPos, const int32_t dockOffset, const bool rightSideHandle) {
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
i_ctr_drop_area* guictr_layout::makeDropArea(int32_t idx) {
	auto& vec = dragdropContainerAreaHelpers;
	while (vec.size() <= idx) {
		vec.push_back(std::make_shared<i_ctr_drop_area>(this));
	}
	vec[idx]->childContainerIndex = -1;
	vec[idx]->dockPosOffset = -1;
	return vec[idx].get();
}
void guictr_layout::getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& vecHandles) {
	if (!this->entries.size()) {
		setOverlayPos(makeDropArea(0), dock_pos::CENTER, ivec2(0), size, -1, -1);
		vecHandles.push_back(dragdropContainerAreaHelpers[0]);
	} else {
		int32_t areaOffset = 0;
        for (int i = 0; i < entries.size(); i++) {
            if (entries[i]->hasHandle) {
                setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, false);
                vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                areaOffset++;
                if ((size_t)i + 1 == entries.size() || this->ctrLayout != container_layout::TABBED) {
                    setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, true);
                    vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                    areaOffset++;
                }
            }
        }
        if (ctrLayout == container_layout::SPLIT_H || ctrLayout == container_layout::SPLIT_V || ctrLayout == container_layout::SOLE) {
            // return overlays for all 4 sides of each container entry that is not of type guictr_layout
            for (int i = 0; i < entries.size(); i++) {
				for (int j = 0; j < 4; j++) {
					setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT) + j), entries[i]->pos, entries[i]->size, -1, i);
					vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
					areaOffset++;
				}
            }
        }
        if (ctrLayout == container_layout::SPLIT_V) {
            setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT)), ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
            areaOffset++;
            setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::RIGHT)), ivec2(0), size, entries.size(), -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
            areaOffset++;
        }
        if (ctrLayout == container_layout::SPLIT_H) {
            setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::TOP)), ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
            areaOffset++;
            setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::BOTTOM)), ivec2(0), size, entries.size(), -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
            areaOffset++;
        }
	}
}

void guictr_layout::layout() {
	ivec2 cs = getSizeContent();
	int32_t entryIdx = 0;
	int padding = cs != size ? 0 : 0;
	if (parentCtrl && parentCtrl->isDraggingContainer()) {
		padding = 4;
	}
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
		vec2 handleInset(0);
		vec2 segSizeF = vec2((vec2(cs)-handleInset*2.0f).x / (float)nEntries, (float)controlHeight);
		axis.x = 1;
		entryIdx = 0;
		for (auto &entry : entries) {
			auto *gui = entry->getGui();
			auto *guiHandle = entry->getHandle();
			entry->pos = gui->pos = ivec2(padding) + ivec2(0, controlHeight);
			entry->size = gui->size = math::maxvec2(cs - ivec2(padding * 2) - ivec2(0, controlHeight), ivec2(4, 4));
			if (guiHandle && showHandles) {
				guiHandle->pos = ivec2(handleInset + (float)entryIdx * segSizeF * vec2(1, 0) + vec2(paddingHandle, 0));
				guiHandle->size = ivec2(math::maxvec2f(vec2(segSizeF.x, controlHeight), vec2(4, 4)) - vec2(paddingHandle*2, 0));
			}
			entryIdx++;
		}
	} else {

		for (auto &entry : entries) {
			int paddingHandle = padding;
			int controlHeight = entry->hasHandle ? FONT_SIZE_CTXT_SMALL : 0;
			auto *gui = entry->getGui();
			entry->pos = gui->pos = ivec2(vec2((float)entryIdx * segSizeF * axis) + vec2(0, controlHeight) + vec2(padding));
			entry->size = gui->size = ivec2(math::maxvec2f(segSizeF - vec2(padding * 2) - vec2(0, controlHeight), vec2(4, 4)));
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
	nvgFillColor(vg, GUI_COLORRGB(30, 255, 30, 255));
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
	nvgFillColor(vg, rgbaToNvg(0xffffffff));

	renderText(vg, pos.x, pos.y+size.y/2, 300, StringAsCStr(this->label));
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
	// a handle is present if the child is not a guictr_layout, or if this container is using tabbed layout
	ctr->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
	guictr_base::add(guiCtr);
	guiCtr->snapSides = ivec4(1);
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
	std::shared_ptr<guictr_layout_entry> out;
	if (ctr->parentLayoutContainer != nullptr) {
		auto prevParent = ctr->parentLayoutContainer;
		//undock from current container
		if (!ctr->getContainerRef(out, true)) {
			return false;
		}
//		prevParent->postContentChanged();
	}
	auto it = std::find(entries.begin(), entries.end(), ctr);
	if (it != entries.end()) {
		throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
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
	guiCtr->snapSides = ivec4(1);
	auto* guiHandle = ctr->getHandle();

	if (guiHandle && ctr->hasHandle) {
		guictr_base::add(guiHandle);
		handles.push_back(guiHandle);
	}
	ctr->parentLayoutContainer = this;
	//if (!updatedVisible) {
	//	updateVisible();
	//}
	//layout();
	return true;
}
void guictr_layout::render(NVGcontext* vg)
{
		if (!isVisible()) {
			log_printf("warning, skip rendering container with state !isVisible()\n", 0);
			return;
		}
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (parentCtrl->isDraggingContainer()) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgStrokeWidth(vg, 1.0f);
			nvgStrokeColor(vg, G_WHITE);
			nvgStroke(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		if (this->id&(1<<16)) {
			for (auto h : handles) {
				int32_t stateFlags = getStateFlags();
				nvgBeginPath(vg);
			//	nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, 3.0f);
				nvgRect(vg, h->pos.x, h->pos.y, h->size.x, h->size.y);
				nvgFillColor(vg, rgbaToNvg(0x7f00ff00));
				nvgFill(vg);
			}
			for (auto e : entries) {
				auto h = e->getGui();
				if (!h) continue;
				int32_t stateFlags = getStateFlags();
				nvgBeginPath(vg);
			//	nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, 3.0f);
				nvgRect(vg, h->pos.x, h->pos.y, h->size.x, h->size.y);
				nvgFillColor(vg, rgbaToNvg(0x7fffff00));
				nvgFill(vg);
			}
		}
		for (auto c : guis) {
			if (!c->isVisible()) {
	//			log_printf("warning, skip rendering child container with state !isVisible()\n", 0);
				continue;
			}
			if (c->size.x <= 0 || c->size.y <= 0) {
				log_printf("warning, skip rendering child container with size <= 0 0\n", 0);
				continue;
			}
			{
				nvgSave(vg);
				c->render(vg);
				nvgRestore(vg);
			}
		}
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

    int32_t posOffset = it - entries.begin();
	entries.erase(it);

	std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(newContainer);
	auto insertPos = entries.begin() + posOffset;

	entries.insert(insertPos, entry1);
	auto guiCtr = entry1->getGui();
	entry1->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
	guictr_base::add(guiCtr);
	guiCtr->snapSides = ivec4(1);
	guiHandle = entry1->getHandle();
	if (guiHandle && entry1->hasHandle) {
		guictr_base::add(guiHandle);
		handles.push_back(guiHandle);
	}
	entry1->parentLayoutContainer = this;

	return entry;
}

guictr_layout_entry::guictr_layout_entry(String _label, std::shared_ptr<guictr_base> _ctr)
    : type(_ctr->getContainerType()), frameType(_ctr->getContainerType() == CTR_TYPE_LAYOUT ? layout_ctr_type::GUICTR_LAYOUT : layout_ctr_type::GUICTR_BASE), ctr(_ctr), label(_label) {
	ctrHandle = new guictr_layout_entry_handle(this, _ctr.get());
//		if (dynamic_cast<guictr_layout*>(_ctr.get()) == nullptr) {
//			ctrHandle->setLabel("...");
//		} else {
//			ctrHandle = nullptr;
//		}
}
guictr_layout_entry::~guictr_layout_entry() {
	delete ctrHandle;
}

guibase* guictr_layout_entry::getHandle() {
	return ctrHandle;
}

bool guictr_layout_entry::getContainerRef(std::shared_ptr<guictr_layout_entry>& out, bool remove) {
	return parentLayoutContainer->getContainerRef(this, out, remove);
}

guictr_base* guictr_layout_entry::getGui() {
	return ctr.get();
}

void loadContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot);
void storeContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot);

void storeContainerEntrySnapshot(guictr_layout_entry* ctrlayoutEntry, std::shared_ptr<guictrlayout_entry_snapshot_t>& snapshot) {
	dbgassert(ctrlayoutEntry->getType() != container_type::CTR_TYPE_BASE);
	if (ctrlayoutEntry->getFrameType() == layout_ctr_type::GUICTR_LAYOUT) {
		auto sharedSnapshot = std::make_shared<guictrlayout_snapshot_t>();
		guictr_layout* ctrLayout = dynamic_cast<guictr_layout*>(ctrlayoutEntry->getGui());
		dbgassert(ctrLayout);
		storeContainerSnapshot(ctrLayout, sharedSnapshot.get());
		snapshot = sharedSnapshot;
	} else {
		auto sharedSnapshot = std::make_shared<guictrlayout_entry_snapshot_t>();
		snapshot = sharedSnapshot;
	}
	snapshot->type = ctrlayoutEntry->getType();
	snapshot->label = ctrlayoutEntry->getLabel();
}


void loadContainerEntrySnapshot(std::shared_ptr<guictrlayout_entry_snapshot_t>& snapshot, std::shared_ptr<guictr_layout_entry>& out) {
	auto& fac = getContainerFactory();
	out = nullptr;
	if (fac.count(snapshot->type)) {
		ContainerBuilder& builder = fac[snapshot->type];
		std::shared_ptr<guictr_base> sharedContainer = builder();
		if (!sharedContainer) {
			log_printf("Failed building container of type %d\n", snapshot->type);
			return;
		}
		sharedContainer->label = snapshot->label;
		out = createGuiCtrLayoutEntry(sharedContainer);
		if (out->getFrameType() == layout_ctr_type::GUICTR_LAYOUT) {
			guictrlayout_snapshot_t* ctrLayoutSnapshot = dynamic_cast<guictrlayout_snapshot_t*>(snapshot.get());
			guictr_layout* ctrLayout = dynamic_cast<guictr_layout*>(out->getGui());
			dbgassert(ctrLayout);
			dbgassert(ctrLayoutSnapshot);
			loadContainerSnapshot(ctrLayout, ctrLayoutSnapshot);
		}
	} else {
		log_printf("Failed loading container of type %d\n", snapshot->type);
	}
}

void writeStringStream(const String& path, Stringstream& sstream);
bool saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path) {
	using namespace cereal;
	try {
		Stringstream sstream;
		{
			JSONOutputArchive ar(sstream);
			ar(make_nvp("layout", snapshot));
		}
		sstream.flush();
		writeStringStream(path, sstream);
		return true;
	}
	catch (const FileIOException& e)
	{
		log_printf("savePluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		log_printf("savePluginSnapshot exception: %s\n", e.what());
	}
	return false;
}

std::shared_ptr<dawview_layout_t> loadDawViewLayoutSnapshot(const String& path) {
	using namespace cereal;
	try {
		std::vector<uint8_t> vec;
		ReadFileVector(path, vec);
		Stringstream sstream(std::string(vec.begin(), vec.end()));
		std::shared_ptr<dawview_layout_t> snapshot = std::make_shared<dawview_layout_t>();
		dawview_layout_t& ref = *snapshot.get();
		{
			JSONInputArchive ar(sstream);
			ar(make_nvp("layout", ref));
		}
		return snapshot;
	}
	catch (const FileIOException& e)
	{
		log_printf("loadPluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		log_printf("loadPluginSnapshot exception: %s\n", e.what());
	}
    return nullptr;
}

void storeContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot) {
	auto& entries = ctrlayout->getEntries();
	snapshot->activePosition = ctrlayout->getActivePosition();
	snapshot->ctrLayout = ctrlayout->getLayout();
	snapshot->entries.reserve(entries.size());
	for (auto& sharedEntry : entries) {
		std::shared_ptr<guictrlayout_entry_snapshot_t> shrdEntrySnapshot;
		storeContainerEntrySnapshot(sharedEntry.get(), shrdEntrySnapshot);
		snapshot->entries.emplace_back(std::move(shrdEntrySnapshot));
	}
}
void loadContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot) {
	ctrlayout->setLayout(snapshot->ctrLayout);
	for (auto& shrdEntrySnapshot : snapshot->entries) {
		std::shared_ptr<guictr_layout_entry> sharedEntry;
		loadContainerEntrySnapshot(shrdEntrySnapshot, sharedEntry);
		if (sharedEntry) {
			ctrlayout->addEntry(sharedEntry, -2);
		}
		ctrlayout->setActiveEntry(snapshot->activePosition);
//		ctrlayout->postContentChanged();
	}
}
template<class Archive>
void serialize(Archive & archive, guictrlayout_snapshot_t & m)
{
	archive(m.label, m.type, m.activePosition, m.ctrLayout, m.entries);
}
template<class Archive>
void serialize(Archive & archive, guictrlayout_entry_snapshot_t & m)
{
	archive(m.label, m.type);
}
template<class Archive>
void serialize(Archive & archive, dawview_layout_t & m)
{
	archive(m.left, m.right, m.splitterPositions);
}
CEREAL_REGISTER_TYPE(guictrlayout_snapshot_t);
CEREAL_REGISTER_POLYMORPHIC_RELATION(guictrlayout_entry_snapshot_t, guictrlayout_snapshot_t)
