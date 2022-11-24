#include <algorithm>
#include <archive_entry.h>
#include <archive.h>
#include <cstddef>
#include <ctime>
#include <functional>
#include <GLFW/glfw3.h>
#include <memory>
#include <nanovg.h>
#include <utility>
#include <vector>
#include "appconfig.h"
#include "appsettings.h"
#include "assert_dbg.h"
#include "basectrl.h"
#include "color_util.h"
#include "commands.h"
#include "config.h"
#include "cursor.h"
#include "daw_async_project_load.h"
#include "daw.h"
#include "edithistory.h"
#include "error.h"
#include "event.h"
#include "exceptions.h"
#include "file/projectfile.h"
#include "fileio.h"
#include "fileloader.h"
#include "glheaders.h"
#include "grid.h"
#include "gui/clipeditor/clipeditor.h"
#include "gui/container/container_builder.h"
#include "gui/container/container_dnd_layout.h"
#include "gui/container/container_layout_types.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/button.h"
#include "gui/controls/list.h"
#include "gui/controls/scrollbar.h"
#include "gui/controls/splitter.h"
#include "gui/controls/statusbar.h"
#include "gui/dialog/about.h"
#include "gui/dialog/dialog_io.h"
#include "gui/dialog/dialogs.h"
#include "gui/gui.h"
#include "gui/menu/menu.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/tooltip/tooltip.h"
#include "gui/track/trackcontent.h"
#include "gui/track/trackctr_nodes.h"
#include "gui/track/trackctr.h"
#include "gui/views/controls.h"
#include "gui/views/debugctr.h"
#include "gui/views/notify.h"
#include "gui/views/pluginlist.h"
#include "gui/views/shaderview.h"
#include "guicolors.h"
#include "host/audiocache/audiocache.h"
#include "host/audiohost/audio_host.h"
#include "host/clip/clip.h"
#include "host/daw/daw_async_task.h"
#include "host/daw/mainctrl.h"
#include "host/graph/effect_graph.h"
#include "host/graph/track_graph.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "host/midihost/midi_host.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/project/project.h"
#include "host/track/track_impl.h"
#include "host/track/track.h"
#include "keyboard.h"
#include "logging.h"
#include "math/seq_math.h"
#include "menu.h"
#include "msgbox.h"
#include "note.h"
#include "platform.h"
#include "saferef.h"
#include "seq_time.h"
#include "seq_util.h"
#include "str_util.h"
#include "thread.h"
#include "threads/playbackthread.h"
#include "threads/workerthread.h"
#include "tls.h"
#include "types.h"
#include "util/profiling.h"
#include "wave/waveform_render_impl.h"
#include "window_impl.h"
#include "window.h"

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

class MainCtrlErrorStatusBarLogger final : public Logger {
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

template<typename T, typename Y>
SPLayoutEntry addLayoutEntry(T& t, const std::shared_ptr<Y>& ctr, String title) {
    ctr->setLabel(std::move(title));
    auto entry1 = createGuiCtrLayoutEntry(ctr);
    t->addEntry(entry1);
    return entry1;
}
template<typename T, typename Y>
SPLayoutEntry addLayoutEntryRelayout(BaseCtrl* ctrl, T& t, const std::shared_ptr<Y>& ctr, String title) {
    ctr->setLabel(std::move(title));
    auto entry1 = createGuiCtrLayoutEntry(ctr);
    DropAreaUILayout area(t.get());
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
    return entry1;
}

std::shared_ptr<guictr_layout> makeTabListCtr1(DawCtrl* const dawCtrl) {
    using namespace DAW::UI;
    auto daw = dawCtrl->getDaw();
    auto createContainer = create_ctr_t{daw};
    auto ctr = std::make_shared<guictr_layout>();
    ctr->setLayout(container_layout::TABBED);
    ctr->addEntry(createGuiCtrLayoutEntry(std::make_shared<DAW::DialogSettings::guidialog_settings>(dawCtrl->getDaw())));
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiThemeEditor(createContainer))));
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiHistoryList(createContainer))));
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiObjectProperties(createContainer))));
    ctr->addEntry(createGuiCtrLayoutEntry(std::make_shared<gui_shaderview>()));
    ctr->setActiveEntry(0);
    return ctr;
}

std::shared_ptr<guictr_layout> makeTabListCtr2(DawCtrl* const dawCtrl) {
    using namespace DAW::UI;
    auto daw = dawCtrl->getDaw();
    auto createContainer = create_ctr_t{daw};
    auto ctr = std::make_shared<guictr_layout>();
    ctr->setLayout(container_layout::TABBED);
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiEffectLibrary(createContainer))));
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiPluginsLoadedList(createContainer))));
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiPerformance(createContainer))));
    ctr->addEntry(createGuiCtrLayoutEntry(std::shared_ptr<guictr_base>(makeGuiObjectProperties(createContainer))));
    ctr->setActiveEntry(0);
    return ctr;
}

class DawViewContainersMain final : public DawViewContainers {
    enum SplitterPos : uint32_t {
        LEFT = 0,
        RIGHT,
    };

    DawCtrl* const dawCtrl;

public:
    std::shared_ptr<guictr_layout> ctr_Left;
    std::shared_ptr<guictr_layout> ctr_Center;
    std::shared_ptr<guictr_layout> ctr_Right;
    SPLayoutEntry ctrEntryTracks;
    SPLayoutEntry ctrEntryNodes;
    SPLayoutEntry ctrEntryClipEdit;
    SPLayoutEntry ctrEntryPlugins;
    std::vector<std::shared_ptr<guictr_clipeditor>> vecClipEditors;

    guictr_menubar ctr_menu;
    guictr_daw_controls ctr_tempo;
    guictr_test ctr_test;
    gui_statusbar statusbar;
    guictr_pluginview ctr_pluginview;
    guictr_clipeditorview ctr_clipeditorview;
    std::vector<std::shared_ptr<Splitter>> splitters;
    DAW::EditAreaLayout editAreaLayout = DAW::EditAreaLayout::EDIT_AREA_SINGLE;
    DAW::EditAreaType editAreaType = DAW::EditAreaType::EDIT_AREA_PLUGIN_CONTAINER;

private:
    SPLayoutEntry placeInCenterContainer(GuiContainerTag tag) {
        auto entry = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
        entry->setEntryTag(tag);
        entry->getAsLayoutCtr()->setLayout(container_layout::TABBED);
        switch (ctr_Center->getLayout()) {
            case container_layout::SPLIT_H:
                break;
            case container_layout::SPLIT_V:
            case container_layout::TABBED:
            default:
                if (!ctr_Center->getEntries().empty()) {
                    // create a new SPLIT_H container and replace ctr_Center (by calling dawCtrl)
                    auto newCtrContainer = std::make_shared<guictr_layout>();
                    newCtrContainer->setLayout(container_layout::SPLIT_H);
                    auto cur = ctr_Center;
                    auto replacedCtr = dawCtrl->replaceContainerWith(ctr_Center.get(), newCtrContainer);
                    dbgassert(cur == replacedCtr);
                    dbgassert(ctr_Center == newCtrContainer);
                    auto replacedEntry = createGuiCtrLayoutEntry(replacedCtr);
                    newCtrContainer->addEntry(replacedEntry);
                    newCtrContainer->setLayout(container_layout::SPLIT_H);
                    replacedEntry->updateLabel();
                }
                break;
        }
        switch (tag) {
            case GuiContainerTag::TAG_TAB_TOP:
                ctr_Center->addEntry(entry, -1);
                break;
            case GuiContainerTag::TAG_TAB_BOTTOM:
                ctr_Center->addEntry(entry, -2);
                break;
            default:
                break;
        }
        return entry;
    }

    Splitter* getSplitter(SplitterPos pos) {
        return splitters[pos].get();
    }

    void activateEntry(GuiCtrLayoutEntry* focusCtr) {
        visitLayoutContainers([focusCtr](std::shared_ptr<guictr_layout>& ctr) {
            ctr->activateEntry(focusCtr);
            return true;
        });
    }

public:
    DawViewContainersMain(DawCtrl* const _dawCtrl, ngui::MenuBar& menubar, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, dragdrop_midifile& dragdropclip, int32_t instanceId)
        : dawCtrl(_dawCtrl),
          ctr_Left(std::make_shared<guictr_layout>()),
          ctr_Center(std::make_shared<guictr_layout>()),
          ctr_Right(std::make_shared<guictr_layout>()),
          ctrEntryTracks(createGuiCtrLayoutEntry(std::make_shared<guictr_tracks>(_dawCtrl, _cursor, _trackSelection, _project, _projectGlobals, dragdropclip))),
          ctrEntryNodes(createGuiCtrLayoutEntry(std::make_shared<guictr_nodes_splitview>(_cursor, _project, dragdropclip))),
          ctrEntryPlugins(createGuiCtrLayoutEntry(std::make_shared<guictr_plugins>(instanceId))),
          ctr_menu(menubar),
          ctr_tempo(_project, _projectGlobals),
          ctr_pluginview(),
          ctr_clipeditorview()
    {
        auto spClipEdit = std::make_shared<guictr_clipeditor>();
        ctrEntryClipEdit = createGuiCtrLayoutEntry(spClipEdit);
        vecClipEditors.push_back(spClipEdit);
        add(ctr_Left);
        add(ctr_Center);
        add(ctr_Right);

        auto subctr_tabbed  = makeTabListCtr1(_dawCtrl);
        auto subctr_tabbed2 = makeTabListCtr2(_dawCtrl);

        float splitterDef = 0.15f;
        float splitterMin = 0.08f;
        splitters.push_back(std::make_shared<Splitter>(1, splitterDef)); // left
        splitters.push_back(std::make_shared<Splitter>(1, 1.0f - splitterDef)); // right
        splitters[0]->setMinMax(splitterMin, 0.5f - splitterMin);
        splitters[1]->setMinMax(0.5f + splitterMin, 1.0f - splitterMin);

        subctr_tabbed2->setLabel("Top");
        subctr_tabbed->setLabel("Bottom");
        SPLayoutEntry entry1 = createGuiCtrLayoutEntry(subctr_tabbed2);
        SPLayoutEntry entry2 = createGuiCtrLayoutEntry(subctr_tabbed);
        ctr_Left->setLabel("Left Docker");
        ctr_Right->setLabel("Right Docker");
        ctr_Center->setLabel("Center Docker");
        ctr_Center->setTooltipText("");
        ctr_Right->setLayout(container_layout::SPLIT_H);
        ctr_Right->addEntry(entry1);
        ctr_Right->addEntry(entry2);

        auto ctrCtrTop = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
        auto ctrCtrBottom = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
        ctrCtrTop->setEntryTag(GuiContainerTag::TAG_TAB_TOP);
        ctrCtrBottom->setEntryTag(GuiContainerTag::TAG_TAB_BOTTOM);
        ctrEntryTracks->setEntryTag(GuiContainerTag::TAG_TRACKS);
        ctrEntryNodes->setEntryTag(GuiContainerTag::TAG_NODES);
        ctrEntryClipEdit->setEntryTag(GuiContainerTag::TAG_CLIPEDIT);
        ctrEntryPlugins->setEntryTag(GuiContainerTag::TAG_PLUGINS);


        ctr_tempo.setSnapSides(ivec4(0, 0, 0, 1));
        statusbar.setSnapSides(ivec4(0, 1, 0, 1));
        ctr_clipeditorview.setSnapSides(ivec4(0, 1, 0, 0));
        ctr_pluginview.setSnapSides(ivec4(0, 1, 0, 0));
        // ctr_clipeditor.setSnapSides(ivec4(0, 1, 0, 0));
        // ctr_plugins.setSnapSides(ivec4(0, 1, 0, 0));
        subctr_tabbed2->setSnapSides(ivec4(1, 0, 0, 1));
        ctr_Left->setSnapSides(ivec4(0, 0, 1, 1));
        ctr_Center->setSnapSides(ivec4(0));
        ctr_Right->setSnapSides(ivec4(1, 0, 0, 1));

        subctr_tabbed->setSnapSides(ivec4(1, 0, 0, 0));
    }

