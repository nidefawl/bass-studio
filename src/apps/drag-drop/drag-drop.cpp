
#include <vector>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#include "math/vec.h"
#include "math/seq_math.h"

#include "str_util.h"
#include "color_util.h"

#include "mouse.h"
#include "event.h"
#include "exceptions.h"
#include "renderresources.h"

#include "basectrl.h"
//#include "host/mainctrl.h"

#include "guiglobals.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/knob.h"

#include "gui/guicontainer.h"
#include "gui/guicontainer_layout.h"
#include "gui/guicolorpick.h"
#include "gui/guiinputfield.h"
#include "gui/button.h"
#include "gui/splitter.h"
#if BUILD_VSTHOST
#include "gui/debugctr.h"
#endif

class guictr_dragged_container_instance;

/**
 * TODO:
 * remove previewLayout. let getOverlay also handle preview
 */

/**
 * Replace all container entries with guictr_layout:
 * in implementation of determineTarget:
 *  check mouse collisions depth first.
 *  for each gui:
 *  	if gui.mouseHitTest:
 *  		if gui is instance of i_ctr_layout:
 *  			requestFocus(gui)
 *  		if gui->parent is instance of i_ctr_layout:
 *  			requestFocus(gui)
 *
 * in getOverlay:
 * target = determineTarget(evt)
 * bool replaceInsideContainer = target not instance of i_ctr_layout and target->parent instance of i_ctr_layout
 *
 * if replaceInsideContainer:
 *   guictr* ctrToReplaceWithGuyLayoutInstance = target
 *   iCtrLayoutInstance = target->parent
 *   return replace overlay
 * else:
 *   return move overlay
 *
 *
 */

