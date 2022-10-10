#include "assert_dbg.h"
#include "event.h"
#include "fileio.h"
#include "glheaders.h"
#include <cstddef>
#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <ctime>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>
#include <memory>

#include "gui/container/container_layout_types.h"
#include "gui/tooltip/tooltip.h"
#include "guicolors.h"
#include "host/mainctrl.h"
#include "mainctrl.h"
#include "math/seq_math.h"
#include "error.h"
#include "basectrl.h"
#include "saferef.h"
#include "util/profiling.h"
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
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/splitter.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/views/controls.h"
#include "gui/controls/scrollbar.h"
#include "gui/controls/statusbar.h"
#include "gui/plugin/pluginctr.h"
#include "gui/clipeditor/clipeditor.h"
#include "gui/track/trackctr.h"
#include "gui/track/trackctr_nodes.h"
#include "gui/track/trackcontent.h"
#include "gui/controls/list.h"
#include "gui/views/pluginlist.h"
#include "gui/menu/menu.h"
#include "gui/views/debugctr.h"
#include "gui/views/notify.h"
#include "wave/waveform_render_impl.h"
#include "gui/views/shaderview.h"
#include "gui/dialog/about.h"
#include "gui/dialog/dialog_io.h"
#include "gui/dialog/dialogs.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "track_impl.h"
#include "audiocache.h"
#include "seq_time.h"
#include "track_graph.h"
#include "effect_graph.h"

#include "gui/plugin/plugin.h"
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"
#include "plugindatabase.h"
#include "window_impl.h"

#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "host/audio_host.h"
#include "host/midi_host.h"
#include "appconfig.h"
#include "sse.h"
#ifdef _WIN32
#include "platform/win/windowsize.h"
#endif
#ifdef __linux__
#include "platform/linux/windowsize.h"
#endif

const int FLAG_DEFER_LOAD               = 0x1;
const int FLAG_INVOKE_USER_CB_DEFERLOAD = 0x2;

int32_t getNumClipAllocations();
void printLeakedAudioBuffers();
void printClipAllocations();

extern "C" {
void resetShaderTimeOffset(void);
}

void dragdrop_midifile::reset() {
    auto dragTarget = safeRefGet(this->target);
    if (dragTarget) {
        dragTarget->clipDropCancel();
    }
    target = {};
    isValidTarget = false;
    isLoaded      = false;
    clipboard.reset();
}

guictr_base* makeCtrProperties();//guiproperties.cpp
guictr_base* makeCtrTheme();     //guiproperties.cpp
guictr_base* makeCtrHistory();   //guihistory.cpp

class MainCtrlErrorStatusBarLogger : public Logger {
    gui_statusbar* const statusbar;
public:
    explicit MainCtrlErrorStatusBarLogger(gui_statusbar* _statusbar) noexcept 
        : statusbar(_statusbar)
    {

    }
    void log(Log::Level lvl, const char* data, size_t len) override {
        if (Log::LEVEL_ALL != getLevel() && lvl < getLevel())
            return;
        auto color = GuiColor::COL_LABEL_ACTIVE;
        if (lvl >= Log::L_WARN)
            color = GuiColor::COL_INVALID_INPUT;
        statusbar->setTitle(data, color);
    }
    void logStr(Log::Level lvl, String s) override {
    }
};

class guictr_effectlibrary : public guictr_base {
public:
    guictr_pluginlibrary ctr_pluginlist;
    guictr_modulelibrary ctr_effectlist;
    bool initialized = false;
    int revision     = -1;
    guictr_effectlibrary() : guictr_base() {
        setGuiType(gui_type::CTR_TYPE_EFFECTLIBRARY);
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
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
template<typename T>
void addLayoutEntryRelayout(BaseCtrl* ctrl, T& t, const std::shared_ptr<guictr_base>& ctr, String title) {
    ctr->setLabel(std::move(title));
    std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(ctr);
    i_ctr_drop_area area(t.get());
    switch (t->getLayout()) {
            break;
        case container_layout::SOLE:
        case container_layout::SPLIT_H:
            area.dockPos = dock_pos::RIGHT;
            break;
        case container_layout::SPLIT_V:
            area.dockPos = dock_pos::BOTTOM;
            break;
        case container_layout::TABBED:
            area.dockPos = dock_pos::STACK;
            break;
        default:
            break;
    }
    ctrl->dropContainer(entry1, &area);
    ctrl->dragContainerRelayout(BaseCtrl::drag_ctr_event{BaseCtrl::drag_ctr_event_type::DRAG_END});
}

std::shared_ptr<guictr_layout> makeTabListCtr1(DawCtrl* const dawCtrl) {
    auto ctr = std::make_shared<guictr_layout>();

    auto ctr_dbg0       = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_0);
    auto ctr_dbg1       = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::DEBUG_APPCTRL);
    auto ctr_dbg2       = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2);
    auto ctr_properties = std::shared_ptr<guictr_base>(makeCtrProperties());
    auto ctr_theme      = std::shared_ptr<guictr_base>(makeCtrTheme());
    auto ctr_history    = std::shared_ptr<guictr_base>(makeCtrHistory());
    auto shaderView     = std::make_shared<gui_shaderview>();
    auto settings       = std::make_shared<DAW::DialogSettings::guidialog_settings>(dawCtrl->getDaw());
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

