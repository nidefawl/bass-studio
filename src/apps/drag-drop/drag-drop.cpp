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

#include "gui/container/guicontainer_dnd_layout.h"

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
guictr_base* makeCtrTheme();
guictr_base* makeCtrProperties();
class guictr_drag_test : public guictr_layout {
public:
	guictr_base* const ctr_properties;
	guictr_base* const ctr_theme;
	guictr_base* const ctr_theme2;
	guictr_base* const ctr_theme3;

#if BUILD_VSTHOST
	gui_ctr_debug ctrDebug;
#endif
	guictr_layout ctrLayout;
//    std::vector<std::shared_ptr<guictr_layout_entry>> entriesContainers;
    void addLayoutEntry(guictr_base* ctr, String title) {

		std::shared_ptr<guictr_layout_entry> entry1 = std::make_shared<my_test_ctr>(ctr);
		ctr->setLabel(title);
//        entriesContainers.push_back(entry1);
		addEntry(entry1);
    }
	guictr_drag_test() : guictr_layout(),
		ctr_properties(makeCtrProperties()),
		ctr_theme(makeCtrTheme()),
		ctr_theme2(makeCtrTheme()),
		ctr_theme3(makeCtrTheme())
#if BUILD_VSTHOST
		,ctrDebug(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2)
#endif
	{
		this->setLayout(container_layout::TABBED);
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
		delete ctr_properties;
		delete ctr_theme;
		delete ctr_theme2;
		delete ctr_theme3;
	}
};
class guictr_dnd_test : public guictr_base {
	guictr_drag_test ctrLayoutTest1;
	guictr_layout ctrLayoutTest2;
public:
	guictr_dnd_test() {
		ctrLayoutTest1.setLabel("Layout Ctr 1");
		ctrLayoutTest2.setLabel("Layout Ctr 2");
		add(&ctrLayoutTest1);
		add(&ctrLayoutTest2);
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
		ctrLayoutTest1.pos = { cs.x*2 / 3, 0 };
		ctrLayoutTest1.size.x = cs.x/3;
		ctrLayoutTest1.size.y = cs.y;
		ctrLayoutTest2.pos = { cs.x*1/3, 0 };
		ctrLayoutTest2.size.x = cs.x/3;
		ctrLayoutTest2.size.y = cs.y;
		for (auto* gui : guis) {
			gui->layout();
		}
	}

};
guictr_base* makeDnDTestCtr() {
	return new guictr_dnd_test();
}