class guictr_layout;
class guictr_layout_dock_overlay_impl : public guictr_layout_dock_overlay {
	public:
		class gui_dock_overlay_control : public guibutton {
			const dock_pos dockpos;
		public:
			gui_dock_overlay_control(dock_pos _dockpos) : dockpos(_dockpos) {

			}
			dock_pos getDockPos() const {
				return dockpos;
			}
			void render(NVGcontext* vg) {
				guibutton::render(vg);
			}

		};
	public:
		std::vector<gui_dock_overlay_control*> controls;
	private:
		void addControl(gui_dock_overlay_control* ctrl) {
			add(ctrl);
			controls.push_back(ctrl);
		}
	public:
		guictr_layout* parentguictrlayout=nullptr;
		guictr_layout_dock_overlay_impl() {
			guis.reserve(5);
			controls.reserve(5);
			addControl(new gui_dock_overlay_control(dock_pos::CENTER));
			addControl(new gui_dock_overlay_control(dock_pos::LEFT));
			addControl(new gui_dock_overlay_control(dock_pos::RIGHT));
			addControl(new gui_dock_overlay_control(dock_pos::TOP));
			addControl(new gui_dock_overlay_control(dock_pos::BOTTOM));
		}
		~guictr_layout_dock_overlay_impl() {
			destroyGuis();
		}
		void setPositions(std::vector<dock_pos> positions) {
			for (auto* dockCtrl : controls) {
				dockCtrl->setVisible(STL_CONTAINS(positions, dockCtrl->getDockPos()));
			}
		}
		void layout() {
			ivec2 cs = getSizeContent();
			ivec2 wh(32);
			ivec2 spacing(4);
			for (auto* dockCtrl : controls) {
				dockCtrl->size = wh;
				switch (dockCtrl->getDockPos()) {
				case dock_pos::CENTER:
					dockCtrl->pos = ivec2(cs/2-wh/2);
					break;
				case dock_pos::LEFT:
					dockCtrl->pos = ivec2(cs/2-wh/2-ivec2(wh.x+spacing.x, 0));
					break;
				case dock_pos::RIGHT:
					dockCtrl->pos = ivec2(cs/2-wh/2+ivec2(wh.x+spacing.x, 0));
					break;
				case dock_pos::TOP:
					dockCtrl->pos = ivec2(cs/2-wh/2-ivec2(0, wh.y+spacing.y));
					break;
				case dock_pos::BOTTOM:
					dockCtrl->pos = ivec2(cs/2-wh/2+ivec2(0, wh.y+spacing.y));
					break;
				default:
					dbgassert(0 && "Unhandled switch case");
					break;
				}
			}
			for (guibase* gui : guis) {
				gui->layout();
			}
		}
//		bool previewLayout(guictr_dragged_container_instance* layoutGui, MouseEvent& evt) override;
};
class g_layout_ctr_drop_area : public i_ctr_drop_area {
public:
	dock_pos dockPos;
	g_layout_ctr_drop_area(i_ctr_layout* _parent, dock_pos _dockPos) : i_ctr_drop_area(_parent), dockPos(_dockPos) {
	}
	~g_layout_ctr_drop_area() {

	}
};
class guictr_layout : public guictr_base, public i_ctr_layout {
public:
private:
	container_layout ctrLayout = container_layout::SOLE;
	std::vector<std::shared_ptr<guictr_layout_entry>> entries;
	std::vector<guibase*> handles;
	std::shared_ptr<guictr_layout_dock_overlay_impl> ctrDockOverlay;
	std::vector<std::shared_ptr<g_layout_ctr_drop_area>> dragdropContainerAreaHelpers;
public:
	guictr_layout() : guictr_base() {
		setBackgroundRendered(false);
		setBackgroundRenderedInset(true);
		this->setCanMouseHit(true);
		ctrDockOverlay = std::make_shared<guictr_layout_dock_overlay_impl>();
		ctrDockOverlay->parentguictrlayout = this;
		dragdropContainerAreaHelpers.resize(16);
		margin = 0;
		padding = 0;
	}
	virtual ~guictr_layout() {
		removeGuis();
		entries.clear();
	}
	dock_pos getHitPos(MouseEvent& evt);
	std::vector<std::shared_ptr<guictr_layout_entry>>& getEntries() {
		return entries;
	}
	void simplify() {
		if (entries.size() < 2)
			this->ctrLayout = container_layout::SOLE;
	}
	void layout() {
		ivec2 cs = size;
		int32_t pos = 0;
		int controlH = 8;
		int padding = 4;
//		int margin = CTR_SPACING;
		ivec2 segSize = cs;
		ivec2 axis = ivec2(0, 0);
		int32_t nEntries = math::max<int32_t>(1, entries.size());

		switch (this->ctrLayout) {
		case container_layout::SOLE:
			break;
		case container_layout::SPLIT_V:
			segSize = ivec2(cs.x/nEntries, cs.y);
			axis.x = 1;
			break;
		case container_layout::SPLIT_H:
			segSize = ivec2(cs.x, cs.y/nEntries);
			axis.y = 1;
			break;

		}
		for (auto& entry : entries) {
			auto* gui = entry->getGui();
            gui->pos = pos * segSize * axis + ivec2(padding) + ivec2(0, controlH);
			gui->size = segSize - ivec2(padding*2) - ivec2(0, controlH);
			auto* guiHandle = entry->getHandle();
			if (guiHandle) {
				guiHandle->pos = gui->pos + ivec2(controlH, -controlH);
				guiHandle->size = ivec2(gui->size.x-controlH*2, controlH);
			}
			pos++;
		}
		for (auto* gui : guis) {
			gui->layout();
		}
	}
	dock_pos getDockPosition(ivec2 mpos) {
		const int snapDiv = 3;
		switch (this->ctrLayout) {
		case container_layout::SOLE:
			if (entries.empty())
				return dock_pos::CENTER;
			/* no break */
		case container_layout::SPLIT_V:
			if (mpos.x < size.x/snapDiv)
				return dock_pos::LEFT;
			if (mpos.x >= size.x-size.x/snapDiv)
				return dock_pos::RIGHT;
			break;
		case container_layout::SPLIT_H:
			if (mpos.y < size.y/snapDiv)
				return dock_pos::TOP;
			if (mpos.y >= size.y-size.y/snapDiv)
				return dock_pos::BOTTOM;
			break;
		default:
			break;
//			return dock_pos::CENTER;
		}
		return dock_pos::NONE;
	}

	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : handles) {
				if (!gui->isVisible())
					continue;
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			for (guibase* gui : guis) {
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
    container_layout getLayout() const override {
    	return this->ctrLayout;
    }
    bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) override
    {
		auto it = std::find_if(this->entries.begin(), this->entries.end(), [ctr](auto& e){
			return ctr == e.get();
		});
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
		auto* guiHandle = ctr->getHandle();
		if (guiHandle) {
			removeEntry(handles, guiHandle);
			guictr_base::remove(guiHandle);
		}
		ctr->parentLayoutContainer = nullptr;
        simplify();
		layout();
		return true;
	}
//	void handleDraggedBegin(MouseEvent &evt) {
//		hasDragged = false;
//	}
//	void handleDraggedMove(MouseEvent &evt) {
//		dbgassert(!hasDragged);
//		if (!hasDragged && (evt.dragDistance->x != 0 || evt.dragDistance->y != 0)) {
//			if (entries.size() > 1) {
//				for (int32_t idx = 0; idx < entries.size(); ++idx) {
//					guictr_tabbed_draggable::tabbed_entry *entry = entries[idx];
//					if (this->activeEntry != entry) {
//						tabbed_entry* entryActive = this->activeEntry;
//						setActiveEntry(idx);
//						parentCtrl->dragContainerBegin(evt, entryActive->tabCtr);
//						hasDragged = true;
//					}
//				}
//			}
////			parentCtrl->dragContainer(this);
//		}
//	}
	container_layout dock_pos_to_container_layout(dock_pos pos) {
		switch (pos) {
			case dock_pos::TOP:
			case dock_pos::BOTTOM:
				return container_layout::SPLIT_H;
			case dock_pos::LEFT:
			case dock_pos::RIGHT:
				return container_layout::SPLIT_V;
			default:
				break;
		}
		return container_layout::SOLE;
	}
	bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) override {
		auto itHelper = std::find_if(dragdropContainerAreaHelpers.begin(), dragdropContainerAreaHelpers.end(), [area](std::shared_ptr<g_layout_ctr_drop_area>& e) {
			return e.get() == area;
		});
		if (itHelper == dragdropContainerAreaHelpers.end()) {
			throw applogicexception("pointer area does not point to a member of this container");
		}
		std::shared_ptr<g_layout_ctr_drop_area>& dropArea = *itHelper;
		dock_pos dockPos = dropArea->dockPos;

		auto it = std::find(entries.begin(), entries.end(), ctr);
		if (it != entries.end()) {
			throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
		}
		auto updatedCtrLayout = dock_pos_to_container_layout(dockPos);
		if (ctrLayout != updatedCtrLayout) {
			log_printf("Container layout changed to %d\n", static_cast<int>(updatedCtrLayout));
		}
		ctrLayout = updatedCtrLayout;
		if (dockPos == dock_pos::RIGHT || dockPos == dock_pos::BOTTOM) {
			entries.insert(entries.end(), ctr);
		} else {
			entries.insert(entries.begin(), ctr);
		}
//		ctr->getGui()->setFlag(FLG_RENDER_LABEL, true);
		guictr_base::add(ctr->getGui());
		auto* guiHandle = ctr->getHandle();
		if (guiHandle) {
			guictr_base::add(guiHandle);
			handles.push_back(guiHandle);
		}
		ctr->parentLayoutContainer = this;
		layout();

		return true;
	}

	void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) override;