std::shared_ptr<guictr_layout> makeTabListCtr2(DawCtrl* const dawCtrl) {
    auto ctr = std::make_shared<guictr_layout>();

    auto ctr_effectlib     = std::make_shared<guictr_effectlibrary>();
    auto ctr_properties    = std::shared_ptr<guictr_base>(makeCtrProperties());
    auto ctr_loadedplugins = std::shared_ptr<guictr_base>(makeGuiPluginsLoadedList());
    auto ctr_performance   = std::shared_ptr<guictr_base>(makeGuiPerformance());
    auto settings          = std::make_shared<DAW::DialogSettings::guidialog_settings>(dawCtrl->getDaw());

    auto ctr_dbg0 = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_0);
    auto ctr_dbg1 = std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::DEBUG_APPCTRL);
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
    Splitter splitterCenter;
    DawViewContainersCompanion(DawCtrl* const _dawCtrl, ngui::MenuBar& menubar, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& grid, clip_view& clipView, dragdrop_midifile& dragdropclip)
        : dawCtrl(_dawCtrl),
          ctr_menu(menubar),
          ctr_nodes(_cursor, _project, dragdropclip),
          ctr_tracks2(_dawCtrl, _cursor, _trackSelection, _project, _projectGlobals, grid, dragdropclip),
          ctr_clipeditor(clipView),
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
        splitterCenter.pos  = ivec2(winX, winY + hTrackCtr - Splitter::SPLITTER_LAYOUT_THICKNESS/2);
        splitterCenter.size = ivec2(winW, Splitter::SPLITTER_LAYOUT_THICKNESS);
        ctr_clipeditor.size = ctr_nodes.size = centerCtr.size = { winW, hTrackCtr };
        ctr_clipeditor.pos  = ctr_nodes.pos  = centerCtr.pos  = { winX, winY };
    }
    void addTo(std::vector<guictr_base*>& v) override {
        ctr_clipeditor.setControl(dawCtrl);
        ctr_tracks2.setControl(dawCtrl);
        ctr_nodes.setControl(dawCtrl);

        v.push_back(&splitterCenter);
        dbgassert(CtrSize(v) == indexContent);

        v.push_back(&ctr_tracks2);

#if USE_GUI_MENU
        v.push_back(&ctr_menu);
#endif
    }
    void updateVisibility() {
        this->ctr_tracks2.setVisible(dawCtrl->containers[indexContent] == &this->ctr_tracks2);
        this->ctr_nodes.setVisible(dawCtrl->containers[indexContent] == &this->ctr_nodes);
        this->ctr_clipeditor.setVisible(dawCtrl->containers[indexContent] == &this->ctr_clipeditor);
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
          ctr_clipeditorview(clipView, ctr_clipeditor.noteeditor),
          ctr_tracks(_mainCtrl, _cursor, _trackSelection, _project, _projectGlobals, grid, dragdropclip),
          ctr_nodes(_cursor, _project, dragdropclip),
          ctr_Right() {
        indexContent        = 3;
        auto subctr_tabbed  = makeTabListCtr1(_mainCtrl);
        auto subctr_tabbed2 = makeTabListCtr2(_mainCtrl);
        splitters.push_back(std::make_shared<Splitter>(1, 0.02f));//left
        splitters.push_back(std::make_shared<Splitter>(0, 0.5f)); //center
        splitters.push_back(std::make_shared<Splitter>(1, 0.8f)); //right
        subctr_tabbed2->setLabel("Top");
        subctr_tabbed->setLabel("Bottom");
        std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(subctr_tabbed2);
        std::shared_ptr<guictr_layout_entry> entry2 = createGuiCtrLayoutEntry(subctr_tabbed);
        ctr_Left  = std::make_shared<guictr_layout>();
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
        statusbar.setSnapSides(ivec4(0, 1, 0, 1));
        ctr_clipeditorview.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_pluginview.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_clipeditor.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_plugins.setSnapSides(ivec4(0, 1, 0, 0));
        subctr_tabbed2->setSnapSides(ivec4(1, 0, 0, 1));
        ctr_Left->setSnapSides(ivec4(0, 0, 1, 1));
        ctr_Right->setSnapSides(ivec4(1, 0, 0, 1));

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
        int hTopControls     = 48;
        int heightViewSelect = 60;
        int heightStatusBar = 16;
        int hCenter      = winH - hTopControls - heightViewSelect - heightStatusBar;
        int hContent     = winH - hTopControls - heightStatusBar;
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
        // ctr_pluginview.size     = { widthCenter/2, heightViewSelect };
        ctr_clipeditorview.size = { widthCenter/2, heightViewSelect };

        statusbar.size = { winW, heightStatusBar };

        ctr_tempo.pos          = { winX, winY };
        ctr_tracks.pos         = { widthLeft, winY + hTopControls };
        ctr_nodes.pos          = { widthLeft, winY + hTopControls };
        statusbar.pos          = { winX, winBottom - heightStatusBar };
        ctr_clipeditorview.pos = { widthLeft, winBottom - heightViewSelect - heightStatusBar };
        ctr_pluginview.pos     = { ctr_clipeditorview.right(), winBottom - heightViewSelect - heightStatusBar };
        ctr_plugins.pos        = { widthLeft, winBottom - heightViewSelect - hEditor - heightStatusBar };
        ctr_pluginview.size     = { ctr_plugins.right()-ctr_pluginview.left(), heightViewSelect };
        ctr_clipeditor.pos     = { widthLeft, winBottom - heightViewSelect - hEditor - heightStatusBar };
        ctr_Left->pos          = { winX, winY + hTopControls };
        ctr_Left->size         = { widthLeft, hContent };

        getSplitter(SplitterPos::LEFT)->pos    = ivec2(widthLeft - Splitter::SPLITTER_LAYOUT_THICKNESS/2, hTopControls);
        getSplitter(SplitterPos::LEFT)->size   = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, hContent);
        getSplitter(SplitterPos::CENTER)->pos  = ivec2(widthLeft, ctr_clipeditor.pos.y - Splitter::SPLITTER_LAYOUT_THICKNESS/2);
        getSplitter(SplitterPos::CENTER)->size = ivec2(widthCenter, Splitter::SPLITTER_LAYOUT_THICKNESS);

        ctr_Right->pos  = { widthLeft + widthCenter, winY + hTopControls };
        ctr_Right->size = { widthRight, hContent };

        getSplitter(SplitterPos::RIGHT)->pos  = ivec2(ctr_Right->pos.x - Splitter::SPLITTER_LAYOUT_THICKNESS/2, hTopControls);
        getSplitter(SplitterPos::RIGHT)->size = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, hContent);

        ctr_Right->postContentChanged();
        ctr_Left->postContentChanged();
    }

    void addTo(std::vector<guictr_base*>& v) override {
        this->ctr_plugins.setControl(mainCtrl);
        this->ctr_clipeditor.setControl(mainCtrl);
        this->ctr_nodes.setControl(mainCtrl);
        for (auto& s : splitters)
            v.push_back(s.get());
        dbgassert(CtrSize(v) == indexContent);
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

    void loadLayout(const dawview_layout_t& viewLayout) {
        ctr_Right->removeAllEntries();
        ctr_Left->removeAllEntries();
        dbgassert(ctr_Left->dawCtrl);
        DawInstance* const daw = ctr_Left->dawCtrl->getDaw();
        dbgassert(daw);
        if (viewLayout.left && viewLayout.right) {
            auto& fac = getContainerFactory();
            auto context = ContainerInstanceContext{daw};
            loadContainerSnapshot(fac, context, ctr_Right.get(), viewLayout.right.get());
            loadContainerSnapshot(fac, context, ctr_Left.get(), viewLayout.left.get());
        }
        if (viewLayout.splitterPositions.size() == splitters.size()) {
            for (size_t i = 0; i < splitters.size(); i++) {
                splitters[i]->setScale(viewLayout.splitterPositions[i]);
            }
        }
    }

    void storeLayout(dawview_layout_t& layout) {
        layout.left  = std::make_shared<guictrlayout_snapshot_t>();
        layout.right = std::make_shared<guictrlayout_snapshot_t>();
        storeContainerSnapshot(ctr_Right.get(), layout.right.get());
        storeContainerSnapshot(ctr_Left.get(), layout.left.get());
        layout.splitterPositions.resize(splitters.size());
        for (size_t i = 0; i < splitters.size(); i++) {
            layout.splitterPositions[i] = splitters[i]->getScale();
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
void DawCtrl::onPluginSelected() {
    getNodesContainer()->onPluginSelected();
}

void MainCtrl::showPluginView() {
    containers[view->indexContent + 1] = &view->ctr_plugins;
}

void MainCtrl::showClipEditor() {
    containers[view->indexContent + 1] = &view->ctr_clipeditor;
}

void CompanionCtrl::showPluginView() {
}

void CompanionCtrl::showClipEditor() {
    setViewMode(view_mode_t::NODE_EDITOR);
}

bool MainCtrl::isClipEditorVisible() {
    return containers[view->indexContent + 1] == &view->ctr_clipeditor;
}

bool MainCtrl::isPluginViewVisible() {
    return containers[view->indexContent + 1] == &view->ctr_plugins;
}

bool CompanionCtrl::isClipEditorVisible() {
    return this->viewMode == view_mode_t::NODE_EDITOR;
}

bool CompanionCtrl::isPluginViewVisible() {
    return false;
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
    dbgassert(!playThread.isRunning() || playThread.isLocked());
    for (auto* ctrl : dawCtrls) {
        ctrl->closeContextMenu();
        ctrl->resetMouseContext();
    }
    projectPath = "";
    setSelectedTrack(nullptr);
    for (auto* dawctrl : dawCtrls) {
        dawctrl->clipView.set(nullptr);
    }
    setEmptyClipboard();

    projectGlobals.cursor.setEmptySelection();

    hist.clear(this);

    std::vector<track_t*> _tracks     = project.trackList.getAllTracksFlatVec();// iterate a copy
    std::vector<track_t*> _rootTracks = project.trackList.getAllTracksTreeVec();
    log_lf(Log::L_DEBUG, "Unloading project with %zu tracks\n", _tracks.size());
    for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
        track_t* track = *it;
        removeTrackImpl(track, FLG_TRK_CHANGE_LOAD);
    }
    project.trackList.clear();
    for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
        track_t* track = *it;
        releaseTrackResources(track, this);
        delete track;
    }

    tls.host->unload();
    tls.audioCache->unloadAll();
    auto* ctrl = tls.mainCtrl;
    if (ctrl) {
        auto& trackView = ctrl->view->ctr_tracks.trackView;
        trackView.m_resizePreModifyState.reset();
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
        tls.host->getDeferredEffects(pluginsDeferred);
        dbgassert(pluginsDeferred.empty());
    }
    AppWndProc_disableBlockReentrant();
}

void DawCtrl::updateMenubar() {
    menubar.disableAll = this->ctxtmenu != nullptr;

    ngui::Menu* undo = menus.edit.getByCmd(CMD_UNDO);
    auto cmdUndo     = commands->getCommand(CMD_UNDO);
    dbgassert(undo && cmdUndo);
    if (undo && cmdUndo) {
        undo->disabled    = !daw.hist.canUndo();
        String customText = cmdUndo->desc.name;
        if (!undo->disabled) {
            customText += " " + daw.hist.getUndoStep();
        }
        undo->setTitle(customText);
    }

    ngui::Menu* redo = menus.edit.getByCmd(CMD_REDO);
    auto cmdRedo     = commands->getCommand(CMD_REDO);
    dbgassert(redo && cmdRedo);
    if (redo && cmdRedo) {
        redo->disabled    = !daw.hist.canRedo();
        String customText = cmdRedo->desc.name;
        if (!redo->disabled) {
            customText += " " + daw.hist.getRedoStep();
        }
        redo->setTitle(customText);
    }

    menus.recent.clear();
    auto& settings = daw_tls::getSettings();
    for (auto& strFileRecentPath : settings.recentfiles.sortedEntries) {
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
        bool bSuccess = saveProject(f, path);
        if (tls.mainCtrl) {
            if (bSuccess) {
                tls.mainCtrl->setStatusText(StringFormat("Saved project to %s", StringAsCStr(path)));
            } else {
                tls.mainCtrl->setStatusText(StringFormat("Failed to save project to %s", StringAsCStr(path)), GuiColor::COL_INVALID_INPUT);
            }
        }
        projectPath = path;
    }
}

void DawInstance::loadFile(String path, int flags) {
    timer.reset();
    std::shared_ptr<project_file> f = loadProjectFile(path);
    if (!f) {
        if (tls.mainCtrl) {
            tls.mainCtrl->setStatusText(StringFormat("Failed loading %s", StringAsCStr(FileNameFromPath(path))));
        }
    } else {
        SplitPath(path, &lastProjectDirectory, nullptr, nullptr, nullptr);
        tls.settings->recentfiles.add(path);
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
        if (!tls.mainCtrl || (flags & FLAG_INVOKE_USER_CB_DEFERLOAD) == 0) {
            cb(flags & FLAG_DEFER_LOAD);
        } else {
            guidialog_cb_yes_no* dlg = new guidialog_cb_yes_no();
            dlg->cb = cb;
            dlg->message = "Load plugins?";
            tls.mainCtrl->openDialog(dlg);
        }
    }
}

void DawInstance::setEmptyProject() {
    ThreadLock lock = playThread.lockThread();
    unloadProject();
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_lf(Log::L_WARN, "getNumClipAllocations == %d!\n", totalAllocs);
        // dbgassert(getNumClipAllocations() == 0);
    }
    insertNewTrack(-1, TRACK_TYPE_MIDI, FLG_TRK_CHANGE_LOAD);
    insertNewTrack(-1, TRACK_TYPE_MASTER, 0);
    resetShaderTimeOffset();
    tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::BUILD_BINARY_NAME, "New Project"));
}

void DawInstance::onDawCompanionWindowClose(DawWindowCompanion& entry) {
    auto it = std::find_if(dawCtrls.begin(), dawCtrls.end(), [pDawCtrlClosing = entry.ctrl.get()](auto* pDawCtrl) {
        return pDawCtrl == pDawCtrlClosing;
    });
    if (it != dawCtrls.end()) {
        dawCtrls.erase(it);
    }
    entry.wnd->setInvalid();
}

