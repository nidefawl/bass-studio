
#include <GLFW/glfw3.h>
#include <nanovg_min.h>
#include <optional>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <array>

#include "assert_dbg.h"
#include "gui/controls/button.h"
#include "gui/dialog/dialogs.h"
#include "guicolors.h"
#include "math/vec.h"
#include "renderresources.h"
#include "str_util.h"
#include "basectrl.h"
#include "gui/container/container.h"
#include "gui/container/container_layout.h"
#include "gui/controls/knob.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/colorpick.h"
#include "gui/controls/background_image_settings.h"
#include "gui/dropdown/dropdown_generic.h"
#include "logging.h"
#include "platform.h"

class guictr_debugstrings : public guictr_base {
public:
    guictr_debugstrings() = default;

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        auto const ctrl = this->parentCtrl;

        std::vector<String> strings;
        String str;
        str = StringFormat("%012zX", reinterpret_cast<int64_t>(ctrl->getTheme()));
        strings.push_back(String("ctrl->getTheme: ") + str);
        str = StringFormat("%012zX", parentCtrl ? reinterpret_cast<int64_t>(parentCtrl->getTheme()) : 0);
        strings.push_back(String("parentCtrl->getTheme: ") + str);
        str = StringFormat("%012zX", reinterpret_cast<int64_t>(theme));
        strings.push_back(String("this->theme: ") + str);
        auto guiOver = parentCtrl->getGuiOver();
        auto guiDragged = parentCtrl->getGuiDragged();
        auto guiCaptured = parentCtrl->getGuiCaptured();
        auto guiCtrFocused = parentCtrl->getGuiCtrFocused();
        auto guiFocused = parentCtrl->getGuiFocused();
        str = guiOver ? guiOver->getClassName() : "<null>";
        strings.push_back(String("guiOver: ") + str);
        str = guiDragged ? guiDragged->getClassName() : "<null>";
        strings.push_back(String("guiDragged: ") + str);
        str = guiCaptured ? guiCaptured->getClassName() : "<null>";
        strings.push_back(String("guiCaptured: ") + str);
        str = guiCtrFocused ? guiCtrFocused->getClassName() : "<null>";
        strings.push_back(String("guiCtrFocused: ") + str);
        str = guiFocused ? guiFocused->getClassName() : "<null>";
        strings.push_back(String("guiFocused: ") + str);
        str = "<null>";

