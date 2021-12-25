#include <vector>
#include <memory>
#include <algorithm>

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
#include "gui/container/guicontainer_dnd_layout.h"

/**
 * TODO:
 * remove previewLayout. let getOverlay also handle preview
 */
guictr_base* makeCtrTheme();
guictr_base* makeCtrProperties();

template <typename T>
void addLayoutEntry(T& t, std::shared_ptr<guictr_base> ctr, String title) {
    ctr->setLabel(title);
    std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(ctr);
    t->addEntry(entry1);
}
std::shared_ptr<guictr_layout> makeDragTestCtr() {
    auto ctr = std::make_shared<guictr_layout>();
    ctr->setLayout(container_layout::TABBED);
    addLayoutEntry(ctr, std::shared_ptr<guictr_base>(makeCtrTheme()), "Theme 1");
    addLayoutEntry(ctr, std::shared_ptr<guictr_base>(makeCtrProperties()), "Properties");
    addLayoutEntry(ctr, std::shared_ptr<guictr_base>(makeCtrTheme()), "Theme 2");
    addLayoutEntry(ctr, std::shared_ptr<guictr_base>(makeCtrTheme()), "Theme 3");
#if BUILD_VSTHOST
    addLayoutEntry(ctr, std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2), "Theme 3");
#endif
    //	addLayoutEntry(ctr, std::make_shared<guictr_layout>(), "guictr_layout");

    ctr->setActiveEntry(0);
    return ctr;
}
class guictr_dnd_test : public guictr_base {
    std::shared_ptr<guictr_layout> ctrLayoutTest1;
    std::shared_ptr<guictr_layout> ctrLayoutTest2;

public:
    guictr_dnd_test() {
        ctrLayoutTest1 = makeDragTestCtr();
        ctrLayoutTest2 = makeDragTestCtr();
        ctrLayoutTest1->setLabel("Layout Ctr 1");
        ctrLayoutTest2->setLabel("Layout Ctr 2");
        add(ctrLayoutTest1.get());
        add(ctrLayoutTest2.get());
        padding = 0;
        margin  = 0;
    }
    ~guictr_dnd_test() { removeGuis(); }
    void buttonClicked(guibase* button) {}
    void render(NVGcontext* vg) {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        BaseCtrl* ctrl = this->parentCtrl;

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
        int lvl    = 0;
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
        int y = lineh * 3;
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
        ctrLayoutTest1->postContentChanged();
        ctrLayoutTest2->postContentChanged();
        ivec2 cs               = getSizeContent();
        ctrLayoutTest1->pos    = {cs.x * 2 / 3, 0};
        ctrLayoutTest1->size.x = cs.x / 3;
        ctrLayoutTest1->size.y = cs.y;
        ctrLayoutTest2->pos    = {cs.x * 1 / 3, 0};
        ctrLayoutTest2->size.x = cs.x / 3;
        ctrLayoutTest2->size.y = cs.y;
        for (auto* gui : guis) {
            gui->layout();
        }
    }
};
guictr_base* makeDnDTestCtr() {
    return new guictr_dnd_test();
}
