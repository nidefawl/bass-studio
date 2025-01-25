#pragma once
#include <list>
#include <optional>
#include <utility>
#include <vector>
#include <set>
#include "assert_dbg.h"
#include "commands.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "gui/dialog/dialog.hpp"
#include "gui/views/asynctask.hpp"
#include "host/daw/daw_async_task.hpp"
#include "host/project/project.hpp"
#include "saferef.hpp"
#include "types.hpp"
#include <memory>

#include "config.hpp"
#include "math/vec.hpp"
#include "math/seq_math.hpp"
#include "str_util.hpp"
#include "seq_util.hpp"
#include "seq_time.hpp"
#include "basectrl.hpp"
#include "daw.hpp"
#include "window.hpp"
#include "menu.hpp"
#include "mouse.hpp"
#include "keyboard.hpp"
#include "event.hpp"
#include "cursor.hpp"
#include "host/track/track.hpp"
#include "host/clip/clip.hpp"
#include "clipboard.hpp"
#include "note.hpp"
#include "logging.hpp"
#include "host/automation/automation.hpp"
#include "gui/automation/modulation.hpp"
#include "threads/workerthread.hpp"
#include "threads/playbackthread.hpp"
#include "edithistory.hpp"
#include "hires_timer.hpp"
#include "rand.hpp"
#include "dragdrop.hpp"
#include "gui/container/container_dnd_layout.hpp"
#include "gui/controls/draggedfiles.hpp"
#include "buildinfo.h"


struct Menus {
    ngui::Menu file;
    ngui::Menu recent;
    ngui::Menu edit;
    ngui::Menu tools;
    ngui::Menu views;
};
enum view_mode_t {
    TRACK_TIMELINE,
    MIXER,
    NODE_EDITOR,
};
namespace DAW {
enum EditAreaType : uint32_t {
    EDIT_AREA_CLIP_EDITOR,
    EDIT_AREA_PLUGIN_CONTAINER,
    EDIT_AREA_MIXER,
};
enum EditAreaLayout : uint32_t {
    EDIT_AREA_SINGLE = 0,
    EDIT_AREA_SPLIT_VERTICAL,
    EDIT_AREA_SPLIT_HORIZONTAL,
};
}

class DawViewContainers {
protected:
    std::vector<std::shared_ptr<guictr_layout>> topLevelContainers;
public:
    DawViewContainers() = default;
    virtual ~DawViewContainers() = default;
    void add(std::shared_ptr<guictr_layout> container) {
        topLevelContainers.emplace_back(std::move(container));
    }
    template<typename T>
    bool visitLayoutContainers(T&& visitor) {
        for (auto& container : topLevelContainers) {
            if (!visitor(container)) {
                return false;
            }
        }
        return true;
    }

    virtual void addTo(std::vector<guictr_base*>& v) = 0;
    virtual void layout(int32_t winW, int32_t winH) = 0;
    virtual guictr_menubar* getMenu() = 0;
};


class DawCtrl : public AppCtrl {
    Menus menus;
protected:
    waveformrender* waveformRenderer = nullptr;
    hires_timer_t timer;
    seq_rand rand;
    int32_t numCallsWaitEvents = 0;

    track_gui_entry_t* lastHoveredTrack = nullptr;
    int64_t tmLastRenderUpdatesMs       = 0;

    track_t* selectedTrack = nullptr;
    int32_t layoutIndex = 0;
    size_t dawCtrlWindowIndex = 0; // (dawCtrlWindowIndex > 0) == isCompanion()
    std::array<dawview_layout_t, 10> layouts;
    SafeRef<guibase> guiEditModulation;
    size_t statsTickDelay = 0;
    track_gui_manager_t* trackGuis = nullptr;
public:
    std::vector<guictr_base*> viewGuiContainers;
    gui_asyc_progress guiCtrProgress;
    gui_dragged_files guiDraggedFiles;
    gui_notify* guiNotify = nullptr;
    std::vector<guictr_base*> viewAsyncProgress = {&guiCtrProgress};
    std::vector<guictr_base*> viewRender;
    struct ui_modulation_targets_t {
        SafeRef<guibase> target;
        NVGstate state;
    };
    bool bIsContainerRenderPass = false;
    bool getIsContainerRenderPass() const {
        return bIsContainerRenderPass;
    }
    std::vector<ui_modulation_targets_t> uiModulationTargets;
    std::vector<ui_modulation_targets_t>& getUIModulationTargets() {
        return uiModulationTargets;
    }
    const std::vector<guictr_base*>& getRenderContainers() const override { return viewRender; }
    String lastKeyDebug;
    DawViewContainersMain* view = nullptr;
    DawInstance& daw;
    view_mode_t viewMode = view_mode_t::TRACK_TIMELINE;
    DawCtrl(AppCtrl* parent, DawInstance& _daw, int32_t _dawCtrlWindowIndex);
    ~DawCtrl() override = default;
    void updateViewGuiContainers();