        guibase* p = guiFocused;
        int lvl    = 0;
        while (p) {
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

        setFont(vg, 14, THEMECOL_TEXT, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
        float lineh;
        nvgTextMetrics(vg, NULL, NULL, &lineh);


        nvgText(vg, x, 0, StringAsCStr(label), NULL);
        int y = lineh;
        for (String& s : strings) {
            nvgText(vg, x, y, StringAsCStr(s), NULL);
            y += lineh;
        }
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
};

class guictr_bgimage : public guictr_base {
    public:
    container_background_image bgImage;
    guictr_bgimage() : guictr_base() {
        setBackgroundRendered(true);
        setCanMouseHit(true);
    }
    ~guictr_bgimage() override {
        removeGuis();
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        bgImage.render(this, vg);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        guictr_base::handleDraggedBegin(evt);
        if (evt.guiDragged == this) {
            guictr_set_background* loadImg = new guictr_set_background();
            loadImg->size = {480, 480};
            loadImg->pos = {0, 0};
            loadImg->setEditBackground(bgImage);
            loadImg->fnEditBackground = [this](const container_background_image& bg) {
                bgImage = bg;
            };
            guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
            ctxtMenu->size = loadImg->size;
            ctxtMenu->add(loadImg);
            ctxtMenu->canTakeInputFocus = true;
            ctxtMenu->maxHeight = loadImg->size.y;
            ctxtMenu->setBackgroundRendered(false);
            parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
        }
    }
};
class guictr_testgui : public guictr_base {
    static constexpr int64_t numKnobs = 220;
    std::array<guiknob*, numKnobs> knobs{};
    std::array<float, numKnobs> knobVals{};
    std::array<float, numKnobs> knobValsNext{};
    int nTicks = 0;
    seq_rand rand;

    gui_color_pick colorPick;
    guidropdown_generic<String> dropdown;
    guibutton testButton;
    gui_numberinput_i32 field;
    gui_textfield textField;
    guictr_debugstrings debugstrings;
    std::array<guictr_bgimage, 12> bgTests;
    guictr_base* ctrTabbed;
    int nr{};
    String testImage = "wallhaven-we5g56.png";
public:
    guictr_testgui();
    ~guictr_testgui() override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        if (nTicks-- <= 0) {
            for (int i = 0; i < numKnobs; i++) {
                auto bits       = rand.rng_bits(4);
                knobValsNext[i] = bits / 16.0f;
            }
            nTicks = 40;
        }
        for (int i = 0; i < numKnobs; i++) {
            knobVals[i] += 0.01f * (knobValsNext[i] - knobVals[i]);
        }
    }
};


guictr_base* makeCtrTheme();
class guictr_tabbed_test : public guictr_tabbed {
public:
    guictr_base* const ctr_properties;
    guictr_base* const ctr_theme;
    guictr_tabbed_test() 
        : guictr_tabbed(),
          ctr_properties(DAW::UI::makeGuiObjectProperties({nullptr})), 
          ctr_theme(DAW::UI::makeGuiThemeEditor({nullptr})) {
        ctr_properties->setLabel("Properties");
        ctr_theme->setLabel("Theme");
        addEntry(ctr_theme, ctr_theme->label);
        addEntry(ctr_properties, ctr_properties->label);
        setActiveEntry(0);
    }
    ~guictr_tabbed_test() override {
        //remove the entries we have to delete, base class would see dangling ptr otherwise
        remove(ctr_properties);
        remove(ctr_theme);
        delete ctr_properties;
        delete ctr_theme;
    }
};

guictr_testgui::guictr_testgui() : guictr_base(), field(nullptr), ctrTabbed(new guictr_tabbed_test()) {
    setBackgroundRendered(true);
    add(&this->colorPick);
    add(&testButton);
    add(&textField);
    add(&field);
    add(ctrTabbed);
    add(&dropdown);
    add(&debugstrings);
    vec2 bgScale(0.6f);
    for (int32_t v = 0; v <= container_background_image::position_t::bottom; ++v) {
        for (int32_t h = 0; h <= container_background_image::position_t::right; ++h) {
            container_background_image img{};
            img.scale = vec2(0.6f);
            img.layout = container_background_image::layout_t::position;
            img.horizontalPos = static_cast<container_background_image::position_t>(h);
            img.verticalPos = static_cast<container_background_image::position_t>(v);
            bgTests[v*3+h].bgImage = img;
            add(&bgTests[v*3+h]);
        }
    }
    {
        container_background_image img{};
        img.layout = container_background_image::layout_t::fill;
        bgTests[9].bgImage = img;
        add(&bgTests[9]);
    }
    {
        container_background_image img{};
        img.layout = container_background_image::layout_t::contain;
        bgTests[10].bgImage = img;
        add(&bgTests[10]);
    }
    {
        container_background_image img{};
        img.layout = container_background_image::layout_t::cover;
        bgTests[11].bgImage = img;
        add(&bgTests[11]);
    }
    for (auto& bg : bgTests) {
        bg.bgImage.path = testImage;
    }
    rand.rng_seed(static_cast<uint64_t>(getTimeMicros()));
    setCanMouseHit(true);
    field.setRef(&this->nr);

    textField.setPlaceholder("Search");

    textField.setChangeCallback([](const std::string& str) {
        log_printf("text callback %s\n", StringAsCStr(str));
        return true;
    });
    std::vector<String> strDropDownOptions;
    strDropDownOptions.reserve(7);
    for (int i = 0; i < 7; i++) {
        strDropDownOptions.push_back(StringFormat("Option %d", i));
    }
    dropdown.setOptions(strDropDownOptions);
    dropdown.setSelectedIndex(0);
    //	textField.setPlaceholder("Search");
    for (int i = 0; i < numKnobs; i++) {
        knobs[i] = new guiknob(guiknob::knobtype::KNOB_UNLABELED);
    }
    for (int i = 0; i < numKnobs; i++) {
        knobVals[i] = rand.rng_bits(4) / 16.0f;
        knobs[i]->setValueRef(&knobVals[i]);
        add(knobs[i]);
    }
    testButton.setText("Show Yes/No Dialog");
}
guictr_testgui::~guictr_testgui() {
    removeGuis();
    delete ctrTabbed;
    for (int i = 0; i < numKnobs; i++) {
        delete knobs[i];
    }
}


void guictr_testgui::buttonClicked(guibase* button) {
    if (&testButton == button) {
        auto dlg = new guidialog_cb_yes_no("Load Image", "Are you sure?");
        dlg->cb = [this](int result) {
            // parentCtrl->closeContextMenus();
            parentCtrl->closeDialogs();
            String path;
            static String lastProjectDirectory;
            if (promptUserFilePath(parentCtrl->window, 0, FILE_TYPES_IMAGES, path, lastProjectDirectory)) {
                log_printf("path %s\n", StringAsCStr(path));
                for (auto& bg : bgTests) {
                    bg.bgImage.path = path;
                }
            }
        };
        dynamic_cast<AppCtrl*>(parentCtrl)->openDialog(dlg);
    }
    if (&field == button) {
        log_printf("New val %d\n", this->nr);
    }
}
void guictr_testgui::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    for (auto* gui : guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }

#if 0
	{


		nvgLineJoin(vg, NVGlineCap::NVG_MITER);

		nvgSave(vg);
		nvgTranslate(vg, 320, 32);
		//Lots of room for optimization here (draw texture for dot, or use custom shader)
		nvgShapeAntiAlias(vg, 0);
		nvgBeginPath(vg);
		std::vector<ivec2> pts;
		pts.push_back(ivec2(32, 32));
		pts.push_back(ivec2(64));
		pts.push_back(ivec2(64, 80));
		pts.push_back(ivec2(128, 80));
		pts.push_back(ivec2(128, 80));
		const float radiusHandle = 2.5f;
		int ndivs = 6;
		for (ivec2 pt : pts) {
			nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, ndivs);

		}
		nvgFillColor(vg, theme->getColor(GuiColor::COL_KNOB));
		nvgFill(vg);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_KNOB_IND));
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
		nvgShapeAntiAlias(vg, USE_NANOVG_AA);
		nvgBeginPath(vg);
		nvgRect(vg, 16, 16, 256, 256);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
		nvgFill(vg);
		nvgRestore(vg);
	}
	{


		nvgLineJoin(vg, NVGlineCap::NVG_MITER);

		nvgSave(vg);
		nvgIntersectScissor(vg, 320, 32+128, 320, 32);
		nvgTranslate(vg, 320, 32+128);
		//Lots of room for optimization here (draw texture for dot, or use custom shader)
		nvgShapeAntiAlias(vg, 0);
		nvgBeginPath(vg);
		std::vector<ivec2> pts;
		pts.push_back(ivec2(32, 32));
		pts.push_back(ivec2(64));
		pts.push_back(ivec2(64, 80));
		pts.push_back(ivec2(128, 80));
		pts.push_back(ivec2(128, 80));
		const float radiusHandle = 2.5f;
		int ndivs = 6;
		for (ivec2 pt : pts) {
			nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, ndivs);

		}
		nvgFillColor(vg, theme->getColor(GuiColor::COL_KNOB));
		nvgFill(vg);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_KNOB_IND));
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
		nvgShapeAntiAlias(vg, USE_NANOVG_AA);
		nvgRestore(vg);
		nvgSave(vg);
		nvgTranslate(vg, 320, 32+128);
		nvgBeginPath(vg);
		nvgRect(vg, 16, 16, 256, 256);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
		nvgFill(vg);
		nvgRestore(vg);

	}