void DawInstance::setSoloState(audio_stage_ref_t ref, bool enableSolo) {
    track_t* track = getTracks().resolveTrack(ref);
    dbgassert(track);
    dbgassert(track->audio);
    track->audio->flags ^= audiostageflags_t::SOLO;
    DAW::updateSoloFlag(tls.host, &project, getTracks().getAllTracksFlatVecRef());
}
void DawInstance::setTrackArmed(audio_stage_ref_t ref, bool enabledArmed) {
    track_t* track = getTracks().resolveTrack(ref);
    dbgassert(track);
    dbgassert(track->audio);
    track->audio->flags ^= audiostageflags_t::RECORD_ARMED;
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

std::shared_ptr<window_abstract_t> getWindowDebugWaveformCache();
std::shared_ptr<window_abstract_t> getWindowPerf();
std::shared_ptr<window_abstract_t> getWindowDebugNanoVG();

bool DawInstance::menuCommand(const menucmd_t& command) {
    try {
        auto mainCtrl = tls.mainCtrl;
        switch (command.command) {
            case CMD_EXPORT_TRACK: {
                auto selTrack = getSelectedTrack();
                if (!selTrack) {
                    return true;
                }
                track_snapshot_t snapshot(selTrack, tracksnapshot_store_opts_t::All());
                trackcontainer_snapshot_t trackContainerSnapshot;
                trackContainerSnapshot.tracks.push_back(snapshot);
                String path;
                auto exportDir = getProjectDirectory();
                auto exportFilename = selTrack->name + "." + vFILE_TYPES_TRACKSNAPSHOT[0].ext;
                if (promptUserFilePath(mainCtrl->window, 1, vFILE_TYPES_TRACKSNAPSHOT, path, exportDir, exportFilename)) {
                    String ext;
                    SplitPath(path, nullptr, nullptr, &ext);
                    if (ext.empty()) {
                        path += "." + vFILE_TYPES_TRACKSNAPSHOT[0].ext;
                    }
                    saveTrackContainer(trackContainerSnapshot, path);
                }
                return true;
            }
            case CMD_IMPORT_TRACK: {
                String path;
                auto importDir = getProjectDirectory();
                if (promptUserFilePath(mainCtrl->window, 0, vFILE_TYPES_TRACKSNAPSHOT, path, importDir)) {
                    std::shared_ptr<trackcontainer_snapshot_t> ctr = loadTrackContainer(path);
                    dbgassert(ctr);
                    if (ctr) {
                        auto* pluginMgr = getPluginManager();
                        ThreadLock lock = getPlayThread()->lockThread();
                        for (track_snapshot_t& ts : ctr->tracks) {
                            ts.trackLoaded = new track_t(ts);
                            addTrackImpl(-1, ts.trackLoaded, 0);
                        }

                        //load plugins
                        for (track_snapshot_t& ts : ctr->tracks) {
                            log_printf("track '%s' loading %zu plugins\n", StringAsCStr(ts.trackLoaded->name), ts.data.pluginSnapshots.size());
                            DAW::assignFreeStageIdsTrackSnapshot(pluginMgr, ts);
                            ts.trackLoaded->loadSnapshot(ts);
                            std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
                            for (auto effect: effects) {
                                pluginMgr->activateDeferred(effect, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                            }
                        }
                        for (track_snapshot_t& ts: ctr->tracks) {
                            ts.trackLoaded->getStage()->pluginsChanged();
                        }
                        onPluginsChanged();
                        updateVisibleTrackContents();
                    }
                }
                return true;
            }
            case CMD_REACTIVATE_AUTOMATION: {
                ThreadLock lock = playThread.lockThread();
                std::vector<automatable_t*> targets;
                for (auto& track : project.trackList.getAllTracksFlatVecRef()) {
                    targets.clear();
                    track->audio->getAutomatableTrackTargets(targets);
                    for (auto& target : targets) {
                        target->visitAutomatedParams([](auto& param) {
                            param.src.activate();
                        });
                    }
                }
                return true;
            }
            case CMD_CREATE_VIEW:
                if (getMainControl()) {
                    dbgassert(command.argInt >= 0);
                    std::shared_ptr<guictr_base> ctr;
                    auto context = ContainerInstanceContext{this};
                    if (makeContainer(context, static_cast<gui_type>(command.argInt), ctr)) {
                        auto ctrLayoutLeft = getMainControl()->view->ctr_Left;
                        addLayoutEntryRelayout(getMainControl(), ctrLayoutLeft, ctr, ctr->label);
                    }
                }
                return true;
            case CMD_OPEN_SECOND_WINDOW:
                if (companionWindows.empty()) {
                    auto companionCtrlStdPtr = std::make_shared<CompanionCtrl>(mainCtrl, *this);
                    ivec2 windowSize;
                    mainCtrl->mainWindow->getSize(&windowSize);
                    auto compWindowNew = mainCtrl->mainWindow->createOverlay(companionCtrlStdPtr, windowSize, WINDOW_IS_MAINWINDOW_SLAVE | WINDOW_IS_RESIZABLE);
                    auto idxOfWindow = companionWindows.size();
                    companionWindows.push_back(DawWindowCompanion{ compWindowNew, companionCtrlStdPtr });
                    compWindowNew->initControl();
                    if (companionCtrlStdPtr->isOk()) {
                        companionWindows[0].wnd->show();
                        if (this->layoutsFromProjectFile.size() > idxOfWindow) {
                            companionCtrlStdPtr->loadLayout(this->layoutsFromProjectFile[idxOfWindow]);
                        }
                        for (track_t* tr : project.trackList) {
                            companionCtrlStdPtr->view->ctr_tracks2.addTrack(tr, FLG_TRK_CHANGE_LOAD);
                        }
                        companionCtrlStdPtr->fixCursor();
                        companionCtrlStdPtr->updateVisibleTrackContents();
                    }
                    if (companionCtrlStdPtr->isOk()) {
                        this->dawCtrls.push_back(companionCtrlStdPtr.get());
                    }
                } else if (companionWindows.size() && companionWindows[0].ctrl && companionWindows[0].ctrl->isOk()) {
                    companionWindows[0].wnd->show();
                }
                return true;
            case CMD_UNDO:
                if (hist.canUndo()) {
                    ThreadLock lock = playThread.lockThread();
                    hist.undoStep(this);
                    updateVisibleTrackContents();
                }
                return true;
            case CMD_REDO:
                if (hist.canRedo()) {
                    ThreadLock lock = playThread.lockThread();
                    hist.redoStep(this);
                    updateVisibleTrackContents();
                }
                return true;
            case CMD_FILE_NEW: {
                stopPlaying();
                setAudioThreadState(playback_state::status_no_process);
                setEmptyProject();
                layoutTrackEditors();
                updateVisibleTrackContents();
                setAudioThreadState(playback_state::status_stop);
                return true;
            }
            case CMD_FILE_OPEN: {
                if (command.arg1.empty()) {
                    String path;
                    if (promptUserFilePath(mainCtrl->window, 0, vFILE_TYPE_PROJECT, path, lastProjectDirectory)) {
                        loadFile(path, FLAG_INVOKE_USER_CB_DEFERLOAD);
                    }
                } else {
                    loadFile(command.arg1, FLAG_INVOKE_USER_CB_DEFERLOAD);
                }
                return true;
            }
            case CMD_SET_STARTUP_PROJECT:
            case CMD_FILE_SAVEAS:
            case CMD_FILE_SAVE: {
                if (command.command == CMD_SET_STARTUP_PROJECT && !projectPath.empty()) {
                    tls.settings->dawsettings.startupProjectPath = projectPath;
                    saveSettings(*tls.settings);
                    return true;
                }
                String path = projectPath;
                if (command.command == CMD_FILE_SAVEAS || path.empty()) {
                    if (!promptUserFilePath(mainCtrl->window, 1, vFILE_TYPE_PROJECT, path, lastProjectDirectory)) {
                        return true;
                    }
                    String ext;
                    SplitPath(path, nullptr, nullptr, &ext);
                    if (ext.empty()) {
                        path += "." PROJECT_FILE_EXT;
                    }
                }
                saveFile(path);
                String projectFileName;
                SplitPath(path, &lastProjectDirectory, &projectFileName, nullptr, nullptr);
                tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::BUILD_BINARY_NAME, StringAsCStr(projectFileName)));
                tls.settings->recentfiles.add(path);
                if (command.command == CMD_SET_STARTUP_PROJECT && !projectPath.empty()) {
                    tls.settings->dawsettings.startupProjectPath = projectPath;
                    saveSettings(*tls.settings);
                }
                return true;
            }
            case CMD_INSERT_AUDIO_TRACK:
            case CMD_INSERT_MIDI_TRACK:
            case CMD_INSERT_RETURN_TRACK:
            case CMD_INSERT_MASTER_TRACK: {
                
                int32_t trackType = (command.command - CMD_INSERT_MASTER_TRACK) % NUM_TRACK_TYPES;
                insertNewTrack(-1, trackType);
                return true;
            }
            case CMD_ABOUT:
                mainCtrl->openDialog(new guidialog_about());
                return true;
            case CMD_SHOW_DEBUG_WINDOW:
                if (command.argInt == 3) {

                    auto guidialog  = new guidialog_about();
                    auto popupCtrl = std::make_shared<PopupCtrl>(mainCtrl);
                    popupCtrl->setDawCtrl(mainCtrl);
                    popupCtrl->m_scale = mainCtrl->m_scale;
                    popupCtrl->m_size = math::maxvec2(ivec2(20, 20), guidialog->size);
                    *popupCtrl->getTheme() = *mainCtrl->getTheme();
                    const ivec2 windowSize = ivec2(vec2(popupCtrl->m_size) * popupCtrl->m_scale);
                    popupCtrl->setWindowName(guidialog->getLabel());
                    auto dialogWindow = mainCtrl->mainWindow->createOverlay(popupCtrl, windowSize, 0);
                    
                    dialogWindow->setSizeLimits(windowSize, windowSize);

                    companionWindows.push_back(DawWindowCompanion{ dialogWindow, popupCtrl });

                    if (popupCtrl->isOk()) {
                        ivec2 wndPos(0);
                        determineWindowPos(guidialog, mainCtrl->mainWindow, mainCtrl->m_scale, 0, ivec2(0), wndPos);
                        popupCtrl->open(guidialog, wndPos, true, true);
                    }
                    return true;
                }
#if CREATE_DEBUG_COMPANION_WINDOW
                if (command.argInt == 0) {
                    window_dialog* dialog = mainCtrl->mainWindow->createDialog("waveform atlas cache", 1280, 720, getWindowDebugWaveformCache());
                    dialog->show();
                    return true;
                }
                if (command.argInt == 1) {
                    window_dialog* dialog = mainCtrl->mainWindow->createDialog("nanovg debug", 1280, 720, getWindowDebugNanoVG());
                    dialog->show();
                    return true;
                }
                if (command.argInt == 2) {
                    window_dialog* dialog = mainCtrl->mainWindow->createDialog("performance graphs", 1280, 720, getWindowPerf());
                    dialog->show();
                    return true;
                }
#endif
                return true;
            case CMD_PREFERENCES:
                mainCtrl->openDialog(new DAW::DialogSettings::guidialog_settings(this));
                return true;
            case CMD_EXIT:
                mainCtrl->mainWindow->requestClose();
                return true;
        }
    } catch (std::exception& e) {
        handleStdException(e);
    }
    return false;
}