    template<typename T>
    bool visitEntries(T&& visitor) {
        for (auto& container : topLevelContainers) {
            if (!container->visitEntries(visitor)) {
                return false;
            }
        }
        if (!ctrEntryTracks->getParentContainer()) {
            if (!visitor(ctrEntryTracks)) {
                return false;
            }
        }
        if (!ctrEntryNodes->getParentContainer()) {
            if (!visitor(ctrEntryNodes)) {
                return false;
            }
        }
        if (!ctrEntryClipEdit->getParentContainer()) {
            if (!visitor(ctrEntryClipEdit)) {
                return false;
            }
        }
        if (!ctrEntryPlugins->getParentContainer()) {
            if (!visitor(ctrEntryPlugins)) {
                return false;
            }
        }
        return true;
    }

    SPLayoutEntry findByTagEntry(GuiContainerTag tag) {
        for (auto& ctr : {ctr_Left.get(), ctr_Right.get(), ctr_Center.get()}) {
            if (auto ctrWithTag = ctr->findByTagEntry(tag)) {
                return ctrWithTag;
            }
        }
        return nullptr;
    }

    SPLayoutEntry findByGuiType(gui_type guitype) {
        for (auto& ctr : {ctr_Left.get(), ctr_Right.get(), ctr_Center.get()}) {
            if (auto ctrWithTag = ctr->findByGuiType(guitype)) {
                return ctrWithTag;
            }
        }
        return nullptr;
    }
    SPLayoutEntry findByTagOrGuiType(GuiContainerTag tag, gui_type guitype) {
        SPLayoutEntry entries;
        visitEntries([&entries, tag, guitype](SPLayoutEntry& entry) {
            if (entry->getGui()->isVisible() && entry->getEntryTag() == tag) {
                entries = entry;
                return false;
            }
            if (entry->getGui()->isVisible() && entry->getGui()->getGuiType() == guitype) {
                entries = entry;
                return false;
            }
            return true;
        });
        return entries;
    }
    std::shared_ptr<guictr_layout> replaceLayoutCtr(guictr_base* ctr, std::shared_ptr<guictr_layout>& newContainer) {
        for (auto& container : topLevelContainers) {
            if (ctr == container.get()) {
                auto copy = container;
                if (ctr_Left.get() == ctr)
                    ctr_Left = newContainer;
                else if (ctr_Right.get() == ctr)
                    ctr_Right = newContainer;
                else if (ctr_Center.get() == ctr)
                {
                    ctr_Center = newContainer;
                    ctr_Center->setHideHandlesWhenLocked(true);
                    ctr_Center->setLabel("Center Docker");
                    copy->setHideHandlesWhenLocked(false);
                    copy->setLabel("Layout");
                }
                container = newContainer;
                copy->parent = nullptr;
                container->parent = nullptr;
                return copy;
            }
        }
        return nullptr;
    }

    void init() {
        ctrEntryTracks->removeEntryFromParent();
        ctrEntryNodes->removeEntryFromParent();
        ctrEntryClipEdit->removeEntryFromParent();
        ctrEntryPlugins->removeEntryFromParent();
        ctr_Center->removeAllEntries();
        ctr_Center->setLayout(container_layout::SPLIT_H);
        auto ctrCtrTop = findByTagEntry(GuiContainerTag::TAG_TAB_TOP);
        if (ctrCtrTop) {
            ctrCtrTop->getAsLayoutCtr()->removeAllEntries();
            ctrCtrTop->removeEntryFromParent();
            ctrCtrTop.reset();
        }
        auto ctrCtrBottom = findByTagEntry(GuiContainerTag::TAG_TAB_BOTTOM);
        if (ctrCtrBottom) {
            ctrCtrBottom->getAsLayoutCtr()->removeAllEntries();
            ctrCtrBottom->removeEntryFromParent();
            ctrCtrBottom.reset();
        }

        ctrCtrTop = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
        ctrCtrTop->setEntryTag(GuiContainerTag::TAG_TAB_TOP);
        ctrCtrTop->getAsLayoutCtr()->setLayout(container_layout::TABBED);
        ctrCtrBottom = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
        ctrCtrBottom->setEntryTag(GuiContainerTag::TAG_TAB_BOTTOM);
        ctrCtrBottom->getAsLayoutCtr()->setLayout(container_layout::TABBED);
        ctr_Center->setHideHandlesWhenLocked(true);

        ctrCtrTop->getAsLayoutCtr()->addEntry(ctrEntryTracks);
        ctrCtrTop->getAsLayoutCtr()->addEntry(ctrEntryNodes);
        ctrCtrBottom->getAsLayoutCtr()->addEntry(ctrEntryClipEdit);
        ctrCtrBottom->getAsLayoutCtr()->addEntry(ctrEntryPlugins);
        ctr_Center->addEntry(ctrCtrTop);
        ctr_Center->addEntry(ctrCtrBottom);
        editAreaLayout = DAW::EditAreaLayout::EDIT_AREA_SPLIT_HORIZONTAL;
        editAreaType = DAW::EditAreaType::EDIT_AREA_PLUGIN_CONTAINER;
        setEditAreaLayout(DAW::EditAreaLayout::EDIT_AREA_SINGLE);
        setEditAreaType(DAW::EditAreaType::EDIT_AREA_CLIP_EDITOR);
        ctr_Center->postContentChanged();
        auto focusCtr = ctrEntryTracks.get();
        if (dawCtrl->viewMode == NODE_EDITOR) {
            focusCtr = ctrEntryNodes.get();
        }
        activateEntry(focusCtr);
    }

    void destroy() {
        
        auto trackCtr = std::static_pointer_cast<guictr_tracks>(ctrEntryTracks->getSharedGui());
        if (trackCtr) {
            // onRemove is not called for nested layout container entries
            // so we need to call it manually
            trackCtr->removeAllTracks(); 
        }
        auto ctrCtrTop = findByTagEntry(GuiContainerTag::TAG_TAB_TOP);
        if (ctrCtrTop) {
            ctrCtrTop->getAsLayoutCtr()->removeAllEntries();
            ctrCtrTop->removeEntryFromParent();
            ctrCtrTop.reset();
        }
        auto ctrCtrBottom = findByTagEntry(GuiContainerTag::TAG_TAB_BOTTOM);
        if (ctrCtrBottom) {
            ctrCtrBottom->getAsLayoutCtr()->removeAllEntries();
            ctrCtrBottom->removeEntryFromParent();
            ctrCtrBottom.reset();
        }
        ctrEntryTracks->removeEntryFromParent();
        ctrEntryNodes->removeEntryFromParent();
        ctrEntryClipEdit->removeEntryFromParent();
        ctrEntryPlugins->removeEntryFromParent();
        ctr_Left->removeAllEntries();
        ctr_Center->removeAllEntries();
        ctr_Right->removeAllEntries();
    }

    guictr_menubar* getMenu() override {
        return &ctr_menu;
    }

    void setViewMode(view_mode_t mode) {
        if (dawCtrl->viewMode == mode)
            return;
        dawCtrl->viewMode = mode;

        bool bContentChanged = false;
        SPLayoutEntry spShowEntry;
        int insertPos = -1;
        switch (mode) {
            case TRACK_TIMELINE:
            case MIXER:
                spShowEntry = ctrEntryTracks;
                break;
            case NODE_EDITOR:
                spShowEntry = ctrEntryNodes;
                insertPos = -2;
                break;
        }
        if (!spShowEntry)
            return;
        if (!spShowEntry->getParentContainer()) {
            auto ctrTabTop = findByTagEntry(GuiContainerTag::TAG_TAB_TOP);
            if (!ctrTabTop) {
                ctrTabTop = placeInCenterContainer(GuiContainerTag::TAG_TAB_TOP);
                bContentChanged = true;
            }
            if (ctrTabTop) {
                ctrTabTop->getAsLayoutCtr()->addEntry(spShowEntry, insertPos);
                bContentChanged = true;
            }
        }

        if (spShowEntry->getParentContainer() && spShowEntry->getGui()->parent) {
            activateEntry(spShowEntry.get());
            dawCtrl->focusGui(spShowEntry->getGui());
        }

        if (bContentChanged) {
            dawCtrl->dragContainerRelayout({ BaseCtrl::drag_ctr_event_type::DRAG_END });
            dawCtrl->relayout();
            dawCtrl->updateVisibleTrackContents();
        }
    }

