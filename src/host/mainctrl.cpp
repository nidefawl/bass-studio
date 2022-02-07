#include "glheaders.h"
#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <ctime>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>
#include <memory>

#include "mainctrl.h"
#include "math/seq_math.h"
#include "error.h"
#include "basectrl.h"
#include "window.h"
#include "platform.h"
#include "keyboard.h"
#include "commands.h"
#include "project.h"
#include "projectfile.h"
#include "grid.h"
#include "note.h"
#include "cursor.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "appsettings.h"
#include "track.h"
#include "clip.h"
#include "fileloader.h"
#include "edithistory.h"
#include "logging.h"
#include "menu.h"
#include "thread.h"
#include "msgbox.h"
#include "tls.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/button.h"
#include "../gui/splitter.h"
#include "../gui/guicontextmenu_base.h"
#include "../gui/tempocontrols.h"
#include "../gui/scrollbar.h"
#include "../gui/statusbar.h"
#include "../gui/pluginctr.h"
#include "../gui/clipeditor.h"
#include "../gui/trackctr.h"
#include "../gui/trackctr_nodes.h"
#include "../gui/trackcontent.h"
#include "../gui/list.h"
#include "../gui/pluginlist.h"
#include "../gui/guimenu.h"
#include "../gui/debugctr.h"
#include "wave/waveform_render_impl.h"
#include "../gui/guishaderview.h"
#include "../gui/about.h"
#include "../gui/dialog_io.h"
#include "../gui/dialogs.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "track_impl.h"
#include "audiocache.h"
#include "seq_time.h"
#include "track_graph.h"
#include "effect_graph.h"

#include "../gui/guiplugin.h"
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"
#include "plugindatabase.h"
#include "window_impl.h"

#include "vst_host.h"
#include "audio_host.h"
#include "midi_host.h"
#include "appconfig.h"

const int FLAG_DEFER_LOAD               = 0x1;
const int FLAG_INVOKE_USER_CB_DEFERLOAD = 0x2;

int32_t getNumClipAllocations();

extern "C" {
void resetShaderTimeOffset();
}

void dragdrop_midifile::reset() {
    if (isLoaded) {

        log_printf("meeeh, reset!\n", 0);
    }
    isValidTarget = false;
    isLoaded      = false;
    clipboard.reset();
}

guictr_base* makeCtrProperties();//guiproperties.cpp
guictr_base* makeCtrTheme();     //guiproperties.cpp
guictr_base* makeCtrHistory();   //guihistory.cpp
guictr_base* makeDnDTestCtr();   //apps/drag-drop.cpp

class guictr_effectlibrary : public guictr_base {
public:
    guictr_pluginlibrary ctr_pluginlist;
    guictr_modulelibrary ctr_effectlist;
    bool initialized = false;
    int revision     = -1;
    guictr_effectlibrary() : guictr_base() {
        ctrType = CTR_TYPE_EFFECTLIBRARY;
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
        add(&ctr_pluginlist);
        add(&ctr_effectlist);
    }

    ~guictr_effectlibrary() override {
        removeGuis();
    }

    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        if (parent && !initialized) {
            initialized = true;
            update();
        }
        if (dawCtrl && dawCtrl->getDaw()->getPluginDatabase().getRevision() != this->revision) {
            update();
        }
    }

    void update() {
        ctr_pluginlist.update();
        ctr_effectlist.update();
        if (dawCtrl) {
            this->revision = dawCtrl->getDaw()->getPluginDatabase().getRevision();
        }
    }

    void layout() override {
        ctr_pluginlist.size.x = size.x;
        ctr_pluginlist.size.y = size.y / 2;
        ctr_effectlist.size.x = size.x;
        ctr_effectlist.size.y = size.y / 2;
        ctr_pluginlist.pos    = { 0, 0 };
        ctr_effectlist.pos    = { 0, ctr_pluginlist.bottom() };
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
};

guictr_base* makeGuiPluginsLoadedList();
guictr_base* makeGuiPerformance();
guictr_base* makeGuiEffectLibrary() {
    return new guictr_effectlibrary();
}

template<typename T>
void addLayoutEntry(T& t, const std::shared_ptr<guictr_base>& ctr, String title) {
    ctr->setLabel(std::move(title));
    std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(ctr);
    t->addEntry(entry1);
}

std::shared_ptr<guictr_layout> makeTabListCtr1() {
    auto ctr = std::make_shared<guictr_layout>();

    auto ctr_dbg0       = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_0);
    auto ctr_dbg1       = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_1);
    auto ctr_dbg2       = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2);
    auto ctr_properties = std::shared_ptr<guictr_base>(makeCtrProperties());
    auto ctr_theme      = std::shared_ptr<guictr_base>(makeCtrTheme());
    auto ctr_history    = std::shared_ptr<guictr_base>(makeCtrHistory());
    auto shaderView     = std::make_shared<gui_shaderview>();
    auto settings       = std::make_shared<guidialog_settings>();
    auto layout         = std::make_shared<guictr_layout>();

    ctr->setLayout(container_layout::TABBED);
    //ctr->setBackgroundRendered(true);
    ctr_dbg0->setLabel("Debug 0");
    ctr_dbg1->setLabel("Debug 1");
    ctr_dbg2->setLabel("Debug 2");
    ctr_properties->setLabel("Properties");
    ctr_theme->setLabel("Theme");
    ctr_history->setLabel("History");
    shaderView->setLabel("Shader");
    settings->setLabel("settings");

    addLayoutEntry(ctr, ctr_dbg0, ctr_dbg0->label);
    addLayoutEntry(ctr, ctr_dbg1, ctr_dbg1->label);
    addLayoutEntry(ctr, ctr_dbg2, ctr_dbg2->label);
    addLayoutEntry(ctr, ctr_history, ctr_history->label);
    addLayoutEntry(ctr, ctr_properties, ctr_properties->label);
    addLayoutEntry(ctr, ctr_theme, ctr_theme->label);
    addLayoutEntry(ctr, shaderView, shaderView->label);
    addLayoutEntry(ctr, settings, settings->label);
    addLayoutEntry(ctr, layout, "Empty layoutctr");
    ctr->setActiveEntry(0);

    return ctr;
}

std::shared_ptr<guictr_layout> makeTabListCtr2() {
    auto ctr = std::make_shared<guictr_layout>();

    auto ctr_effectlib     = std::make_shared<guictr_effectlibrary>();
    auto ctr_properties    = std::shared_ptr<guictr_base>(makeCtrProperties());
    auto ctr_loadedplugins = std::shared_ptr<guictr_base>(makeGuiPluginsLoadedList());
    auto ctr_performance   = std::shared_ptr<guictr_base>(makeGuiPerformance());
    auto settings          = std::make_shared<guidialog_settings>();

    auto ctr_dbg0 = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_0);
    auto ctr_dbg1 = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_1);
    auto ctr_dbg2 = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2);

    ctr->setLayout(container_layout::TABBED);
    //ctr->setBackgroundRendered(true);
    ctr_effectlib->setLabel("Plugins");
    ctr_loadedplugins->setLabel("Instances");
    ctr_performance->setLabel("Performance");
    ctr_properties->setLabel("Properties");
    ctr_dbg0->setLabel("Debug 0");
    addLayoutEntry(ctr, ctr_dbg0, ctr_dbg0->label);
    ctr_dbg1->setLabel("Debug 1");
    addLayoutEntry(ctr, ctr_dbg1, ctr_dbg1->label);
    ctr_dbg2->setLabel("Debug 2");
    addLayoutEntry(ctr, ctr_dbg2, ctr_dbg2->label);
    addLayoutEntry(ctr, ctr_effectlib, ctr_effectlib->label);
    addLayoutEntry(ctr, ctr_loadedplugins, ctr_loadedplugins->label);
    addLayoutEntry(ctr, ctr_performance, ctr_performance->label);
    addLayoutEntry(ctr, ctr_properties, ctr_properties->label);
    ctr->setActiveEntry(0);

    return ctr;
}
class DawViewContainersCompanion : public DawViewContainers {
    DawCtrl* const dawCtrl;

public:
    guictr_menubar ctr_menu;
    guictr_nodes_splitview ctr_nodes;
    guictr_tracks ctr_tracks2;
    guictr_clipeditor ctr_clipeditor;
    guictr_base* ctr_dnd_test;
    Splitter splitterCenter;
    DawViewContainersCompanion(DawCtrl* const _dawCtrl, ngui::MenuBar& menubar, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& grid, clip_view& clipView, dragdrop_midifile& dragdropclip)
        : dawCtrl(_dawCtrl),
          ctr_menu(menubar),
          ctr_nodes(_cursor, _project, dragdropclip),
          ctr_tracks2(_dawCtrl, _cursor, _trackSelection, _project, _projectGlobals, grid, dragdropclip),
          ctr_clipeditor(clipView),
          ctr_dnd_test(makeDnDTestCtr()),
          splitterCenter(0, 0.8f) {
        indexContent = 1;
        splitterCenter.setMinMax(0.2f, 0.86f);
    }
    guictr_menubar* getMenu() override {
        return &ctr_menu;
    }

    void layout(int32_t winW, int32_t winH) override {
        auto& centerCtr = ctr_tracks2;
        int winX        = 0;
        int winY        = 0;
#if USE_GUI_MENU
        int hMenu = 28;
        winH -= hMenu;
        winY += hMenu;
        ctr_menu.pos  = vec2(0, 0);
        ctr_menu.size = vec2(winW, hMenu);
#endif
        int hTopControls    = 0;
        int hStatusBar      = 0;
        int hCenter         = winH - hTopControls - hStatusBar;
        int hTrackCtr       = hCenter;
        splitterCenter.pos  = ivec2(winX, winY + hTrackCtr - 5);
        splitterCenter.size = ivec2(winW, 10);
        ctr_clipeditor.size = ctr_dnd_test->size = ctr_nodes.size = centerCtr.size = { winW, hTrackCtr };
        ctr_clipeditor.pos = ctr_dnd_test->pos = ctr_nodes.pos = centerCtr.pos = { winX, winY };
    }
    void addTo(std::vector<guictr_base*>& v) override {
        ctr_clipeditor.setControl(dawCtrl);
        ctr_tracks2.setControl(dawCtrl);
        ctr_nodes.setControl(dawCtrl);
        ctr_dnd_test->setControl(dawCtrl);

        v.push_back(&splitterCenter);
        dbgassert(v.size() == indexContent);

        v.push_back(&ctr_tracks2);

#if USE_GUI_MENU
        v.push_back(&ctr_menu);
#endif
    }
    void updateVisibility() {
        this->ctr_tracks2.setVisible(dawCtrl->containers[indexContent] == &this->ctr_tracks2);
        this->ctr_nodes.setVisible(dawCtrl->containers[indexContent] == &this->ctr_nodes);
        this->ctr_clipeditor.setVisible(dawCtrl->containers[indexContent] == &this->ctr_clipeditor);
        this->ctr_dnd_test->setVisible(dawCtrl->containers[indexContent] == this->ctr_dnd_test);
    }
};