bool DawCtrl::menuCommand(const menucmd_t& command) {
    switch (command.command) {
        case CMD_GUI_GLOBAL_ZOOM_DECREASE:
            m_scale = math::max(0.05f, m_scale - 0.05f);
            BaseCtrl::relayout();
            return true;
        case CMD_GUI_GLOBAL_ZOOM_INCREASE:
            m_scale = math::min(10.0f - 0.05f, m_scale + 0.05f);
            BaseCtrl::relayout();
            return true;
    }
    return daw.menuCommand(command);
}

void MainCtrl::startApp() {
    statusbarLogger = std::make_shared<MainCtrlErrorStatusBarLogger>(&view->statusbar);
    statusbarLogger->setLevel(Log::L_WARN);
    getMultiLogger().addLogger(statusbarLogger.get());
    Profiling::profilingRegisterEntry<prof_stats_render_t>(this, "Main Render Stats");
    daw_tls::getTls().runtime->systeminfo = appsysteminfo{
        String((char*)glGetString(GL_RENDERER)),
        String((char*)glGetString(GL_VENDOR)),
        String((char*)glGetString(GL_VERSION))
    };

    BaseCtrl::relayout();
    updateVisibleTrackContents();

    //TODO: move this out of here
    if (!loadProject.empty()) {
        daw.loadFile(loadProject, loadFlags);
    } else {
        daw.setEmptyProject();
    }
    
    view->storeLayout(layouts[0]);
    for (size_t i = 1; i < layouts.size(); i++) {
        std::shared_ptr<dawview_layout_t> viewLayout = loadDawViewLayoutSnapshot(StringFormat("data/view%zu.layout", i));
        if (viewLayout) {
            layouts[i] = *viewLayout.get();
        }
    }
    view->loadLayout(layouts[1]);
    dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
    DawCtrl::startApp();
}

void DawInstance::initProcessingResources() {
    dbgassert(initState == 2);
    initState++;
    tls.host->initThreads();
}

void DawInstance::initRealtimeResources() {
    dbgassert(initState == 3);
    initState++;
    tls.audioHost->initPa();
    tls.midiHost->initPm();
    if (tls.settings->dawsettings.audioEnabled) {
        if (tls.audioHost->startAudio(tls.settings->iosettings)) {
            auto stream = tls.audioHost->getStreamSharedPtr(0);
            tls.host->setOutput(stream);
        } else {
            //notify user
            log_lf(Log::L_ERROR, "audioHost->startAudio() failed\n");
        }
    }
    tls.midiHost->startMidi();

    this->playThread.setTls(tls);
    this->playThread.startThread(this);
    dbgassert(this->playThread.getState() == playback_state::status_no_process);


    this->workerThread.setTls(tls);
    this->workerThread.startThread();

    setAudioThreadState(playback_state::status_stop);
}
std::pair<String, String> DawInstance::createUniqueNonExistingFilename(const String& baseDir, const String& trackName, const String& sampleName, const String& fileExt) {


    String testFileName;
    if (!trackName.empty()) {
        testFileName += trackName;
        testFileName += " - ";
    }
    testFileName += sampleName;

    String tempPath = baseDir;
    tempPath += FILE_PATHSEP_CHAR;
    String projName = getProjectName();
    if (projName.empty()) {
        projName = "Untitled";
    }
    tempPath += projName;
    tempPath += FILE_PATHSEP_CHAR;
    tempPath += testFileName;
    tempPath += ".";
    tempPath += fileExt;

    String sampleFilePath = App::Platform::toUserdataPath(tempPath);
    App::Platform::sanitizePathToFile(sampleFilePath);
    String name;
    String ext;
    String path;
    int32_t idx = 0;
    String uniqueName = sampleFilePath;
    SplitPath(sampleFilePath, &path, &name, &ext);
    App::Platform::sanitizePathToDirectory(path);
    while ((FileExists(uniqueName) || tls.audioCache->getByFilename(uniqueName) != nullptr) && ++idx < 10000) {
        // String nextPath = path;
        testFileName = name;
        testFileName += "-";
        testFileName += std::to_string(idx);
        testFileName += ".";
        uniqueName = path;
        uniqueName += testFileName;
        uniqueName += ext;
        idx++;
    }
    return {uniqueName, testFileName};
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
    dbgassert(initState > 2);
    const bool isRealtimeInstance = initState > 3;
    initState = -1;

    if (isRealtimeInstance) {
        setAudioThreadState(playback_state::status_no_process);
        tls.midiHost->stopMidi();
        tls.audioHost->stopAudio();
    }
    projectToLoad = nullptr;
    clipboardPlugins = nullptr;
    dragdropclip.reset();

    plugindb.closeDatabase();


    if (isRealtimeInstance) {

        this->workerThread.stopThread();
        this->workerThread.joinThread();
        this->playThread.stopThread();
        this->playThread.joinThread();
#ifndef NDEBUG
        for (auto& companion : companionWindows) {
            dbgassert(!companion.ctrl->isOk());
        }
#endif // NDEBUG
        companionWindows.clear();
        tls.audioHost->deinitPa();
        tls.midiHost->deinitPm();
    }

    tls.host->unload();
    tls.host->destroy();

    try {
        if (tls.settings->saveOnExit) {
            saveSettings(*tls.settings);
        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "Failed saving settings %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(SETTINGS_NAME)), e.what());
        ngui::showNotification(ngui::Style::Warning, "Couldn't write config file", "Some settings may have been reset");
    }
    delete tls.commandManager;
    delete tls.runtime;
    delete tls.settings;
    delete tls.audioCache;
    delete tls.midiHost;
    delete tls.audioHost;
    delete tls.host;
    tls.dawInstance    = nullptr;
    tls.host           = nullptr;
    tls.pluginManager  = nullptr;
    tls.runtime        = nullptr;
    tls.settings       = nullptr;
    tls.midiHost       = nullptr;
    tls.audioHost      = nullptr;
    tls.mainCtrl       = nullptr;
    tls.project        = nullptr;
    tls.pluginDatabase = nullptr;
    tls.audioCache     = nullptr;
    tls.commandManager = nullptr;
    tls.tlsInitialized = false;
    daw_tls::setTls(tls);
    printClipAllocations();
    printLeakedAudioBuffers();
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_printf("getNumClipAllocations == %d!\n", totalAllocs);
        dbgassert(getNumClipAllocations() == 0);
    }
}

void DawCtrl::destroy() {
    if (!isOK) {
        return;
    }

    waveformRenderer->destroy();
    delete waveformRenderer;
    waveformRenderer = nullptr;

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

void DawInstance::initDaw() {
    dbgassert(initState == 0);
    initState++;
    //TODO allow passing in optional tls or refactor loadSettings in seperate function
    bool isAlreadyInitialized = daw_tls::isTlsInitialized();
    auto& initTls = isAlreadyInitialized ? daw_tls::getTls() : daw_tls::initNewTls();
    auto& settings = *initTls.settings;

    if (!isAlreadyInitialized) {
        try {
            loadSettings(settings);
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Failed loading settings %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(SETTINGS_NAME)), e.what());
            ngui::showNotification(ngui::Style::Warning, "Couldn't read config file", "Some settings may have been reset");
        }
    }
    initTls.dawInstance = this;
    initTls.host = new DAW::Host::Host();
    initTls.pluginManager = initTls.host;
    if (!DAW::Host::PluginManager::assignMasterCallback(initTls.pluginManager)) {
        delete initTls.host;
        dbgassert(0);
        throw applogicexception("no empty vst callback slot");
    }

    initTls.project        = this;
    initTls.audioHost      = new audiohost();
    initTls.midiHost       = new midihost();
    initTls.pluginDatabase = &plugindb;
    initTls.audioCache     = new audiocache(settings.iosettings.samplerate);
    initTls.commandManager = new DAW::UI::CommandManager();
    initTls.host->setTls(initTls);
    this->tls = initTls;

    setSSEFlushDenormals();
    initTls.host->setSampleFormat(sampleformat_t{
        static_cast<samplerate_t>(settings.iosettings.internalSamplerate),
        settings.iosettings.internalBlocksize,
        sampleformat_bits_t::FLOAT_32
    });
    initTls.commandManager->init();
}

MainCtrl::MainCtrl(DawInstance& _daw) : DawCtrl(nullptr, _daw) {
}

void MainCtrl::initApp(const std::vector<String>& args) {
    auto& settings = daw_tls::getSettings();
    auto pathProjStartup = settings.dawsettings.startupProjectPath;
    if (!pathProjStartup.empty() && FileExists(pathProjStartup)) {
        loadProject = pathProjStartup;
        if (settings.dawsettings.startupLoadDeffered) {
            loadFlags |= FLAG_DEFER_LOAD;
        }
    }
    
    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "--load" && i + 1 < args.size()) {
            loadProject = args[i + 1];
            i++;
            continue;
        }
        if (args[i] == "--defer" && i + 1 < args.size()) {
            loadProject = args[i + 1];
            loadFlags = FLAG_DEFER_LOAD;
            i++;
            continue;
        }
        if (StrEndsWith(args[i], "." PROJECT_FILE_EXT)) {
            loadProject = args[i];
            continue;
        }
    }
}