    void setEditAreaLayout(DAW::EditAreaLayout layout) {
        editAreaLayout = layout;

        auto ctrTabBottom = findByTagEntry(GuiContainerTag::TAG_TAB_BOTTOM);
        switch (editAreaLayout) {
            case DAW::EDIT_AREA_SINGLE:
                if (ctrTabBottom) {
                    ctrTabBottom->getAsLayoutCtr()->setLayout(container_layout::TABBED);
                }
                switch (editAreaType) {
                    case DAW::EditAreaType::EDIT_AREA_PLUGIN_CONTAINER:
                        if (ctrEntryPlugins->getParentContainer()) {
                            activateEntry(ctrEntryPlugins.get());
                        }
                        break;
                    case DAW::EditAreaType::EDIT_AREA_CLIP_EDITOR:
                        if (ctrEntryClipEdit->getParentContainer()) {
                            activateEntry(ctrEntryClipEdit.get());
                        }
                        break;
                }
                if (ctrTabBottom) {
                    ctrTabBottom->getAsLayoutCtr()->setLayout(container_layout::TABBED);
                    ctrTabBottom->getGui()->layout();
                }
                break;
            case DAW::EDIT_AREA_SPLIT_VERTICAL:
                if (ctrTabBottom) {
                    ctrTabBottom->getAsLayoutCtr()->setLayout(container_layout::SPLIT_V);
                    ctrTabBottom->getGui()->layout();
                }
                break;
            case DAW::EDIT_AREA_SPLIT_HORIZONTAL:
                if (ctrTabBottom) {
                    ctrTabBottom->getAsLayoutCtr()->setLayout(container_layout::SPLIT_H);
                    ctrTabBottom->getGui()->layout();
                }
                break;
        }
    }

    void setEditAreaType(DAW::EditAreaType editAreaType) {
        this->editAreaType = editAreaType;

        SPLayoutEntry spShowEntry;
        int insertPos = -1;
        switch (editAreaType) {
            case DAW::EditAreaType::EDIT_AREA_PLUGIN_CONTAINER:
                spShowEntry = ctrEntryPlugins;
                insertPos = -2;
                break;
            case DAW::EditAreaType::EDIT_AREA_CLIP_EDITOR:
                spShowEntry = findActiveClipEditor();
                break;
        }
        if (!spShowEntry)
            return;
        bool bContentChanged = false;
        SPLayoutEntry ctrTabBottom;
        if (!spShowEntry->getParentContainer()) {
            ctrTabBottom = findByTagEntry(GuiContainerTag::TAG_TAB_BOTTOM);
            if (!ctrTabBottom) {
                ctrTabBottom = placeInCenterContainer(GuiContainerTag::TAG_TAB_BOTTOM);
                bContentChanged = true;
            }
            container_layout layouts[] = {container_layout::TABBED, container_layout::SPLIT_V, container_layout::SPLIT_H};
            ctrTabBottom->getAsLayoutCtr()->setLayout(layouts[editAreaLayout]);
            if (ctrTabBottom) {
                ctrTabBottom->getAsLayoutCtr()->addEntry(spShowEntry, insertPos);
                bContentChanged = true;
            }
        }

        if (spShowEntry->getParentContainer() && spShowEntry->getGui()->parent) {
            activateEntry(spShowEntry.get());
            dawCtrl->focusGui(spShowEntry->getGui());
        }

        if (bContentChanged) {
            dawCtrl->dragContainerRelayout({ BaseCtrl::drag_ctr_event_type::DRAG_END });
            dawCtrl->relayout();
        }
    }

    SPLayoutEntry findActiveClipEditor() {
        SPLayoutEntry firstMatch = nullptr;
        visitEntries([&](SPLayoutEntry& entry) {
            if (entry->getEntryTag() == GuiContainerTag::TAG_CLIPEDIT) {
                if (!firstMatch || !firstMatch->isVisible())
                    firstMatch = entry;
                return false;
            }
            return true;
        });
        if (!firstMatch) {
            visitEntries([&](SPLayoutEntry& entry) {
                if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR) {
                    if (!firstMatch || !firstMatch->isVisible())
                        firstMatch = entry;
                    return false;
                }
                return true;
            });
        }
        if (!firstMatch) {
            return ctrEntryClipEdit;
        }
        return firstMatch;
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
            leftSplitter->setScale(leftSplitter->getDefault());
        }
        if (ctr_Right->getEntries().empty()) {
            rightSplitter->setScale(1);
        } else if (rightSplitter->getScale() > rightSplitter->getMax()) {
            rightSplitter->setScale(rightSplitter->getDefault());
        }
        int hTopControls     = 48;
        int heightViewSelect = 60;
        int heightStatusBar = 16;
        int hCenter      = winH - hTopControls - heightViewSelect - heightStatusBar;
        int hContent     = winH - hTopControls - heightStatusBar;
        int widthLeft           = leftSplitter->leftOrTop(winW);
        int widthRight          = rightSplitter->rightOrBottom(winW);
        int widthCenter         = winW - widthLeft - widthRight;

        ctr_Center->size = vec2(widthCenter, hCenter);
        ctr_Center->pos  = vec2(widthLeft, winY + hTopControls);
        ctr_tempo.size          = { winW, hTopControls };
        ctr_clipeditorview.size = { widthCenter/2, heightViewSelect };
        statusbar.size = { winW, heightStatusBar };
        ctr_tempo.pos          = { winX, winY };
        statusbar.pos          = { winX, winBottom - heightStatusBar };
        ctr_Left->pos          = { winX, winY + hTopControls };
        ctr_Left->size         = { widthLeft, hContent };

        leftSplitter->pos    = ivec2(widthLeft - Splitter::SPLITTER_LAYOUT_THICKNESS/2, hTopControls);
        leftSplitter->size   = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, hContent);

        ctr_Right->pos  = { widthLeft + widthCenter, winY + hTopControls };
        ctr_Right->size = { widthRight, hContent };

        rightSplitter->pos  = ivec2(ctr_Right->pos.x - Splitter::SPLITTER_LAYOUT_THICKNESS/2, hTopControls);
        rightSplitter->size = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, hContent);

        rightSplitter->setWindowPosSize(ctr_Left->getLeftTop(), ctr_Right->getRightBottom() - ctr_Left->getLeftTop());
        leftSplitter->setWindowPosSize(ctr_Left->getLeftTop(), ctr_Right->getRightBottom() - ctr_Left->getLeftTop());

        ctr_clipeditorview.pos = { widthLeft, winBottom - heightViewSelect - heightStatusBar };
        ctr_pluginview.pos     = { ctr_clipeditorview.right(), winBottom - heightViewSelect - heightStatusBar };
        ctr_pluginview.size     = { ctr_Center->right()-ctr_pluginview.left(), heightViewSelect };
    }

    void addTo(std::vector<guictr_base*>& v) override {
        for (auto& s : splitters)
            v.push_back(s.get());
        v.push_back(&ctr_tempo);
        v.push_back(&ctr_pluginview);
        v.push_back(&ctr_clipeditorview);
        visitLayoutContainers([&](std::shared_ptr<guictr_layout>& ctr) {
            v.push_back(ctr.get());
            return true;
        });
        v.push_back(&statusbar);
#if USE_GUI_MENU
        v.push_back(&ctr_menu);
#endif
    }

    void resetToDefault() {
        auto leftSplitter  = getSplitter(SplitterPos::LEFT);
        auto rightSplitter = getSplitter(SplitterPos::RIGHT);
        leftSplitter->setScale(leftSplitter->getDefault());
        rightSplitter->setScale(rightSplitter->getDefault());
        ctr_Right->removeAllEntries();
        ctr_Left->removeAllEntries();
        init();
        dbgassert(ctrEntryClipEdit->getEntryTag() == GuiContainerTag::TAG_CLIPEDIT);
        dbgassert(ctrEntryNodes->getEntryTag() == GuiContainerTag::TAG_NODES);
        dbgassert(ctrEntryPlugins->getEntryTag() == GuiContainerTag::TAG_PLUGINS);
        dbgassert(ctrEntryTracks->getEntryTag() == GuiContainerTag::TAG_TRACKS);
        dbgassert(findByTagEntry(GuiContainerTag::TAG_TAB_TOP));
        dbgassert(findByTagEntry(GuiContainerTag::TAG_TAB_BOTTOM));
    }

    void loadLayout(const dawview_layout_t& viewLayout) {
        auto ctrCtrTop = findByTagEntry(GuiContainerTag::TAG_TAB_TOP);
        auto ctrCtrBottom = findByTagEntry(GuiContainerTag::TAG_TAB_BOTTOM);
        if (ctrCtrTop) {
            ctrCtrTop->removeEntryFromParent();
            ctrCtrTop->getAsLayoutCtr()->removeAllEntries();
        } else {
            ctrCtrTop = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
            ctrCtrTop->setEntryTag(GuiContainerTag::TAG_TAB_TOP);
            ctrCtrTop->getAsLayoutCtr()->setLayout(container_layout::TABBED);
        }
        if (ctrCtrBottom) {
            ctrCtrBottom->removeEntryFromParent();
            ctrCtrBottom->getAsLayoutCtr()->removeAllEntries();
        } else {
            ctrCtrBottom = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
            ctrCtrBottom->setEntryTag(GuiContainerTag::TAG_TAB_BOTTOM);
            ctrCtrBottom->getAsLayoutCtr()->setLayout(container_layout::TABBED);
        }
        ctr_Right->removeAllEntries();
        ctr_Left->removeAllEntries();
        ctrEntryTracks->removeEntryFromParent();
        ctrEntryNodes->removeEntryFromParent();
        ctrEntryClipEdit->removeEntryFromParent();
        ctrEntryPlugins->removeEntryFromParent();
        ctr_Center->removeAllEntries();
        ctrEntryPlugins->assertState();
        ctr_Left->assertEntries();
        ctr_Right->assertEntries();
        ctr_Center->assertEntries();
        auto context = ContainerInstanceContext{dawCtrl->getDaw(), dawCtrl, {}};
        dbgassert(ctrEntryClipEdit->getEntryTag() == GuiContainerTag::TAG_CLIPEDIT);
        dbgassert(ctrEntryNodes->getEntryTag() == GuiContainerTag::TAG_NODES);
        dbgassert(ctrEntryPlugins->getEntryTag() == GuiContainerTag::TAG_PLUGINS);
        dbgassert(ctrEntryTracks->getEntryTag() == GuiContainerTag::TAG_TRACKS);
        context.entriesPreconstructed[GuiContainerTag::TAG_TRACKS] = { ctrEntryTracks };
        context.entriesPreconstructed[GuiContainerTag::TAG_NODES] = { ctrEntryNodes };
        context.entriesPreconstructed[GuiContainerTag::TAG_CLIPEDIT] = { ctrEntryClipEdit };
        context.entriesPreconstructed[GuiContainerTag::TAG_PLUGINS] = { ctrEntryPlugins };
        context.entriesPreconstructed[GuiContainerTag::TAG_TAB_TOP] = { ctrCtrTop };
        context.entriesPreconstructed[GuiContainerTag::TAG_TAB_BOTTOM] = { ctrCtrBottom };

        if (viewLayout.left && viewLayout.right) {
            auto& fac = getContainerFactory();
            loadContainerSnapshot(fac, context, ctr_Right.get(), viewLayout.right.get());
            loadContainerSnapshot(fac, context, ctr_Left.get(), viewLayout.left.get());
        }
        if (viewLayout.center) {
            auto& fac = getContainerFactory();
            loadContainerSnapshot(fac, context, ctr_Center.get(), viewLayout.center.get());
        } else {
            init();
        }
        vecClipEditors.clear();
        if (context.entriesConstructed.count(gui_type::CTR_TYPE_CLIPEDITOR)) {
            auto& vecCtrEntries = context.entriesConstructed[gui_type::CTR_TYPE_CLIPEDITOR];
            vecClipEditors.reserve(vecCtrEntries.size());
            for (auto& entry : vecCtrEntries) {
                if (!assert_expr(entry->getGui() && entry->getGui()->getGuiType() == gui_type::CTR_TYPE_CLIPEDITOR))
                    continue;
                vecClipEditors.push_back(std::static_pointer_cast<guictr_clipeditor>(entry->getSharedGui()));
            }
        }
        struct TagGuiType {
            GuiContainerTag tag;
            gui_type type;
            SPLayoutEntry* pSp;
        };
        const std::array<TagGuiType, 4> tagGuiTypes = {
            TagGuiType{GuiContainerTag::TAG_TRACKS, gui_type::CTR_TYPE_TRACKS, &ctrEntryTracks},
            TagGuiType{GuiContainerTag::TAG_NODES, gui_type::CTR_TYPE_NODES, &ctrEntryNodes},
            TagGuiType{GuiContainerTag::TAG_CLIPEDIT, gui_type::CTR_TYPE_CLIPEDITOR, &ctrEntryClipEdit},
            TagGuiType{GuiContainerTag::TAG_PLUGINS, gui_type::CTR_TYPE_PLUGINS, &ctrEntryPlugins},
        };
        for (auto& tagGuiType : tagGuiTypes) {
            auto byTag = findByTagEntry(tagGuiType.tag);
            if (!byTag) {
                auto byGuiType = findByGuiType(tagGuiType.type);
                if (byGuiType) {
                    byGuiType->setEntryTag(tagGuiType.tag);
                    *tagGuiType.pSp = byGuiType;
                }
            }
            dbgassert(!!findByGuiType(tagGuiType.type) == !!findByTagEntry(tagGuiType.tag));
        }

        for (size_t i = 0; i < viewLayout.splitterPositions.size() && i < splitters.size(); i++) {
            splitters[i]->setScaleClamped(viewLayout.splitterPositions[i]);
        }
        ctr_Left->assertEntries();
        ctr_Right->assertEntries();
        ctr_Center->assertEntries();
        ctrEntryPlugins->assertState();
        if (ctrEntryTracks->isVisible()) {
            dawCtrl->viewMode = view_mode_t::TRACK_TIMELINE;
        } else if (ctrEntryNodes->isVisible()) {
            dawCtrl->viewMode = view_mode_t::NODE_EDITOR;
        }
    }

    void storeLayout(dawview_layout_t& layout) {
        dbgassert(ctrEntryClipEdit->getEntryTag() == GuiContainerTag::TAG_CLIPEDIT);
        dbgassert(ctrEntryNodes->getEntryTag() == GuiContainerTag::TAG_NODES);
        dbgassert(ctrEntryPlugins->getEntryTag() == GuiContainerTag::TAG_PLUGINS);
        dbgassert(ctrEntryTracks->getEntryTag() == GuiContainerTag::TAG_TRACKS);
        layout.left  = std::make_shared<guictrlayout_snapshot_t>();
        layout.right = std::make_shared<guictrlayout_snapshot_t>();
        layout.center = std::make_shared<guictrlayout_snapshot_t>();
        storeContainerSnapshot(ctr_Center.get(), layout.center.get());
        storeContainerSnapshot(ctr_Right.get(), layout.right.get());
        storeContainerSnapshot(ctr_Left.get(), layout.left.get());
        layout.splitterPositions.resize(splitters.size());
        for (size_t i = 0; i < splitters.size(); i++) {
            layout.splitterPositions[i] = splitters[i]->getScale();
        }
    }
};