class DawViewContainersMain : public DawViewContainers {
    MainCtrl* const mainCtrl;

public:
    guictr_menubar ctr_menu;
    guictr_tempocontrols ctr_tempo;
    guictr_plugins ctr_plugins;
    guictr_test ctr_test;
    gui_statusbar statusbar;
    guictr_pluginview ctr_pluginview;
    guictr_clipeditor ctr_clipeditor;
    guictr_clipeditorview ctr_clipeditorview;
    guictr_tracks ctr_tracks;
    guictr_nodes_splitview ctr_nodes;
    std::shared_ptr<guictr_layout> ctr_Left;
    std::shared_ptr<guictr_layout> ctr_Right;
    std::vector<std::shared_ptr<Splitter>> splitters;
    enum class SplitterPos : uint32_t {
        LEFT = 0,
        CENTER,
        RIGHT
    };
    DawViewContainersMain(MainCtrl* const _mainCtrl, ngui::MenuBar& menubar, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& grid, clip_view& clipView, dragdrop_midifile& dragdropclip)
        : mainCtrl(_mainCtrl),
          ctr_menu(menubar),
          ctr_tempo(_project, _projectGlobals),
          ctr_pluginview(&ctr_plugins),
          ctr_clipeditor(clipView),
          ctr_clipeditorview(ctr_clipeditor.noteeditor),
          ctr_tracks(_mainCtrl, _cursor, _trackSelection, _project, _projectGlobals, grid, dragdropclip),
          ctr_nodes(_cursor, _project, dragdropclip),
          ctr_Right() {
        indexContent        = 3;
        auto subctr_tabbed  = makeTabListCtr1();
        auto subctr_tabbed2 = makeTabListCtr2();
        splitters.push_back(std::make_shared<Splitter>(1, 0.02f));//left
        splitters.push_back(std::make_shared<Splitter>(0, 0.5f)); //center
        splitters.push_back(std::make_shared<Splitter>(1, 0.8f)); //right
        subctr_tabbed2->setLabel("Top");
        subctr_tabbed->setLabel("Bottom");
        std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(subctr_tabbed2);
        std::shared_ptr<guictr_layout_entry> entry2 = createGuiCtrLayoutEntry(subctr_tabbed);
        ctr_Left                                    = std::make_shared<guictr_layout>();
        ctr_Left->setLabel("Left Docker");
        ctr_Right = std::make_shared<guictr_layout>();
        ctr_Right->setLabel("Right Docker");
        ctr_Right->setLayout(container_layout::SPLIT_H);
        ctr_Right->addEntry(entry1);
        ctr_Right->addEntry(entry2);

        splitters[0]->setMinMax(0.05f, 0.9f);
        splitters[1]->setMinMax(0.25f, 0.9f);
        splitters[2]->setMinMax(0.05f, 0.9f);

        ctr_tempo.setSnapSides(ivec4(0, 0, 0, 1));
        statusbar.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_clipeditorview.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_pluginview.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_clipeditor.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_plugins.setSnapSides(ivec4(0, 1, 0, 0));
        subctr_tabbed2->setSnapSides(ivec4(1, 0, 0, 1));
        ctr_Left->setSnapSides(ivec4(0, 0, 1, 0));
        ctr_Right->setSnapSides(ivec4(1, 0, 0, 0));

        subctr_tabbed->setSnapSides(ivec4(1, 0, 0, 0));
    }

    guictr_menubar* getMenu() override {
        return &ctr_menu;
    }

    Splitter* getSplitter(SplitterPos pos) {
        return splitters[static_cast<uint32_t>(pos)].get();
    }

    void layout(int32_t winW, int32_t winH) override {
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
        auto leftSplitter  = getSplitter(SplitterPos::LEFT);
        auto rightSplitter = getSplitter(SplitterPos::RIGHT);
        if (ctr_Left->getEntries().empty()) {
            leftSplitter->setScale(0);
        } else if (leftSplitter->getScale() < leftSplitter->getMin()) {
            leftSplitter->setScale(leftSplitter->getMin());
        }
        if (ctr_Right->getEntries().empty()) {
            rightSplitter->setScale(1);
        } else if (rightSplitter->getScale() > rightSplitter->getMax()) {
            rightSplitter->setScale(rightSplitter->getMax());
        }
        int hTopControls = 48;
        int hStatusBar   = 60;
        int hCenter      = winH - hTopControls - hStatusBar;
        int hContent     = winH - hTopControls;
        int hTrackCtr    = getSplitter(SplitterPos::CENTER)->leftOrTop(hCenter);
        int hEditor      = getSplitter(SplitterPos::CENTER)->rightOrBottom(hCenter);

        int widthLeft           = getSplitter(SplitterPos::LEFT)->leftOrTop(winW);
        int widthCenterAndRight = getSplitter(SplitterPos::LEFT)->rightOrBottom(winW);
        int widthCenter         = getSplitter(SplitterPos::RIGHT)->leftOrTop(widthCenterAndRight);
        int widthRight          = getSplitter(SplitterPos::RIGHT)->rightOrBottom(widthCenterAndRight);

        ctr_tempo.size          = { winW, hTopControls };
        ctr_tracks.size         = { widthCenter, hTrackCtr };
        ctr_nodes.size          = { widthCenter, hTrackCtr };
        ctr_clipeditor.size     = { widthCenter, hEditor };
        ctr_plugins.size        = { widthCenter, hEditor };
        ctr_pluginview.size     = { 300, hStatusBar };
        ctr_clipeditorview.size = { 300, hStatusBar };

        int wbottom = widthCenter;
        wbottom -= 60;//rightmost part
        wbottom -= ctr_pluginview.size.x;
        wbottom -= ctr_clipeditorview.size.x;
        statusbar.size = { wbottom, hStatusBar };

        ctr_tempo.pos          = { winX, winY };
        ctr_tracks.pos         = { widthLeft, winY + hTopControls };
        ctr_nodes.pos          = { widthLeft, winY + hTopControls };
        statusbar.pos          = { widthLeft, winBottom - hStatusBar };
        ctr_clipeditorview.pos = { statusbar.right(), winBottom - hStatusBar };
        ctr_pluginview.pos     = { ctr_clipeditorview.right(), winBottom - hStatusBar };
        ctr_plugins.pos        = { widthLeft, winBottom - hStatusBar - hEditor };
        ctr_clipeditor.pos     = { widthLeft, winBottom - hStatusBar - hEditor };
        ctr_Left->pos          = { winX, winY + hTopControls };
        ctr_Left->size         = { widthLeft, hContent };

        getSplitter(SplitterPos::LEFT)->pos    = ivec2(widthLeft - 5, hTopControls);
        getSplitter(SplitterPos::LEFT)->size   = ivec2(10, hContent);
        getSplitter(SplitterPos::CENTER)->pos  = ivec2(widthLeft, ctr_clipeditor.pos.y - 5);
        getSplitter(SplitterPos::CENTER)->size = ivec2(widthCenter, 10);

        ctr_Right->pos  = { widthLeft + widthCenter, winY + hTopControls };
        ctr_Right->size = { widthRight, hContent };

        getSplitter(SplitterPos::RIGHT)->pos  = ivec2(ctr_Right->pos.x - 5, hTopControls);
        getSplitter(SplitterPos::RIGHT)->size = ivec2(10, hContent);

        ctr_Right->postContentChanged();
        ctr_Left->postContentChanged();
    }

    void addTo(std::vector<guictr_base*>& v) override {
        this->ctr_plugins.setControl(mainCtrl);
        this->ctr_clipeditor.setControl(mainCtrl);
        this->ctr_nodes.setControl(mainCtrl);
        for (auto& s : splitters)
            v.push_back(s.get());
        dbgassert(v.size() == indexContent);
        v.push_back(&ctr_tracks);
        v.push_back(&ctr_clipeditor);
        v.push_back(&ctr_tempo);
        v.push_back(&ctr_pluginview);
        v.push_back(&ctr_clipeditorview);
        v.push_back(ctr_Left.get());
        v.push_back(ctr_Right.get());
        v.push_back(&statusbar);
#if USE_GUI_MENU
        v.push_back(&ctr_menu);
#endif
    }

    void dragContainerRelayout(MainCtrl* ctrl, BaseCtrl::drag_ctr_event evt) override {
        if (evt.evtType != BaseCtrl::drag_ctr_event_type::DRAG_MOVE) {
            ctr_Right->postContentChanged();
            ctr_Left->postContentChanged();
            ctr_Right->layout();
            ctr_Left->layout();
        }
        if (evt.evtType == BaseCtrl::drag_ctr_event_type::DRAG_END) {
            bool bRelayout = false;

            guictr_layout* layoutCtrs[2] = { ctr_Left.get(), ctr_Right.get() };
            for (auto* layoutCtr : layoutCtrs) {
                if (layoutCtr->getLayout() == container_layout::SOLE && layoutCtr->getEntries().size() == 1 && layoutCtr->getEntries().front()->getFrameType() == layout_ctr_type::GUICTR_LAYOUT) {
                    std::shared_ptr<guictr_layout_entry> out;
                    layoutCtr->getContainerRef(layoutCtr->getEntries().front().get(), out, true);
                    dbgassert(out);
                    std::shared_ptr<guictr_layout> shrdLayoutCtr;
                    dbgassert(out->getSharedGui());
                    shrdLayoutCtr = std::dynamic_pointer_cast<guictr_layout>(out->getSharedGui());
                    ctrl->replaceContainerWith(layoutCtr, shrdLayoutCtr);
                    bRelayout = true;
                }
            }
            if (bRelayout) {
                //TODO: rename relayout(void)
                static_cast<BaseCtrl*>(ctrl)->relayout();
            }
        }
    }

    void loadLayout(const dawview_layout_t* viewLayout) {
        ctr_Right->removeAllEntries();
        ctr_Left->removeAllEntries();
        if (viewLayout->left && viewLayout->right) {
            loadContainerSnapshot(ctr_Right.get(), viewLayout->right.get());
            loadContainerSnapshot(ctr_Left.get(), viewLayout->left.get());
        }
        if (viewLayout->splitterPositions.size() == splitters.size()) {
            for (int i = 0; i < splitters.size(); i++) {
                splitters[i]->setScale(viewLayout->splitterPositions[i]);
            }
        }
    }

    void storeLayout(dawview_layout_t* layout) {
        layout->left  = std::make_shared<guictrlayout_snapshot_t>();
        layout->right = std::make_shared<guictrlayout_snapshot_t>();
        storeContainerSnapshot(ctr_Right.get(), layout->right.get());
        storeContainerSnapshot(ctr_Left.get(), layout->left.get());
        layout->splitterPositions.resize(splitters.size());
        for (int i = 0; i < splitters.size(); i++) {
            layout->splitterPositions[i] = splitters[i]->getScale();
        }
    }

    void updateVisibility() {
        this->ctr_tracks.setVisible(mainCtrl->containers[indexContent] == &this->ctr_tracks);
        this->ctr_nodes.setVisible(mainCtrl->containers[indexContent] == &this->ctr_nodes);
    }
};

void CompanionCtrl::setupView() {
    view = new DawViewContainersCompanion(this, menubar, cursor, trackSelection, daw.project, daw.projectGlobals, grid, clipView, daw.dragdropclip);
    view->addTo(this->containers);
    viewContainers = view;
    for (guictr_base* ctr : containers) {
        ctr->setControl(this);
    }
    view->updateVisibility();
}

void MainCtrl::setupView() {
    view = new DawViewContainersMain(this, menubar, daw.projectGlobals.cursor, daw.projectGlobals.trackSelection, daw.project, daw.projectGlobals, grid, clipView, daw.dragdropclip);
    view->addTo(this->containers);
    viewContainers = view;
    for (guictr_base* ctr : containers) {
        ctr->setControl(this);
    }
    view->updateVisibility();
}

std::shared_ptr<guictr_layout> MainCtrl::replaceContainerWith(guictr_base* ctr,
                                                              std::shared_ptr<guictr_layout> newContainer) {
    std::shared_ptr<guictr_layout> ret;
    if (ctr == view->ctr_Right.get()) {
        replaceEntry(containers, view->ctr_Right.get(), newContainer.get());
        ret                     = view->ctr_Right;
        ret->parent             = nullptr;
        view->ctr_Right         = newContainer;
        view->ctr_Right->parent = nullptr;
        view->ctr_Right->setControl(this);
    }
    if (ctr == view->ctr_Left.get()) {
        replaceEntry(containers, view->ctr_Left.get(), newContainer.get());
        ret                    = view->ctr_Left;
        ret->parent            = nullptr;
        view->ctr_Left         = newContainer;
        view->ctr_Left->parent = nullptr;
        view->ctr_Left->setControl(this);
    }
    return ret;
}

void MainCtrl::setViewMode(view_mode_t mode) {
    this->viewMode = mode;
    switch (mode) {
        case MIXER:
        case TRACK_TIMELINE:
            containers[view->indexContent] = &view->ctr_tracks;
            break;
        case NODE_EDITOR:
            containers[view->indexContent] = &view->ctr_nodes;
            break;
    }
    view->updateVisibility();
    if (view->ctr_nodes.isVisible()) {
        view->ctr_nodes.refresh();
    }
    focusGui(containers[view->indexContent]);
}

void CompanionCtrl::setViewMode(view_mode_t mode) {
    this->viewMode = mode;
    switch (mode) {
        case MIXER:
            containers[view->indexContent] = &view->ctr_nodes;
            break;
        case TRACK_TIMELINE:
            containers[view->indexContent] = &view->ctr_tracks2;
            break;
        case NODE_EDITOR:
            containers[view->indexContent] = &view->ctr_clipeditor;
            break;
    }
    view->updateVisibility();
    if (view->ctr_nodes.isVisible()) {
        view->ctr_nodes.refresh();
    }
    focusGui(containers[view->indexContent]);
}