bool DawCtrl::initAppWindow(window_main* window, NVGcontext* nanovg) {
    dbgassert(!this->mainWindow);
    this->mainWindow = window;
    this->window     = window;
    this->vg         = nanovg;

    this->waveformRenderer = new waveformrender(pathrenderer_type_e::PAR_BASIC);
    this->waveformRenderer->init();

    themes.loadThemes();

    getDefaultTheme()->bindFonts();
    setupView();

    menus.recent.type  = ngui::menu_type::submenu;
    menus.recent.title = "Open recent";
    menus.file.type  = ngui::menu_type::submenu;
    menus.file.title = "File";
    menus.file.addCommand(this, GlobalCommandType::CMD_FILE_NEW);
    menus.file.addCommand(this, GlobalCommandType::CMD_FILE_OPEN);
    menus.file.add(&menus.recent);
    menus.file.addCommand(this, GlobalCommandType::CMD_FILE_SAVE);
    menus.file.addCommand(this, GlobalCommandType::CMD_FILE_SAVEAS);
    menus.file.addCommand(this, GlobalCommandType::CMD_SET_STARTUP_PROJECT);
    menus.file.addSeperator();
    menus.file.addCommand(this, GlobalCommandType::CMD_EXPORT_TRACK);
    menus.file.addCommand(this, GlobalCommandType::CMD_IMPORT_TRACK);
    menus.file.addSeperator();
    menus.file.addCommand(this, GlobalCommandType::CMD_EXIT);
    menus.edit.type  = ngui::menu_type::submenu;
    menus.edit.title = "Edit";
    menus.edit.addCommand(this, GlobalCommandType::CMD_UNDO);
    menus.edit.addCommand(this, GlobalCommandType::CMD_REDO);
    menus.edit.addSeperator();
    menus.edit.addCommand(this, GlobalCommandType::CMD_INSERT_MIDI_TRACK);
    menus.edit.addCommand(this, GlobalCommandType::CMD_INSERT_AUDIO_TRACK);
    menus.edit.addCommand(this, GlobalCommandType::CMD_INSERT_RETURN_TRACK);
    menus.edit.addCommand(this, GlobalCommandType::CMD_INSERT_MASTER_TRACK);
    menus.edit.addSeperator();
    menus.edit.addCommand(this, GlobalCommandType::CMD_REACTIVATE_AUTOMATION);
    menus.tools.type  = ngui::menu_type::submenu;
    menus.tools.title = "Tools";
    menus.tools.addCommand(this, GlobalCommandType::CMD_PREFERENCES);
    menus.tools.addCommand(this, GlobalCommandType::CMD_ABOUT);
    menus.views.type  = ngui::menu_type::submenu;
    menus.views.title = "View";
    menus.views.addCommand(this, GlobalCommandType::CMD_OPEN_SECOND_WINDOW);
    menus.views.addSeperator();
    auto& mapGuiTypeToCstr = getContainerFactory();
    for (auto& it : mapGuiTypeToCstr) {
        auto guiType = it.first;
        String name;
        getContainerLabel(guiType, name);
        if (name.empty())
            continue;
        menus.views.addCommand(this, GlobalCommandType::CMD_CREATE_VIEW, static_cast<int>(guiType), "Show " + name);
    }
    menus.views.addSeperator();
    menus.views.addCommand(this, GlobalCommandType::CMD_SHOW_DEBUG_WINDOW, 0, "Show Waveform Cache");
    menus.views.addCommand(this, GlobalCommandType::CMD_SHOW_DEBUG_WINDOW, 1, "Show dbg window");
    menus.views.addCommand(this, GlobalCommandType::CMD_SHOW_DEBUG_WINDOW, 2, "Show profiling results");
    menus.views.addCommand(this, GlobalCommandType::CMD_SHOW_DEBUG_WINDOW, 3, "Show test dialog");

    menubar.add(&menus.file);
    menubar.add(&menus.edit);
    menubar.add(&menus.tools);
    menubar.add(&menus.views);
    this->updateMenubar();
#if !USE_GUI_MENU
    this->mainWindow->updateMenu();
#endif

    auto& settings = daw_tls::getSettings();
    if (isCompanion()) {
        grid.grid_dens = settings.wndCompanion.dens;
    } else {
        grid.grid_dens = settings.wndMain.dens;
    }

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
    //log_lf(Log::L_DEBUG, "onTick %d\n", std::this_thread::get_id());
    mainWindow->requestRedraw();
}

void ProjectGraphMonitor::onTick(MainCtrl* ctrl) {
    if (processingGraph) {
        lastWorkingProcGraph = processingGraph;
    }
    processingGraph = nullptr;
    auto daw = ctrl->getDaw();
    auto project = daw->getProject();
    bool bSuccess = DAW::buildProcessingGraph(daw->getHost(), project, project->trackList.getAllTracksFlatVecRef(), processingGraph);
    if (bWorkingProcessingGraph && !bSuccess) {
        if (!popupNotifyError) {
            auto guiNotify = new gui_notify();
            guiNotify->guis[0]->setVisible(false);
            guiNotify->setMessage("Found loop in routing graph", "Remove feedback loop in routing graph");
            guiNotify->setColors(GuiColor::COL_INVALID_INPUT, GuiColor::COL_TEXT);
            guiNotify->size      = ivec2(420, 90);
            guiNotify->layout();
            popupNotifyError = guiNotify;
        }
    }
    if (popupNotifyError)
        popupNotifyError->setVisible(!bSuccess);
    bWorkingProcessingGraph = bSuccess;
}