#if 0
	bool previewLayout(guictr_dragged_container_instance* layoutGui, MouseEvent& evt) override {
		dock_pos currentDockPos = dock_pos::NONE;
		auto mpos = toControlsObjectSpace(evt.mousepos, this);
		if (currentDockPos == dock_pos::NONE) {
			currentDockPos = getDockPosition(mpos);
		}
		if (ctrDockOverlay->parentCtrl) {
			ivec2 localMouse = ctrDockOverlay->toContainerSpace(evt.mousepos);

			for (auto* dockCtrl : ctrDockOverlay->controls) {
				if (dockCtrl->contains(localMouse) || (currentDockPos != dock_pos::NONE&&currentDockPos==dockCtrl->getDockPos())) {
					currentDockPos = dockCtrl->getDockPos();
					dockCtrl->setButtonColor(GuiColor::COL_GUI_HANDLE_FOCUSED);
				} else {
					dockCtrl->setButtonColor(GuiColor::COL_GUI_HANDLE);
				}
			}
		}
		ivec2 relPos = ivec2(0,0);
		ivec2 relSize = size;
		switch (currentDockPos) {
		case dock_pos::LEFT:
			relPos = ivec2(0, 0);
			relSize = ivec2(size.x / 2, size.y);
			break;
		case dock_pos::RIGHT:
			relPos = ivec2(size.x / 2, 0);
			relSize = ivec2(size.x / 2, size.y);
			break;
		case dock_pos::TOP:
			relPos = ivec2(0, 0);
			relSize = ivec2(size.x, size.y / 2);
			break;
		case dock_pos::BOTTOM:
			relPos = ivec2(0, size.y / 2);
			relSize = ivec2(size.x, size.y / 2);
			break;
		case dock_pos::CENTER:
			relPos = ivec2(0, 0);
			relSize = ivec2(size.x, size.y);
			break;
		default:
			return false;
		}
		layoutGui->dockPos = currentDockPos;
		layoutGui->dummyPreview.pos = this->toScreenSpace(relPos);
		layoutGui->dummyPreview.size = relSize;
		return true;
	}