view_mode_t DawCtrl::getViewMode() const {
    return this->viewMode;
}

void MainCtrl::showPluginView() {
    containers[view->indexContent + 1] = &view->ctr_plugins;
}

void MainCtrl::showClipEditor() {
    containers[view->indexContent + 1] = &view->ctr_clipeditor;
}

bool MainCtrl::isClipEditorVisible() {
    return containers[view->indexContent + 1] == &view->ctr_clipeditor;
}

bool MainCtrl::isPluginViewVisible() {
    return containers[view->indexContent + 1] == &view->ctr_plugins;
}

void MainCtrl::addDebug(String s) {
}

void DawCtrl::resetMouseContext() {
    BaseCtrl::resetMouseContext();
}

void MainCtrl::resetMouseContext() {
    DawCtrl::resetMouseContext();
    if (view)
        view->ctr_nodes.reset();
}

void CompanionCtrl::resetMouseContext() {
    DawCtrl::resetMouseContext();
    if (view)
        view->ctr_nodes.reset();
}

void DawInstance::unloadProject() {
    AppWndProc_enableBlockReentrant();
    dbgassert(playThread.isLocked());
    for (auto* ctrl : dawCtrls) {
        ctrl->closeContextMenu();
        ctrl->resetMouseContext();
    }
    projectPath = "";
    setSelectedTrack(nullptr);
    for (auto* dawctrl : dawCtrls) {
        dawctrl->clipView.set(nullptr);
    }

    projectGlobals.cursor.setEmptySelection();

    hist.clear(this);

    std::vector<track_t*> _tracks     = project.trackList.getAllTracksFlatVec();// iterate a copy
    std::vector<track_t*> _rootTracks = project.trackList.getAllTracksTreeVec();
    log_printf("unloading project with %d tracks\n", _tracks.size());
    for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
        track_t* track = *it;

        log_printf("remove track %s\n", StringAsCStr(track->name));
        removeTrackImpl(track, FLG_TRK_CHANGE_LOAD);
    }
    project.trackList.clear();
    for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
        track_t* track = *it;
        log_printf("delete track %s\n", StringAsCStr(track->name));
        releaseTrackResources(track, this);
        delete track;
    }

    host->releaseProjectResources();
    daw_tls::getTls().audioCache->unloadAll();

    auto* ctrl = mainCtrl;
    if (ctrl) {
        auto& trackView = ctrl->view->ctr_tracks.trackView;
        trackView.m_resizePreModifyState.reset();
        trackView.m_clipboard.reset();
        trackView.action.clipboard.reset();
        trackView.iGuiMgr.reset();
    }
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->resetView();
        }
    }

    {
        std::vector<effectbase*> pluginsDeferred;
        host->getDeferredEffects(pluginsDeferred);
        dbgassert(pluginsDeferred.empty());
    }
    AppWndProc_disableBlockReentrant();
}

void DawCtrl::updateMenubar() {
    menubar.disableAll = this->ctxtmenu != nullptr;
    ngui::Menu* undo   = menus.edit.getByCmd(CMD_UNDO);
    ngui::Menu* redo   = menus.edit.getByCmd(CMD_REDO);
    if (daw.hist.canUndo()) {
        undo->disabled = false;
        undo->title    = menuName(StringFormat("Undo %s", StringAsCStr(daw.hist.getUndoStep())), KC_UNDO);
    } else {
        undo->disabled = true;
        undo->title    = menuName("Undo", KC_UNDO);
    }
    if (daw.hist.canRedo()) {
        redo->disabled = false;
        redo->title    = menuName(StringFormat("Redo %s", StringAsCStr(daw.hist.getRedoStep())), KC_REDO);
    } else {
        redo->disabled = true;
        redo->title    = menuName("Redo", KC_REDO);
    }
    menus.recent.clear();

    for (auto& strFileRecentPath : DAW::settings.recentfiles.sortedEntries) {
        String a, b, c, d;//path, name, ext, nameExt
        SplitPath(strFileRecentPath, &a, &b, &c, &d);
        menus.recent.addCommand(menucmd_t{ CMD_FILE_OPEN, strFileRecentPath, 0 }, d);
    }
}

static SupportedFileType FILE_TYPE_PROJECT{ "Project File", PROJECT_FILE_EXT };
std::vector<SupportedFileType> vFILE_TYPE_PROJECT = { FILE_TYPE_PROJECT };

void DawInstance::loadFileCStr(const char* str) {
    loadFile(str, 0);
}

void DawInstance::saveFile(const String& path) {
    if (!path.empty()) {
        std::shared_ptr<project_file> f = createProjectFile();
        saveProject(f, path);
        projectPath = path;
    }
}

void DawInstance::loadFile(String path, int flags) {
    timer.reset();
    std::shared_ptr<project_file> f = loadProjectFile(path);
    if (!f) {
        mainCtrl->setStatusText(StringFormat("Failed loading %s", StringAsCStr(FileNameFromPath(path))));
    } else {
        DAW::settings.recentfiles.add(path);
        const bool wasUserCallback = (flags & FLAG_INVOKE_USER_CB_DEFERLOAD) != 0;
        auto cb                    = [this, path, projFile = f, wasUserCallback](int n) {
            int loadFlags = 0;
            if (wasUserCallback) {
                loadFlags = n == 0 ? FLAG_DEFER_LOAD : 0;
            } else {
                loadFlags = n;
            }
            setProjectToLoad(projFile, loadFlags);
            closeContextMenus();
            closeDialogs();
        };
        if ((flags & FLAG_INVOKE_USER_CB_DEFERLOAD) == 0) {
            cb(flags & FLAG_DEFER_LOAD);
        } else {
            guidialog_cb_yes_no* dlg = new guidialog_cb_yes_no();
            dlg->cb                  = cb;
            dlg->message             = "Load plugins?";
            mainCtrl->openDialog(dlg);
        }
    }
}

void DawInstance::setEmptyProject() {
    ThreadLock lock = playThread.lockThread();
    unloadProject();
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_printf("getNumClipAllocations == %d!\n", totalAllocs);
        dbgassert(getNumClipAllocations() == 0);
    }
    insertNewTrack(-1, TRACK_TYPE_MIDI, FLG_TRK_CHANGE_LOAD);
    insertNewTrack(-1, TRACK_TYPE_MASTER, 0);
    resetShaderTimeOffset();
}

#if CREATE_DEBUG_COMPANION_WINDOW
void drawDebugWindowWaveformCache(NVGcontext* ctx, int winW, int winH, float pxratio);
int initDebugWindowWaveformCache(NVGcontext* ctx);

void openDebugWindowWaveformCache(window_main* mainwindow) {
    dbgassert(mainwindow);
    window_dialog* dialog = mainwindow->createDialog("waveform atlas cache", 1280, 720);
    window_init_fn init;
    window_draw_fn drawFn;
    init.initCallback = [](NVGcontext* ctx) {
        initDebugWindowWaveformCache(ctx);
    };
    drawFn.drawCallback = [](NVGcontext* ctx, int winW, int winH, float pxratio) {
        drawDebugWindowWaveformCache(ctx, winW, winH, pxratio);
    };
    dialog->setDrawFunction(drawFn);
    dialog->setInitFunction(init);
    dialog->show();
}

void drawDebugWindowPerformance(NVGcontext* ctx, int winW, int winH, float pxratio);
int initDebugWindowPerformance(NVGcontext* ctx);

void openDebugWindowPerformance(window_main* mainwindow) {
    dbgassert(mainwindow);
    window_dialog* dialog = mainwindow->createDialog("performance graphs", 1280, 720);
    window_init_fn init;
    window_draw_fn drawFn;
    init.initCallback = [](NVGcontext* ctx) {
        initDebugWindowPerformance(ctx);
    };
    drawFn.drawCallback = [](NVGcontext* ctx, int winW, int winH, float pxratio) {
        drawDebugWindowPerformance(ctx, winW, winH, pxratio);
    };
    dialog->setDrawFunction(drawFn);
    dialog->setInitFunction(init);
    dialog->show();
}

void drawDebugWindowNanoVG(NVGcontext* ctx, int winW, int winH, float pxratio);
int initDebugWindowNanoVG(NVGcontext* ctx);

void openDebugWindowNanoVG(window_main* mainwindow) {
    dbgassert(mainwindow);
    window_dialog* dialog = mainwindow->createDialog("nanovg debug", 1280, 720);
    window_init_fn init;
    window_draw_fn drawFn;
    init.initCallback = [](NVGcontext* ctx) {
        initDebugWindowNanoVG(ctx);
    };
    drawFn.drawCallback = [](NVGcontext* ctx, int winW, int winH, float pxratio) {
        drawDebugWindowNanoVG(ctx, winW, winH, pxratio);
    };
    dialog->setDrawFunction(drawFn);
    dialog->setInitFunction(init);
    dialog->show();
}
#endif

void initWindowControl(window_main* windowInitialize);
void destroyWindowControl(window_main* windowInitialize);

void DawInstance::onDawCompanionWindowClose(DawWindowCompanion& entry) {
    auto it = std::find_if(dawCtrls.begin(), dawCtrls.end(), [pDawCtrlClosing = entry.ctrl.get()](auto* pDawCtrl) {
        return pDawCtrl == pDawCtrlClosing;
    });
    if (it != dawCtrls.end()) {
        dawCtrls.erase(it);
    }
    destroyWindowControl(entry.wnd);
}

void DawInstance::setSoloState(audio_stage_ref_t ref, bool enableSolo) {
    track_t* track = getTracks().resolveTrack(ref);
    dbgassert(track);
    dbgassert(track->audio);
    track->audio->flags ^= audiostageflags_t::SOLO;
    DAW::updateSoloFlag(host, &project, getTracks().getAllTracksFlatVecRef());
}

bool DawInstance::onChildOverlayWindowClose(window_main* window) {
    auto it = std::find_if(companionWindows.begin(), companionWindows.end(), [this, window](auto& wndEntry) {
        if (wndEntry.wnd == window) {
            this->onDawCompanionWindowClose(wndEntry);
            return true;
        }
        return false;
    });
    if (it != companionWindows.end()) {
        companionWindows.erase(it);
        return true;
    }
    return false;
}

void MainCtrl::onChildOverlayWindowClose(window_main* window) {
    if (daw.onChildOverlayWindowClose(window)) {
        this->mainWindow->closeOverlay(window);
    } else {
        DawCtrl::onChildOverlayWindowClose(window);
    }
}