void MainCtrl::onTick() {
    daw.onTick();
    DawCtrl::onTick();
    graphMonitor.onTick(this);
    auto notify = graphMonitor.getNotifyError();
    bool bIsInContainers = stl_contains(this->containers, notify);
    if (notify && (notify->isVisible() != bIsInContainers)) {
        if (!bIsInContainers) {
            containers.push_back(notify);
            notify->setControl(this);
        } else {
            removeEntry(containers, notify);
            notify->setControl(nullptr);
        }
    }
    if (notify && notify->isVisible()) {
        notify->size = ivec2(m_size.x/3, 90);
        notify->pos = (m_size - notify->size) / 2;
        notify->layout();
    }

    if (guiDragged && !guiCaptured && guiDragged->isDragMoveable()) {
        track_gui_entry_t* tr  = nullptr;
        int32_t hoverTicks     = 0;
        guictr_base& ctrMixers = view->ctr_tracks.trackControls;
        guictr_base& ctrTrackView = view->ctr_tracks.trackView;
        if (view->ctr_tracks.isVisible()) {
            ivec2 trackViewLocalPos = toControlsObjectSpace(m_mousePos, &view->ctr_tracks);
            ivec2 posRelative(0, 0);
            bool bHit = false;
            if (ctrMixers.contains(trackViewLocalPos)) {
                posRelative = m_mousePos - ctrMixers.toScreenSpace(ivec2(0));
                bHit = true;
            } else if (ctrTrackView.contains(trackViewLocalPos)) {
                posRelative = m_mousePos - ctrTrackView.toScreenSpace(ivec2(0));
                bHit = true;
            }
            if (bHit) {
                tr = getTrackFromMouse(this->view->ctr_tracks.guiMgr, posRelative);
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

void DawInstance::onPluginsChanged() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->onPluginsChanged();
    }
    tls.pluginManager->onTrackLayoutChange();
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

void DawInstance::getTrackContainers(std::vector<guictr_tracks*>& trackContainers) {
    for (size_t i = 0; i < dawCtrls.size(); i++) {
        dawCtrls[i]->getTrackContainers(trackContainers);
    }
}

void DawInstance::setMainControl(MainCtrl* _mainCtrl) {
    dbgassert(!tls.mainCtrl);
    tls.mainCtrl = _mainCtrl;
    daw_tls::getTls().mainCtrl = tls.mainCtrl;
    tls.host->setTls(tls);
    this->dawCtrls.push_back(tls.mainCtrl);
}

MainCtrl* DawInstance::getMainControl() {
    return this->tls.mainCtrl;
}

guictxtmenu_base* makeGuiAutosave(int64_t delay);

String getProjectAutosaveFilename(String projectPath) {
    String bakPathName;
    if (projectPath.empty()) {
        App::Platform::createUniqueFilename(bakPathName, App::Platform::toUserdataPath("unsaved.project"));
    } else {
        String path, name, ext, nameExt;//path, name, ext, nameExt
        SplitPath(projectPath, &path, &name, &ext, &nameExt);
        bakPathName = path;
        bakPathName += FILE_PATHSEP_STR;
        bakPathName += name;
        bakPathName += "-autosave.";
        bakPathName += ext;
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
void DawInstance::configureSampleRate() {
    const bool wasPlaying = isPlaying();
    if (wasPlaying) {
        stopPlaying();
    }
    setAudioThreadState(playback_state::status_stop);
    setAudioThreadState(playback_state::status_no_process);
    auto& settings = daw_tls::getSettings();
    {

        ThreadLock lock  = getPlayThread()->lockThread();
        auto host  = getHost();
        auto ahost = getAudioHost();
        ahost->stopAudio();
        host->setOutput(nullptr);
        if (settings.dawsettings.audioEnabled) {
            auto oldSampleRate = host->m_sampleFormatInternal.sampleRate;
            host->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(settings.iosettings.internalSamplerate),
                                                 settings.iosettings.internalBlocksize, sampleformat_bits_t::FLOAT_32});
            auto newSampleRate = host->m_sampleFormatInternal.sampleRate;
            for (track_t* t : project.trackList) {
                t->updateAudioClipLengths(projectGlobals.tempo100, oldSampleRate, newSampleRate);
            }
            if (ahost->startAudio(settings.iosettings)) {
                host->setOutput(ahost->getStreamSharedPtr(0));
            } else {
                //settings.dawsettings.audioEnabled = false;
            }
        }
    }
    if (settings.dawsettings.audioEnabled) {
        if (wasPlaying) {
            startPlaying();
        } else {
            setAudioThreadState(playback_state::status_stop);
        }
    }
}
void DawInstance::onTick() {
    const bool bWroteMidiData = tls.host->writeRecordedData(this);

    if (bWroteMidiData) {
        updateVisibleTrackContents();
    }

    tls.host->onTick();

    static int scriptState = -1;
    static const int64_t tmDelayStateChange = 555;
    static int64_t tmStateChange = 0;
    bool noPopups = true;
    for (auto* ctrl : dawCtrls) {
        noPopups &= !ctrl->guiDragged && !ctrl->guiCaptured && !ctrl->ctxtmenu;
    }
    if (noPopups && projectToLoad) {
        std::shared_ptr<project_to_load_t> projectToLoadCpy = projectToLoad;
        projectToLoad = nullptr;
        bool projectLoadErrored = false;
        AppWndProc_enableBlockReentrant();
        try {
            setLoadedProject(projectToLoadCpy->projectfile, projectToLoadCpy->loadflags);
        } catch (std::exception& e) {
            log_printf("Failed loading project: %s\n", e.what());
            projectLoadErrored = true;
        } catch (...) {
            log_printf("Failed loading project. Unhandled exception\n");
            projectLoadErrored = true;
        }
        AppWndProc_disableBlockReentrant();
        if (cbProjectLoadCompleteCallback) {
            cbProjectLoadCompleteCallback(this, projectToLoadCpy->projectfile, projectLoadErrored ? 1 : 0);
            cbProjectLoadCompleteCallback = nullptr;
        }
        log_printf("Project load completed %s\n", projectLoadErrored ? "with errors" : "succesfully");
        if (scriptState != -1) {
            scriptState = 2;
            tmStateChange = getTimeMillis();
        }
    } else {
        static size_t fileIndexToLoad = 0;
        static recentfilelist fileListStatic = tls.settings->recentfiles;
        static seq_rand rnd;
                
        if (fileListStatic.sortedEntries.empty()) {
            fileListStatic = tls.settings->recentfiles;
        }
        auto tmMillis = getTimeMillis();
        if (scriptState == 0 && (tmMillis - tmStateChange) > tmDelayStateChange) {
            tmStateChange = tmMillis;
            rnd.rng_seed(static_cast<uint64_t>(tmStateChange));
            fileIndexToLoad++;
            if (fileIndexToLoad >= fileListStatic.sortedEntries.size()) {
                fileIndexToLoad = 0;
            }
            if (fileIndexToLoad < fileListStatic.sortedEntries.size()) {
                String filename = fileListStatic.sortedEntries[fileIndexToLoad];
                loadFile(filename, FLAG_INVOKE_USER_CB_DEFERLOAD);
                if (projectToLoad) {
                    scriptState = 1;
                }
            }
        }
        if (scriptState == 2 && (tmMillis - tmStateChange) > tmDelayStateChange) {
            tmStateChange = tmMillis;
            startPlaying();
            scriptState = 3;
        }
        if (scriptState >= 3 && (tmMillis - tmStateChange) > tmDelayStateChange) {
            std::vector<effectbase*> pluginsDeferred;
            tls.host->getDeferredEffects(pluginsDeferred);
            if (!pluginsDeferred.empty()) {
                auto idx = rnd.rng_bits(8)%pluginsDeferred.size();
                auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[idx]);
                effectbase* pluginLoaded;
                ThreadLock lock = getPlayThread()->lockThread();
                tls.host->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &pluginLoaded);
            } else {
                scriptState = 10;
            }
            tmStateChange = getTimeMillis();
            scriptState++;
            if (scriptState >= 10) {
                scriptState = 0;
            }
        }
    }
    
    if (tls.mainCtrl) {
        auto& settings = daw_tls::getSettings();
        if (settings.autosave.tmSaveDelayMinutes > 0) {
            if (0 == autosaveState.tmLastTrigger) {
                autosaveState.tmLastTrigger = getTimeMillis();
            }
            int64_t tmNow = getTimeMillis();
            const auto ms60k = 60 * 1000;
            if ((tmNow - tmLastSave) / ms60k > settings.autosave.tmSaveDelayMinutes) {
                bool canOpenAutosave = noPopups;
                bool hasAnyInputFocus = false;
                for (auto* ctrl : dawCtrls) {
                    canOpenAutosave &= !ctrl->window->isMouseCaptured();
                    canOpenAutosave &= !ctrl->hasDialogWindows();
                    canOpenAutosave &= !ctrl->hasContextMenu();
                    hasAnyInputFocus |= ctrl->hasInputFocus();
                    /*canOpenAutosave &= last click was n seconds ago*/
                }
                canOpenAutosave &= hasAnyInputFocus;
                if (canOpenAutosave &&  (tmNow - autosaveState.tmLastTrigger) / ms60k > math::max<int64_t>(settings.autosave.tmReminderDelayMinutes, 1)) {
                    autosaveState.tmLastTrigger = tmNow;
                    auto tooltip                = makeGuiAutosave(1500);
                    auto ctrlSize               = tls.mainCtrl->m_size;
                    tooltip->size               = ivec2(420, 90);
                    tooltip->maxHeight          = tooltip->size.y;
                    tls.mainCtrl->openContextMenu(tooltip, ivec2(ctrlSize.x / 2, ctrlSize.y - 100) - tooltip->size / 2);
                }
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
    file->layouts.resize(dawCtrls.size());
    auto itOut = file->layouts.begin();
    for (auto& ctrl : dawCtrls) {
        ctrl->storeLayout(*itOut++);
    }
    file->project.globals        = projectGlobals;
    file->project.exportSettings = getExportSettings();
    file->project.quantizeSettings = getQuantizeSettings();
    if (tls.host) {
        file->project.samplerate = tls.host->m_sampleFormatInternal.sampleRate;
    }
    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    tls.audioCache->saveSamples(uniqueSampleIds);
    tls.audioCache->store(uniqueSampleIds, file->sampleFileIndex);
    if (tls.mainCtrl) {
        file->layout.layoutGrid    = tls.mainCtrl->grid;
        file->layout.scrollOffsetX = tls.mainCtrl->view->ctr_tracks.getScrollOffset();
    }
    return file;
}
namespace DAW {
void GetProjectReferencedSampleIds(const project_t& project, std::vector<int32_t>& uniqueSampleIds) {
    for (track_t* t : project.trackList) {
        auto& clipContainer = t->getConstMidi();
        for (auto& clip : clipContainer.getConstClips()) {
            if (clip->audio.id >= 0 && !std::binary_search(uniqueSampleIds.cbegin(), uniqueSampleIds.cend(), clip->audio.id)) {
                insertSorted(uniqueSampleIds, clip->audio.id);
            }
        }
    }
}
}
void DawInstance::unloadUnreferencedSamples() {
    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    log_lf(Log::L_DEBUG, "Found %zu sample ids\n", uniqueSampleIds.size());
    tls.audioCache->unloadUnreferenced(uniqueSampleIds);
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
    log_printf("Loading project %s: %zu tracks\n", StringAsCStr(file->path), project.trackList.size());
    ThreadLock lock = playThread.lockThread();
    unloadProject();
    /** make sure call to unloadProject unloaded all vst2 instances **/
    dbgassert(tls.host->getVst2Instances().empty());
    //TODO: assert that audiocache is empty
    dbgassert(tls.audioCache->isEmpty());

    /** populates trackList **/
    project.copyFrom(file->project);
    projectGlobals      = file->project.globals;
    getExportSettings() = file->project.exportSettings;
    getQuantizeSettings() = file->project.quantizeSettings;


    /** create all audio instances **/
    for (track_t* t : project.trackList) {
        t->updateAudioClipLengths(projectGlobals.tempo100, file->project.samplerate, tls.host->m_sampleFormatInternal.sampleRate);
        tls.host->createAudio(t);
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
    tls.host->updateMaximumStageId();

    /** remove routings to missing track **/
    DAW::validateTrackRoutings(tls.host, project.getTracksFlatVec());
    /** create all gui instances **/
    for (track_t* tr : project.trackList) {
        DAW::validateEffectRoutings(tls.host, tr->audio);
    }

    /** inform host about track layout changes so it resets and updates internal structures **/
    tls.host->onTrackLayoutChange();
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
    std::vector<effectbase*> pluginsDeferred;
    tls.host->getDeferredEffects(pluginsDeferred);
    MainCtrl* renderCtrl = tls.mainCtrl;
    bool showLoadingScreen = renderCtrl != nullptr && false;
    if (!showLoadingScreen || !renderCtrl) {
        if ((flags & FLAG_DEFER_LOAD) == 0) {
            int len = pluginsDeferred.size();
            for (int i = 0; i < len; i++) {
                dbgassert(pluginsDeferred[i]->getModuleType() == PLUGIN_TYPE_DEFERRED);
                auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[i]);
                effectbase* pluginLoaded;
                tls.host->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &pluginLoaded);
            }
        }
        tls.audioCache->load(file->sampleFileIndex);
    } else {
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
                setFont(vg, 32, THEMECOL_TEXT, NVG_ALIGN_TOP | NVG_ALIGN_CENTER);
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


            /** get the list of all plugins in deferred loading state **/

            int len = pluginsDeferred.size();
            for (int i = 0; i < len; i++) {
                dbgassert(pluginsDeferred[i]->getModuleType() == PLUGIN_TYPE_DEFERRED);
                auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[i]);
                windowMain->preRender();

                NVGcolor col = renderCtrl->getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
                glClearColor(col.r, col.g, col.b, col.a);
                glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

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
                effectbase* pluginLoaded;
                tls.host->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &pluginLoaded);
            }
            log_printf("end plugin list loading\n");
        const int32_t numSamplesToLoad = file->sampleFileIndex.list.size();
        {
            windowMain->preRender();
            NVGcolor col = renderCtrl->getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
            glClearColor(col.r, col.g, col.b, col.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
            tls.audioCache->load(file->sampleFileIndex);
        }
        ctr.setControl(nullptr);
        AppWndProc_disableBlockReentrant();
    }
    for (track_t* tr : project.trackList) {
        tr->getStage()->pluginsChanged();
    }
    tls.host->onTrackLayoutChange();

    this->layoutsFromProjectFile = file->layouts;
    /** validate cursor state **/
    auto ctrl = tls.mainCtrl;
    if (ctrl) {
        if (this->layoutsFromProjectFile.size() > 0) {
            ctrl->loadLayout(this->layoutsFromProjectFile[0]);
        }
        ctrl->view->ctr_tracks.loadTrackLayouts(file->project.trackCtr);
        ctrl->view->ctr_tracks.loadTrackLayouts(file->project.trackReturnCtr);
        ctrl->view->ctr_tracks.loadTrackLayouts(file->project.trackMasterCtr);
        ctrl->grid.setLayout(file->layout.layoutGrid);
        ctrl->view->ctr_tracks.setScrollOffset(file->layout.scrollOffsetX);
    }

    updateVisibleTrackContents();
    for (DawCtrl* pDawCtrl : dawCtrls) {
        pDawCtrl->fixCursor();
    }

    /** set as current project **/
    this->projectPath = file->path;

    this->tmLastSave  = getTimeMillis();
    if (tls.mainCtrl) {
        tls.mainCtrl->setStatusText(StringFormat("Loaded project %s", StringAsCStr(this->projectPath)));
        String projectFileName;
        SplitPath(this->projectPath, nullptr, &projectFileName, nullptr);
        tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::BUILD_BINARY_NAME, StringAsCStr(projectFileName)));
    }

    setAudioThreadState(playback_state::status_stop);
    return true;
}

