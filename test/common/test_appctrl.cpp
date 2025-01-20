#include "test_appctrl.hpp"
#include "appsettings.hpp"
#include "glheaders.h"

#include <nanovg.h>
#include <algorithm>
#include <vector>
#include <functional>
#include <memory>
#include <GLFW/glfw3.h>

#include "gui/shape/shapeeditor.hpp"
#include "host/shape/shape.hpp"
#include "plugins/visualizer/visualizer-plugin.hpp"
#include "tls.hpp"
#include "window.hpp"
#include "platform.hpp"

#include "keyboard.hpp"
#include "commands.hpp"

#include "basectrl.hpp"
#include "exceptions.hpp"
#include "color_util.hpp"
#include "str_util.hpp"
#include "logging.hpp"
#include "menu.hpp"

#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/button.hpp"
#include "gui/menu/menu.hpp"
#include "gui/dialog/dialog.hpp"

#define CMD_DRAW_BACKBUFFER_TOGGLE 21312

namespace TestApp {
    class ViewContainers {
    public:
        guictr_menubar ctr_menu;
        guictr_base* ctr_app = nullptr;
        ViewContainers(ngui::MenuBar& menubar)
            : ctr_menu(menubar) {
        }
        ~ViewContainers() {
            delete ctr_app;
        }
        void layout(int32_t winW, int32_t winH) {
            int winX      = 0;
            int winY      = 0;
            int winBottom = winH;
#if USE_GUI_MENU
            int hMenu = 28;
            winH -= hMenu;
            winY += hMenu;
            ctr_menu.pos  = vec2(0, 0);
            ctr_menu.size = vec2(winW, hMenu);
#endif
            if (ctr_app) {
                ctr_app->pos  = { winX, winY };
                ctr_app->size = { winW, winBottom - winY };
                ctr_app->layout();
            }
        }
        void addTo(std::vector<guictr_base*>& v) {
            if (ctr_app) {
                v.push_back(ctr_app);
            }
#if USE_GUI_MENU
            v.push_back(&ctr_menu);
#endif
        }
        void setAppCtr(guictr_base* ctr) {
            this->ctr_app = ctr;
        }
        guictr_menubar* getMenu() {
            return &ctr_menu;
        }
    };

}// namespace TestApp

void TestAppCtrl::destroy() {
    if (!isOK) {
        return;
    }
    isOK = false;
    delete view;
}

std::shared_ptr<window_abstract_t> getWindowPerf();
bool TestAppCtrl::menuCommand(const menucmd_t& command) {
    switch (command.command) {
        case CMD_DRAW_BACKBUFFER_TOGGLE:
            bDrawBackbuffer = !bDrawBackbuffer;
            return true;
        case CMD_SHOW_DEBUG_WINDOW: {
            window_dialog* dialog = this->mainWindow->createDialog("performance graphs", 1280, 720, getWindowPerf());
            dialog->show();
            return true;
        }
        case CMD_EXIT:
            mainWindow->requestClose();
            return true;
    }
    return false;
}

struct gui_select_entry_t {
    int id;
    String name;
};
class guidialog_select_app : public guidialog_base {
    const float heightTitle = 0.2f;

    String message;
    std::function<void(gui_select_entry_t&)> cb;