#endif
};
struct my_test_ctr;
class guictr_layout_entry_handle : public guibutton {
	my_test_ctr* const parentCtr;
	guictr_base* const ctr;
	bool hasDragged = false;
public:
	guictr_layout_entry_handle(my_test_ctr* _parentCtr, guictr_base* _ctr) : parentCtr(_parentCtr), ctr(_ctr) {
		setText("...");
//		setEnabledRef(&active);
////		setFontScale(0.7f);
	}
	~guictr_layout_entry_handle() = default;
	void handleDraggedBegin(MouseEvent &evt) override;
	void handleDraggedMove(MouseEvent &evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	int32_t getStateFlags() const override {
		int32_t state = guibuttonbase::getStateFlags();
//		if (active()) {
//			state |= FLG_ACT;
//		}
		return state;
	}
	void render(NVGcontext* vg) override;
};
struct my_test_ctr : public guictr_layout_entry {
	guictr_base* const ctr; /* non-owning */
	guictr_layout_entry_handle* const ctrHandle;
	my_test_ctr(guictr_base* _ctr) : ctr(_ctr), ctrHandle(new guictr_layout_entry_handle(this, _ctr)) {

		ctrHandle->setLabel("...");
	}
	~my_test_ctr() {
		delete ctrHandle;
	}
	guibase* getGui() override {
		return ctr;
	}
	guibase* getHandle() override {
		return ctrHandle;
	}
	std::shared_ptr<guictr_layout_entry> duplicateContainer() override {
		return nullptr;
	}
	bool getContainerRef(std::shared_ptr<guictr_layout_entry>& out, bool remove) override {
        return parentLayoutContainer->getContainerRef(this, out, remove);
	}
};
void guictr_layout_entry_handle::handleDraggedBegin(MouseEvent &evt){
	hasDragged = false;
}
void guictr_layout_entry_handle::handleDraggedMove(MouseEvent &evt) {
	dbgassert(!hasDragged);
	if (!hasDragged && (evt.dragDistance->x != 0 || evt.dragDistance->y != 0)) {

		parentCtrl->dragContainerBegin(evt, parentCtr);
		hasDragged = true;
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
	nvgFillColor(vg, rgbToNvg(0xff00ff00));
	nvgFill(vg);
}
class guibutton_drag: public guibutton {
public:
	guibutton_drag() :
			guibutton() {

	}
};
class guictr_tabbed_draggable;
class guictr_layout_dock_overlay_tabbed_impl : public guictr_layout_dock_overlay {
	public:
		class gui_dock_overlay_control : public guibutton {
			const dock_pos dockpos;
		public:
			gui_dock_overlay_control(dock_pos _dockpos) : dockpos(_dockpos) {

			}
			dock_pos getDockPos() const {
				return dockpos;
			}
			void render(NVGcontext* vg) {
				guibutton::render(vg);
			}

		};
		int32_t hitPosTabList = -1;

		dock_pos hitPosDockPos = dock_pos::NONE;
	public:
		std::vector<gui_dock_overlay_control*> controls;
	private:
		void addControl(gui_dock_overlay_control* ctrl) {
			add(ctrl);
			controls.push_back(ctrl);
		}
	public:
		guictr_tabbed_draggable* parentGuictrTabbedDraggable=nullptr;
		guictr_layout_dock_overlay_tabbed_impl() {
			guis.reserve(5);
			controls.reserve(5);
			addControl(new gui_dock_overlay_control(dock_pos::CENTER));
			addControl(new gui_dock_overlay_control(dock_pos::LEFT));
			addControl(new gui_dock_overlay_control(dock_pos::RIGHT));
			addControl(new gui_dock_overlay_control(dock_pos::TOP));
			addControl(new gui_dock_overlay_control(dock_pos::BOTTOM));
		}
		~guictr_layout_dock_overlay_tabbed_impl() {
			destroyGuis();
		}
		void setPositions(std::vector<dock_pos> positions) {
			for (auto* dockCtrl : controls) {
				dockCtrl->setVisible(STL_CONTAINS(positions, dockCtrl->getDockPos()));
			}
		}
		void layout() {
			ivec2 cs = getSizeContent();
			ivec2 wh(32);
			ivec2 spacing(4);
			for (auto* dockCtrl : controls) {
				dockCtrl->size = wh;
				switch (dockCtrl->getDockPos()) {
				case dock_pos::CENTER:
					dockCtrl->pos = ivec2(cs/2-wh/2);
					break;
				case dock_pos::LEFT:
					dockCtrl->pos = ivec2(cs/2-wh/2-ivec2(wh.x+spacing.x, 0));
					break;
				case dock_pos::RIGHT:
					dockCtrl->pos = ivec2(cs/2-wh/2+ivec2(wh.x+spacing.x, 0));
					break;
				case dock_pos::TOP:
					dockCtrl->pos = ivec2(cs/2-wh/2-ivec2(0, wh.y+spacing.y));
					break;
				case dock_pos::BOTTOM:
					dockCtrl->pos = ivec2(cs/2-wh/2+ivec2(0, wh.y+spacing.y));
					break;
				default:
					dbgassert(0 && "Unhandled switch case");
					break;
				}
			}
			for (guibase* gui : guis) {
				gui->layout();
			}
		}

};
class g_tabbed_ctr_drop_area : public i_ctr_drop_area {
public:
	int32_t insertPos = -1;
	g_tabbed_ctr_drop_area(i_ctr_layout* _parent) : i_ctr_drop_area(_parent) {

	}
	~g_tabbed_ctr_drop_area() {

	}
};
class guictr_tabbed_draggable : public guictr_base, public i_ctr_layout {
public:
	struct tabbed_entry {
		std::shared_ptr<guictr_layout_entry> tabCtr;
		guibase* tabbedEntryHandle;
		bool active = false;
		tabbed_entry(std::shared_ptr<guictr_layout_entry> _ctr, String title) : tabCtr(_ctr), tabbedEntryHandle(_ctr->getHandle()) {
//			tabButton.setText(title);
//			tabButton.setEnabledRef(&active);
//			tabButton.setFontScale(0.7f);
////			tabButton.setDragRendered(true);
		}
	};
	std::vector<tabbed_entry*> entries;
	tabbed_entry* activeEntry = nullptr;
	ivec2 insetMenuBar{0};
	bool hasDragged = false;
	std::shared_ptr<guictr_layout_dock_overlay_tabbed_impl> ctrDockOverlay;
	std::vector<std::shared_ptr<g_tabbed_ctr_drop_area>> dragdropContainerAreaHelpers;
public:
	ivec2 posContentTab{0};
	ivec2 sizeContentTab{0};
	std::vector<tabbed_entry*> getEntries() {
		return entries;
	}

	guictr_tabbed_draggable() : guictr_base() {
		setCanMouseHit(true);
		ctrDockOverlay = std::make_shared<guictr_layout_dock_overlay_tabbed_impl>();
		ctrDockOverlay->parentGuictrTabbedDraggable = this;
		dragdropContainerAreaHelpers.resize(16);
	}
	~guictr_tabbed_draggable() {
		for (tabbed_entry *entry : entries) {
			remove(entry->tabbedEntryHandle);
			delete entry;
		}
		// only this->activeEntry->tabCtr should be in this cointainer
		// at this point. And it must be a valid pointer
		dbgassert(guis.size() <= 1);
		removeGuis();
	}
	dock_pos getHitPos(MouseEvent& evt);
	void setTabMenuInset(ivec2 offset) {
		this->insetMenuBar = offset;
	}
	int32_t getNumEntries() {
		return entries.size();
	}
	void setActiveEntry(int32_t idx) {
		if (idx >= 0 && idx < entries.size()) {
			guictr_tabbed_draggable::tabbed_entry *entry = entries[idx];
			if (this->activeEntry) {
				this->activeEntry->active = false;
				this->removeUNCHECKED(this->activeEntry->tabCtr->getGui());
			}
			this->activeEntry = entry;
			this->activeEntry->active = true;
			this->add(this->activeEntry->tabCtr->getGui());
			if (this->parentCtrl) {
				this->layout();
			}
        }
        else if (!entries.size() && this->activeEntry) {
            this->activeEntry->active = false;
            this->removeUNCHECKED(this->activeEntry->tabCtr->getGui());
            this->activeEntry = nullptr;
            if (this->parentCtrl) {
                this->layout();
            }
        }
	}
	void addEntry(std::shared_ptr<guictr_layout_entry> _ctr, String title) {
		guictr_tabbed_draggable::tabbed_entry *entry = new guictr_tabbed_draggable::tabbed_entry { _ctr, title };
		guictr_base::add(entry->tabbedEntryHandle);
		_ctr->parentLayoutContainer = this;
		this->entries.push_back(entry);
	}
	void buttonClicked(guibase *button) override
	{
		auto it = std::find_if(entries.begin(), entries.end(), [button](const guictr_tabbed_draggable::tabbed_entry *entry) {
			return entry->tabbedEntryHandle == button;
		}
		);
		if (it != entries.end()) {
			size_t pos = it - entries.begin();
			setActiveEntry((int32_t) (pos));
		}
		if (parent) {
			parent->buttonClicked(button);
		}
	}
	void simplify() {
	}
	void layout() override
	{
		const int32_t CONST_LAYOUT_MARGIN = math::max(0, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
		ivec2 csize = getSizeContent();
		int nEntries = entries.size();
		int csW = csize.x - insetMenuBar.x;
		int sizePer = nEntries ? (csW) / nEntries : csW;
		ivec2 posHandle(insetMenuBar);
		for (tabbed_entry *entry : entries) {
			entry->tabbedEntryHandle->pos = ivec2(posHandle.x+CONST_LAYOUT_MARGIN / 2, insetMenuBar.y);
			entry->tabbedEntryHandle->size = ivec2(sizePer - CONST_LAYOUT_MARGIN, HEIGHT_DEFAULT_INPUT);
			posHandle.x += sizePer;
			posHandle.y = math::max(posHandle.y, entry->tabbedEntryHandle->bottom());
			entry->tabbedEntryHandle->layout();
		}
		posHandle.y+=CONST_LAYOUT_MARGIN;
		posContentTab = ivec2(0, posHandle.y);
		sizeContentTab = ivec2(csize.x, csize.y - posHandle.y);
		for (tabbed_entry *entry : entries) {
			if (entry->active) {
				auto entryGui = entry->tabCtr->getGui();
				if (entryGui->parent) {
					entryGui->pos = posContentTab;
					entryGui->size = sizeContentTab;
					entryGui->determineSize(entryGui->size);
					entryGui->layout();
				}
			}
		}
	}
	//void guictr_base::render(NVGcontext* vg) {
	//	if (isBackgroundRendered()) {
	//		renderBackground(vg);
	//	}
	//	if (!setScissorTransform(vg)) {
	//		return;
	//	}
	//	for (auto c : guis) {
	//		if (c->size == ivec2{0, 0}) {
	//			log_printf("warning, rendering container with size 0 0\n", 0);
	//		} else {
	//			nvgSave(vg);
	//			c->render(vg);
	//			nvgRestore(vg);
	//		}
	//	}
	//}
	void render(NVGcontext *vg) override
	{
		guictr_base::render(vg);
	}
	void handleDraggedBegin(MouseEvent &evt) {
		hasDragged = false;
	}
	void handleDraggedMove(MouseEvent &evt) {
		dbgassert(!hasDragged);
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}

	bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) override
    {
		auto it = std::find_if(entries.begin(), entries.end(), [ctr](const tabbed_entry* entry) {
			return entry->tabCtr.get() == ctr;
		});
        if (it == entries.end()) {
            dbgassert(0);
            //throw applogicexception(StringFormat("%s - attempt to remove non-present element", StringAsCStr(getClassName())));
            return false;
		}
		tabbed_entry* tabEntry = *it;
        out = tabEntry->tabCtr;
        if (!remove) {
            return true;
		}
        bool wasActiveEntry = (activeEntry == tabEntry);
		guictr_base::remove(tabEntry->tabbedEntryHandle);
		entries.erase(it);
		guictr_base::remove(ctr->getGui());
		ctr->parentLayoutContainer = nullptr;
        if (wasActiveEntry) {
            setActiveEntry(0);
        }
        delete tabEntry;
        simplify();
		layout();
		return true;
	}
	bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) override {

		auto itHelper = std::find_if(dragdropContainerAreaHelpers.begin(), dragdropContainerAreaHelpers.end(), [area](std::shared_ptr<g_tabbed_ctr_drop_area>& e) {
			return e.get() == area;
		});
		if (itHelper == dragdropContainerAreaHelpers.end()) {
			throw applogicexception("pointer area does not point to a member of this container");
		}

		auto it = std::find_if(entries.begin(), entries.end(), [pCtr=ctr.get()](const tabbed_entry* entry) {
			return entry->tabCtr.get() == pCtr;
		});
		if (it != entries.end()) {
			throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
		}


		int32_t insertPos = itHelper->get()->insertPos;
		if (insertPos == -1)
			insertPos = entries.size();
		if (insertPos > -1) {
			guictr_tabbed_draggable::tabbed_entry *entry = new guictr_tabbed_draggable::tabbed_entry { ctr, ctr->getGui()->label };
			guictr_base::add(entry->tabbedEntryHandle);
			if (this->entries.size() >= insertPos) {
				this->entries.insert(entries.begin()+insertPos, entry);
			} else {
				this->entries.push_back(entry);
			}
			ctr->parentLayoutContainer = this;
			layout();
			return true;
		}

		return false;

	}


	void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) override;
    container_layout getLayout() const override {
    	return container_layout::TABBED;
    }
#if 0
	bool previewLayout(guictr_dragged_container_instance* layoutGui, MouseEvent& evt) override {
			dock_pos currentDockPos = dock_pos::CENTER;
			auto mpos = toControlsObjectSpace(evt.mousepos, this);
	//		if (currentDockPos == dock_pos::NONE) {
	//			currentDockPos = getDockPosition(mpos);
	//		}
			if (ctrDockOverlay->parentCtrl) {
				ivec2 localMouse = ctrDockOverlay->toContainerSpace(evt.mousepos);

				for (auto* dockCtrl : ctrDockOverlay->controls) {
					if (dockCtrl->contains(localMouse) || (currentDockPos != dock_pos::NONE&&currentDockPos==dockCtrl->getDockPos())) {
						currentDockPos = dockCtrl->getDockPos();
						dockCtrl->setButtonColor(GuiColor::COL_GUI_HANDLE_FOCUSED);
					} else {
						dockCtrl->setButtonColor(GuiColor::COL_GUI_HANDLE);
					}
				}
			}
			ivec2 relPos = ivec2(0,0);
			ivec2 relSize = size;
			switch (currentDockPos) {
			case dock_pos::LEFT:
				relPos = ivec2(0, 0);
				relSize = ivec2(size.x / 2, size.y);
				break;
			case dock_pos::RIGHT:
				relPos = ivec2(size.x / 2, 0);
				relSize = ivec2(size.x / 2, size.y);
				break;
			case dock_pos::TOP:
				relPos = ivec2(0, 0);
				relSize = ivec2(size.x, size.y / 2);
				break;
			case dock_pos::BOTTOM:
				relPos = ivec2(0, size.y / 2);
				relSize = ivec2(size.x, size.y / 2);
				break;
			case dock_pos::CENTER:
				relPos = ivec2(0, 0);
				relSize = ivec2(size.x, size.y);
				break;
			default:
				return false;
			}
	//		layoutGui->dockPos = currentDockPos;
			layoutGui->dummyPreview.pos = this->toScreenSpace(relPos);
			layoutGui->dummyPreview.size = relSize;
			return true;
		}
#endif
};