void MainCtrl::dragContainerRelayout(drag_ctr_event evt) {
    viewContainers->dragContainerRelayout(this, evt);
    if (evt.evtType == BaseCtrl::drag_ctr_event_type::DRAG_END) {
        BaseCtrl::relayout();
    }
}

void MainCtrl::getTrackContainers(std::vector<guictr_tracks*>& trackContainers) {
    trackContainers.push_back(&view->ctr_tracks);
}

void CompanionCtrl::getTrackContainers(std::vector<guictr_tracks*>& trackContainers) {
    trackContainers.push_back(&view->ctr_tracks2);
}

guictr_tracks* MainCtrl::getTrackContainer() {
    return &view->ctr_tracks;
}

guictr_nodes_splitview* MainCtrl::getNodesContainer() {
    return &view->ctr_nodes;
}

guictr_clipeditor* MainCtrl::getClipEditor() {
    return &view->ctr_clipeditor;
}

guictr_clipeditor* CompanionCtrl::getClipEditor() {
    return &view->ctr_clipeditor;
}

guictr_plugins* MainCtrl::getPluginsView() {
    return &view->ctr_plugins;
}

guictr_plugins* CompanionCtrl::getPluginsView() {
    return nullptr;
}

guictr_tracks* CompanionCtrl::getTrackContainer() {
    return &view->ctr_tracks2;
}

guictr_nodes_splitview* CompanionCtrl::getNodesContainer() {
    return &view->ctr_nodes;
}

void MainCtrl::layoutView(int32_t w, int32_t h) {
    w = math::max(640, w);
    h = math::max(480, h);
    viewContainers->layout(w, h);

    view->ctr_plugins.layout();
    view->ctr_clipeditor.layout();
    view->ctr_tracks.layout();
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

    view->ctr_tracks2.layout();
    view->ctr_nodes.layout();
    view->ctr_clipeditor.layout();
    for (guictr_base* ctr : containers) {
        if (ctr == &view->ctr_tracks2)
            continue;
        if (ctr == &view->ctr_clipeditor)
            continue;
        if (ctr == &view->ctr_nodes)
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
    if (tls.mainCtrl) {
        tls.mainCtrl->view->ctr_plugins.showTrack(trackEntry && trackEntry->track ? trackEntry->track->audio : nullptr);
    }
}

void DawInstance::setSelectedTrack(track_t* track) {
    selectedTrack = track;
    if (tls.mainCtrl) {
        tls.mainCtrl->view->ctr_plugins.showTrack(track ? track->audio : nullptr);
    }
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

guitrack_editor& MainCtrl::getTrackEditor() {
    return view->ctr_tracks.trackView;
}

void MainCtrl::onPluginsChanged() {
    log_lf(Log::L_DEBUG, "onPluginsChanged\n");
    view->ctr_nodes.reset();
    view->ctr_nodes.refresh();
    view->ctr_plugins.relayout();
}

void DawCtrl::updateVisibleTrackContents() {
    static int n = 0;
    if (n++ % 20 == 0) {
        log_lf(Log::L_DEBUG, "updateVisibleTrackContents call #%d\n", n);
    }
    guictr_tracks* trackContainer = getTrackContainer();
    trackContainer->updateVisibleTracks();
    double scrollPixelOffset = trackContainer->getScrollOffsetPixels();
    trackContainer->layout();
    grid.update(trackContainer->trackView.getSizeContent());
    trackContainer->layoutVisibleTracks();
    trackContainer->scrollToPixelOffset(scrollPixelOffset);
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

void DawCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) {
    daw.dragdropTarget.reset();
#if USE_GUI_MENU
    if (ctxtmenu && !ctxtmenu->isTransient() && viewContainers->getMenu()) {
        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER, kbmods);
        if (viewContainers->getMenu()->mouseHitTest(mousePos, evt)) {
        }
        return;
    }
#endif
    BaseCtrl::mouseMoved(mousePos, deltaPos, kbmods);
}

bool DawCtrl::filesDropBegin(std::vector<String>& files, ivec2 mousepos, KeyboardMods kbmods) {
    log_lf(Log::L_DEBUG, "filesDropBegin %d %d isdragging=%d\n", mousepos.x, mousepos.y, daw.dragdropclip.isLoaded);
    daw.dragdropclip.reset();
    if (guiDragged || guiCaptured) {
        return false;
    }
    if (files.size()) {
        String path = files.front();
        if (StrEndsWith(path, ".wav")) {
            String a, b, c, d;
            SplitPath(path, &a, &b, &c, &d);
            audiofile_t* audio = daw.getAudioCache()->loadFile(path);
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
            if (!daw.workerThread.pushTask(&task)) {
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
                        log_lf(Log::L_DEBUG, "drag-drop clipboard loaded\n");
                    } else {
                        log_lf(Log::L_WARN, "Failed loading drag-drop clipboard\n");
                    }
                }
            }
        }
        if (daw.dragdropclip.isLoaded) {
            MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP, kbmods);
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

bool DawCtrl::filesDropMove(ivec2 mousepos, KeyboardMods kbmods) {
    if (guiDragged || guiCaptured) {
        daw.dragdropclip.reset();
        return false;
    }
    if (daw.dragdropclip.isLoaded) {
        daw.dragdropclip.isValidTarget = false;

        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP, kbmods);
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

void DawCtrl::filesDropCancel() {
    daw.dragdropclip.reset();
}
bool DawCtrl::filesDropFinal(std::vector<String>& files, ivec2 mousepos, KeyboardMods kbmods) {
    clipreset rst(daw.dragdropclip);
    if (guiDragged || guiCaptured) {
        return false;
    }
    if (daw.dragdropclip.isLoaded && daw.dragdropclip.isValidTarget) {
        log_lf(Log::L_DEBUG, "filesDropFinal %d %d isdragging=%d\n", mousepos.x, mousepos.y, daw.dragdropclip.isLoaded);
        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP, kbmods);
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

bool MainCtrl::processGlobalKeyevent(const KeyEvent& event) {
    if (event.type == KeyboardState::K_PRESS) {
        if (!event.cmd && event.keyCode == KeyboardKey::DAW_KB_L) {
            bShowDebugFrames = !bShowDebugFrames;
            dragContainerRelayout(drag_ctr_event{ drag_ctr_event_type::DRAG_END });
            return true;
        }
    }
    return DawCtrl::processGlobalKeyevent(event);
}

bool DawCtrl::handleGlobalCommand(DAW::UI::CommandContext& ctxt) {
    auto& kevt = ctxt.kevt;
    switch (ctxt.type) {
        case CMD_STARTSTOP_PLAYBOCK: {
            if (kevt.type != KeyboardState::K_RELEASE) {
                if (daw.isPlaying()) {
                    daw.stopPlaying();
                } else {
                    daw.startPlaying(); //TODO: pass cursor position
                }
            }
            return true;
        }
        default:
            break;
    }
    if (kevt.type == KeyboardState::K_PRESS) {
        if (menuCommand(CMD_NOARG(ctxt.type))) {
            return true;
        }
    }
    if (this->getTrackContainer()->handleEditorCommand(ctxt)) {
        return true;
    }
    if (this->getClipEditor()->handleEditorCommand(ctxt)) {
        return true;
    }
    return false;
}
bool CompanionCtrl::handleGlobalCommand(DAW::UI::CommandContext& ctxt) {
    switch (ctxt.type) {
        case CMD_SWITCH_VIEW: {
            if (ctxt.kevt.type != KeyboardState::K_RELEASE) {
                switch (this->viewMode) {
                    case view_mode_t::TRACK_TIMELINE:
                        this->setViewMode(view_mode_t::MIXER);
                        break;
                    case view_mode_t::MIXER:
                        this->setViewMode(view_mode_t::NODE_EDITOR);
                        break;
                    case view_mode_t::NODE_EDITOR:
                        this->setViewMode(view_mode_t::TRACK_TIMELINE);
                        break;
                }
            }
            return true;
        }
        default:
            break;
    }
    return DawCtrl::handleGlobalCommand(ctxt);
}

bool MainCtrl::handleGlobalCommand(DAW::UI::CommandContext& ctxt) {
    auto& kevt = ctxt.kevt;
    switch (ctxt.type) {
        case CMD_SWITCH_LAYOUT: {
            if (kevt.type == KeyboardState::K_PRESS) {
                if ((kevt.mods & KB_MOD_SHIFT) == kevt.mods && ctxt.argInt0 >= 0 && ctxt.argInt0 < CtrSize(layouts)) {
                    auto index = ctxt.argInt0 % layouts.size();
                    bool store    = (kevt.mods & KB_MOD_SHIFT);
                    if (store) {
                        view->storeLayout(layouts[index]);
                        saveDawViewLayoutSnapshot(layouts[index], StringFormat("data/view%zu.layout", index));
                    } else {
                        view->loadLayout(layouts[index]);
                        BaseCtrl::relayout();
                        dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
                    }
                    return true;
                }
            }
            return true;
        }
        case CMD_SWITCH_VIEW: {
            if (kevt.type != KeyboardState::K_RELEASE) {
                if (this->viewMode == view_mode_t::TRACK_TIMELINE) {
                    this->setViewMode(view_mode_t::NODE_EDITOR);
                } else {
                    this->setViewMode(view_mode_t::TRACK_TIMELINE);
                }
            }
            return true;
        }
        default:
            break;
    }
    if (DawCtrl::handleGlobalCommand(ctxt)) {
        return true;
    }
    if (view->ctr_plugins.handleCommand(ctxt)) {
        return true;
    }
    return false;
}

bool DawCtrl::processGlobalKeyevent(const KeyEvent& kevt) {
    if (kevt.type == KeyboardState::K_PRESS) {
        lastKeyDebug = getKeyName(kevt.scancode);
        if (!lastKeyDebug.length()) {
            const char* ca = GlfwKeycodeToString(kevt.keyCode, kevt.scancode);
            if (ca) {
                lastKeyDebug = ca;
            }
        }
    }
    return false;
}

void DawInstance::startPlaying() {
    setAudioThreadState(playback_state::status_playback);
}

void DawInstance::startExport() {
    setAudioThreadState(playback_state::status_no_process);
    if (tls.audioHost) {
        tls.audioHost->stopAudio();
    }
    if (tls.midiHost) {
        tls.midiHost->stopMidi();
    }
    tls.host->setOutput(nullptr);
    playThread.addRequestWithCallback(REQ_STATE, (int) playback_state::status_render, []() {
        auto& tls = daw_tls::getTls();
        auto& settings = daw_tls::getSettings();
        if (settings.dawsettings.audioEnabled) {
            if (tls.audioHost->startAudio(settings.iosettings)) {
                auto stream = tls.audioHost->getStreamSharedPtr(0);
                tls.host->setOutput(stream);
            }
        }
        if (tls.midiHost) {
            tls.midiHost->startMidi();
        }
    }, true);
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
        tls.host->createAudio(newTrack);
    }
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->addTrackToView(newTrack, flags);
        }
    }
    if (flags & FLG_TRK_CHANGE_USER) {
        pushHist(new action_modify_track_add(StringFormat("Add %s Track", TrackTypeToName(newTrack->type)), newTrack));
    }

    tls.host->onTrackLayoutChange();
}