void DawCtrl::setupView() {
    for (size_t i = 1; i < layouts.size(); i++) {
        std::shared_ptr<dawview_layout_t> viewLayout = loadDawViewLayoutSnapshot(StringFormat("data/view%zu.layout", i));
        if (viewLayout) {
            layouts[i] = *viewLayout.get();
        }
    }
    view = new DawViewContainersMain(this, menubar, getCursor(), daw.projectGlobals.trackSelection, daw.project, daw.projectGlobals, daw.dragdropclip, isCompanion() ? 2 : 1);
    view->init();
    view->addTo(this->viewGuiContainers);
    for (guictr_base* ctr : viewGuiContainers) {
        ctr->setControl(this);
        ctr->onAdded();
    }
    for (guictr_base* ctr : viewAsyncProgress) {
        ctr->setControl(this);
        ctr->onAdded();
    }
    updateViewGuiContainers();
    setViewMode(view_mode_t::NODE_EDITOR);
    setViewMode(view_mode_t::TRACK_TIMELINE);
    setEditAreaLayout(DAW::EditAreaLayout::EDIT_AREA_SPLIT_HORIZONTAL);
}

std::shared_ptr<guictr_layout> DawCtrl::replaceContainerWith(guictr_base* ctr, std::shared_ptr<guictr_layout> newContainer) {                         
    std::shared_ptr<guictr_layout> ret = view->replaceLayoutCtr(ctr, newContainer);
    if (ret) {
        newContainer->parent = nullptr;
        newContainer->setControl(this);
        newContainer->onAdded();
        ret->parent = nullptr;
        ret->setControl(nullptr); // TODO: this might give problems
        ret->onRemove();
        viewGuiContainers.clear();
        view->addTo(viewGuiContainers);
        updateViewGuiContainers();
    }
    return ret;
}


void DawCtrl::showPluginView() {
    setEditAreaType(DAW::EditAreaType::EDIT_AREA_PLUGIN_CONTAINER);
}

void DawCtrl::showClipEditor() {
    setEditAreaType(DAW::EditAreaType::EDIT_AREA_CLIP_EDITOR);
}

void DawCtrl::setEditAreaType(DAW::EditAreaType editAreaType) {
    view->setEditAreaType(editAreaType);
    setEditAreaLayout(view->editAreaLayout);
}

void DawCtrl::setEditAreaLayout(DAW::EditAreaLayout layout) {
    view->setEditAreaLayout(layout);
    relayout();
}

void DawCtrl::toggleViewModeEditArea() {
    switch (view->editAreaLayout) {
        case DAW::EDIT_AREA_SINGLE:
            setEditAreaLayout(DAW::EDIT_AREA_SPLIT_HORIZONTAL);
            break;
        case DAW::EDIT_AREA_SPLIT_HORIZONTAL:
            setEditAreaLayout(DAW::EDIT_AREA_SPLIT_VERTICAL);
            break;
        case DAW::EDIT_AREA_SPLIT_VERTICAL:
            setEditAreaLayout(DAW::EDIT_AREA_SINGLE);
            break;
    }
}

void DawCtrl::setViewMode(view_mode_t mode) {
    view->setViewMode(mode);
}


view_mode_t DawCtrl::getViewMode() const {
    return this->viewMode;
}

void DawCtrl::onPluginSelected() {
    view->visitEntries([](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_NODES) {
            guictr_cast<guictr_nodes_splitview>(entry)->onPluginSelected();
        }
        return true;
    });
}

void DawCtrl::setAsyncTask(DAW::async_task_t* task) {
    updateViewGuiContainers();
    relayout();
    updateVisibleTrackContents();
}

bool DawCtrl::isClipEditorVisible() {
    bool bFound = false;
    view->visitEntries([&bFound](SPLayoutEntry& ctr) {
        bFound |= ctr->getType() == gui_type::CTR_TYPE_CLIPEDITOR && ctr->isVisible();
        return !bFound;
    });
    return bFound;
}

bool DawCtrl::isPluginViewVisible() {
    bool bFound = false;
    view->visitEntries([&bFound](SPLayoutEntry& ctr) {
        bFound |= ctr->getType() == gui_type::CTR_TYPE_PLUGINS && ctr->isVisible();
        return !bFound;
    });
    return bFound;
}

void MainCtrl::addDebug(String s) {
}