void DawInstance::menuCommand(const menucmd_t&& command) {
    try {
        switch (command.command) {

            case CMD_OPEN_VIEW:
                if (getMainControl()) {
                    dbgassert(command.argInt >= 0);
                    std::shared_ptr<guictr_base> ctr;
                    if (makeContainer(static_cast<container_type>(command.argInt), ctr)) {
                        auto ctrLayoutLeft = getMainControl()->view->ctr_Left;
                        addLayoutEntry(ctrLayoutLeft, ctr, ctr->label);
                        ctrLayoutLeft->postContentChanged();
                        ctrLayoutLeft->layout();
                    }
                }
                break;
            case CMD_OPEN_SECOND_WINDOW:
                if (companionWindows.empty()) {
                    auto companionCtrlStdPtr = std::make_shared<CompanionCtrl>(*this);
                    auto compWindowNew       = mainCtrl->mainWindow->createOverlay(companionCtrlStdPtr, WINDOW_IS_MAINWINDOW_SLAVE);
                    dbgassert(compWindowNew);
                    companionWindows.push_back(DawWindowCompanion{ compWindowNew, companionCtrlStdPtr });
                    initWindowControl(compWindowNew);
                    if (companionCtrlStdPtr->isOk()) {
                        for (track_t* tr : project.trackList) {
                            companionCtrlStdPtr->view->ctr_tracks2.addTrack(tr, FLG_TRK_CHANGE_LOAD);
                        }
                        companionCtrlStdPtr->updateVisibleTrackContents();
                        companionCtrlStdPtr->layoutView();
                        companionCtrlStdPtr->fixCursor();
                    }
                    if (companionCtrlStdPtr->isOk()) {
                        this->dawCtrls.push_back(companionCtrlStdPtr.get());
                    }
                } else if (companionWindows.size() && companionWindows[0].ctrl && companionWindows[0].ctrl->isOk()) {
                    companionWindows[0].wnd->show();
                }
                return;
            case CMD_UNDO:
                if (hist.canUndo()) {
                    ThreadLock lock = playThread.lockThread();
                    hist.undoStep(this);
                    updateVisibleTrackContents();
                }
                break;
            case CMD_REDO:
                if (hist.canRedo()) {
                    ThreadLock lock = playThread.lockThread();
                    hist.redoStep(this);
                    updateVisibleTrackContents();
                }
                break;
            case CMD_FILE_NEW: {
                stopPlaying();
                setAudioThreadState(playback_state::status_no_process);
                //TODO: stop playback here
                setEmptyProject();
                layoutTrackEditors();
                updateVisibleTrackContents();
                setAudioThreadState(playback_state::status_stop);
            } break;
            case CMD_FILE_OPEN: {
                if (command.arg1.empty()) {
                    String path;
                    if (promptUserFilePath(mainCtrl->window, 0, vFILE_TYPE_PROJECT, path)) {
                        loadFile(path, FLAG_INVOKE_USER_CB_DEFERLOAD);
                    }
                } else {
                    loadFile(command.arg1, FLAG_INVOKE_USER_CB_DEFERLOAD);
                }
            } break;
            case CMD_FILE_SAVEAS:
            case CMD_FILE_SAVE: {
                String path = projectPath;
                if (command.command == CMD_FILE_SAVEAS || path.empty()) {
                    if (!promptUserFilePath(mainCtrl->window, 1, vFILE_TYPE_PROJECT, path)) {
                        break;
                    }
                }
                saveFile(path);
                DAW::settings.recentfiles.add(path);
                break;
            }
            case CMD_FILE_CLOSE:
                break;
            case CMD_CUT:
                break;
            case CMD_COPY:
                break;
            case CMD_PASTE:
                break;
            case CMD_DELETE:
                break;
            case CMD_SELECT_ALL:
                break;
            case CMD_DUPLICATE:
                break;
            case CMD_INSERT_AUDIO_TRACK:
            case CMD_INSERT_MIDI_TRACK:
            case CMD_INSERT_RETURN_TRACK:
            case CMD_INSERT_MASTER_TRACK: {

                int32_t trackType = (command.command - CMD_INSERT_AUDIO_TRACK) % NUM_TRACK_TYPES;
                insertNewTrack(-1, trackType);
            } break;
            case CMD_ABOUT:
                mainCtrl->openDialog(new guidialog_about());
                break;
            case CMD_SHOW_DEBUG_WINDOW:
                if (command.argInt == 3) {
                    auto companionCtrlStdPtr = std::make_shared<PopupCtrl>();
                    auto compWindowNew       = mainCtrl->mainWindow->createOverlay(companionCtrlStdPtr, WINDOW_IS_MAINWINDOW_SLAVE);
                    dbgassert(compWindowNew);
                    companionWindows.push_back(DawWindowCompanion{ compWindowNew, companionCtrlStdPtr });
                    initWindowControl(compWindowNew);
                    if (companionCtrlStdPtr->isOk()) {
                        auto* ctxtWindowTheme = compWindowNew->getCtrl()->getTheme();
                        //copy theme from this control to contextWindows control
                        *ctxtWindowTheme                  = *mainCtrl->getTheme();
                        compWindowNew->getCtrl()->m_scale = mainCtrl->m_scale;
                        auto b                            = new guidialog_about;
                        ivec2 windowPos;
                        ivec2 windowSize;
                        mainCtrl->mainWindow->getPos(&windowPos);
                        mainCtrl->mainWindow->getSize(&windowSize);
                        ivec2 wndPos = windowPos + (windowSize - b->size) / 2;
                        static_cast<PopupCtrl*>(compWindowNew->getCtrl())->open(b, wndPos, true);//ugly cast
                    }
                    return;
                }
#if CREATE_DEBUG_COMPANION_WINDOW
                if (command.argInt == 0) {
                    openDebugWindowWaveformCache(dynamic_cast<window_main*>(mainCtrl->window));
                    return;
                }
                if (command.argInt == 1) {
                    openDebugWindowNanoVG(dynamic_cast<window_main*>(mainCtrl->window));
                    return;
                }
                if (command.argInt == 2) {
                    openDebugWindowPerformance(dynamic_cast<window_main*>(mainCtrl->window));
                    return;
                }
#endif
                break;
            case CMD_PREFERENCES:
                mainCtrl->openDialog(new guidialog_settings());
                break;
            case CMD_EXIT:
                mainCtrl->mainWindow->requestClose();
                break;
        }
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

void DawCtrl::menuCommand(const menucmd_t&& command) {
    switch (command.command) {

        case CMD_GUI_GLOBAL_ZOOM_DECREASE:
            m_scale = math::max(0.05f, m_scale - 0.05f);
            BaseCtrl::relayout();
            return;
        case CMD_GUI_GLOBAL_ZOOM_INCREASE:
            m_scale = math::min(10.0f - 0.05f, m_scale + 0.05f);
            BaseCtrl::relayout();
            return;
    }
    daw.menuCommand(std::move(command));
}

void DawCtrl::postInit() {
    BaseCtrl::relayout();
    updateVisibleTrackContents();
}

void MainCtrl::postInit() {
    daw.startDaw();
    waveformrender::getInstance()->init();// move into init()
    daw.postInit();
    DawCtrl::postInit();
    view->storeLayout(&layouts[0]);
    for (auto i = 1; i < layouts.size(); i++) {
        std::shared_ptr<dawview_layout_t> viewLayout = loadDawViewLayoutSnapshot(StringFormat("data/view%d.layout", i));
        if (viewLayout) {
            layouts[i] = *viewLayout.get();
        }
    }
    view->loadLayout(&layouts[1]);
    dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
    BaseCtrl::relayout();
}

void DawInstance::postInit() {
    dbgassert(initState == 2);
    using DAW::settings;
    initState++;
    audiohost::getInstance()->initPa();
    midihost::getInstance()->initPm();
    if (settings.startEngine) {
        audiohost* audioHost = audiohost::getInstance();
        if (audioHost->startAudio(settings.iosettings)) {
            host->setOutput(audioHost);
        } else {
            //notify user
            log_printf("audioHost->startAudio() failed\n", 0);
        }
    }
    midihost::getInstance()->startMidi();
    this->playThread.setTls(daw_tls::getTls());
    this->playThread.startThread(this);
    dbgassert(this->playThread.getState() == playback_state::status_no_process);
    host->initThreads();
    this->workerThread.setTls(daw_tls::getTls());
    this->workerThread.startThread();

    setAudioThreadState(playback_state::status_stop);
    this->workerThread.call([]() {
                          log_printf("WorkerThreadCallTest\n", 0);
                      })
            ->wait();
    if (!loadProject.empty()) {
        loadFile(loadProject, FLAG_DEFER_LOAD);
    } else {
        setEmptyProject();
    }
}

void DawInstance::updateClipViews(clip_t* notifyClip, clip_cursor_t cursor) {
    for (auto* ctrl : dawCtrls) {
        clip_view& view = ctrl->getClipView();
        if (view.clip() == notifyClip) {
            view.cursor = cursor;
            view.copySelectedNoteList();
            view.updateNotePitches(false);
        }
    }
}

void DawInstance::destroy() {
    dbgassert(initState == 3);
    initState = 4;
    setAudioThreadState(playback_state::status_no_process);
    //dbgassert(playThread.getState() == playback_state::status_no_process);
    ThreadLock lock = playThread.lockThread();
    midihost::getInstance()->stopMidi();
    audiohost::getInstance()->stopAudio();
    unloadProject();
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_printf("getNumClipAllocations == %d!\n", totalAllocs);
        dbgassert(getNumClipAllocations() == 0);
    }


    for (auto& companion : companionWindows) {
        dbgassert(!companion.ctrl->isOk());
    }
    companionWindows.clear();
    host->unload();
    host->destroy();
    audiohost::getInstance()->deinitPa();
    midihost::getInstance()->deinitPm();
    waveformrender::getInstance()->destroy();
    plugindb.closeDatabase();
    this->workerThread.stopThread();
    this->workerThread.joinThread();
    this->playThread.stopThread();
    this->playThread.joinThread();
    daw_tls::tlsinstance& tls = daw_tls::getTls();
    delete tls.waveform;
    delete tls.audioCache;
    delete tls.midiHost;
    delete tls.audioHost;
    tls.host           = nullptr;
    tls.midiHost       = nullptr;
    tls.audioHost      = nullptr;
    tls.mainCtrl       = nullptr;
    tls.project        = nullptr;
    tls.pluginDatabase = nullptr;
    tls.waveform       = nullptr;
    tls.audioCache     = nullptr;
    delete host;
    host = nullptr;
}

void DawCtrl::destroy() {
    if (!isOK) {
        return;
    }

    isOK = false;
    if (viewContainers) {
        delete viewContainers;
        viewContainers = nullptr;
    }
}

void DawInstance::startDaw() {
    dbgassert(initState == 1);
    initState++;
    plugindb.openDatabase();
}

void DawInstance::initDaw(int argc, char* argv[]) {
    dbgassert(initState == 0);
    using DAW::settings;
    initState++;
    for (int i = 1; i < argc; i++) {
        String s = argv[i];
        if (s == "--load" && i + 1 < argc) {
            loadProject = argv[i + 1];
        }
    }
    daw_tls::tlsinstance& tls = daw_tls::getTls();
    auto audioHost            = new audiohost();
    host                      = new vsthost();
    auto midiHost             = new midihost();
    if (!vsthost::assignMasterCallback(host)) {
        delete host;
        dbgassert(0);
        throw applogicexception("no empty vst callback slot");
    }
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(settings.iosettings.internalSamplerate), settings.iosettings.internalBlocksize, sampleformat_bits_t::FLOAT_32 });
    tls.project = this;
    //    tls.mainCtrl = this;
    tls.audioHost      = audioHost;
    tls.host           = host;
    tls.midiHost       = midiHost;
    tls.pluginDatabase = &plugindb;
    tls.audioCache     = new audiocache(settings.iosettings.samplerate);
    tls.waveform       = new waveformrender(pathrenderer_type_e::ADV);
}

void DawCtrl::initApp(int argc, char* argv[]) {
}

MainCtrl::MainCtrl(DawInstance& _daw) : DawCtrl(_daw) {
    log_printf("MainCtrl constructor\n", 0);
}

void MainCtrl::initApp(int argc, char* argv[]) {
    daw_tls::tlsinstance initTls;
    initTls.tlsInitialized = true;
    initTls.config         = new app_config_t{};
    initTls.mainCtrl       = this;
    daw_tls::setTls(initTls);

    daw.initDaw(argc, argv);
}

bool MainCtrl::init(window_main* window, NVGcontext* nanovg) {
    return DawCtrl::init(window, nanovg);
}