void DawInstance::removeTrackId(uint32_t trackId) {
    if (project.trackList.validTrackIdx(trackId)) {
        removeTrackImpl(project.trackList[trackId], FLG_TRK_CHANGE_USER);
    }
}

void DawInstance::removeTrackImpl(track_t* track, int flags) {
    if (tls.mainCtrl) {
        tls.mainCtrl->getPluginCtr()->hideTrack(track->audio);
        // TODO: handle plugins correctly, right now they remain loaded in pluginhost
        if (tls.mainCtrl->clipView.gui && tls.mainCtrl->clipView.gui->m_track == track) {
            tls.mainCtrl->clipView.set(nullptr);
        }
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
    tls.host->onTrackLayoutChange();
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
            setEditClip(nullptr);
            break;
        }
    }
    resetMouseContext();
}

void DawInstance::setTempo(int32_t _tempo100) {
    playThread.call([this, _tempo100]() {
        projectGlobals.tempo100 = CLAMP_I(_tempo100, 100, 99900);
        auto sr = daw_tls::getTls().host->m_sampleFormatInternal.sampleRate;
        for (track_t* t : project.trackList) {
            t->updateAudioClipLengths(projectGlobals.tempo100, sr, sr);
        }
    }, true);
}

void MainCtrl::destroy() {
    auto& settings = daw_tls::getSettings();
    settings.wndMain.dens = grid.grid_dens;
    {
        ThreadLock lock = daw.playThread.lockThread();
        //TODO: MultiLogger::removeLogger is not thread safe. This will eventually cause a race condition 
        // and a crash since not all threads and modules are synchronized here (just playthread and workerthreads)
        getMultiLogger().removeLogger(statusbarLogger.get());
        daw.unloadProject();
    }
    view = nullptr;
    DawCtrl::destroy();
    daw.destroy();
}

void CompanionCtrl::destroy() {
    auto& settings = daw_tls::getSettings();
    settings.wndCompanion.dens = grid.grid_dens;
    view->ctr_tracks2.removeAllTracks();
    view = nullptr;
    DawCtrl::destroy();
}

void CompanionCtrl::onPluginsChanged() {
    view->ctr_nodes.reset();
    view->ctr_nodes.refresh();
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
    // view->ctr_tracks2.layout();
}

void MainCtrl::layoutView() {
    // view->ctr_tracks.layout();
}

bool CompanionCtrl::isZooming() {
    return guiCaptured == &view->ctr_tracks2.trackTimeline;
}

void CompanionCtrl::fixCursor() {
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

void MainCtrl::setStatusText(const String& s, GuiColor::constant_t color) {
    view->statusbar.setTitle(s, color);
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
    dbgassert(0);
}

void MainCtrl::setEditClip(gui_clip* gclip) {
    view->ctr_clipeditor.storeLayout();
    clipView.set(gclip);
    view->ctr_clipeditor.showEditClip();
    view->ctr_clipeditorview.resetCache();
}

void CompanionCtrl::setEditClip(gui_clip* gclip) {
    view->ctr_clipeditor.storeLayout();
    clipView.set(gclip);
    view->ctr_clipeditor.showEditClip();
}
void MainCtrl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    DawCtrl::render(nanovgCtxt, x, y, w, h, ratio);
    daw_tls::tlsinstance& tls = daw_tls::getTls();
    Profiling::profilingCommitStats(this, 0, tls.runtime->renderStats);
    tls.runtime->prevRenderStats = tls.runtime->renderStats;
    tls.runtime->renderStats     = {};
}
void DawCtrl::prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {

    auto& renderStats = daw_tls::getTls().runtime->renderStats;

    renderStats.playThreadLockCount = 0;
    renderStats.clipsRendered       = 0;
    renderStats.notesRendered       = 0;
    //log_printf("prerender %d\n", seqthreads::getCurrentThreadId());

    hires_timer_t timer;
    for (guictr_base* ctr : containers) {
        ctr->prerender(nanovgCtxt);
    }
    renderStats.timePrerender = timer.getTime();

    // auto tmNow = getTimeMillis();
    //if (tmNow - tmLastRenderUpdatesMs >= 1000)
    //if (tmLastRenderUpdatesMs++%2==0)
    {
        timer.reset();

        int nUpdates = waveformRenderer->renderUpdates(nanovgCtxt, 0);
        if (nUpdates) {
            //tmLastRenderUpdatesMs = tmNow;
        }
        renderStats.numWaveFormsRendered += nUpdates;
        renderStats.timeUpdateWaveforms = timer.getTime();
        if (nUpdates > 15 || renderStats.timeUpdateWaveforms > 20 * 1000) {
            log_printf("%d updates took %zd\n", nUpdates, renderStats.timeUpdateWaveforms);
            auto timings = waveformRenderer->getTimings();
            log_lf(Log::L_DEBUG, "waveform.tmPassed\t\t%zd\n", timings.tmPassed);
            log_lf(Log::L_DEBUG, "waveform.tmProcessInputQ\t%zd\n", timings.tmProcessInputQ);
            log_lf(Log::L_DEBUG, "waveform.tmFindSimiliar\t%zd\n", timings.tmFindSimiliar);
            log_lf(Log::L_DEBUG, "waveform.tmFindSpot\t\t%zd\n", timings.tmFindSpot);
            log_lf(Log::L_DEBUG, "waveform.tmTesselate\t\t%zd\n", timings.tmTesselate);
            log_lf(Log::L_DEBUG, "waveform.tmBakePaths\t\t%zd\n", timings.tmBakePaths);
            log_lf(Log::L_DEBUG, "waveform.tmDrawGL\t\t%zd\n", timings.tmDrawGL);
            log_lf(Log::L_DEBUG, "waveform.comparisonsA\t%zd\n", timings.comparisonsA);
            log_lf(Log::L_DEBUG, "waveform.comparisonsB\t%zd\n", timings.comparisonsB);
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
    /* auto daw = DawInstance::get();
    seqthreads::thread_base* thread = daw->getPlayThread();
    if (thread && seqthreads::getCurrentThreadId() == thread->getThreadId()) {
        host_processing_stats_t processing;
        auto host = daw->getHost();
        host->getProcessingStats(processing);
        if (processing.pluginId) {
            effectbase* eff = daw->getPluginManager()->getPluginById(processing.pluginId);
            if (eff) {
                log_printf("Crash was most likely caused by %s\n", StringAsCStr(eff->getName()));
            }
        }
    } */
    return 0;
}

int32_t project_controller_t::tickToSamples(tick_t ticks) {
    auto host = DawInstance::get()->getHost();
    dbgassert(host);
    return tickToSampleConvert<int32_t, roundmode::round>(ticks, projectGlobals->tempo100, host->m_sampleFormatInternal.sampleRate);
}

tick_t project_controller_t::samplesToTicks(int32_t sample) {
    auto host = DawInstance::get()->getHost();
    dbgassert(host);
    return sampleToTickConvert<tick_t, roundmode::round>(sample, projectGlobals->tempo100, host->m_sampleFormatInternal.sampleRate);
}

beatbar16th_t project_controller_t::toBeatBar16th(tick_t tick, bool isRelative) {
    return ::tickToBarBeat16th(tick, projectGlobals->signatureNum, projectGlobals->signatureDenom, isRelative);
}

tick_t project_controller_t::beatBarNthToTick(const beatbar16th_t& beatBarNth, bool isRelative) {
    return ::beatBarNthToTick(beatBarNth, projectGlobals->signatureNum, projectGlobals->signatureDenom, isRelative);
}

void MainCtrl::storeLayout(dawview_layout_t& layout) {
    view->storeLayout(layout);
}

void MainCtrl::loadLayout(const dawview_layout_t& viewLayout) {
    view->loadLayout(viewLayout);
    dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
}

void CompanionCtrl::storeLayout(dawview_layout_t& layout) {
}

void CompanionCtrl::loadLayout(const dawview_layout_t& viewLayout) {
}

void DawInstance::setEmptyClipboard() {
    clipboardType    = CLIPBOARD_NONE;
    clipboardPlugins = std::make_shared<plugin_clipboard_t>();
    clipboardClips   = std::make_shared<clip_clipboard>();
    clipboardNotes   = std::make_shared<notes_clipboard>();
}