    std::vector<gui_select_entry_t> guiEntries;
    std::vector<guibutton*> buttons;

public:
    guidialog_select_app(String _message, std::function<void(gui_select_entry_t&)> _cb)
        : guidialog_base(ivec2(360, 340)),
          message(std::move(_message)),
          cb(std::move(_cb)) {
        guiEntries.push_back({ 0, "GUI tests" });
        guiEntries.push_back({ 1, "Line Tesselation" });
        guiEntries.push_back({ 2, "IEEE754 Float Editor" });
        guiEntries.push_back({ 3, "GUI test 2" });
        guiEntries.push_back({ 4, "Shape Editor" });
        for (auto& entry : guiEntries) {
            auto btn = new guibutton();
            btn->setText(entry.name);
            buttons.push_back(btn);
            add(btn);
        }
        setBackgroundRendered(true);
    }
    ~guidialog_select_app() override {
        for (auto& btn : buttons) {
            remove(btn);
            delete btn;
        }
    }
    void layout() override {
        const auto btnH = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        const auto cs   = getSizeContent();
        auto inset      = padding / 2;
        auto y          = cs.y * heightTitle;
        for (auto& entry : buttons) {
            entry->pos  = { inset * 2, y };
            entry->size = { cs.x - inset * 4, btnH };
            y += entry->size.y + inset;
        }
    }
    void buttonClicked(guibase* button) override {
        int n = indexOfCtr(buttons, button);
        if (n >= 0)
            cb(guiEntries[n]);
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
        const auto cs              = getSizeContent();
        const auto TITLE_FONT_SIZE = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        renderTextLabel(vg, vec2(cs.x * 0.5f, cs.y * heightTitle * 0.5f), vec2(size.x, TITLE_FONT_SIZE), message, theme, TITLE_FONT_SIZE, THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
};

guictr_base* makeGuiTestCtr();
guictr_base* makeEmptyTestCtr();
guictr_base* makeFloatEditCtr();
guictr_base* makeGuiTestCtr2();
i_ctr_shape_editor* makeShapeEditor();
void makeWindowContextCurrent(window_base* w);
namespace DAW::UI {
    guictr_base* MakeAudioVisualizer(::PluginVisualizer::module_visualizer* const eff);
} // namespace DAW::UI

guictr_base* getApp(int appType) {
    guictr_base* ctr = nullptr;
    switch (appType) {
        case 0:
            ctr = makeGuiTestCtr();
            break;
        case 1:
            ctr = DAW::UI::MakeAudioVisualizer(nullptr); 
            break;
        case 2:
            ctr = makeFloatEditCtr();
            break;
        case 3:
            ctr = makeGuiTestCtr2();
            break;
        case 4:
            {
                auto* pShape = new DAW::Shape::shape_t();
                pShape->pts.push_back({ {0, 0}, 0.5f });
                auto c = makeShapeEditor(); 
                c->setShapeEditorCallback([pShape](const DAW::Shape::shape_t& s, bool bIsDragMove) {
                    pShape->pts = s.pts;
                });
                c->setShapeEditorShapeRef(pShape);
                ctr = c->getGuiContainer(); 
            }
            break;
        default:
            throw applogicexception("Invalid app type");
    }
    return ctr;
}

void TestAppCtrl::startApp() {
    auto loadApp = [this](auto appCtr) {
        if (!appCtr) {
            return;
        }
        view->setAppCtr(appCtr);
        containers.clear();
        view->addTo(containers);
        for (guictr_base* ctr : containers) {
            ctr->setControl(this);
        }
        relayout();
    };
    if (preselectedApp < 0) {
        auto const dlg = new guidialog_select_app("Select app", [this, loadApp](gui_select_entry_t& n) {
            log_printf("cb: %s\n", StringAsCStr(n.name));
            closeDialogs();
            makeWindowContextCurrent(this->mainWindow);
            loadApp(getApp(n.id));
            makeWindowContextCurrent(nullptr);
        });
        openDialog(dlg);
    } else {
        loadApp(getApp(preselectedApp));
    }
}

void TestAppCtrl::initApp(const std::vector<String>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--app" && i + 1 < args.size()) {
            preselectedApp = atoi(StringAsCStr(args[i + 1]));
            i++;
        }
        if (args[i] == "--timeout" && i + 1 < args.size()) {
            appTimeoutSeconds = atoi(StringAsCStr(args[i + 1]));
            i++;
        }
    }
    auto& tls = daw_tls::initNewTls();
    loadSettings(*tls.settings);
}

bool TestAppCtrl::initAppWindow(window_main* window, NVGcontext* nanovg) {
    this->mainWindow = window;
    this->window     = window;
    this->vg         = nanovg;
    themes.loadThemes();

    view = new TestApp::ViewContainers(menubar);
    view->addTo(this->containers);
    for (guictr_base* ctr : containers) {
        ctr->setControl(this);
    }

    menus.file.type  = ngui::menu_type::submenu;
    menus.file.title = "File";
    menus.file.addCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 2), "Show profiling results");
    menus.file.addCommand(CMD_NOARG(CMD_DRAW_BACKBUFFER_TOGGLE), "Draw to backbuffer");
    menus.file.addCommand(CMD_NOARG(CMD_EXIT), "Quit");
    menubar.add(&menus.file);
    this->updateMenubar();
#if !USE_GUI_MENU
    this->mainWindow->updateMenu();
#endif

    isOK = true;
    return isOK;
}

void TestAppCtrl::onTick() {
    for (guictr_base* ctr : containers) {
        ctr->onTick(this);
    }
    mainWindow->requestRedraw();
    if (tmStart == 0) {
        tmStart = getTimeMillis();
    } else if (appTimeoutSeconds >= 0 && getTimeMillis() - tmStart > appTimeoutSeconds * 1000) {
        mainWindow->requestClose();
    }
}

void TestAppCtrl::relayout(int32_t w, int32_t h) {
    closeAllAppMenus();
    closeAllContextMenus();
    m_size = ivec2(w, h);
    view->layout(w, h);

    for (guictr_base* ctr : containers) {
        ctr->layout();
    }
}
void TestAppCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) {
#if USE_GUI_MENU
    if (ctxtmenu && !ctxtmenu->isTransient() && view->getMenu()) {
        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER, kbmods);
        if (view->getMenu()->mouseHitTest(mousePos, evt)) {
        }
        return;
    }
#endif
    BaseCtrl::mouseMoved(mousePos, deltaPos, kbmods);
}
void TestAppCtrl::prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {
    if (!bDrawBackbuffer) {
        AppCtrl::prerender(nanovgCtxt, x, y, w, h, pixelRatio);
    } else {
        view->ctr_app->prerender(nullptr);
    }
}

void TestAppCtrl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    if (!bDrawBackbuffer) {
        AppCtrl::render(nanovgCtxt, x, y, w, h, ratio);
    } else {
        glViewport(x, y, w, h);
        view->ctr_app->render(nullptr);
    }
}

bool TestAppCtrl::mouseDownPre() {
    if (this->ctxtmenu && this->ctxtmenu->isDialog()) {
        return false;
    }
    closeAllContextMenus();
    return true;
}

int handleFatalError(int type, int implSpecType) {
    return 0;
}