guictr_base* makeCtrTheme();
guictr_base* makeCtrProperties();
class guictr_drag_test : public guictr_tabbed_draggable {
public:
	guictr_base* const ctr_properties;
	guictr_base* const ctr_theme;
	guictr_base* const ctr_theme2;
	guictr_base* const ctr_theme3;

#if BUILD_VSTHOST
	gui_ctr_debug ctrDebug;
#endif
	guictr_layout ctrLayout;
    std::vector<std::shared_ptr<guictr_layout_entry>> entriesContainers;
    void addLayoutEntry(guictr_base* ctr, String title) {

		std::shared_ptr<guictr_layout_entry> entry1 = std::make_shared<my_test_ctr>(ctr);
		ctr->setLabel(title);
        entriesContainers.push_back(entry1);
		addEntry(entry1, ctr->label);
    }
	guictr_drag_test() : guictr_tabbed_draggable(),
		ctr_properties(makeCtrProperties()),
		ctr_theme(makeCtrTheme()),
		ctr_theme2(makeCtrTheme()),
		ctr_theme3(makeCtrTheme())
#if BUILD_VSTHOST
		,ctrDebug(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2)
#endif
	{
		addLayoutEntry(ctr_theme, "Theme 1");
		addLayoutEntry(ctr_properties, "Properties");
		addLayoutEntry(ctr_theme2, "Theme 2");
		addLayoutEntry(ctr_theme3, "Theme 3");
#if BUILD_VSTHOST
		addLayoutEntry(&ctrDebug, "Debug");
#endif
		addLayoutEntry(&ctrLayout, "Layout");

		setActiveEntry(0);
	}
	virtual ~guictr_drag_test() {
		std::vector<tabbed_entry*> entries = getEntries();
		for (tabbed_entry* ctr : entries) {
			remove(ctr->tabCtr->getGui());
			if (ctr->tabbedEntryHandle) {
				remove(ctr->tabbedEntryHandle);
			}
		}
		delete ctr_properties;
		delete ctr_theme;
		delete ctr_theme2;
		delete ctr_theme3;
	}
};
class guictr_dnd_test : public guictr_base {
	guictr_drag_test ctrDragTest;
	guictr_layout ctrLayoutTest;
public:
	guictr_dnd_test() {
		ctrDragTest.setLabel("Tabbed Ctr");
		ctrLayoutTest.setLabel("Layout Ctr");
		add(&ctrDragTest);
		add(&ctrLayoutTest);
		padding = 0;
		margin = 0;
	}
	~guictr_dnd_test() {
		removeGuis();
	}
	void buttonClicked(guibase* button) {
	}
	void render(NVGcontext* vg) {
		if (isBackgroundRendered()){
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		BaseCtrl *ctrl = this->parentCtrl;

		std::vector<String> strings;
		String str;
		str = ctrl->guiOver ? ctrl->guiOver->getClassName() : "<null>";
		strings.push_back(String("guiOver: ") + str);
		str = ctrl->guiDragged ? ctrl->guiDragged->getClassName() : "<null>";
		strings.push_back(String("guiDragged: ") + str);
		str = ctrl->guiCaptured ? ctrl->guiCaptured->getClassName() : "<null>";
		strings.push_back(String("guiCaptured: ") + str);
		str = ctrl->guiCtrFocused ? ctrl->guiCtrFocused->getClassName() : "<null>";
		strings.push_back(String("guiCtrFocused: ") + str);
		str = ctrl->guiFocused ? ctrl->guiFocused->getClassName() : "<null>";
		strings.push_back(String("guiFocused: ") + str);

		guibase* p = ctrl->guiFocused;
		int lvl = 0;
		while (p != NULL) {
			String s = "";
			if (lvl == 0) {
				s = "guiFocused: ";
			}
			for (int i = 0; i < lvl; i++) {
				s += "  ";
			}
			strings.push_back(s + p->getClassName());
			p = p->parent;
			lvl++;
		}
		int x = 5;

		setFont(vg, 14, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		float lineh;
		nvgTextMetrics(vg, NULL, NULL, &lineh);


		nvgText(vg, x, 0, StringAsCStr(label), NULL);
		int y = lineh*3;
		for (String& s : strings) {
			nvgText(vg, x, y, StringAsCStr(s), NULL);
			y += lineh;
		}
		for (auto c : guis) {
			if (c->size == ivec2{0, 0}) {
				log_printf("warning, rendering container with size 0 0\n", 0);
			} else {
				nvgSave(vg);
				c->render(vg);
				nvgRestore(vg);
			}
		}

	}
	void layout() {
		ivec2 cs = getSizeContent();
		ctrDragTest.pos = { cs.x*2 / 3, 0 };
		ctrDragTest.size.x = cs.x/3;
		ctrDragTest.size.y = cs.y;
		ctrLayoutTest.pos = { cs.x*1/3, 0 };
		ctrLayoutTest.size.x = cs.x/3;
		ctrLayoutTest.size.y = cs.y;
		for (auto* gui : guis) {
			gui->layout();
		}
	}

};
guictr_base* makeDnDTestCtr() {
	return new guictr_dnd_test();
}

void guictr_tabbed_draggable::getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& vecHandles) {
	auto mpos = toControlsObjectSpace(evt.mousepos, this);

	if (mpos.y < HEIGHT_DEFAULT_INPUT) {
		if (!dragdropContainerAreaHelpers[0]) {
			dragdropContainerAreaHelpers[0] = std::make_shared<g_tabbed_ctr_drop_area>(this);
		}
		g_tabbed_ctr_drop_area* area = dragdropContainerAreaHelpers[0].get();
		tabbed_entry* hit = nullptr;
		bool wasRightTopHit = false;
		int32_t insertPos = 0;
		if (entries.size()) {
			// find the closest hit
			hit = nullptr;
			int32_t dist = 0;
			for (tabbed_entry* entry : entries) {
				auto* handle = entry->tabbedEntryHandle;
				dbgassert(handle);

				float distLT = math::distvec2(handle->getLeftTop(), mpos);
				if (hit == nullptr || distLT < dist) {
					hit = entry;
					dist = distLT;
					wasRightTopHit = false;
				}
				float distRT = math::distvec2(handle->getRightTop(), mpos);
				if (hit == nullptr || distRT < dist) {
					hit = entry;
					dist = distRT;
					wasRightTopHit = true;
				}
			}
		}
		if (hit) {
			insertPos = indexOfCtr(entries, hit);
			auto* handle = hit->tabbedEntryHandle;
			dbgassert(handle);
			const int32_t dropIndicatorWidth = 20;
			area->pos = toScreenSpace(wasRightTopHit?handle->getRightTop():handle->getLeftTop()) - ivec2(dropIndicatorWidth/2, 0);
			area->size = ivec2(dropIndicatorWidth, handle->size.y);
		} else {
			area->pos = toScreenSpace(ivec2(0));
			area->size = ivec2(size.x, HEIGHT_DEFAULT_INPUT);
		}
		area->insertPos = insertPos;
		vecHandles.push_back(dragdropContainerAreaHelpers[0]);
	} else {
		dragdropContainerAreaHelpers[0] = nullptr;
	}

//	vecHandles
//
//
//	std::vector<dock_pos> positions;
//
////	if (parentGuictrTabbedDraggable->entries.size()) {
//		positions.push_back(dock_pos::CENTER);
//		positions.push_back(dock_pos::LEFT);
//		positions.push_back(dock_pos::RIGHT);
//		positions.push_back(dock_pos::TOP);
//		positions.push_back(dock_pos::BOTTOM);
////	}
//
//	dock_pos currentDockPos = dock_pos::NONE;
//	auto& tabEntries = entries;
//
//	layoutGui->dockPos = currentDockPos;
//	layoutGui->boxes.clear();
//	layoutGui->validPreview = false;
//	int32_t hitPostTab = -1;
//    guibase* tabbedEntryHandleHit = nullptr;
//    int32_t minDist = -1;
//    if (mpos.y < HEIGHT_DEFAULT_INPUT) {
//    	if (tabEntries.size()) {
//    		int32_t pos = 0;
//    		for (auto* tabEntry : tabEntries) {
//    			auto tabCtrl = tabEntry->tabbedEntryHandle;
//    			if (tabCtrl) {
//    				ivec2 posMiddle = tabCtrl->pos;
//    				auto mDist = glm::distance(vec2(posMiddle), vec2(mpos));
//    				if (minDist == -1 || minDist > mDist) {
//    					minDist = mDist;
//    					tabbedEntryHandleHit = tabCtrl;
//    					hitPostTab = pos;
//    				}
//    			}
//    			pos++;
//    		}
//    	    if (tabEntries.size() && tabEntries.back()->tabbedEntryHandle) {
//    	        ivec2 posMiddle = tabEntries.back()->tabbedEntryHandle->getRightTop();
//    	        auto mDist = glm::distance(vec2(posMiddle), vec2(mpos));
//    	        if (minDist == -1 || minDist > mDist) {
//    	            minDist = mDist;
//    	            tabbedEntryHandleHit = tabEntries.back()->tabbedEntryHandle;
//    	            hitPostTab = tabEntries.size();
//    	        }
//    	    }
//    	}
//    	ivec2 tabInsertPos = parentGuictrTabbedDraggable->toScreenSpace(ivec2(0, 0));
//    	ivec2 tabInsertSize = ivec2(getSizeContent().x, HEIGHT_DEFAULT_INPUT);
//    	if (tabbedEntryHandleHit) {
//    		if (hitPosTabList == tabEntries.size()) {
//    			tabInsertPos = parentGuictrTabbedDraggable->toScreenSpace(tabEntries.back()->tabbedEntryHandle->getRightTop());
//    		} else {
//    			tabInsertPos = parentGuictrTabbedDraggable->toScreenSpace(tabbedEntryHandleHit->pos);
//    		}
//    		tabInsertSize = tabbedEntryHandleHit->size;
//            tabInsertSize.x = 4;
//        }
//        layoutGui->validPreview = true; //TODO: minDist < 4
//    	layoutGui->boxes.push_back(ivec4(tabInsertPos, tabInsertSize));
//    } else {
//    	// content hit
//    	hitPosTabList = 0;
//        layoutGui->validPreview = true;
//    	layoutGui->boxes.push_back(ivec4(parentGuictrTabbedDraggable->toScreenSpace(parentGuictrTabbedDraggable->posContentTab), parentGuictrTabbedDraggable->sizeContentTab));
//
//    }
//	hitPosTabList = hitPostTab;
//	hitPosDockPos = currentDockPos;
//
//
//
//	return true;
}
//bool guictr_layout_dock_overlay_tabbed_impl::previewLayout(guictr_dragged_container_instance* layoutGui, MouseEvent& evt) {
//	return false;
//}
dock_pos guictr_layout::getHitPos(MouseEvent& evt) {
	auto mpos = toControlsObjectSpace(evt.mousepos, this);

	const int snapDiv = 3;
	switch (getLayout()) {
	case container_layout::TABBED:
	case container_layout::SOLE:
		return dock_pos::NONE;
		//				if (entries.empty())
//								return dock_pos::CENTER;
//				/* no break */
	case container_layout::SPLIT_V:
		if (mpos.x < size.x/snapDiv)
			return dock_pos::LEFT;
		if (mpos.x >= size.x-size.x/snapDiv)
			return dock_pos::RIGHT;
		break;
	case container_layout::SPLIT_H:
		if (mpos.y < size.y/snapDiv)
			return dock_pos::TOP;
		if (mpos.y >= size.y-size.y/snapDiv)
			return dock_pos::BOTTOM;
		break;
	default:
		break;
//			return dock_pos::CENTER;
	}
	return dock_pos::NONE;

}
dock_pos guictr_tabbed_draggable::getHitPos(MouseEvent& evt) {
	auto mpos = toControlsObjectSpace(evt.mousepos, this);

	const int snapDiv = 3;
	switch (getLayout()) {
	case container_layout::TABBED:
	case container_layout::SOLE:
		return dock_pos::NONE;
		//				if (entries.empty())
//								return dock_pos::CENTER;
//				/* no break */
	case container_layout::SPLIT_V:
		if (mpos.x < size.x/snapDiv)
			return dock_pos::LEFT;
		if (mpos.x >= size.x-size.x/snapDiv)
			return dock_pos::RIGHT;
		break;
	case container_layout::SPLIT_H:
		if (mpos.y < size.y/snapDiv)
			return dock_pos::TOP;
		if (mpos.y >= size.y-size.y/snapDiv)
			return dock_pos::BOTTOM;
		break;
	default:
		break;
//			return dock_pos::CENTER;
	}
	return dock_pos::NONE;

}

void guictr_layout::getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& vecHandles) {

	auto mpos = toControlsObjectSpace(evt.mousepos, this);
	ctrDockOverlay->pos = this->toScreenSpace(ivec2(0, 0));
	ctrDockOverlay->size = this->size;
	std::vector<dock_pos> positions;

	if (!this->entries.size()) {
		positions.push_back(dock_pos::CENTER);
	} else {
		switch (this->ctrLayout) {
			case container_layout::SPLIT_H:
				positions.push_back(dock_pos::TOP);
				positions.push_back(dock_pos::BOTTOM);
				break;
			case container_layout::SPLIT_V:
				positions.push_back(dock_pos::LEFT);
				positions.push_back(dock_pos::RIGHT);
				break;
			case container_layout::SOLE:
				positions.push_back(dock_pos::LEFT);
				positions.push_back(dock_pos::RIGHT);
				positions.push_back(dock_pos::TOP);
				positions.push_back(dock_pos::BOTTOM);
				break;
		}
	}
	const int32_t dropIndicatorWidth = 20;
	for (dock_pos dockPos : positions) {

		ivec2 relPos = ivec2(0,0);
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
		default:
			break;
	//		layoutGui->validPreview = false;
	//		layoutGui->boxes.clear();
	//		return false;
		}
		int32_t idx = static_cast<int32_t>(dockPos);
		if (!dragdropContainerAreaHelpers[idx]) {
			dragdropContainerAreaHelpers[idx] = std::make_shared<g_layout_ctr_drop_area>(this, dockPos);
		}
		g_layout_ctr_drop_area* area = dragdropContainerAreaHelpers[idx].get();
		area->pos = toScreenSpace(relPos);
		area->size = ivec2(relSize);
		vecHandles.push_back(dragdropContainerAreaHelpers[idx]);
	}