#endif
}
void guictr_testgui::layout() {
    ivec2 cs          = getSizeContent();
    int q             = 240;
    colorPick.size    = ivec2(q * 2, q);
    colorPick.pos     = ivec2(cs.x / 2 - colorPick.size.x, cs.y - colorPick.size.y);
    field.size        = ivec2(320, 32);
    field.pos         = ivec2(0, 0);
    textField.size    = ivec2(320, 32);
    textField.pos     = ivec2(field.left(), field.bottom() + 22);
    dropdown.size     = ivec2(320, 32);
    dropdown.pos      = ivec2(field.left(), textField.bottom() + 22);
    testButton.size   = ivec2(320, 32);
    testButton.pos    = ivec2(field.left(), dropdown.bottom() + 22);
    ctrTabbed->pos    = { cs.x / 2, 0 };
    ctrTabbed->size.x = cs.x / 2;
    ctrTabbed->size.y = cs.y;
    debugstrings.pos  = ivec2(5, testButton.bottom()+22);
    debugstrings.size = ivec2(400, 200);
    debugstrings.setBackgroundRendered(true);
    auto ctrPos = debugstrings.getLeftBottom();
    for (auto& bg : bgTests) {
        bg.pos = ctrPos;
        bg.size = ivec2(400, 200);
        bg.setBackgroundRendered(true);
        ctrPos.x += bg.size.x;
        if (ctrPos.x + bg.size.x >= ctrTabbed->left()) {
            ctrPos.x = debugstrings.pos.x;
            ctrPos.y += bg.size.y;
        }
    }
    ivec2 kp(4);
    ivec2 ks(32, 32);
    for (int i = 0; i < numKnobs; i++) {
        auto kn = knobs[i];
        bool b  = true;
        int n = 10000;
        while (n-- > 0 && b) {
            b = false;
            for (guibase* g : guis) {
                if (g == kn)
                    continue;
                if (!(kp.x + ks.x < g->left() || kp.x > g->right() || kp.y + ks.y < g->top() || kp.y > g->bottom())) {
                    kp.x += 4;
                    if (kp.x + ks.x >= cs.x) {
                        kp.y += 4;
                        kp.x = 4;
                    }
                    b = true;
                    // static int thr=0;
                    // if (thr++%100==0)
                    //   log_printf("%X collides with %X\n", (int)&kn, (int)&g);
                    break;
                }
            }
        }
        kn->pos  = kp;
        kn->size = ks;
        kp.x += ks.x + 4;
        if (kp.x + ks.x >= cs.x) {
            kp.y += ks.y;
            kp.x = 4;
        }
    }
    for (auto* gui : guis) {
        gui->layout();
    }
}

guictr_base* makeGuiTestCtr() {
    return new guictr_testgui();
}