bool DawCtrl::init(window_main* window, NVGcontext* nanovg) {
    dbgassert(!this->mainWindow);
    this->mainWindow = window;
    this->window     = window;
    this->vg         = nanovg;
    themes.loadThemes();

    getDefaultTheme()->initTheme();
    getDefaultTheme()->bindFonts();
    setupView();

    menus.recent.type  = ngui::menu_type::submenu;
    menus.recent.title = "Open recent";
    menus.recent.addCommand(CMD_NOARG(CMD_FILE_OPEN), "File 1");
    menus.recent.addCommand(CMD_NOARG(CMD_FILE_OPEN), "File 2");
    menus.recent.addCommand(CMD_NOARG(CMD_FILE_OPEN), "File 4");
    menus.recent.addCommand(CMD_NOARG(CMD_FILE_OPEN), "File 5");
    menus.file.type  = ngui::menu_type::submenu;
    menus.file.title = "File";
    menus.file.addCommand(CMD_NOARG(CMD_FILE_NEW), menuName("New", KC_NEW), ICON_FILE);
    menus.file.addCommand(CMD_NOARG(CMD_FILE_OPEN), menuName("Open", KC_OPEN), ICON_FOLDER);
    menus.file.add(&menus.recent);
    menus.file.addCommand(CMD_NOARG(CMD_FILE_SAVE), menuName("Save", KC_SAVE), ICON_SAVE);
    menus.file.addCommand(CMD_NOARG(CMD_FILE_SAVEAS), "Save As");
    menus.file.addSeperator();
    menus.file.addCommand(CMD_NOARG(CMD_EXIT), "Quit");
    menus.edit.type  = ngui::menu_type::submenu;
    menus.edit.title = "Edit";
    menus.edit.addCommand(CMD_NOARG(CMD_UNDO), menuName("Undo", KC_UNDO));
    menus.edit.addCommand(CMD_NOARG(CMD_REDO), menuName("Redo", KC_REDO));
    menus.edit.addSeperator();
    menus.edit.addCommand(CMD_NOARG(CMD_CUT), menuName("Cut", KC_CUT));
    menus.edit.addCommand(CMD_NOARG(CMD_COPY), menuName("Copy", KC_COPY));
    menus.edit.addCommand(CMD_NOARG(CMD_PASTE), menuName("Paste", KC_PASTE));
    menus.edit.addCommand(CMD_NOARG(CMD_DUPLICATE), menuName("Duplicate", KC_DUPLICATE));
    menus.edit.addSeperator();
    menus.edit.addCommand(CMD_NOARG(CMD_DELETE), menuName("Delete", KC_DELETE));
    menus.edit.addCommand(CMD_NOARG(CMD_SELECT_ALL), menuName("Select All", KC_SELECTALL));
    menus.tools.type  = ngui::menu_type::submenu;
    menus.tools.title = "Tools";
    menus.tools.addCommand(CMD_NOARG(CMD_PREFERENCES), "Preferences");
    menus.tools.addCommand(CMD_NOARG(CMD_ABOUT), "About");
    menus.views.addCommand(CMD_NOARG(CMD_OPEN_SECOND_WINDOW), "Show Second Window");
    menus.views.addSeperator();
    menus.views.title = "View";
    for (int i = static_cast<int>(container_type::CTR_TYPE_PROPERTIES); i < CTR_TYPE_COUNT; i++) {
        String name;
        getContainerLabel(static_cast<container_type>(i), name);
        menus.views.addCommand(menucmd_t{
                                       CMD_OPEN_VIEW,
                                       name,
                                       static_cast<int>(i) },
                               "Show " + name);
    }
    menus.views.addSeperator();
    menus.views.addCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 0), "Show Waveform Cache");
    menus.views.addCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 1), "Show dbg window");
    menus.views.addCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 2), "Show profiling results");
    menus.views.addCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 3), "Show test dialog");

    menubar.add(&menus.file);
    menubar.add(&menus.edit);
    menubar.add(&menus.tools);
    menubar.add(&menus.views);
    this->updateMenubar();
#if !USE_GUI_MENU
    this->mainWindow->updateMenu();
#endif

    using DAW::settings;
    if (isCompanion()) {
        grid.grid_dens = settings.wndCompanion.dens;
    } else {
        grid.grid_dens = settings.wndMain.dens;
    }

    updateGrid();
    isOK = true;
    return isOK;
}

void DawCtrl::onTick() {
    for (guictr_base* ctr : containers) {
        ctr->onTick(this);
    }
    for (guictr_base* ctr : containers) {
        ctr->onIdle();
    }
    //if (rand.rng_rand(100000) == 0) {
    //    throw std::bad_alloc();
    //}
    //log_printf("onTick %d\n", std::this_thread::get_id());
    mainWindow->requestRedraw();

    if (!guiDragged && !guiCaptured && guiOver && (!this->ctxtmenu || ctxtmenu->isTransient())) {
        auto hoverTime = tmLastHoveredTooltip;
        if (ctxtmenu && ctxtmenu->isTransient() && (lastTooltipSrc && guiOver && guiOver != lastTooltipSrc)) {
            closeContextMenu();
        }
        if (ctxtmenu && !ctxtmenu->isTransient()) {
            hoverTime = 0;
        }
        if (!ctxtmenu) {
            auto timeNow = getTimeMillis();
            if (guiOver == lastHoveredTooltip && timeNow - tmLastHoveredTooltip >= 360) {
                auto newContextMenu = guiOver->getTooltip(this);
                if (newContextMenu) {
                    newContextMenu->theme = getTheme();
                    lastTooltipSrc        = guiOver;
                    daw.nextTooltipId++;
                    openContextMenu(newContextMenu, m_mousePos + ivec2(-16, 26));
                }
                hoverTime = 0;
            } else if (guiOver != lastHoveredTooltip) {
                hoverTime = timeNow;
            }
        }
        tmLastHoveredTooltip = hoverTime;
        lastHoveredTooltip   = guiOver;
    } else {
        if (ctxtmenu && ctxtmenu->isTransient()) {
            closeContextMenu();
        }
    }
}

void MainCtrl::onTick() {
    daw.onTick();
    DawCtrl::onTick();

    if (guiDragged && !guiCaptured && guiDragged->isDragMoveable()) {
        track_gui_entry_t* tr  = nullptr;
        int32_t hoverTicks     = 0;
        guictr_base& ctrMixers = view->ctr_tracks.trackControls;
        if (view->ctr_tracks.isVisible()) {
            ivec2 trackViewLocalPos = toControlsObjectSpace(m_mousePos, &view->ctr_tracks);
            if (ctrMixers.contains(trackViewLocalPos)) {
                ivec2 posRelative = m_mousePos - ctrMixers.toScreenSpace(ivec2(0));
                tr                = getTrackFromMouse(this->view->ctr_tracks.guiMgr, posRelative, false);
                if (tr && tr == lastHoveredTrack && daw.getSelectedTrack() != tr->track) {
                    hoverTicks = lastHoveredTrackTicks + 1;
                    if (lastHoveredTrackTicks >= 6) {
                        daw.setSelectedTrackEntry(tr);
                        showPluginView();
                        hoverTicks = 0;
                    }
                }
            }
        }
        if (view->ctr_pluginview.isVisible() && view->ctr_pluginview.contains(m_mousePos)) {
            hoverTicks = lastHoveredTrackTicks + 1;
            if (lastHoveredTrackTicks >= 6) {
                showPluginView();
                hoverTicks = 0;
            }
        } else if (view->ctr_clipeditorview.isVisible() && view->ctr_clipeditorview.contains(m_mousePos)) {
            hoverTicks = lastHoveredTrackTicks + 1;
            if (lastHoveredTrackTicks >= 6) {
                showClipEditor();
                hoverTicks = 0;
            }
        }
        lastHoveredTrackTicks = hoverTicks;
        lastHoveredTrack      = tr;
    }
}

void DawInstance::updateGrid() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->updateGrid();
    }
}

void DawInstance::onPluginsChanged() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->onPluginsChanged();
    }
}

void DawInstance::updateVisibleTrackContents() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->updateVisibleTrackContents();
    }
}

void DawInstance::layoutTrackEditors() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->layoutView();
    }
}

void DawInstance::getTrackContainers(std::vector<guictr_tracks*>& trackCointainers) {
    for (int i = 0; i < dawCtrls.size(); i++) {
        dawCtrls[i]->getTrackContainers(trackCointainers);
    }
}

void DawInstance::setMainControl(MainCtrl* _mainCtrl) {
    dbgassert(!this->mainCtrl);
    this->mainCtrl = _mainCtrl;
    this->dawCtrls.push_back(_mainCtrl);
}

MainCtrl* DawInstance::getMainControl() {
    return this->mainCtrl;
}

guictxtmenu_base* makeGuiAutosave(int64_t delay);

String getProjectAutosaveFilename(String projectPath) {
    String bakPathName;
    if (projectPath.empty()) {
        String tmpPath = toUserdataPath("unsaved.project");
        int count      = 1;
        while (FileExists(tmpPath)) {
            tmpPath = toUserdataPath(StringFormat("unsaved-%d.project", count));
            count++;
        }
        bakPathName = tmpPath;
    } else {
        String path, name, ext, nameExt;//path, name, ext, nameExt
        SplitPath(projectPath, &path, &name, &ext, &nameExt);
        bakPathName = path + DAW_FILEIO_PATHSEP + name + "-autosave." + ext;
    }
    return bakPathName;
}

void DawInstance::triggerAutoSave() {
    tmLastSave          = getTimeMillis();
    projectPathAutosave = getProjectAutosaveFilename(projectPath);

    std::shared_ptr<project_file> f = createProjectFile();
    saveProject(f, projectPathAutosave);
}

String DawInstance::getAutoSaveFilename() {
    return getProjectAutosaveFilename(projectPath);
}

void DawInstance::onTick() {
    const bool bWroteMidiData = host->writeRecordedData(&project);

    if (bWroteMidiData) {
        for (auto ctrl : dawCtrls) {
            ctrl->updateVisibleTrackContents();
        }
    }

    host->onTick();

    bool noPopups        = true;
    bool canOpenAutosave = true;
    for (auto* ctrl : dawCtrls) {
        noPopups &= !ctrl->guiDragged && !ctrl->guiCaptured && !ctrl->ctxtmenu;
        canOpenAutosave &= noPopups;
        canOpenAutosave &= !ctrl->hasDialogWindows();
        canOpenAutosave &= !ctrl->hasContextMenu();
        canOpenAutosave &= ctrl->hasInputFocus();
        /*canOpenAutosave &= last click was n seconds ago*/
    }
    if (noPopups && projectToLoad) {
        std::shared_ptr<project_to_load_t> projectToLoadCpy = projectToLoad;
        projectToLoad                                       = nullptr;
        bool projectLoadErrored                             = false;
        AppWndProc_enableBlockReentrant();
        try {
            setLoadedProject(projectToLoadCpy->projectfile, projectToLoadCpy->loadflags);
        } catch (std::exception& e) {
            log_printf("Failed loading project: %s %s\n", StringAsCStr(typeName(e)), e.what());
            projectLoadErrored = true;
        } catch (...) {
            log_printf("Failed loading project. Unhandled exception\n", 0);
            projectLoadErrored = true;
        }
        AppWndProc_disableBlockReentrant();
        if (cbProjectLoadCompleteCallback) {
            cbProjectLoadCompleteCallback(this, projectToLoadCpy->projectfile, projectLoadErrored ? 1 : 0);
            cbProjectLoadCompleteCallback = nullptr;
        }
        log_printf("end of setLoadedProject\n", 0);
    }
    if (canOpenAutosave && autosaveState.isEnabled) {
        if (0 == autosaveState.tmLastTrigger) {
            autosaveState.tmLastTrigger = getTimeMillis();
        }
        int64_t tmNow = getTimeMillis();
        if (tmNow - tmLastSave > autosaveState.tmSaveDelay) {
            if (tmNow - autosaveState.tmLastTrigger > autosaveState.tmReminderDelay) {
                autosaveState.tmLastTrigger = tmNow;
                auto tooltip                = makeGuiAutosave(5000);
                auto ctrlSize               = mainCtrl->m_size;
                tooltip->size               = ivec2(420, 90);
                tooltip->maxHeight          = tooltip->size.y;
                mainCtrl->openContextMenu(tooltip, ivec2(ctrlSize.x / 2, ctrlSize.y - 100) - tooltip->size / 2, BASECTRL_WND_POS_RELATIVE);
            }
        }
    }
}

void DawInstance::pushHist(action_base* action) {
    hist.push(this, action);
}

std::shared_ptr<project_file> DawInstance::createProjectFile() {
    ThreadLock lock                    = playThread.lockThread();
    std::shared_ptr<project_file> file = std::make_shared<project_file>();
    file->path                         = projectPath;
    project.copyTo(file->project);
    file->project.globals        = projectGlobals;
    file->project.exportSettings = getExportSettings();
    audiocache::getInstance()->store(file->sampleFileIndex);
    file->layout.layoutGrid    = mainCtrl->grid;
    file->layout.scrollOffsetX = mainCtrl->view->ctr_tracks.getScrollOffset();
    return file;
}

bool DawInstance::setProjectToLoad(std::shared_ptr<project_file> file, int flags) {
    projectToLoad = std::make_shared<project_to_load_t>(project_to_load_t{ std::move(file), flags });
    return true;
}

/**
 * setLoadedProject - releases current project and resources and loads in new project from passed project_file
 *
 * - puts audio thread into state playback_state::status_no_process
 * - establishes lock against AudioThread
 * - unloads project (freeing resources)
 * - loads samplefile index
 * - populates tracklist
 * - creates audio instances for all tracks
 * - adds tracks to MainCtrls guictr_tracks, creating gui instances
 * - pre loads plugins
 * - optionally fully loads plugin instances
 * - loads track layouts
 * - loads cursor state
 * - sets project_file::path as current project path
 * - puts audio thread into state playback_state::status_stop
 *
 * @param file - shared_ptr to project_file instance containg project data to load from
 * @param flags - 0 or FLAG_DEFER_LOAD (don't load vst plugins, use placeholders)
 * @return
 */