void DawCtrl::resetMouseContext() {
    BaseCtrl::resetMouseContext();
    if (view) {
        view->visitEntries([](SPLayoutEntry& entry) {
            if (entry->getType() == gui_type::CTR_TYPE_NODES) {
                guictr_cast<guictr_nodes_splitview>(entry)->reset();
            }
            return true;
        });
    }
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

void MainCtrl::onChildOverlayWindowClose(window_main* window) {
    if (daw.onChildOverlayWindowClose(window)) {
        this->mainWindow->closeOverlay(window);
    } else {
        DawCtrl::onChildOverlayWindowClose(window);
    }
}

bool DawCtrl::menuCommand(const menucmd_t& command) {
    switch (command.command) {
        case CMD_GUI_GLOBAL_ZOOM_DECREASE:
            updateZoomLevel(math::max(0.5f, m_scale - 0.05f));
            BaseCtrl::relayout();
            return true;
        case CMD_GUI_GLOBAL_ZOOM_INCREASE:
            updateZoomLevel(math::min(4.0f, m_scale + 0.05f));
            BaseCtrl::relayout();
            return true;
        case CMD_EXPORT_TRACK: {
            auto selTrack = getSelectedTrack();
            if (!selTrack) {
                return true;
            }
            track_snapshot_t snapshot(selTrack, tracksnapshot_store_opts_t::All());
            trackcontainer_snapshot_t trackContainerSnapshot;
            trackContainerSnapshot.tracks.push_back(snapshot);
            String path;
            auto exportDir = daw.getProjectDirectory();
            auto exportFilename = selTrack->name + "." + FILE_TYPES_TRACKSNAPSHOT.types.front().ext;
            if (promptUserFilePath(window, 1, FILE_TYPES_TRACKSNAPSHOT, path, exportDir, exportFilename)) {
                String ext;
                SplitPath(path, nullptr, nullptr, &ext);
                if (ext.empty()) {
                    path += "." + FILE_TYPES_TRACKSNAPSHOT.types.front().ext;
                }
                saveTrackContainer(trackContainerSnapshot, path);
            }
            return true;
        }
        case CMD_CREATE_VIEW: {
                auto guiType = static_cast<gui_type>(command.argInt);
                dbgassert(command.argInt >= 0);
                std::shared_ptr<guictr_base> ctr;
                auto context = ContainerInstanceContext{&daw, this, {}};
                if (makeContainer(context, guiType, ctr)) {
                    auto ctrLayoutLeft = this->view->ctr_Left;
                    auto out = addLayoutEntryRelayout(this, ctrLayoutLeft, ctr, ctr->label);
                    onViewCreated(out);
                }
            }
            return true;
        case CMD_RESET_UI_DEFAULT_LAYOUT: {
            for (auto& dawCtrl : daw.dawCtrls) {
                dawCtrl->closeContextMenu();
                dawCtrl->resetMouseContext();
            }
            view->resetToDefault();
            dragContainerRelayout({ BaseCtrl::drag_ctr_event_type::DRAG_END });
            showClipEditor();
            setViewMode(view_mode_t::TRACK_TIMELINE);
            for (auto& dawCtrl : daw.dawCtrls) {
                dawCtrl->updateZoomLevel(1.0);
                dawCtrl->updateVisibleTrackContents();
            }
            return true;
        }
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

    DawCtrl::relayout();
    updateVisibleTrackContents();

    //TODO: move this out of here
    if (!loadProject.empty()) {
        String file;
        std::swap(file, loadProject);
        daw.loadFile(file, loadFlags);
    } else {
        daw.setEmptyProject();
    }
    auto& layouts = getLayouts();
    // view->storeLayout(layouts[0]);
    view->loadLayout(layouts[0]);
    dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
    DawCtrl::startApp();

    auto& settings = daw_tls::getSettings();

    for (size_t i = 1; i < settings.windowSettings.size() && i < 2; ++i) {
        auto& ws = settings.windowSettings[i];
        if (ws.flags & 1) { // opened
            auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_OPEN_SECOND_WINDOW, {}, int32_t(i)};
            handleGlobalCommand(temp);
        }
    }
}


void DawCtrl::destroy() {
    if (!isOK) {
        return;
    }
    isOK = false;
    if (view) {
        view->destroy();
        delete view;
        view = nullptr;
    }
    delete waveformRenderer;
    waveformRenderer = nullptr;
}

MainCtrl::MainCtrl(DawInstance& _daw) : DawCtrl(nullptr, _daw, 0) {
}

void MainCtrl::initApp(const std::vector<String>& args) {
    auto& settings = daw_tls::getSettings();
    auto pathProjStartup = settings.dawsettings.startupProjectPath;
    if (!pathProjStartup.empty() && FileExists(pathProjStartup)) {
        loadProject = pathProjStartup;
        if (settings.dawsettings.startupLoadDeffered) {
            loadFlags |= DAW::PluginLoadFlags::FLAG_DEFER_LOAD;
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
            loadFlags = DAW::PluginLoadFlags::FLAG_DEFER_LOAD;
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
    menus.file.addCommand(this, GlobalCommandType::CMD_BUNDLE_PROJECT_DIRECTORY);
    menus.file.addCommand(this, GlobalCommandType::CMD_BUNDLE_PROJECT_ZIP);
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
    menus.views.addCommand(this, GlobalCommandType::CMD_RESET_UI_DEFAULT_LAYOUT);
    menus.views.addSeperator();
    auto& mapGuiTypeToStr = getContainerRegistry();
    for (auto& [guiType, name] : mapGuiTypeToStr) {
        if (guiType == gui_type::CTR_TYPE_LAYOUT) {
            continue;
        }
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
#else
    this->view->ctr_menu.updateMenu();
#endif

    auto& settings = daw_tls::getSettings();

    auto* optWindowSettings = settings.windowSettings.size() > this->dawCtrlWindowIndex ? &settings.windowSettings[this->dawCtrlWindowIndex] : nullptr;
    if (optWindowSettings) {
        view->visitEntries([&](SPLayoutEntry& entry) {
            if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
                auto tracks = guictr_cast<guictr_tracks>(entry);
                auto& grid = tracks->getGrid();
                grid.grid_dens = optWindowSettings->dens;
            }
            return true;
        });
    }

    isOK = true;
    return isOK;
}

void DawCtrl::onTick() {
    for (guictr_base* ctr : containers) {
        if (ctr->isVisible())
            ctr->onTick(this);
    }
    for (guictr_base* ctr : containers) {
        if (ctr->isVisible())
            ctr->onIdle();
    }
    //if (rand.rng_rand(100000) == 0) {
    //    throw std::bad_alloc();
    //}
    //log_lf(Log::L_DEBUG, "onTick %d\n", std::this_thread::get_id());
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

void MainCtrl::onFastTick() {
    daw.processTasksMainThread();
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

    auto guiCaptured = getGuiCaptured();
    auto guiDragged = getGuiDragged();
    if (guiDragged && !guiCaptured && guiDragged->isDragMoveable()) {
        int32_t hoverTicks     = 0;
        track_gui_entry_t* tr  = nullptr;
        view->visitEntries([&](SPLayoutEntry& entry) {
            if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
                guictr_tracks* tracksCtr = guictr_cast<guictr_tracks>(entry);
                guictr_base& ctrMixers = tracksCtr->trackControls;
                guictr_base& ctrTrackView = tracksCtr->trackView;
                if (tracksCtr->isVisible()) {
                    ivec2 trackViewLocalPos = toControlsObjectSpace(m_mousePos, tracksCtr);
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
                        tr = DAW::getTrackFromMouse(tracksCtr->guiMgr, posRelative);
                        if (tr && tr == lastHoveredTrack && getSelectedTrack() != tr->track) {
                            hoverTicks = lastHoveredTrackTicks + 1;
                            if (lastHoveredTrackTicks >= 6) {
                                setSelectedTrackEntry(tr);
                                showPluginView();
                                hoverTicks = 0;
                            }
                        }
                        return false;
                    }
                }
            }
            return true;
        });
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

void DawCtrl::onTrackContentRemoved(track_gui_entry_t& e) {
}

void DawCtrl::onTrackMixerRemoved(track_gui_entry_t& e) {
}

void DawCtrl::dragContainerRelayout(drag_ctr_event evt) {
    if (evt.evtType != BaseCtrl::drag_ctr_event_type::DRAG_MOVE) {
        view->visitLayoutContainers([](std::shared_ptr<guictr_layout>& ctr) {
            ctr->postContentChanged();
            ctr->layout();
            return true;
        });
    }
    if (evt.evtType == BaseCtrl::drag_ctr_event_type::DRAG_END) {
        view->visitLayoutContainers([this](std::shared_ptr<guictr_layout>& layoutCtr) {
            if (layoutCtr->getLayout() == container_layout::SOLE
                && layoutCtr->canSimplify()
                && layoutCtr->getEntries().size() == 1
                && layoutCtr->getEntries().front()->getFrameType() == LayoutCtrType::GUICTR_LAYOUT
                && layoutCtr->getEntries().front()->getAsLayoutCtr()->canSimplify()) {
                SPLayoutEntry out;
                layoutCtr->getContainerRef(layoutCtr->getEntries().front().get(), out, true);
                dbgassert(out);
                dbgassert(out->getAsLayoutCtr());
                this->replaceContainerWith(layoutCtr.get(), out->getAsLayoutCtr());
                out->updateLabel();
            }
            return true;
        });
        relayout();
    }
}

void DawCtrl::getTrackContainers(std::vector<guictr_tracks*>& trackContainers) {
    view->visitEntries([&trackContainers](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            trackContainers.push_back(guictr_cast<guictr_tracks>(entry));
        }
        return true;
    });
}

std::shared_ptr<guictr_tracks> DawCtrl::getTrackContainer() {
    auto entry = view->findByTagOrGuiType(GuiContainerTag::TAG_TRACKS, gui_type::CTR_TYPE_TRACKS);
    if (entry) {
        return std::static_pointer_cast<guictr_tracks>(entry->getSharedGui());
    }
    return nullptr;
}

std::shared_ptr<guictr_clipeditor> DawCtrl::getClipEditor() {
    auto entry = view->findByTagOrGuiType(GuiContainerTag::TAG_CLIPEDIT, gui_type::CTR_TYPE_CLIPEDITOR);
    if (entry) {
        return std::static_pointer_cast<guictr_clipeditor>(entry->getSharedGui());
    }
    return nullptr;
}

void DawCtrl::layoutView(int32_t w, int32_t h) {
    w = math::max(640, w);
    h = math::max(480, h);
    view->layout(w, h);

    for (guictr_base* ctr : containers) {
        ctr->layout();
    }
    guiCtrProgress.pos = ivec2(w, h) / 2 - guiCtrProgress.size / 2;
    guiCtrProgress.layout();
}

void DawCtrl::relayout(int32_t w, int32_t h) {
    closeAllAppMenus();
    if (ctxtmenu && !ctxtmenu->isDialog()) {
        closeContextMenu();
    }
    layoutView(w, h);
}

void DawCtrl::setSelectedTrackEntry(track_gui_entry_t* trackEntry) {
    setSelectedTrack(trackEntry ? trackEntry->track : nullptr);
}

void DawCtrl::setSelectedTrack(track_t* track) {
    selectedTrack = track;
    view->visitEntries([&](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_PLUGINS) {
            auto spCtrPlugins = std::static_pointer_cast<guictr_plugins>(entry->getSharedGui());
            spCtrPlugins->showTrack(track ? track->audio : nullptr, spCtrPlugins);
            view->ctr_pluginview.setPluginCtr(entry);
        }
        if (entry->getType() == gui_type::CTR_TYPE_NODES) {
            guictr_cast<guictr_nodes_splitview>(entry)->refresh();
        }
        return true;
    });
}

void DawCtrl::revealPlugin(effectbase* effect) {
    view->visitEntries([&](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_PLUGINS) {
            auto spCtrPlugins = std::static_pointer_cast<guictr_plugins>(entry->getSharedGui());
            spCtrPlugins->scrollToPluginGui(effect);
            spCtrPlugins->onSelected(lastMouseEvent, effect);
        }
        return true;
    });
}

void DawCtrl::addTrackToView(track_t* track, int flags) {
    int32_t nTrackViews =0;
    view->visitEntries([track, flags, &nTrackViews](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            nTrackViews++;
            guictr_cast<guictr_tracks>(entry)->addTrack(track, flags);
        }
        if (entry->getType() == gui_type::CTR_TYPE_NODES) {
            guictr_cast<guictr_nodes_splitview>(entry)->refresh();
        }
        return true;
    });
    dbgassert(nTrackViews);
}