//	if (mpos.y < HEIGHT_DEFAULT_INPUT) {
//		if (!dragdropContainerAreaHelpers[0]) {
//			dragdropContainerAreaHelpers[0] = std::make_shared<g_layout_ctr_drop_area>();
//		}
//		g_layout_ctr_drop_area* area = dragdropContainerAreaHelpers[0].get();
//		tabbed_entry* hit = nullptr;
//		bool wasRightTopHit = false;
//		if (entries.size()) {
//			// find the closest hit
//			hit = nullptr;
//			int32_t dist = 0;
//			for (tabbed_entry* entry : entries) {
//				auto* handle = entry->tabbedEntryHandle;
//				dbgassert(handle);
//
//				float distLT = math::distvec2(handle->getLeftTop(), mpos);
//				if (hit == nullptr || distLT < dist) {
//					hit = entry;
//					dist = distLT;
//					wasRightTopHit = false;
//				}
//				float distRT = math::distvec2(handle->getRightTop(), mpos);
//				if (hit == nullptr || distRT < dist) {
//					hit = entry;
//					dist = distRT;
//					wasRightTopHit = true;
//				}
//			}
//		}
//		if (hit) {
//			auto* handle = hit->tabbedEntryHandle;
//			dbgassert(handle);
//			const int32_t dropIndicatorWidth = 20;
//			area->pos = toScreenSpace(wasRightTopHit?handle->getRightTop():handle->getLeftTop()) - ivec2(dropIndicatorWidth/2, 0);
//			area->size = ivec2(dropIndicatorWidth, handle->size.y);
//		} else {
//			area->pos = toScreenSpace(pos);
//			area->size = ivec2(size.x, HEIGHT_DEFAULT_INPUT);
//		}
//		vecHandles.push_back(dragdropContainerAreaHelpers[0]);
//	} else {
//		dragdropContainerAreaHelpers[0] = nullptr;
//	}
//		ctrDockOverlay->setPositions(positions);
//		ctrDockOverlay->layout();
//		ivec2 cs = getSizeContent();
//		return ctrDockOverlay;
#if 0
	std::vector<dock_pos> positions;

	if (!getEntries().size()) {
		positions.push_back(dock_pos::CENTER);
	} else {
		switch (getLayout()) {
			case container_layout::SPLIT_H:
				positions.push_back(dock_pos::TOP);
				positions.push_back(dock_pos::BOTTOM);
				break;
			case container_layout::SPLIT_V:
				positions.push_back(dock_pos::LEFT);
				positions.push_back(dock_pos::RIGHT);
				break;
			case container_layout::TABBED:
			case container_layout::SOLE:
				positions.push_back(dock_pos::LEFT);
				positions.push_back(dock_pos::RIGHT);
				positions.push_back(dock_pos::TOP);
				positions.push_back(dock_pos::BOTTOM);
				break;
		}
	}


	dock_pos currentDockPos = dock_pos::NONE;
	auto mpos = toControlsObjectSpace(evt.mousepos, this);
	if (currentDockPos == dock_pos::NONE) {
		currentDockPos = getHitPos(evt);
	}
	if (parentCtrl) {
		ivec2 localMouse = toContainerSpace(evt.mousepos);

		for (auto* dockCtrl : controls) {
			dockCtrl->setButtonColor(GuiColor::COL_GUI_HANDLE);
			if (!dockCtrl->isVisible()) {
				continue;
			}
			if (dockCtrl->contains(localMouse) || (currentDockPos != dock_pos::NONE && currentDockPos == dockCtrl->getDockPos())) {
				currentDockPos = dockCtrl->getDockPos();
				dockCtrl->setButtonColor(GuiColor::COL_GUI_HANDLE_FOCUSED);
			}
		}
	}
	ivec2 relPos = ivec2(0,0);
	ivec2 relSize = size;
	switch (currentDockPos) {
	case dock_pos::LEFT:
		relPos = ivec2(0, 0);
		relSize = ivec2(size.x / 2, size.y);
		break;
	case dock_pos::RIGHT:
		relPos = ivec2(size.x / 2, 0);
		relSize = ivec2(size.x / 2, size.y);
		break;
	case dock_pos::TOP:
		relPos = ivec2(0, 0);
		relSize = ivec2(size.x, size.y / 2);
		break;
	case dock_pos::BOTTOM:
		relPos = ivec2(0, size.y / 2);
		relSize = ivec2(size.x, size.y / 2);
		break;
	case dock_pos::CENTER:
		relPos = ivec2(0, 0);
		relSize = ivec2(size.x, size.y);
		break;
	default:
//		layoutGui->validPreview = false;
//		layoutGui->boxes.clear();
//		return false;
	}
//	layoutGui->validPreview = true;
//	layoutGui->boxes.clear();
//	layoutGui->boxes.push_back();
//	layoutGui->dockPos = currentDockPos;
//	return true;
#endif
}