bool DawInstance::setLoadedProject(std::shared_ptr<project_file> file, int flags) {

    setAudioThreadState(playback_state::status_no_process);
    log_printf("loading %s: %d tracks\n", StringAsCStr(file->path), project.trackList.size());
    ThreadLock lock = playThread.lockThread();
    unloadProject();
    /** make sure call to unloadProject unloaded all vst2 instances **/
    dbgassert(host->getVst2Instances().empty());
    //TODO: assert that audiocache is empty
    dbgassert(audiocache::getInstance()->isEmpty());

    /** populates trackList **/
    project.copyFrom(file->project);
    projectGlobals      = file->project.globals;
    getExportSettings() = file->project.exportSettings;


    /** create all audio instances **/
    for (track_t* t : project.trackList) {
        t->fixClipLengths();
        host->createAudio(t);
    }


    /** create all gui instances **/
    for (track_t* tr : project.trackList) {
        for (DawCtrl* pDawCtrl : dawCtrls) {
            pDawCtrl->addTrackToView(tr, FLG_TRK_CHANGE_LOAD);
        }
    }
    /** pre-load all plugin instances **/
    project.trackList.loadPlugins(file->project);

    /** reset maximum stage id and determine new maximum stage id **/
    host->updateMaximumStageId();

    /** remove routings to missing track **/
    DAW::validateTrackRoutings(host, project.getTracksFlatVec());
    /** create all gui instances **/
    for (track_t* tr : project.trackList) {
        DAW::validateEffectRoutings(host, tr->audio);
    }

    /** inform host about track layout changes so it resets and updates internal structures **/
    host->onTrackLayoutChange();

    MainCtrl* renderCtrl = this->mainCtrl;
    if (1) {
        /**
         * plugin loading was not deferred.
         * handle request to load all plugins.
         */

        /** loading screen guictr class **/
        class guictr_loading : public guictr_base {
        public:
            String text;
            guictr_loading() {
                setLabel("LOADING ");
            }
            void render(NVGcontext* vg) override {
                guictr_base::render(vg);
                vec2 cs       = getSizeContent();
                auto cStrText = StringAsCStr(text);
                setFont(vg, 32, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_CENTER);
                nvgText(vg, cs.x / 2.0f, cs.y / 2.0f, cStrText, nullptr);
            }
        };
        guictr_loading ctr;
        ctr.size = renderCtrl->m_size;
        ctr.setControl(renderCtrl);
        ctr.layout();

        /** precondition: an existing with opengl+nanoVG context **/
        auto windowMain = dynamic_cast<window_main*>(renderCtrl->window);
        dbgassert(windowMain);

        if ((flags & FLAG_DEFER_LOAD) == 0) {

            /** get the list of all plugins in deferred loading state **/
            std::vector<effectbase*> pluginsDeferred;
            host->getDeferredEffects(pluginsDeferred);

            /**
             * The following loop calls activateDeferred on all tracks, effectively doing the following sequence for each track:
             *  - load shared libraries
             *  - create audioeffect instance
             *  - load binary plugin snapshots
             *  - load plugin, mixer, arp parameter values
             *  - load plugin, mixer, arp automation
             *
             * plugin loading can take a long time and will block the main thread.
             * Ideally this would happen on another thread, but that might not work for all vst plugins.
             */
            std::vector<effectbase*> pluginsLoaded;
            pluginsLoaded.reserve(pluginsDeferred.size());
            log_printf("begin plugin list loading\n", 0);
            int len = pluginsDeferred.size();
            for (int i = 0; i < len; i++) {
                dbgassert(pluginsDeferred[i]->getModuleType() == PLUGIN_TYPE_DEFERRED);
                auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[i]);
                windowMain->preRender();

                NVGcolor col = renderCtrl->getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
                glClearColor(col.r, col.g, col.b, col.a);
                glClear(GL_COLOR_BUFFER_BIT);

                float ratio = 1.0;
                auto vg     = renderCtrl->vg;

                nvgBeginFrame(vg, renderCtrl->m_size.x, renderCtrl->m_size.y, ratio);
                nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);

                nvgSave(vg);
                ctr.text = plugin->getDfrdPluginName();
                ctr.render(vg);
                nvgRestore(vg);

                nvgEndFrame(vg);

                windowMain->postRender();

                /** TODO: vsync **/
                seqthreads::threadSleep(16);
                log_printf("pre activateDeferred %s\n", StringAsCStr(ctr.text));
                effectbase* pluginLoaded;
                host->activateDeferred(plugin, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &pluginLoaded);
                log_printf("post activateDeferred %s\n", StringAsCStr(ctr.text));
                if (pluginLoaded) {
                    pluginsLoaded.push_back(pluginLoaded);
                }
            }
            log_printf("end plugin list loading\n", 0);
        }
        const int32_t numSamplesToLoad = file->sampleFileIndex.list.size();
        {
            windowMain->preRender();
            NVGcolor col = renderCtrl->getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
            glClearColor(col.r, col.g, col.b, col.a);
            glClear(GL_COLOR_BUFFER_BIT);
            float ratio = 1.0;
            auto vg     = renderCtrl->vg;
            nvgBeginFrame(vg, renderCtrl->m_size.x, renderCtrl->m_size.y, ratio);
            nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);

            nvgSave(vg);
            ctr.text = StringFormat("Loading %d samples", numSamplesToLoad);
            ctr.render(vg);
            nvgRestore(vg);
            nvgEndFrame(vg);
            windowMain->postRender();
            /** TODO: vsync **/
            seqthreads::threadSleep(16);
            log_printf("pre load samplefileindex\n", 0);
            audiocache::getInstance()->load(file->sampleFileIndex);
            log_printf("post load samplefileindex\n", 0);
        }
        log_printf("end sample loading\n", 0);
        ctr.setControl(nullptr);
        AppWndProc_disableBlockReentrant();
        for (track_t* tr : project.trackList) {
            tr->getStage()->pluginsChanged();
        }
        host->onTrackLayoutChange();
    }

    /** validate cursor state **/
    auto ctrl = mainCtrl;
    if (ctrl) {
        ctrl->view->ctr_tracks.loadTrackLayouts(file->project.trackCtr);
        ctrl->view->ctr_tracks.loadTrackLayouts(file->project.trackReturnCtr);
        ctrl->view->ctr_tracks.loadTrackLayouts(file->project.trackMasterCtr);

        ctrl->grid.setLayout(file->layout.layoutGrid);
        ctrl->view->ctr_tracks.layout();
        ctrl->view->ctr_plugins.layout();

        ctrl->updateVisibleTrackContents();
        ctrl->view->ctr_tracks.layout();
        ctrl->view->ctr_tracks.setScrollOffset(file->layout.scrollOffsetX);
        auto& cursor = ctrl->getCursor();
        auto& guiMgr = ctrl->view->ctr_tracks.guiMgr;
        if (cursor.isSubtrackSelection() && guiMgr.validTrackIdx(cursor.cursorTrack)) {
            const track_gui_entry_t* tr = guiMgr.at(cursor.cursorTrack);
            fixCursorSubRange(cursor, tr->subtracks.size());
        } else {
            fixCursorTrackRange(cursor, guiMgr.getTracksVisibleFlat().size());
        }
    }
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl == mainCtrl)
            continue;
        pDawCtrl->updateVisibleTrackContents();
        pDawCtrl->layoutView();
        pDawCtrl->fixCursor();
    }

    /** set as current project **/
    this->projectPath = file->path;
    this->tmLastSave  = getTimeMillis();

    setAudioThreadState(playback_state::status_stop);
    return true;
}

void MainCtrl::dragContainerRelayout(drag_ctr_event evt) {
    viewContainers->dragContainerRelayout(this, evt);
    if (evt.evtType == BaseCtrl::drag_ctr_event_type::DRAG_END) {
        BaseCtrl::relayout();
    }
}

void MainCtrl::getTrackContainers(std::vector<guictr_tracks*>& trackCointainers) {
    trackCointainers.push_back(&view->ctr_tracks);
}

void CompanionCtrl::getTrackContainers(std::vector<guictr_tracks*>& trackCointainers) {
    trackCointainers.push_back(&view->ctr_tracks2);
}

void MainCtrl::layoutView(int32_t w, int32_t h) {
    w = math::max(640, w);
    h = math::max(480, h);
    viewContainers->layout(w, h);

    view->ctr_plugins.layout();
    view->ctr_clipeditor.layout();
    view->ctr_tracks.relayout();
    view->ctr_nodes.layout();
    for (guictr_base* ctr : containers) {
        if (ctr == &view->ctr_clipeditor)
            continue;
        if (ctr == &view->ctr_plugins)
            continue;
        if (ctr == &view->ctr_tracks)
            continue;
        if (ctr == &view->ctr_nodes)
            continue;
        ctr->layout();
    }
}

void CompanionCtrl::layoutView(int32_t w, int32_t h) {
    w = math::max(640, w);
    h = math::max(480, h);
    viewContainers->layout(w, h);

    view->ctr_tracks2.relayout();
    view->ctr_nodes.layout();
    view->ctr_dnd_test->layout();
    view->ctr_clipeditor.layout();
    for (guictr_base* ctr : containers) {
        if (ctr == &view->ctr_tracks2)
            continue;
        if (ctr == &view->ctr_clipeditor)
            continue;
        if (ctr == &view->ctr_nodes)
            continue;
        if (ctr == view->ctr_dnd_test)
            continue;
        ctr->layout();
    }
}

void DawCtrl::relayout(int32_t w, int32_t h) {
    closeAllAppMenus();
    if (ctxtmenu && !ctxtmenu->isDialog()) {
        closeContextMenu();
    }
    layoutView(w, h);
}

void DawInstance::setSelectedTrackEntry(track_gui_entry_t* trackEntry) {
    selectedTrack = trackEntry ? trackEntry->track : nullptr;
    mainCtrl->view->ctr_plugins.showTrack(trackEntry && trackEntry->track ? trackEntry->track->audio : nullptr);
}

void DawInstance::setSelectedTrack(track_t* track) {
    selectedTrack = track;
    mainCtrl->view->ctr_plugins.showTrack(track ? track->audio : nullptr);
}

track_t* DawInstance::getSelectedTrack() {
    return selectedTrack;
}

guictr_plugins* MainCtrl::getPluginCtr() {
    auto ctrlThis = get();
    return ctrlThis ? &ctrlThis->view->ctr_plugins : nullptr;
}

guictr_tracks* MainCtrl::getGuiTrackCtr() {
    auto ctrlThis = get();
    return ctrlThis ? &ctrlThis->view->ctr_tracks : nullptr;
}
void MainCtrl::updateGrid() {
    static int n = 0;
    if (n++ % 20 == 0) {
        log_printf("updateGrid call #%d\n", n);
    }
    grid.update(view->ctr_tracks.trackView.getSizeContent());
    view->ctr_tracks.updateVisibleTrackContents();
}

guitrack_editor& MainCtrl::getTrackEditor() {
    return view->ctr_tracks.trackView;
}

void MainCtrl::onPluginsChanged() {
    view->ctr_nodes.reset();
    view->ctr_nodes.refresh();
    view->ctr_plugins.relayout();
}

void MainCtrl::updateVisibleTrackContents() {
    view->ctr_tracks.updateVisibleTrackContents();
}

bool MainCtrl::isZooming() {
    return guiCaptured == &view->ctr_tracks.trackTimeline;
}

void DawCtrl::uncaptureMouse() {
    this->mainWindow->releaseMouse();
}

void DawCtrl::onUncaptureMouse() {
    guiCaptured = nullptr;
}

void DawCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
    daw.dragdropTarget.reset();
#if USE_GUI_MENU
    if (ctxtmenu && !ctxtmenu->isTransient() && viewContainers->getMenu()) {
        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER);
        if (viewContainers->getMenu()->mouseHitTest(mousePos, evt)) {
        }
        return;
    }
#endif
    BaseCtrl::mouseMoved(mousePos, deltaPos);
}