    size_t getDawWindowIndex() const {
        return dawCtrlWindowIndex;
    }

    int32_t& getLayoutIndex() {
        return layoutIndex;
    }

    std::array<dawview_layout_t, 10>& getLayouts() {
        return layouts;
    }

    void setLayoutIndex(int32_t index) {
        layoutIndex = index;
    }

    DawInstance* getDaw() {
        return &daw;
    }
    dragdrop_target_indicator_t& getDragDropTarget() {
        return daw.dragdropTarget;
    }
    plugin_selection& getPluginSel() {
        return daw.pluginSel;
    }
    String& getProjectPath() {
        return daw.projectPath;
    }
    String& getProjectDirectory() {
        return daw.lastProjectDirectory;
    }
    WorkerThread* getWorkerThread() {
        return &daw.workerThread;
    }
    ThreadLock lockPlayThread() {
        return daw.playThread.lockThread();
    }
    waveformrender* getWaveformRenderer() {
        return this->waveformRenderer;
    }
    track_t* getSelectedTrack() {
        return selectedTrack;
    }
    void onViewCreated(SPLayoutEntry& ctrEntry);
    void setSelectedTrackEntry(track_gui_entry_t* trackEntry);
    void setSelectedTrack(track_t* track);
    void revealPlugin(effectbase* effect);
    void focusChanged(guibase* oldFocused, guibase* newFocused) override;
    void resetMouseContext() override;
    bool filesDropMove(ivec2 pos, KeyboardMods kbmods) override;
    bool filesDropBegin(const std::vector<String>& files, ivec2 pos, KeyboardMods kbmods, bool bIsExternal) override;
    void filesDropCancel() override;
    bool filesDropFinal(ivec2 pos, KeyboardMods kbmods) override;
    void mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) override;
    bool menuCommand(const menucmd_t& command) override;
    void updateMenubar() override;
    void onTick() override;
    void destroy() override;
    void onPreDestroy() override;
    void relayout() override {
        BaseCtrl::relayout();
    }
    void relayout(int32_t w, int32_t h) override;
    bool processGlobalKeyevent(const KeyEvent& event) override;
    bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) override;
    bool mouseDownPre() override;
    void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override;


    void initApp(const std::vector<String>& args) override { };
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    size_t getAppWindowIndex() override { return dawCtrlWindowIndex; }
    bool onWindowCloseRequest() override;
    void startApp() override { };

    virtual DAW::Cursor& getCursor()              = 0;
    void setupView();
    void layoutView(int32_t w, int32_t h);
    void setSingleClip(clip_t* _clip);
    void setEditorSelection(clip_t* _clip, const editor_view_selection_t& clipboardView);
    void updateClipViews(clip_t* notifyClip);
    void updateClipViewsAndCursor(clip_t* notifyClip, clip_cursor_t cursor);
    void resetClipViews();
    void onTrackContentRemoved(track_gui_entry_t& e);
    void onTrackMixerRemoved(track_gui_entry_t& e);
    bool visitGuis(gui_type type, std::function<void(guictr_base*)> func);
    void refreshAllUserlibraryBrowsers();
    void updateVisibleTrackContents();

    virtual void setStatusText(String s) {
    }

    bool isCompanion() const {
        return dawCtrlWindowIndex > 0;
    }

    virtual void resetAutomationContext() {
    }

    void addTrackToView(track_t* track, int flags);
    void removeTrackFromView(track_t* track, int flags);

    void updateZoomLevel(float f) override;
    void onPostUnloadProject();
    void layoutView();
    void fixCursor();
    bool isZooming();
    bool isClipEditorVisible();
    bool isPluginViewVisible();
    void showPluginView();
    void showClipEditor();
    void showMixer();
    void setAsyncTask(DAW::async_task_t* task);

    view_mode_t getViewMode() const;
    void setViewMode(view_mode_t mode);
    void toggleViewModeEditArea();
    void setEditAreaLayout(DAW::EditAreaLayout layout);
    void setEditAreaType(DAW::EditAreaType editAreaType);
    void storeLayout(dawview_layout_t& layout);
    void loadLayout(const dawview_layout_t& viewLayout);
    void loadTrackLayouts(const std::shared_ptr<project_file>& file);
    std::shared_ptr<guictr_layout> replaceContainerWith(guictr_base* ctr,
                                                        std::shared_ptr<guictr_layout> newContainer) override;
    void dragContainerRelayout(drag_ctr_event evt) override;
    std::shared_ptr<guictr_tracks> getTrackContainer();
    std::shared_ptr<guictr_mixers> getMixerContainer();
    std::shared_ptr<guictr_clipeditor> getClipEditor();

    virtual void onPluginSelected();
    bool isGlobalKeybindCodepoint(uint32_t codepoint) override {
        return codepoint == 32 || codepoint == 45 || codepoint == 43;
    }
    DAW::UI::IDraggedModulationSource* getDraggedModulation() {
        auto guiDragged = getGuiDragged();
        if (guiDragged && (guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON
                            || guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_DRAGGED)) {
            return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged);
        }
        return nullptr;
    }
    DAW::UI::IDraggedModulationSource* getEditModulation() {
        auto guiDragged = safeRefGet(guiEditModulation);
        if (guiDragged) {
            if (guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
                return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged);
            }
            if (guiDragged->parent && guiDragged->parent->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
                return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged->parent);
            }
        }
        return nullptr;
    }
    void setEditModulation(const SafeRef<guibase>& ref) {
        guiEditModulation = ref;
    }
    DAW::UI::IDraggedModulationSource* getFocusedModulation() {
        auto guiOver = getGuiOver();
        if (guiOver && guiOver->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
            return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiOver);
        }
        return nullptr;
    }
    std::optional<DAW::modulation_channel_ref> getDraggedModulationRef() {
        auto dragged = getDraggedModulation();
        if (dragged) {
            return dragged->getChannelRef();
        }
        return {};
    }
    
    void renderContainers(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
};
namespace DAW::UI::Modulation {
    bool IsHiglightedModulation(const guibase* gui, automatable_t* at, int32_t paramIdx);
    bool IsEditModulation(const guibase* gui, automatable_t* at, int32_t paramIdx);
};