void DawCtrl::removeTrackFromView(track_t* track, int flags) {
    int32_t nTrackViews =0;
    view->visitEntries([track, flags, &nTrackViews](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            nTrackViews++;
            guictr_cast<guictr_tracks>(entry)->removeTrack(track, flags);
        }
        if (entry->getType() == gui_type::CTR_TYPE_NODES) {
            guictr_cast<guictr_nodes_splitview>(entry)->refresh();
        }
        return true;
    });
    dbgassert(nTrackViews);
}

void DawCtrl::onPostUnloadProject() {
    view->visitEntries([](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            auto trackCtr = guictr_cast<guictr_tracks>(entry);
            auto& trackView = trackCtr->trackView;
            trackView.m_resizePreModifyState.reset();
            trackView.action.clipboard.reset();
            trackView.iGuiMgr.reset();
            trackCtr->resetView();
        }
        return true;
    });
}

void DawCtrl::updateVisibleTrackContents() {
    view->visitEntries([](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            auto trackCtr = guictr_cast<guictr_tracks>(entry);
            trackCtr->updateVisibleTracks();
            if (trackCtr->isVisible()) {
                double scrollPixelOffset = trackCtr->getScrollOffsetPixels();
                trackCtr->layout();
                trackCtr->layoutVisibleTracks();
                trackCtr->scrollToPixelOffset(scrollPixelOffset);
            }
        }
        return true;
    });
}

bool DawCtrl::isZooming() {
    auto guiCaptured = getGuiCaptured();
    return guiCaptured && guiCaptured->getGuiType() == gui_type::CTR_TYPE_TRACKS_TIMELINE;
}

void DawCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) {
    daw.dragdropTarget.reset();
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

bool DawCtrl::filesDropBegin(std::vector<String>& files, ivec2 mousepos, KeyboardMods kbmods) {
    log_lf(Log::L_DEBUG, "filesDropBegin %d %d isdragging=%d\n", mousepos.x, mousepos.y, daw.dragdropclip.isLoaded);
    daw.dragdropclip.reset();
    if (!guiDragged.isEmpty() || !guiCaptured.isEmpty()) {
        return false;
    }
    tmpFileDragPaths = files;
    if (files.size()) {
        String path = files.front();
        if (StrEndsWith(path, "." PROJECT_BUNDLE_FILE_EXT)
            || StrEndsWith(path, "." PROJECT_FILE_EXT))
            return true;
        if (StrEndsWith(path, ".wav")) {
            String nameWithoutExt;
            SplitPath(path, nullptr, &nameWithoutExt, nullptr, nullptr);
            audiofile_t* audio = daw.getAudioCache()->loadFile(path, -1, "", nullptr, nullptr);
            if (audio) {
                auto* sample = audio->sample.get();
                if (sample) {
                    clip_t clip;
                    clip.clipType = CLIP_AUDIO;
                    clip.name     = nameWithoutExt;
                    //clip.notes = move(notes);
                    clip.audio.id = audio->id;
                    clip.setLenSamples(sample->nSamples);
                    auto host = daw.getHost();
                    dbgassert(host);
                    clip.setLen(sampleToTickConvert<tick_t, roundmode::round>(sample->nSamples, daw.projectGlobals.tempo100, host->m_sampleFormatInternal.sampleRate));
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
                if (ctr->isVisible() && ctr->mouseHitTest(mousepos, evt)) {
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
    if (!guiDragged.isEmpty() || !guiCaptured.isEmpty()) {
        daw.dragdropclip.reset();
        return false;
    }
    if (!tmpFileDragPaths.empty()) {
        String path = tmpFileDragPaths.front();
        if (StrEndsWith(path, "." PROJECT_BUNDLE_FILE_EXT)
            || StrEndsWith(path, "." PROJECT_FILE_EXT))
        return true;
    }
    if (daw.dragdropclip.isLoaded) {
        daw.dragdropclip.isValidTarget = false;

        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP, kbmods);
        for (guictr_base* ctr : containers) {
            if (ctr->isVisible() && ctr->mouseHitTest(mousepos, evt)) {
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
    explicit clipreset(dragdrop_midifile& _clip) : clip(_clip){};
    ~clipreset() {
        clip.reset();
    }
};

void DawCtrl::filesDropCancel() {
    tmpFileDragPaths.clear();
    daw.dragdropclip.reset();
}

bool DawCtrl::filesDropFinal(std::vector<String>& files, ivec2 mousepos, KeyboardMods kbmods) {
    clipreset rst(daw.dragdropclip);
    if (!guiDragged.isEmpty() || !guiCaptured.isEmpty()) {
        return false;
    }
    if (daw.dragdropclip.isLoaded && daw.dragdropclip.isValidTarget) {
        log_lf(Log::L_DEBUG, "filesDropFinal %d %d isdragging=%d\n", mousepos.x, mousepos.y, daw.dragdropclip.isLoaded);
        MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP, kbmods);
        for (guictr_base* ctr : containers) {
            if (ctr->isVisible() && ctr->mouseHitTest(mousepos, evt)) {
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
    if (files.size()) {
        tmpFileDragPaths.clear();
        String path = files.front();
        if (StrEndsWith(path, "." PROJECT_BUNDLE_FILE_EXT)
            || StrEndsWith(path, "." PROJECT_FILE_EXT)) {
            daw.tls.mainCtrl->loadProject = path;
            return true;
        }
    }
    return false;
}
DAW::async_task_t* createTestTask();
bool MainCtrl::processGlobalKeyevent(const KeyEvent& event) {
    if (event.type == KeyboardState::K_PRESS) {
        if (!event.cmd && event.keyCode == KeyboardKey::DAW_KB_T) {
            daw.setAsyncTask(createTestTask());
            return true;
        }
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
        case CMD_SOLO: {
            auto selTrack = getSelectedTrack();
            if (selTrack && selTrack->audio && kevt.type == KeyboardState::K_PRESS) {
                auto lock = daw.lockPlayThread();
                bool isSolo = (selTrack->audio->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE;
                if (!isShift(kevt.mods)) {
                    daw.unsoloAll();
                }
                if (!isSolo) {
                    selTrack->audio->flags |= audiostageflags_t::SOLO;
                } else {
                    selTrack->audio->flags &= ~audiostageflags_t::SOLO;
                }
                DAW::updateSoloFlag(daw.tls.host, &daw.project, daw.getTracks().getAllTracksFlatVecRef());
            }
            return true;
        }
        case CMD_SWITCH_LAYOUT: {
            if (kevt.type == KeyboardState::K_PRESS) {
                auto& layouts = getLayouts();
                if ((kevt.mods & KB_MOD_SHIFT) == kevt.mods && ctxt.argInt0 >= 0 && ctxt.argInt0 < CtrSize(layouts)) {
                    auto index = ctxt.argInt0 % CtrSize(layouts);
                    bool store    = (kevt.mods & KB_MOD_SHIFT);
                    if (store) {
                        view->storeLayout(layouts[index]);
                        saveDawViewLayoutSnapshot(layouts[index], StringFormat("data/view%d.layout", index));
                    } else {
                        if (this->layoutIndex >= 0 && this->layoutIndex < CtrSize(layouts)) {
                            view->storeLayout(layouts[this->layoutIndex]);
                        }
                        this->layoutIndex = index;
                        loadLayout(layouts[index]);
                    }
                    return true;
                }
            }
            return true;
        }
        case CMD_SWITCH_VIEW: {
            if (kevt.type != KeyboardState::K_RELEASE) {
                auto newMode = view_mode_t::NODE_EDITOR;
                if (ctxt.argInt0 < 0) {
                    if (this->viewMode == view_mode_t::TRACK_TIMELINE) {
                        newMode = view_mode_t::NODE_EDITOR;
                    } else {
                        newMode = view_mode_t::TRACK_TIMELINE;
                    }
                } else {
                    newMode = static_cast<view_mode_t>(ctxt.argInt0);
                }
                this->setViewMode(newMode);
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
    bool bHandled = false;
    view->visitEntries([&ctxt, &bHandled](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS
            && guictr_cast<guictr_tracks>(entry)->handleEditorCommand(ctxt)) {  
            bHandled = true;
            return false;
        }
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR
            && guictr_cast<guictr_clipeditor>(entry)->handleEditorCommand(ctxt)) {  
            bHandled = true;
            return false;
        }
        return true;
    });
    if (bHandled) {
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


bool DawCtrl::mouseDownPre() {
    daw.dragdropclip.reset();
    if (this->ctxtmenu && this->ctxtmenu->isDialog()) {
        return false;
    }
    closeAllContextMenus();
    return true;
}

void DawCtrl::onPreDestroy() {
    auto ctrTracks = getTrackContainer();
    //TODO: layout settings should be handled on editor container level
    if (ctrTracks) {
        auto& settings = daw_tls::getSettings();
        while (settings.windowSettings.size() <= dawCtrlWindowIndex) {
            settings.windowSettings.push_back({});
        }
        settings.windowSettings[dawCtrlWindowIndex].dens = ctrTracks->getGrid().grid_dens;
    }
    waveformRenderer->destroy();
}
void MainCtrl::onPreDestroy() {
    {
        ThreadLock lock = daw.playThread.lockThread();
        //TODO: MultiLogger::removeLogger is not thread safe. This will eventually cause a race condition 
        // and a crash since not all threads and modules are synchronized here (just playthread and workerthreads)
        getMultiLogger().removeLogger(statusbarLogger.get());
        daw.unloadProject();
    }
    DawCtrl::onPreDestroy();
    daw.onPreDestroy();
}

void MainCtrl::destroy() {
    DawCtrl::destroy();
    daw.destroy();
}

void CompanionCtrl::destroy() {
    {
        auto ctrTracks = getTrackContainer();
        //TODO: layout settings should be handled on editor container level
        if (ctrTracks) {
            auto& settings = daw_tls::getSettings();
            while (settings.windowSettings.size() <= dawCtrlWindowIndex) {
                settings.windowSettings.push_back({});
            }
            settings.windowSettings[dawCtrlWindowIndex].dens = ctrTracks->getGrid().grid_dens;
        }
        ctrTracks.reset();
    }
    DawCtrl::destroy();
}

void DawCtrl::layoutView() {
}

void DawCtrl::fixCursor() {
    // auto& cursor = getCursor();
    // auto& guiMgr = view->ctr_tracks2.guiMgr;
    // if (cursor.isSubtrackSelection() && guiMgr.validTrackIdx(cursor.cursorTrack)) {
    //     const track_gui_entry_t* tr = guiMgr.at(cursor.cursorTrack);
    //     cursor.fixCursorSubRange(tr->subtracks.size());
    // } else {
    //     cursor.fixCursorTrackRange(guiMgr.getTracksVisibleFlat().size());
    // }
}

DAW::Cursor& MainCtrl::getCursor() {
    return daw.projectGlobals.cursor;
}

void MainCtrl::setStatusText(const String& s, GuiColor::constant_t color) {
    view->statusbar.setTitle(s, color);
}

void MainCtrl::setStatusText(String s) {
    view->statusbar.setTitle(s);
}

void DawCtrl::setSingleClip(clip_t* clip) {
    view->ctr_clipeditorview.resetCache();
    view->visitEntries([clip](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR) {  
            auto clipEditor = guictr_cast<guictr_clipeditor>(entry);
            clipEditor->setSingleClip(clip);
        }
        return true;
    });
}

void DawCtrl::setEditorSelection(clip_t* clip, const editor_view_selection_t& clipboardView) {
    view->ctr_clipeditorview.resetCache();
    view->visitEntries([clip, &clipboardView](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR) {  
            auto clipEditor = guictr_cast<guictr_clipeditor>(entry);
            clipEditor->setEditorSelection(clip, clipboardView);
        }
        return true;
    });
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
    for (guictr_base* ctr : getRenderContainers()) {
        if (ctr->isVisible()) {
            ctr->prerender(nanovgCtxt);
        }
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
        if (nUpdates > 15 || renderStats.timeUpdateWaveforms > 20L * 1000) {
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

bool clip_ref_t::isValid() const {
    if (m_clip && m_project) {
        if (!m_project->trackList.validTrack(m_track)) {
            return false;
        }
        if (!m_track->getClips().hasClip(m_clip)) {
            return false;
        }
        return true;
    }
    return false;
}
bool clip_ref_t::isClipValid(const clip_t* clip) const {
    if (m_clip && m_project) {
        if (!m_project->trackList.validTrack(m_track)) {
            return false;
        }
        if (!m_track->getClips().hasClip(clip)) {
            return false;
        }
        return true;
    }
    return false;
}
bool clip_ref_t::isTrackValid(const track_t* track) const {
    if (m_clip && m_project) {
        if (!m_project->trackList.validTrack(track)) {
            return false;
        }
        return true;
    }
    return false;
}

bool clip_ref_t::isValidUpdate() {
    bool b = isValid();
    if (m_clip && !b) {
        m_project = nullptr;
        m_track   = nullptr;
        m_clip    = nullptr;
    }
    return b;
}


track_t* clip_ref_t::track() const {
    if (!isValid()) {
        return nullptr;
    }
    return this->m_track;
}

clip_t* clip_ref_t::clip() const {
    if (!isValid()) {
        return nullptr;
    }
    return this->m_clip;
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

beatbar16th_t project_controller_t::toBeatBar16th(tick_t tick, bool isRelative) {
    return ::tickToBarBeat16th(tick, projectGlobals->signatureNum, projectGlobals->signatureDenom, isRelative);
}

tick_t project_controller_t::beatBarNthToTick(const beatbar16th_t& beatBarNth, bool isRelative) {
    return ::beatBarNthToTick(beatBarNth, projectGlobals->signatureNum, projectGlobals->signatureDenom, isRelative);
}

void DawCtrl::updateClipViewsAndCursor(clip_t* notifyClip, clip_cursor_t cursor) {
    view->visitEntries([notifyClip, &cursor](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR) {  
            auto clipEditor = guictr_cast<guictr_clipeditor>(entry);
            auto& view = clipEditor->getClipView();
            if (view.contains(notifyClip)) {
                view.m_cursor = cursor;
                view.copySelectedNoteList();
                view.updateNotePitches(false);
                clipEditor->updateClipViewReferences();
            }
        }
        return true;
    });
}

void DawCtrl::updateClipViews(clip_t* notifyClip) {
    view->visitEntries([notifyClip](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR) {  
            auto clipEditor = guictr_cast<guictr_clipeditor>(entry);
            auto& view = clipEditor->getClipView();
            if (view.contains(notifyClip)) {
                view.copySelectedNoteList();
                view.updateNotePitches(false);
                clipEditor->updateClipViewReferences();
            }
        }
        return true;
    });
}

void DawCtrl::resetClipViews() {
    // auto countVec = view->vecClipEditors.size();
    for (auto& clipEditor : view->vecClipEditors) {
        clipEditor->resetClipView();
    }
    /* size_t countVisit = 0;
    view->visitEntries([&countVisit](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR)
            countVisit++;
        return true;
    });
    if (countVec != countVisit) {
        log_lf(Log::L_WARN, "countVec=%zu, countVisit=%zu\n", countVec, countVisit);
    } */
}

void DawCtrl::storeLayout(dawview_layout_t& layout) {
    view->storeLayout(layout);
}

void DawCtrl::loadLayout(const dawview_layout_t& viewLayout) {
    closeContextMenu();
    resetMouseContext();
    view->loadLayout(viewLayout);
    dragContainerRelayout(BaseCtrl::drag_ctr_event{ BaseCtrl::drag_ctr_event_type::DRAG_END });
    updateVisibleTrackContents();
    relayout();
}

void DawCtrl::loadTrackLayouts(const std::shared_ptr<project_file>& file) {
    view->visitEntries([f = file.get()](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            auto trackCtr = guictr_cast<guictr_tracks>(entry);
            trackCtr->getGrid().setLayout(f->layout.layoutGrid);
            trackCtr->loadTrackLayouts(f->project.trackCtr);
            trackCtr->loadTrackLayouts(f->project.trackReturnCtr);
            trackCtr->loadTrackLayouts(f->project.trackMasterCtr);
            trackCtr->setScrollOffset(f->layout.scrollOffsetX);
        }
        return true;
    });
}

void DawCtrl::updateZoomLevel(float f) {
    AppCtrl::updateZoomLevel(f);
    if (view) {
        view->ctr_tempo.onGlobalZoomChanged();
        while (daw.tls.settings->windowSettings.size() <= dawCtrlWindowIndex) {
            daw.tls.settings->windowSettings.emplace_back();
        }
        daw.tls.settings->windowSettings[dawCtrlWindowIndex].zoom = f;
    }
}

void DawCtrl::updateViewGuiContainers() {
    viewRender = viewGuiContainers;
    if (daw.getAsyncTask()) {
        resetMouseContext();
        viewRender.push_back(&guiCtrProgress);
        containers = viewAsyncProgress;
        return;
    }
    containers = viewGuiContainers;
}

namespace DAW {

void load_project_task::run() {
    switch (m_state) {
    case state::idle:
        m_state = state::running;
        break;
    case state::running: {
        project_file* file = projectToLoad->projectfile.get();
        AppWndProc_enableBlockReentrant();
        try {
            switch (step) {
                case 0:
                    daw->loadProject0(projectToLoad->projectfile);
                    if ((projectToLoad->loadflags & FLAG_DEFER_LOAD) == 0) {
                        daw->getHost()->getDeferredEffects(pluginsDeferred);
                        numSubsteps = CtrSize(pluginsDeferred);
                        taskDesc = StringFormat("Load %d Plugins", numSubsteps);
                    }
                    step++;
                break;
                case 1:
                    if ((projectToLoad->loadflags & FLAG_DEFER_LOAD) == 0) {
                        if (substep < numSubsteps) {
                            auto* plugin = pluginsDeferred[substep];
                            progressDesc = "Load Plugin " + plugin->getName();
                            effectbase* pluginLoaded = nullptr;
                            daw->getHost()->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &pluginLoaded);
                            substep++;
                        }
                    }
                    if (substep == numSubsteps) {
                        auto cache = daw->getAudioCache();
                        cache->unloadAll();
                        struct archive* ar = nullptr;
                        if (daw->projectFileType == PROJECT_FILETYPE_BUNDLE && !file->path.empty()) {
                            ar = archive_read_new();
                            archive_read_support_filter_all(ar);
                            archive_read_support_format_all(ar);
                            archive_read_open_filename(ar, StringAsCStr(file->path), 10240);
                        }
                        this->sampleLoader = std::make_shared<DAW::samplefile_index_incremental_loader_t>(cache, ar, file->sampleFileIndex, daw->lastProjectDirectory);

                        taskDesc = StringFormat("Load %zu Samples", file->sampleFileIndex.list.size());
                        step++;
                        substep = 0;
                        numSubsteps = 0;
                        // while (!loader.isFinished()) {
                        //     log_lf(Log::L_ERROR, "1Loading sample %s\n", StringAsCStr(loader.curFileName));
                        //     loader.loadSingleStep();
                        //     log_lf(Log::L_ERROR, "2Loading sample %s\n", StringAsCStr(loader.curFileName));
                        // }
                    }
                break;
                case 2: {
                    if (sampleLoader) {
                        sampleLoader->loadSingleStep();
                        progressDesc = sampleLoader->curFileName;
                        if (sampleLoader->isFinished()) {
                            sampleLoader.reset();
                            step++;
                        }
                    }
                    break;
                case 3:
                    progressDesc = "Finalize";
                    for (track_t* tr : daw->project.trackList) {
                        tr->getStage()->pluginsChanged();
                    }
                    daw->getHost()->onTrackLayoutChange();
                    /** load layout data */
                    for (auto& dawCtrl : daw->dawCtrls) {
                        auto index = dawCtrl->getDawWindowIndex();
                        if (daw->layoutsFromProjectFile.size() > index) {
                            auto& layout = daw->layoutsFromProjectFile[index];
                            dawCtrl->setLayoutIndex(0);
                            dawCtrl->loadLayout(layout);
                            dawCtrl->getLayouts()[0] = layout;
                        }
                        dawCtrl->view->visitEntries([f = file](SPLayoutEntry& entry) {
                            if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
                                auto trackCtr = guictr_cast<guictr_tracks>(entry);
                                trackCtr->getGrid().setLayout(f->layout.layoutGrid);  // TODO: per track editor / window
                                trackCtr->loadTrackLayouts(f->project.trackCtr);      // OK: This loads per track editor / window
                                trackCtr->loadTrackLayouts(f->project.trackReturnCtr);
                                trackCtr->loadTrackLayouts(f->project.trackMasterCtr);
                                trackCtr->setScrollOffset(f->layout.scrollOffsetX);   // TODO: per track editor / window
                            }
                            return true;
                        });
                    }
                    daw->loadProjectFinish();
                }
                default:
                    setFinished();
                break;
            }
        } catch (std::exception& e) {
            log_printf("Failed loading project: %s\n", e.what());
            projectLoadErrored = true;
        } catch (...) {
            log_printf("Failed loading project. Unhandled exception\n");
            projectLoadErrored = true;
        }
        AppWndProc_disableBlockReentrant();
        requestFrame();
        break;
    }
    case state::error:
    case state::finished:
    case state::cancelled:
        break;
    }
}
void load_project_task::getPreciseProgress(double& progressOverall, double& progressDetail) {
    if (sampleLoader) {
        progressDetail = sampleLoader->getProgress();
    } else if (numSubsteps) {
        progressDetail = substep / (double) numSubsteps;
    } else {
        progressDetail = step < 2 ? 0 : 1;
    }
    auto stepFinished = step / 2;
    progressOverall = math::clamp((stepFinished + progressDetail) / 2.0, 0.0, 1.0);
}

} // namespace DAW

void clip_view_t::updateNotePitches(bool reset) {
    if (reset)
        notePitches.clear();
    clip_t* currentClip = clip();
    if (currentClip)
        currentClip->notes.getNotePitches(notePitches);
    for (auto& [trackEntry, vecClips] : this->m_selectionView.tracks) {
        for (clip_t* clip : vecClips) {
            if (clip == currentClip) {
                continue;
            }
            clip->notes.getNotePitches(notePitches);
        }
    }
}

void clip_view_t::copySelectedNoteList() {
    dragStartNotes = clip()->notes;
    clip()->notes.copySelectionTo(draggedSelection);
    clip()->notes.copySelectionTo(draggedSelectionBegin);
}

void clip_view_t::setEditorSelection(clip_t* clip, const editor_view_selection_t& clipboardView) {
    m_selectionView = clipboardView;
    bIsAbsoluteMode = clipboardView.totalClipCount > 1;
    m_clipRef.set(clip);
    updateNotePitches(true);
}

void clip_view_t::setSelected(clip_t* clip) {
    m_clipRef.set(clip);
}

void clip_view_t::setSingleClip(clip_t* clip) {
    m_selectionView = {};
    m_clipRef.set(clip);
    updateNotePitches(true);
    bIsAbsoluteMode = false;
}

void clip_view_t::reset() {
    m_selectionView = {};
    m_clipRef.set(nullptr);
    updateNotePitches(true);
}

bool clip_view_t::contains(clip_t* _clip) const {
    auto currentClip = clip();
    if (currentClip == _clip)
        return true;
    for (auto& [trackEntry, vecClips] : this->m_selectionView.tracks) {
        for (clip_t* viewClip : vecClips) {
            if (viewClip == _clip) {
                return true;
            }
        }
    }
    return false;
}

float clip_view_t::toFoldNote(float note) const {
    const auto len   = notePitches.size();
    const auto iNote = math::floorfS32(note);
    for (uint32_t i = 0; i < len; i++) {
        if (notePitches[i] >= iNote) {
            return i;
        }
    }
    if (len) {
        if (iNote >= notePitches[len - 1])
            return len + (note - notePitches[len - 1]);
    }
    return note;
}

float clip_view_t::nextFoldNote(float note, int dir) {
    float f = toFoldNote(note);
    return unfoldNoteClamped(f + dir);
}

float clip_view_t::unfoldNoteClamped(float note) {
    const auto len = CtrSize(notePitches);
    if (len) {
        const auto idx = math::clamp<int32_t>(math::floorfS32(note), 0, len - 1);
        return notePitches[idx];
    }
    return 0;
}

float clip_view_t::unfoldNote(float note) {
    const auto len = CtrSize(notePitches);
    if (len) {
        const auto iNote = math::floorfS32(note);
        if (iNote < 0)
            return notePitches[0] + note;

        if (iNote >= len)
            return note - len + 1 + notePitches[len - 1];
        return notePitches[iNote];
    }
    return 0;
}

void DawCtrl::focusChanged(guibase* oldFocused, guibase* newFocused) {
    AppCtrl::focusChanged(oldFocused, newFocused);
    if (newFocused) {
        auto clipEditor = guiParentType<guictr_clipeditor, gui_type::CTR_TYPE_CLIPEDITOR>(newFocused);
        auto ctrPlugins = guiParentType<guictr_plugins, gui_type::CTR_TYPE_PLUGINS>(newFocused);
        if (clipEditor) {
            auto ctrlLayoutParent = guiParentType<guictr_layout, gui_type::CTR_TYPE_LAYOUT>(clipEditor->parent);
            if (ctrlLayoutParent) {
                auto sharedPtrHandle = ctrlLayoutParent->getEntry(clipEditor);
                view->ctr_clipeditorview.setClipEditor(sharedPtrHandle);
            }
        }
        if (ctrPlugins) {
            auto ctrlLayoutParent = guiParentType<guictr_layout, gui_type::CTR_TYPE_LAYOUT>(ctrPlugins->parent);
            if (ctrlLayoutParent) {
                auto sharedPtrHandle = ctrlLayoutParent->getEntry(ctrPlugins);
                view->ctr_pluginview.setPluginCtr(sharedPtrHandle);
            }
        }
    }
}

void DawCtrl::onViewCreated(SPLayoutEntry& ctrEntry) {
    if (ctrEntry->getType() == gui_type::CTR_TYPE_CLIPEDITOR) {
        view->vecClipEditors.push_back(std::static_pointer_cast<guictr_clipeditor>(ctrEntry->getSharedGui()));
    }
}

void clip_ref_t::set(clip_t* clip) {
    if (!clip || clip->trackEntries.empty()) {
        m_project = nullptr;
        m_track   = nullptr;
        m_clip    = nullptr;
        return;
    }
    auto trackEntry = clip->trackEntries.front();
    if (!assert_expr(trackEntry->parentCtrl)) {
        return;
    }
    auto daw = trackEntry->parentCtrl->getDaw();
    if (!assert_expr(daw)) {
        return;
    }
    project_t* project = daw->getProject();
    if (!assert_expr(project)) {
        return;
    }
    m_project = project;
    m_track   = trackEntry->track;
    m_clip    = clip;
    always_assert(isValidUpdate());
}

void DawInstance::onPluginsChanged() {
    tls.pluginManager->onTrackLayoutChange();
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->view->visitEntries([](SPLayoutEntry& entry) {
            if (entry->getType() == gui_type::CTR_TYPE_PLUGINS) {
                auto trackCtr = guictr_cast<guictr_plugins>(entry);
                trackCtr->relayout();
            }
            if (entry->getType() == gui_type::CTR_TYPE_NODES) {
                auto trackCtr = guictr_cast<guictr_nodes_splitview>(entry);
                trackCtr->reset();
                trackCtr->refresh();
            }
            return true;
        });
    }
}