bool DawCtrl::filesDropBegin(std::vector<String>& files, ivec2 mousepos, int kbmods) {
    log_printf("filesDropBegin %d %d isdragging=%d\n", mousepos.x, mousepos.y, daw.dragdropclip.isLoaded);
    daw.dragdropclip.reset();
    if (guiDragged || guiCaptured) {
        return false;
    }
    if (files.size()) {
        String path = files.front();
        if (StrEndsWith(path, ".wav")) {
            String a, b, c, d;
            SplitPath(path, &a, &b, &c, &d);
            audiofile_t* audio = audiocache::getInstance()->loadFile(path);
            if (audio) {
                auto* sample = audio->sample.get();
                if (sample) {
                    clip_t clip;
                    clip.clipType = CLIP_AUDIO;
                    clip.name     = b;
                    //clip.notes = move(notes);
                    clip.audio.id = audio->id;
                    clip.setLenSamples(sample->nSamples);
                    clip.setLen(daw.samplesToTicks(sample->nSamples));
                    clip.loopEnabled = false;

                    std::shared_ptr<track_clipboard_t> trClipboard = std::make_shared<track_clipboard_t>();
                    trClipboard->clips.push_back(std::make_shared<clip_t>(std::move(clip)));
                    std::shared_ptr<clip_clipboard> fileClipboard = std::make_shared<clip_clipboard>();
                    fileClipboard->tracks.push_back(trClipboard);
                    daw.dragdropclip.reset();
                    daw.dragdropclip.clipboard = fileClipboard;
                    daw.dragdropclip.isLoaded  = true;
                }
            }
        }
        if (StrEndsWith(path, ".mid")) {
            LoadMidiTask task(files.front());
            if (!MainCtrl::get()->getWorkerThread()->pushTask(&task)) {
                return false;
            }
            if (task.isInQueue()) {
                task.wait();
                if (task.isGood()) {
                    std::shared_ptr<clip_clipboard> fileloadedClipboard = task.getClipboard();
                    if (fileloadedClipboard) {
                        daw.dragdropclip.reset();
                        daw.dragdropclip.clipboard = fileloadedClipboard;
                        daw.dragdropclip.isLoaded  = true;
                        log_printf("got clip\n", 0);
                    } else {
                        log_printf("FAIL: no clip\n", 0);
                    }
                }
            }
        }
        if (daw.dragdropclip.isLoaded) {
            MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP);
            evt.setDraggedThing(&daw.dragdropclip);
            for (guictr_base* ctr : containers) {
                if (ctr->mouseHitTest(mousepos, evt)) {
                    break;
                }
            }
            guibase* gui = evt.getGuiHit();
            if (gui) {
                ivec2 mposObj = toControlsObjectSpace(mousepos, gui);
                bool result   = gui->clipDropBegin(daw.dragdropclip, mposObj, kbmods);
                if (!result) {
                }
                return result;
            }
        }
    }
    return false;
}

bool DawCtrl::filesDropMove(ivec2 mousepos, int kbmods) {
    if (guiDragged || guiCaptured) {
        daw.dragdropclip.reset();
        return false;
    }
    if (daw.dragdropclip.isLoaded) {
        daw.dragdropclip.isValidTarget = false;

        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP);
        evt.setDraggedThing(&daw.dragdropclip);
        for (guictr_base* ctr : containers) {
            if (ctr->mouseHitTest(mousepos, evt)) {
                break;
            }
        }
        guibase* gui = evt.getGuiHit();
        if (gui) {
            ivec2 mposObj = toControlsObjectSpace(mousepos, gui);
            bool result   = gui->clipDropMove(daw.dragdropclip, mposObj, kbmods);
            if (!result) {
            }
            return result;
        }
    }
    return false;
}

/* RAII helper to reset clip when scope is left */
class clipreset {
    dragdrop_midifile& clip;

public:
    clipreset(dragdrop_midifile& _clip) : clip(_clip){};
    ~clipreset() {
        clip.reset();
    }
};

bool DawCtrl::filesDropFinal(std::vector<String>& files, ivec2 mousepos, int kbmods) {
    clipreset rst(daw.dragdropclip);
    if (guiDragged || guiCaptured) {
        return false;
    }
    if (daw.dragdropclip.isLoaded && daw.dragdropclip.isValidTarget) {
        log_printf("filesDropFinal %d %d isdragging=%d\n", mousepos.x, mousepos.y, daw.dragdropclip.isLoaded);
        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP);
        evt.setDraggedThing(&daw.dragdropclip);
        for (guictr_base* ctr : containers) {
            if (ctr->mouseHitTest(mousepos, evt)) {
                break;
            }
        }
        guibase* gui = evt.getGuiHit();
        if (gui) {
            ivec2 mposObj = toControlsObjectSpace(mousepos, gui);
            bool result   = gui->clipDropFinal(daw.dragdropclip, mposObj, kbmods);
            return result;
        }
    }
    return false;
}

#if defined(__GNUC__) && defined(ENABLE_MICHAELS_GLIBCXX_HACKS)
namespace STLVectorDebugTracking {
    void dbgPrintVectorAllocs();
}
#endif

bool MainCtrl::processGlobalKeyevent(KeyEvent& event) {
    if (event.type == KeyEventType::K_PRESS) {
        if ((event.mods & KB_MOD_CTRL) == event.mods && event.keyCode >= KEY_F1 && event.keyCode <= KEY_F10) {
            uint8_t index = (event.keyCode - KEY_F1) % layouts.size();
            bool store    = (event.mods & KB_MOD_CTRL);
            if (store) {
                view->storeLayout(&layouts[index]);
                saveDawViewLayoutSnapshot(layouts[index], StringFormat("data/view%d.layout", index));
            } else {
                view->loadLayout(&layouts[index]);
                BaseCtrl::relayout();
                dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
            }
            return true;
        }
    }
    if (event.type == KeyEventType::K_PRESS) {
        if (event.keyCode == KEY_L) {
            bShowDebugFrames = !bShowDebugFrames;
            dragContainerRelayout(drag_ctr_event{ drag_ctr_event_type::DRAG_END });
            return true;
        }
    }
    return DawCtrl::processGlobalKeyevent(event);
}

bool DawCtrl::processGlobalKeyevent(KeyEvent& event) {

    if (event.type != KeyEventType::K_RELEASE) {
        if (event.keyCode == KEY_TAB) {
            switch (this->viewMode) {
                case view_mode_t::TRACK_TIMELINE:
                    this->setViewMode(view_mode_t::MIXER);
                    return true;
                case view_mode_t::MIXER:
                    this->setViewMode(view_mode_t::NODE_EDITOR);
                    return true;
                case view_mode_t::NODE_EDITOR:
                    this->setViewMode(view_mode_t::TRACK_TIMELINE);
                    return true;
            }
            return true;
        }
    }
    if (event.type == KeyEventType::K_PRESS) {
        lastKey = getKeyName(event.scancode);
        if (!lastKey.length()) {
            const char* ca = glfwGetKeyName(event.keyCode, event.scancode);
            if (ca) {
                lastKey = ca;
            }
        }
    }
    if (event.type != KeyEventType::K_RELEASE) {
        if (!event.mods && event.keyCode == KEY_M) {
            ThreadLock lock = daw.playThread.lockThread();
#if defined(__GNUC__) && defined(ENABLE_MICHAELS_GLIBCXX_HACKS)
            STLVectorDebugTracking::dbgPrintVectorAllocs();
            return true;
#endif
        }
        if (!event.mods && event.keyCode == KEY_S) {
            logStackTrace();
            return true;
        }
        if (event.keyCode == KEY_SPACE) {
            if (daw.isPlaying()) {
                daw.stopPlaying();
            } else {
                daw.startPlaying();
            }
            return true;
        }
        if (isKC(KC_UNDO, event)) {
            menuCommand(CMD_NOARG(CMD_UNDO));
            return true;
        }
        if (isKC(KC_REDO, event)) {
            menuCommand(CMD_NOARG(CMD_REDO));
            return true;
        }
        if (isKC(KC_NEW, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_NEW));
            return true;
        }
        if (isKC(KC_OPEN, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_OPEN));
            return true;
        }
        if (isKC(KC_SAVE, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_SAVE));
            return true;
        }
        if (isKC(KC_SAVEAS, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_SAVEAS));
            return true;
        }
        if (isKC(KC_ZOOM_IN, event)) {
            menuCommand(CMD_NOARG(CMD_GUI_GLOBAL_ZOOM_INCREASE));
            return true;
        }
        if (isKC(KC_ZOOM_OUT, event)) {
            menuCommand(CMD_NOARG(CMD_GUI_GLOBAL_ZOOM_DECREASE));
            return true;
        }
        if (isKC({ 0, KEY_F1, nullptr }, event)) {
            menuCommand(CMD_NOARG(CMD_OPEN_SECOND_WINDOW));
            return true;
        }
        if (isKC({ 0, KEY_F2, nullptr }, event)) {
            menuCommand(CMD_NOARG(CMD_PREFERENCES));
            return true;
        }
        if (event.type != KeyEventType::K_REPEAT) {
            if (isKC({ 0, KEY_P, nullptr }, event)) {
                if (this->getDaw() && this->getDaw()->getMainControl()) {
                    auto ctrLayoutLeft   = this->getDaw()->getMainControl()->view->ctr_Left;
                    auto ctr_performance = std::shared_ptr<guictr_base>(makeGuiPerformance());
                    ctr_performance->setLabel("Performance");
                    addLayoutEntry(ctrLayoutLeft, ctr_performance, ctr_performance->label);
                    ctrLayoutLeft->postContentChanged();
                    ctrLayoutLeft->layout();
                }
                return true;
            }
        }
    }
    return false;
}

void DawInstance::startPlaying() {
    setAudioThreadState(playback_state::status_playback);
}

void DawInstance::startExport() {
    setAudioThreadState(playback_state::status_render);
}

void DawInstance::stopPlaying() {
    setAudioThreadState(playback_state::status_stop);
}

void DawInstance::setAudioThreadState(playback_state state) {
    playThread.addRequest(REQ_STATE, (int) state, true);
}

bool DawInstance::toggleLoop() {
    projectGlobals.loopEnabled = !projectGlobals.loopEnabled;
    return projectGlobals.loopEnabled;
}

bool DawInstance::isPlaying() {
    return playThread.getState() == playback_state::status_playback;
}

bool DawCtrl::mouseDownPre() {
    daw.dragdropclip.reset();
    if (this->ctxtmenu && this->ctxtmenu->isDialog()) {
        return false;
    }
    closeAllContextMenus();
    return true;
}

track_t* DawInstance::createNewTrack(int trackType) {
    dbgassert(trackType >= 0 && trackType < NUM_TRACK_TYPES);
    int32_t tryTypeOffset = project.trackTypeCtrs[trackType]->size();

    String name       = StringFormat("%s %d", TrackTypeToName(trackType), tryTypeOffset + 1);
    track_t* newTrack = new track_t(trackType, name, true);
    newTrack->rgb     = colorPalette[rand.rng_rand(COLOR_PALETTE_LEN)];

    switch (trackType) {
        case TRACK_TYPE_MIDI:
            break;
        case TRACK_TYPE_RETURN:
            break;
        case TRACK_TYPE_MASTER:
            break;
    }
    return newTrack;
}

track_t* DawInstance::insertNewTrack(int trackInsertPos, int trackType, int flags) {
    track_t* newTrack = createNewTrack(trackType);
    ThreadLock lock   = playThread.lockThread();
    addTrackImpl(trackInsertPos, newTrack, flags);

    return newTrack;
}

class action_modify_track_add : public action_base {
public:
    int32_t trackIdx = -1;
    int32_t localIdx = -1;
    track_t* trackPtr;
    action_modify_track_add() = delete;

    action_modify_track_add(String description, track_t* _trackPtr) : action_base() {
        desc     = description;
        trackPtr = nullptr;
        trackIdx = _trackPtr->projectIdx;
        localIdx = _trackPtr->localIdxFlat;
        dbgassert(DawInstance::get()->getTrackId(trackIdx) == _trackPtr);
    }

    ~action_modify_track_add() override = default;

    void releaseResources(DawInstance* daw) override {
        if (trackPtr) {
            releaseTrackResources(trackPtr, daw);
            delete trackPtr;
            trackPtr = nullptr;
        }
    }

    void undo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetEditClip();
        trackPtr = daw->getTrackId(trackIdx);
        dbgassert(trackPtr && trackPtr->audio && trackPtr->audio->sampleFormat.blockSize % 8 == 0);// see if pointer is valid
        dbgassert(localIdx == trackPtr->localIdxFlat);
        //SERIALIZE TRACK VSTs
        localIdx = trackPtr->localIdxFlat;
        daw->removeTrackImpl(trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
    }

    void redo(DawInstance* daw) override {
        dbgassert(trackPtr);
        daw->resetMouseContext();
        daw->resetEditClip();
        daw->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(localIdx == trackPtr->localIdxFlat);
        localIdx = trackPtr->localIdxFlat;
        trackPtr = nullptr;
        //UNSERIALIZE TRACK VSTs
    }
};