class ProjectGraphMonitor {
    gui_notify* popupNotifyError = nullptr;
    std::shared_ptr<DAW::processing_graph_t> processingGraph;
    std::shared_ptr<DAW::processing_graph_t> lastWorkingProcGraph;
    bool bWorkingProcessingGraph = false;
    public:
    void onTick(MainCtrl* ctrl);
    gui_notify* getNotifyError() const {
        return popupNotifyError;
    }
};
class MainCtrl final : public DawCtrl {
    friend class DawInstance;
    friend class DawCtrl;
    friend struct DAW::load_project_task;
    String loadProject;
    int loadFlags = 0;
    std::shared_ptr<Logger> statusbarLogger;
    ProjectGraphMonitor graphMonitor;
public:
    static MainCtrl* get();
    explicit MainCtrl(DawInstance& _daw);
    ~MainCtrl() override = default;

    void initApp(const std::vector<String>& args) override;
    void startApp() override;

    const String& getLoadProjectFilePath() {
        return loadProject;
    }
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
    void onTick() override;
    void onFastTick() override;
    bool processGlobalKeyevent(const KeyEvent& event) override;
    void addDebug(String s);
    void setStatusText(String s) override;
    void setStatusText(const String& s, GuiColor::constant_t color);
    void destroy() override;
    void onPreDestroy() override;
    DAW::Cursor& getCursor() override;
    void onChildOverlayWindowClose(window_main*) override;
};

class CompanionCtrl final : public DawCtrl {
    DAW::Cursor cursor;
    DAW::TrackSelection trackSelection;

public:
    explicit CompanionCtrl(AppCtrl* parent, DawInstance& _daw, int32_t windowIndex = 1)
    : DawCtrl(parent, _daw, windowIndex)
    {
        dbgassert(windowIndex > 0);
    }
    ~CompanionCtrl() override = default;
    DAW::Cursor& getCursor() override { return cursor; };
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override {
        return DawCtrl::initAppWindow(window, nanovg);
    }
};