class action_modify_track_remove : public action_base {
public:
    int32_t trackIdx = -1;
    int32_t localIdx = -1;
    track_t* trackPtr;

    action_modify_track_remove() = delete;

    action_modify_track_remove(String description, track_t* _trackPtr) : action_base() {
        desc     = description;
        trackPtr = _trackPtr;
        trackIdx = _trackPtr->projectIdx;
        localIdx = _trackPtr->localIdxFlat;
        dbgassert(DawInstance::get()->getTrackId(trackIdx) != trackPtr);
    }

    ~action_modify_track_remove() override = default;

    void releaseResources(DawInstance* daw) override {
        if (trackPtr) {
            releaseTrackResources(trackPtr, daw);
            delete trackPtr;
            trackPtr = nullptr;
        }
    }

    void undo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetEditClip();
        daw->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(localIdx == trackPtr->localIdxFlat);
        localIdx = trackPtr->localIdxFlat;
        trackPtr = nullptr;
        //UNSERIALIZE TRACK VSTs
    }

    void redo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetEditClip();
        trackPtr = daw->getTrackId(trackIdx);
        dbgassert(trackPtr);
        //SERIALIZE TRACK VSTs
        daw->removeTrackImpl(trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(trackPtr && trackPtr->audio && trackPtr->audio->sampleFormat.blockSize % 8 == 0);// see if pointer is valid
        dbgassert(localIdx == trackPtr->localIdxFlat);
    }
};

void DawInstance::addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int flags) {
    project.trackList.addTrack(trackInsertPos, newTrack);
    if ((flags & FLG_TRK_CHANGE_HISTORY_UNDO) != 0) {
        dbgassert(newTrack->audio);
    } else {
        dbgassert(!newTrack->audio);
        host->createAudio(newTrack);
    }
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->addTrackToView(newTrack, flags);
        }
    }
    if (flags & FLG_TRK_CHANGE_USER) {
        pushHist(new action_modify_track_add(StringFormat("Add %s Track", TrackTypeToName(newTrack->type)), newTrack));
    }

    host->onTrackLayoutChange();
}

void DawInstance::removeTrackId(uint32_t trackId) {
    if (project.trackList.validTrackIdx(trackId)) {
        removeTrackImpl(project.trackList[trackId], FLG_TRK_CHANGE_USER);
    }
}

void DawInstance::removeTrackImpl(track_t* track, int flags) {
    guictr_plugins* plugins = MainCtrl::getPluginCtr();
    plugins->hideTrack(track->audio);
    // TODO: handle plugins correctly, right now they remain loaded in vsthost
    if (mainCtrl->clipView.gui && mainCtrl->clipView.gui->m_track == track) {
        mainCtrl->clipView.set(nullptr);
    }
    project.trackList.removeTrack(track);
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->removeTrackFromView(track, flags);
        }
    }
    DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.stageId);
    DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.inputStageId);
    DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.outputStageId);
    DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.outputPostStageId);
    if (flags & FLG_TRK_CHANGE_USER) {
        pushHist(new action_modify_track_remove(StringFormat("Remove %s Track", TrackTypeToName(track->type)), track));
    }
    host->onTrackLayoutChange();
}

track_t* DawInstance::getTrackId(uint32_t trackId) {
    return project.trackList[trackId];// operator[] returns nullptr on oob
}

void DawInstance::preClipDelete(clip_t* clip) {
    for (auto* ctrl : this->dawCtrls) {
        if (ctrl->clipView.clip() == clip) {
            ctrl->clipView.set(nullptr);
        }
        ctrl->onGuiRemoved(clip);
    }
    //resetMouseContext();
}

void DawInstance::preTrackDelete(track_t* track) {
    for (auto* ctrl : this->dawCtrls) {
        if (ctrl->clipView.gui && ctrl->clipView.gui->m_track == track) {
            ctrl->clipView.set(nullptr);
        }
    }
    resetMouseContext();
}

void MainCtrl::showAutomation(track_t* tr, automatable_t* at, int32_t paramIdx) {
    track_gui_entry_t* entry;
    if (view->ctr_tracks.getTrackEntry(tr, &entry)) {
        view->ctr_tracks.showAutomationLane(entry, at, paramIdx);
    }
}
void DawInstance::setTempo(int32_t _tempo100) {
    playThread.call([this, _tempo100]() {
        projectGlobals.tempo100 = CLAMP_I(_tempo100, 100, 99900);
    },
                    true);
}

void MainCtrl::destroy() {
    DAW::settings.wndMain.dens = grid.grid_dens;
    daw.destroy();
    view = nullptr;
    DawCtrl::destroy();
}

void CompanionCtrl::destroy() {
    DAW::settings.wndCompanion.dens = grid.grid_dens;
    view->ctr_tracks2.removeAllTracks();
    view = nullptr;
    DawCtrl::destroy();
}

void CompanionCtrl::onPluginsChanged() {
    view->ctr_nodes.reset();
    view->ctr_nodes.refresh();
}

void CompanionCtrl::updateVisibleTrackContents() {
    view->ctr_tracks2.updateVisibleTrackContents();
}

void CompanionCtrl::updateGrid() {
    grid.update(view->ctr_tracks2.trackView.getSizeContent());
    view->ctr_tracks2.updateVisibleTrackContents();
}

void MainCtrl::addTrackToView(track_t* track, int flags) {
    view->ctr_tracks.addTrack(track, flags);
}

void MainCtrl::removeTrackFromView(track_t* track, int flags) {
    view->ctr_tracks.removeTrack(track, flags);
}

void CompanionCtrl::addTrackToView(track_t* track, int flags) {
    view->ctr_tracks2.addTrack(track, flags);
}

void CompanionCtrl::removeTrackFromView(track_t* track, int flags) {
    view->ctr_tracks2.removeTrack(track, flags);
}

void CompanionCtrl::resetView() {
    view->ctr_tracks2.resetView();
}

void MainCtrl::resetView() {
    view->ctr_tracks.resetView();
}

void CompanionCtrl::layoutView() {
    view->ctr_tracks2.relayout();
}

void MainCtrl::layoutView() {
    view->ctr_tracks.relayout();
}

bool CompanionCtrl::isZooming() {
    return guiCaptured == &view->ctr_tracks2.trackTimeline;
}

void CompanionCtrl::fixCursor() {
    auto& cursor = getCursor();
    auto& guiMgr = view->ctr_tracks2.guiMgr;
    if (cursor.isSubtrackSelection() && guiMgr.validTrackIdx(cursor.cursorTrack)) {
        const track_gui_entry_t* tr = guiMgr.at(cursor.cursorTrack);
        fixCursorSubRange(cursor, tr->subtracks.size());
    } else {
        fixCursorTrackRange(cursor, guiMgr.getTracksVisibleFlat().size());
    }
}

void MainCtrl::fixCursor() {
    auto& cursor = getCursor();
    auto& guiMgr = view->ctr_tracks.guiMgr;
    if (cursor.isSubtrackSelection() && guiMgr.validTrackIdx(cursor.cursorTrack)) {
        const track_gui_entry_t* tr = guiMgr.at(cursor.cursorTrack);
        fixCursorSubRange(cursor, tr->subtracks.size());
    } else {
        fixCursorTrackRange(cursor, guiMgr.getTracksVisibleFlat().size());
    }
}

void MainCtrl::setStatusText(String s) {
    view->statusbar.setTitle(s);
}

void DawInstance::resetMouseContext() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->resetMouseContext();
    }
}

void DawInstance::closeContextMenus() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->closeContextMenu();
    }
}

void DawInstance::closeDialogs() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->closeDialogs();
    }
}

void DawInstance::resetAutomationContext() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->resetAutomationContext();
    }
}

void DawInstance::resetEditClip() {
    setEditClip(nullptr);
}

void DawInstance::setEditClip(gui_clip* gclip) {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->setEditClip(gclip);
    }
}

void DawCtrl::setEditClip(gui_clip* gclip) {
    clipView.set(gclip);
}

void MainCtrl::setEditClip(gui_clip* gclip) {
    view->ctr_clipeditor.storeLayout();
    clipView.set(gclip);
    view->ctr_clipeditor.showEditClip();
}

void CompanionCtrl::setEditClip(gui_clip* gclip) {
    view->ctr_clipeditor.storeLayout();
    clipView.set(gclip);
    view->ctr_clipeditor.showEditClip();
}

void DawCtrl::prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {

    daw_tls::tlsinstance& tlsInstance = daw_tls::getTls();

    auto& renderStats               = tlsInstance.renderStats;
    renderStats.playThreadLockCount = 0;
    renderStats.clipsRendered       = 0;
    renderStats.notesRendered       = 0;
    //log_printf("prerender %d\n", seqthreads::getCurrentThreadId());

    hires_timer_t timer;
    for (guictr_base* ctr : containers) {
        ctr->prerender(nanovgCtxt);
    }
    renderStats.timePrerender = timer.getTime();

    auto tmNow = getTimeMillis();
    //if (tmNow - tmLastRenderUpdatesMs >= 1000)
    //if (tmLastRenderUpdatesMs++%2==0)
    {
        timer.reset();

        int nUpdates = tlsInstance.waveform->renderUpdates(nanovgCtxt, 0);
        if (nUpdates) {
            //tmLastRenderUpdatesMs = tmNow;
        }
        renderStats.numWaveFormsRendered += nUpdates;
        renderStats.timeUpdateWaveforms = timer.getTime();
        if (nUpdates > 15 || renderStats.timeUpdateWaveforms > 20 * 1000) {
            log_printf("%d updates took %zd\n", nUpdates, renderStats.timeUpdateWaveforms);
            auto timings = tlsInstance.waveform->getTimings();
            log_printf("waveform.tmPassed\t\t%zd\n", timings.tmPassed);
            log_printf("waveform.tmProcessInputQ\t%zd\n", timings.tmProcessInputQ);
            log_printf("waveform.tmFindSimiliar\t%zd\n", timings.tmFindSimiliar);
            log_printf("waveform.tmFindSpot\t\t%zd\n", timings.tmFindSpot);
            log_printf("waveform.tmTesselate\t\t%zd\n", timings.tmTesselate);
            log_printf("waveform.tmBakePaths\t\t%zd\n", timings.tmBakePaths);
            log_printf("waveform.tmDrawGL\t\t%zd\n", timings.tmDrawGL);
            log_printf("waveform.comparisonsA\t%zd\n", timings.comparisonsA);
            log_printf("waveform.comparisonsB\t%zd\n", timings.comparisonsB);
        }
    }
}

track_t* clip_view::track() const {
    if (!this->gui)
        return nullptr;
    return this->gui->m_track;
}

clip_t* clip_view::clip() const {
    if (!this->gui)
        return nullptr;
    return this->gui->m_clip;
}

GLFWwindow* getGlfwFromWindowBase(window_base* w);

GLFWwindow* getTopLevelGlfwWindow() {
    auto main = MainCtrl::get();
    if (main) {
        return getGlfwFromWindowBase(main->window);
    }
    dbgassert(0);
    return nullptr;
}

int handleFatalError(int type, int implSpecType) {
    /*seqthreads::thread_base* thread = MainCtrl::getPlayThread();
    if (thread && seqthreads::getCurrentThreadId() == thread->getThreadId()) {
        host_processing_stats_t processing;
        auto host = vsthost::getInstance();
        host->getProcessingStats(processing);
        if (processing.pluginId) {
            effectbase* eff = host->getPluginById(processing.pluginId);
            if (eff) {
                log_printf("Crash was most likely caused by %s\n", StringAsCStr(eff->getName()));
            }
        }
    }*/
    return 0;
}

int32_t project_controller_t::tickToSamples(tick_t ticks) {
    vsthost* host = vsthost::getInstance();
    dbgassert(host);
    return tickToSampleConvert<int32_t, roundmode::round>(ticks, projectGlobals->tempo100, host->m_sampleFormatInternal.sampleRate);
}

tick_t project_controller_t::samplesToTicks(int32_t sample) {
    vsthost* host = vsthost::getInstance();
    dbgassert(host);
    return sampleToTickConvert<tick_t, roundmode::round>(sample, projectGlobals->tempo100, host->m_sampleFormatInternal.sampleRate);
}

beatbar16th_t project_controller_t::toBeatBar16th(int32_t tick) {
    return tickToBarBeat16th(tick, projectGlobals->signatureNum, projectGlobals->signatureDenom);
}
