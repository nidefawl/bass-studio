#include "assert_dbg.h"
#include "event.h"
#include "fileio.h"
#include "glheaders.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstddef>
#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <ctime>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>
#include <memory>

#include "appsettings.h"
#include "basectrl.h"
#include "color_util.h"
#include "commands.h"
#include "cursor.h"
#include "edithistory.h"
#include "error.h"
#include "exceptions.h"
#include "file/projectfile.h"
#include "fileloader.h"
#include "grid.h"
#include "gui/container/container_dnd_layout.h"
#include "guicolors.h"
#include "host/clip/clip.h"
#include "host/daw/daw_async_task.h"
#include "host/daw/mainctrl.h"
#include "host/project/project.h"
#include "host/track/track.h"
#include "keyboard.h"
#include "logging.h"
#include "math/seq_math.h"
#include "menu.h"
#include "msgbox.h"
#include "note.h"
#include "platform.h"
#include "saferef.h"
#include "seq_util.h"
#include "str_util.h"
#include "thread.h"
#include "tls.h"
#include "types.h"
#include "util/profiling.h"
#include "window.h"

#include "gui/clipeditor/clipeditor.h"
#include "gui/container/container_builder.h"
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
#include "wave/waveform_render_impl.h"

#include "host/plugin/base/base-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/track/track_impl.h"
#include "host/audiocache/audiocache.h"
#include "seq_time.h"
#include "host/graph/track_graph.h"
#include "host/graph/effect_graph.h"

#include "gui/plugin/plugin.h"
#include "threads/workerthread.h"
#include "threads/playbackthread.h"
#include "host/plugindatabase/plugindatabase.h"
#include "window_impl.h"

#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "host/audiohost/audio_host.h"
#include "host/midihost/midi_host.h"
#include "appconfig.h"
#include "sse.h"
#ifdef _WIN32
#include "platform/win/windowsize.h"
#endif
#ifdef __linux__
#include "platform/linux/windowsize.h"
#endif
#include "daw_async_project_load.h"

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

class DawViewContainersMain : public DawViewContainers {
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
    guictr_tempocontrols ctr_tempo;
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

void DawInstance::unloadProject() {
    AppWndProc_enableBlockReentrant();
    dbgassert(!playThread.isRunning() || playThread.isLockedOrNotProcessing());
    for (DawCtrl* ctrl : dawCtrls) {
        ctrl->resetClipViews();
        ctrl->closeContextMenu();
        ctrl->resetMouseContext();
        ctrl->setSelectedTrack(nullptr);
    }
    projectPath = "";
    resetClipViews();
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
        ctrl->view->visitEntries([](SPLayoutEntry& ctr) {
            if (ctr->getType() == gui_type::CTR_TYPE_TRACKS) {
                auto trackCtr = guictr_cast<guictr_tracks>(ctr);
                auto& trackView = trackCtr->trackView;
                trackView.m_resizePreModifyState.reset();
                trackView.action.clipboard.reset();
                trackView.iGuiMgr.reset();
            }
            return true;
        });
    }

    /** reset maximum stage id and determine new maximum stage id */
    tls.host->updateMaximumStageId();
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
static SupportedFileType FILE_TYPE_PROJECT_BUNDLE{ "Project Bundle", PROJECT_BUNDLE_FILE_EXT };
std::vector<SupportedFileType> vFILE_TYPE_PROJECT = { FILE_TYPE_PROJECT };
std::vector<SupportedFileType> vFILE_TYPE_BUNDLE = { FILE_TYPE_PROJECT_BUNDLE };
std::vector<SupportedFileType> vFILE_TYPE_PROJECTS = { FILE_TYPE_PROJECT, FILE_TYPE_PROJECT_BUNDLE };

void DawInstance::loadFileCStr(const char* str) {
    loadFile(str, 0);
}

void DawInstance::saveFile(const String& path) {
    if (!path.empty()) {
        std::shared_ptr<project_file> f = createProjectFile();
        bool bSuccess = false;
        if (projectFileType == PROJECT_FILETYPE_JSON) {
            bSuccess = saveProjectToJsonFile(f, path);
        } else {
            saveProjectBundle(path);
            bSuccess = true;
        }
        if (tls.mainCtrl) {
            if (bSuccess) {
                tls.mainCtrl->setStatusText(StringFormat("Saved project to %s", StringAsCStr(path)));
            } else {
                tls.mainCtrl->setStatusText(StringFormat("Failed to save project to %s", StringAsCStr(path)), GuiColor::COL_INVALID_INPUT);
            }
        }
        projectPath = path;
        String projectFileName;
        SplitPath(path, &lastProjectDirectory, &projectFileName, nullptr, nullptr);
        tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::PRODUCT_NAME_DISPLAY, StringAsCStr(projectFileName)));
        tls.settings->recentfiles.add(path);
    }
}

void DawInstance::loadFile(String path, int flags) {
    String loadFileExt, loadFileDirectory;
    SplitPath(path, &loadFileDirectory, nullptr, &loadFileExt);
    std::vector<uint8_t> projJsonData;
    if (loadFileExt == PROJECT_BUNDLE_FILE_EXT && !path.empty()) {
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        archive_read_open_filename(a, path.c_str(), 10240);
        struct archive_entry* entry = nullptr;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            const char* entryPath = archive_entry_pathname(entry);
            String entryPathExt;;
            SplitPath(entryPath, nullptr, nullptr, &entryPathExt);
            if (entryPathExt == PROJECT_FILE_EXT) {
                projJsonData.resize(archive_entry_size(entry));
                archive_read_data(a, projJsonData.data(), projJsonData.size());
                break;
            }
        }
        archive_read_free(a);
    } else {
        ReadFileVector(path, projJsonData);
    }
    timer.reset();
    std::shared_ptr<project_file> f = loadProject(projJsonData);
    if (!f) {
        if (tls.mainCtrl) {
            tls.mainCtrl->setStatusText(StringFormat("Failed loading %s", StringAsCStr(FileNameFromPath(path))));
        }
    } else {
        f->path = path;
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
    projectFileType = PROJECT_FILETYPE_JSON;
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_lf(Log::L_WARN, "getNumClipAllocations == %d!\n", totalAllocs);
        // dbgassert(getNumClipAllocations() == 0);
    }
    insertNewTrack(-1, TRACK_TYPE_MIDI, FLG_TRK_CHANGE_LOAD);
    insertNewTrack(-1, TRACK_TYPE_MASTER, 0);
    resetShaderTimeOffset();
    tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::PRODUCT_NAME_DISPLAY, "New Project"));
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
    if (enableSolo) {
        track->audio->flags |= audiostageflags_t::SOLO;
    } else {
        track->audio->flags &= ~audiostageflags_t::SOLO;
    }
    DAW::updateSoloFlag(tls.host, &project, getTracks().getAllTracksFlatVecRef());
}
void DawInstance::unsoloAll() {
    DAW::unsoloAll(tls.host, &project, getTracks().getAllTracksFlatVecRef());
}
void DawInstance::setTrackArmed(audio_stage_ref_t ref, bool enabledArmed) {
    track_t* track = getTracks().resolveTrack(ref);
    dbgassert(track);
    dbgassert(track->audio);
    if (enabledArmed) {
        track->audio->flags |= audiostageflags_t::RECORD_ARMED;
    } else {
        track->audio->flags &= ~audiostageflags_t::RECORD_ARMED;
    }
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

void DawInstance::saveProjectBundle(const String& path) {
    String ext;
    String projectFileName;
    String parentDir;
    String bundlePath = path;
    SplitPath(bundlePath, &parentDir, &projectFileName, &ext);
    if (ext != PROJECT_BUNDLE_FILE_EXT) {
        bundlePath = parentDir + FILE_PATHSEP_STR + projectFileName + "." PROJECT_BUNDLE_FILE_EXT;
    }
    String projFileName = projectFileName + "." PROJECT_FILE_EXT;
    
    std::function<void(const String& msg, const String& file)> onError = [this](const String& msg, const String& file) {
        log_lf(Log::L_ERROR, "Failed saving project to %s: %s\n", StringAsCStr(file), StringAsCStr(msg));
        if (tls.mainCtrl) {
            tls.mainCtrl->setStatusText(msg);
        }
    };
    std::function<void(const String&, int32_t, int32_t)> onProgress = [path](const String& curFile, int32_t i, int32_t total) {
        log_lf(Log::L_ERROR, "[%d/%d] Saving %s to %s\n", i, total, StringAsCStr(curFile), StringAsCStr(path));
    };

    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    // create a new archive
    struct archive* ar = archive_write_new();
    if (!ar || ARCHIVE_OK != archive_write_set_format_zip(ar)) {
        onError("Failed to create archive", bundlePath);
        return;
    }
    if (ARCHIVE_OK != archive_write_zip_set_compression_deflate(ar)) {
        onError("Failed to compress archive", bundlePath);
        return;
    }
    if (ARCHIVE_OK != archive_write_open_filename(ar, bundlePath.c_str())) {
        onError("Failed to open archive for writing", bundlePath);
        return;
    }
    if (ARCHIVE_OK != getAudioCache()->writeToArchive(uniqueSampleIds, ar, onProgress, onError)) {
        onError("Failed to write audio cache to archive", bundlePath);
        return;
    }
    std::shared_ptr<project_file> f = createProjectFile();
    std::vector<uint8_t> buffer;
    saveProject(f, buffer);
    // // add a file to the archive
    struct archive_entry* entry = archive_entry_new();
    if (!entry) {
        onError("Failed to create archive entry", bundlePath);
        return;
    }
    auto bufSize = int64_t(buffer.size());
    archive_entry_set_pathname(entry, projFileName.c_str());
    archive_entry_set_mtime(entry, time(nullptr), 0);
    archive_entry_set_size(entry, bufSize);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    if (ARCHIVE_OK != archive_write_header(ar, entry)) {
        onError("Failed to write archive header", bundlePath);
        return;
    }
    auto sizeWritten = archive_write_data(ar, buffer.data(), buffer.size());
    if (sizeWritten != bufSize) {
        onError("Failed to write archive data", bundlePath);
        return;
    }
    archive_entry_free(entry);
    // finish writing the archive
    if (ARCHIVE_OK != archive_write_close(ar)) {
        onError("Failed to close archive", bundlePath);
        return;
    }
    if (ARCHIVE_OK != archive_write_free(ar)) {
        onError("Failed to free archive", bundlePath);
        return;
    }
}

bool DawInstance::menuCommand(const menucmd_t& command) {
    try {
        auto mainCtrl = tls.mainCtrl;
        switch (command.command) {
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
                            ts.trackLoaded->loadSnapshot(tls.host, ts);
                            std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
                            for (auto effect: effects) {
                                pluginMgr->activateDeferred(effect, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                            }
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
                    if (promptUserFilePath(mainCtrl->window, 0, vFILE_TYPE_PROJECTS, path, lastProjectDirectory)) {
                        loadFile(path, FLAG_INVOKE_USER_CB_DEFERLOAD);
                    }
                } else {
                    loadFile(command.arg1, FLAG_INVOKE_USER_CB_DEFERLOAD);
                }
                return true;
            }
            case CMD_BUNDLE_PROJECT_ZIP: {
                String bundlePath;
                if (!promptUserFilePath(tls.mainCtrl->window, 1, vFILE_TYPE_BUNDLE, bundlePath, lastProjectDirectory)) {
                    return true;
                }
                String ext;
                SplitPath(bundlePath, nullptr, nullptr, &ext);
                if (ext.empty()) {
                    bundlePath += "." PROJECT_BUNDLE_FILE_EXT;
                }
                projectFileType = PROJECT_FILETYPE_BUNDLE;
                saveFile(bundlePath);
                return true;
            }
            case CMD_BUNDLE_PROJECT_DIRECTORY: {
                String bundlePath;
                if (browseForFolder("Select project folder", projectPath, bundlePath)) {
                    return true;
                }
                String dirName;
                SplitPath(bundlePath, nullptr, &dirName, nullptr, nullptr);
                
                std::vector<int32_t> uniqueSampleIds;
                DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
                getAudioCache()->rellocateSamples(uniqueSampleIds, bundlePath);
                projectPath = bundlePath + FILE_PATHSEP_STR + dirName + ".project";
                saveFile(projectPath);
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
            auto exportFilename = selTrack->name + "." + vFILE_TYPES_TRACKSNAPSHOT[0].ext;
            if (promptUserFilePath(window, 1, vFILE_TYPES_TRACKSNAPSHOT, path, exportDir, exportFilename)) {
                String ext;
                SplitPath(path, nullptr, nullptr, &ext);
                if (ext.empty()) {
                    path += "." + vFILE_TYPES_TRACKSNAPSHOT[0].ext;
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
            daw.tls.settings->dawsettings.globalZoom = 1.0;
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

    BaseCtrl::relayout();
    updateVisibleTrackContents();

    //TODO: move this out of here
    if (!loadProject.empty()) {
        String file;
        std::swap(file, loadProject);
        daw.loadFile(file, loadFlags);
    } else {
        daw.setEmptyProject();
    }
    auto& layouts = daw.getLayouts();
    view->storeLayout(layouts[0]);
    for (size_t i = 1; i < layouts.size(); i++) {
        std::shared_ptr<dawview_layout_t> viewLayout = loadDawViewLayoutSnapshot(StringFormat("data/view%zu.layout", i));
        if (viewLayout) {
            layouts[i] = *viewLayout.get();
        }
    }
    // view->loadLayout(layouts[1]);
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
    this->workerThread.startThread("File Loader Thread", seqthreads::ThreadType::WorkerThread);

    setAudioThreadState(playback_state::status_stop);
}
std::pair<String, String> DawInstance::createUniqueNonExistingFilename(const String& baseDir, const String& trackName, const String& sampleName, const String& fileExt) {
    String uniqueFileName;
    if (!trackName.empty()) {
        uniqueFileName += trackName;
        uniqueFileName += " - ";
    }
    uniqueFileName += sampleName;

    String pathInput = baseDir;
    pathInput += FILE_PATHSEP_CHAR;
    String projName = getProjectName();
    if (projName.empty()) {
        projName = "Untitled";
    }
    pathInput += projName;
    pathInput += FILE_PATHSEP_CHAR;
    pathInput += uniqueFileName;
    pathInput += ".";
    pathInput += fileExt;

    String sampleFilePath = App::Platform::toUserdataPath(pathInput);
    App::Platform::sanitizePathToFile(sampleFilePath);
    String name;
    String ext;
    String path;
    int32_t idx = 0;
    String uniqueFilePath = sampleFilePath;
    SplitPath(sampleFilePath, &path, &name, &ext);
    App::Platform::sanitizePathToDirectory(path);
    while ((FileExists(uniqueFilePath) || tls.audioCache->getByFilename(uniqueFilePath) != nullptr) && ++idx < 10000) {
        idx++;
        uniqueFileName = name;
        uniqueFileName += "-";
        uniqueFileName += std::to_string(idx);
        uniqueFileName += ".";
        uniqueFileName += ext;
        uniqueFilePath = path;
        uniqueFilePath += FILE_PATHSEP_CHAR;
        uniqueFilePath += uniqueFileName;
    }
    return {uniqueFilePath, uniqueFileName};
}

void DawInstance::updateClipViews(clip_t* notifyClip) {
    for (auto* ctrl : dawCtrls) {
        ctrl->updateClipViews(notifyClip);
    }
}


void DawInstance::updateClipViewsAndCursor(clip_t* notifyClip, clip_cursor_t cursor) {
    for (auto* ctrl : dawCtrls) {
        ctrl->updateClipViewsAndCursor(notifyClip, cursor);
    }
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
    auto countVec = view->vecClipEditors.size();
    for (auto& clipEditor : view->vecClipEditors) {
        clipEditor->resetClipView();
    }
    size_t countVisit = 0;
    view->visitEntries([&countVisit](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_CLIPEDITOR)
            countVisit++;
        return true;
    });
    if (countVec != countVisit) {
        log_lf(Log::L_WARN, "countVec=%zu, countVisit=%zu\n", countVec, countVisit);
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
    isOK = false;
    if (view) {
        delete view;
        view = nullptr;
    }
    waveformRenderer->destroy();
    delete waveformRenderer;
    waveformRenderer = nullptr;
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

    //TODO: layout settings should be handled on editor container level
    view->visitEntries([&](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_TRACKS) {
            auto tracks = guictr_cast<guictr_tracks>(entry);
            auto& grid = tracks->getGrid();
            if (isCompanion()) {
                grid.grid_dens = settings.wndCompanion.dens;
            } else {
                grid.grid_dens = settings.wndMain.dens;
            }
        }
        return true;
    });

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

void DawInstance::onAudioStageChanged(audio_stage_t* stage) {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->getPluginSel().clear();
    }
}

void DawCtrl::onTrackContentRemoved(track_gui_entry_t& e) {
}

void DawCtrl::onTrackMixerRemoved(track_gui_entry_t& e) {
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
    _mainCtrl->updateZoomLevel(tls.settings->dawsettings.globalZoom);
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
    saveProjectToJsonFile(f, projectPathAutosave);
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
void DawInstance::processTasksMainThread() {
    using state = DAW::async_task_t::state;
    auto runAsyncTask = asyncTask;
    if (runAsyncTask) {
        runAsyncTask->run();
        if (runAsyncTask->getAndResetReqFrame()) {
            getMainControl()->requestRedraw();
        }
        switch (runAsyncTask->getState()) {
            case state::idle:
                dbgassert(0);
                break;
            case state::running:
                break;
            case state::error:
                log_lf(Log::L_ERROR, "async task %s error: %s\n", runAsyncTask->getTaskName().c_str(), runAsyncTask->getError().c_str());
            case state::finished:
            case state::cancelled:
                delete runAsyncTask;
                setAsyncTask(nullptr);
                break;
        }
    }
}
void DawInstance::onTick() {
    const bool bWroteMidiData = tls.host->writeRecordedData(this);

    if (bWroteMidiData) {
        updateVisibleTrackContents();
    }

    tls.host->onTick();

    bool noPopups = true;
    for (auto* ctrl : dawCtrls) {
        noPopups &= !ctrl->getGuiDragged() && !ctrl->getGuiCaptured() && !ctrl->ctxtmenu;
    }
    if (noPopups && tls.mainCtrl && !tls.mainCtrl->loadProject.empty()) {
        String file;
        std::swap(tls.mainCtrl->loadProject, file);
        loadFile(file, FLAG_INVOKE_USER_CB_DEFERLOAD);
    }
    if (noPopups && projectToLoad) {
        setAsyncTask(new DAW::load_project_task(this, std::move(projectToLoad)));
        projectToLoad = nullptr;
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

    //TODO: layout settings should be handled on editor container level
    auto mainCtrl = getMainControl();
    if (mainCtrl) {
        auto ctrTracks = mainCtrl->getTrackContainer();
        if (ctrTracks) {
            file->layout.layoutGrid = ctrTracks->getGrid();
            file->layout.scrollOffsetX = ctrTracks->getScrollOffset();
        }
    }
    return file;
}
namespace DAW {
void GetProjectReferencedSampleIds(const project_t& project, std::vector<int32_t>& uniqueSampleIds) {
    for (track_t* t : project.trackList) {
        auto& clipContainer = t->getClips();
        for (auto& clip : clipContainer.getClips()) {
            if (clip->audio.id >= 0 && !std::binary_search(uniqueSampleIds.cbegin(), uniqueSampleIds.cend(), clip->audio.id)) {
                insertSorted(uniqueSampleIds, clip->audio.id);
            }
        }
    }
}
}// namespace DAW
void DawInstance::unloadUnreferencedSamples() {
    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    log_lf(Log::L_DEBUG, "Found %zu sample ids\n", uniqueSampleIds.size());
    tls.audioCache->unloadUnreferenced(uniqueSampleIds);
}

bool DawInstance::setProjectToLoad(const std::shared_ptr<project_file>& file, int flags) {
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
bool DawInstance::setLoadedProject(const std::shared_ptr<project_file>& file, int flags) {
    ThreadLock lock = playThread.lockThread();
    loadProject0(file);
    bool b = loadProject1(file, flags);
    loadProjectFinish();
    return b;
}
void DawInstance::loadProject0(const std::shared_ptr<project_file>& file) {

    setAudioThreadState(playback_state::status_no_process);
    log_printf("Loading project %s: %zu tracks\n", StringAsCStr(file->path), project.trackList.size());
    unloadProject();
    /** make sure call to unloadProject unloaded all vst2 instances */
    dbgassert(tls.host->getNumAudioStages() == 0);
    dbgassert(tls.host->getVst2Instances().empty());
    //TODO: assert that audiocache is empty
    dbgassert(tls.audioCache->isEmpty());
#ifndef NDEBUG
    tls.host->validateIds();
#endif

    String loadFileExt, loadFileDirectory;
    SplitPath(file->path, &loadFileDirectory, nullptr, &loadFileExt);
    if (loadFileExt == PROJECT_BUNDLE_FILE_EXT) {
        this->projectFileType = PROJECT_FILETYPE_BUNDLE;
    } else {
        this->projectFileType = PROJECT_FILETYPE_JSON;
    }

    /** set as current project */
    this->projectPath = file->path;
    lastProjectDirectory = loadFileDirectory;
    this->layoutsFromProjectFile = file->layouts;

    /** populates trackList */
    project.copyFrom(file->project);

    projectGlobals      = file->project.globals;
    getExportSettings() = file->project.exportSettings;
    getQuantizeSettings() = file->project.quantizeSettings;

    /** load track snapshots */
    project.trackList.loadProjectSnapshot(tls.host, file->project);

    /** fix audio clip lengths */
    for (track_t* t : project.trackList) {
        t->updateAudioClipLengths(projectGlobals.tempo100, file->project.samplerate, tls.host->m_sampleFormatInternal.sampleRate);
    }

    /** create all gui instances */
    for (DawCtrl* pDawCtrl : dawCtrls) {
        for (track_t* tr : project.trackList) {
            pDawCtrl->addTrackToView(tr, FLG_TRK_CHANGE_LOAD);
        }
    }
    
#ifndef NDEBUG
    tls.host->validateIds();
#endif
    onPluginsChanged();

    /** reset maximum stage id and determine new maximum stage id */
    tls.host->updateMaximumStageId();

    /** remove routings to missing track */
    DAW::validateTrackRoutings(tls.host, project.getTracksFlatVec());
    /** create all gui instances */
    for (track_t* tr : project.trackList) {
        DAW::validateEffectRoutings(tls.host, tr->audio);
    }

    /** inform host about track layout changes so it resets and updates internal structures */
    tls.host->onTrackLayoutChange();
}
bool DawInstance::loadProject1(const std::shared_ptr<project_file>& file, int flags) {
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

    if ((flags & FLAG_DEFER_LOAD) == 0) {
         auto len = pluginsDeferred.size();
        for (size_t i = 0; i < len; i++) {
            dbgassert(pluginsDeferred[i]->getModuleType() == PLUGIN_TYPE_DEFERRED);
            auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[i]);
            effectbase* pluginLoaded = nullptr;
            tls.host->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &pluginLoaded);
            (void) pluginLoaded;
        }
    }

    onPluginsChanged();

    tls.audioCache->load(file->sampleFileIndex, projectFileType, file->path, lastProjectDirectory);
    for (track_t* tr : project.trackList) {
        tr->getStage()->pluginsChanged();
    }
    tls.host->onTrackLayoutChange();
    /** validate cursor state */
    auto ctrl = tls.mainCtrl;
    if (ctrl) {
        if (this->layoutsFromProjectFile.size() > 0) {
            ctrl->loadLayout(this->layoutsFromProjectFile[0]);
        }
        ctrl->view->visitEntries([f = file.get()](SPLayoutEntry& entry) {
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
    updateVisibleTrackContents();
    return true;
}
void DawInstance::loadProjectFinish() {
    /** reset maximum stage id and determine new maximum stage id */
    tls.host->updateMaximumStageId();

    onPluginsChanged();
    for (DawCtrl* pDawCtrl : dawCtrls) {
        pDawCtrl->fixCursor();
    }
    tls.settings->recentfiles.add(projectPath);
    this->tmLastSave  = getTimeMillis();
    String s = StringFormat("Loaded project %s", StringAsCStr(this->projectPath));
    log_lf(Log::L_INFO, "%s\n", StringAsCStr(s));
    if (tls.mainCtrl) {
        tls.mainCtrl->setStatusText(s);
        String projectFileName;
        SplitPath(this->projectPath, nullptr, &projectFileName, nullptr);
        tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::PRODUCT_NAME_DISPLAY, StringAsCStr(projectFileName)));
    }

    setAudioThreadState(playback_state::status_stop);
    if (cbProjectLoadCompleteCallback) {
        auto projectLoadErrored = false;
        cbProjectLoadCompleteCallback(this, projectToLoad->projectfile, projectLoadErrored ? 1 : 0);
        cbProjectLoadCompleteCallback = nullptr;
    }
    updateVisibleTrackContents();
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
    view->visitEntries([track](SPLayoutEntry& entry) {
        if (entry->getType() == gui_type::CTR_TYPE_PLUGINS) {
            auto spCtrPlugins = std::static_pointer_cast<guictr_plugins>(entry->getSharedGui());
            spCtrPlugins->showTrack(track ? track->audio : nullptr, spCtrPlugins);
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
    view->visitEntries([track, flags, &nTrackViews](SPLayoutEntry& ctr) {
        if (ctr->getType() == gui_type::CTR_TYPE_TRACKS) {
            nTrackViews++;
            guictr_cast<guictr_tracks>(ctr)->addTrack(track, flags);
        }
        return true;
    });
    dbgassert(nTrackViews);
}

void DawCtrl::removeTrackFromView(track_t* track, int flags) {
    int32_t nTrackViews =0;
    view->visitEntries([track, flags, &nTrackViews](SPLayoutEntry& ctr) {
        if (ctr->getType() == gui_type::CTR_TYPE_TRACKS) {
            nTrackViews++;
            guictr_cast<guictr_tracks>(ctr)->removeTrack(track, flags);
        }
        return true;
    });
    dbgassert(nTrackViews);
}

void DawCtrl::resetView() {
    view->visitEntries([](SPLayoutEntry& ctr) {
        if (ctr->getType() == gui_type::CTR_TYPE_TRACKS) {
            guictr_cast<guictr_tracks>(ctr)->resetView();
        }
        return true;
    });
}

void DawCtrl::updateVisibleTrackContents() {
    view->visitEntries([](SPLayoutEntry& ctr) {
        if (ctr->getType() == gui_type::CTR_TYPE_TRACKS) {
            auto trackCtr = guictr_cast<guictr_tracks>(ctr);
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
    return guiCaptured && guiCaptured->getGuiType() == gui_type::CTR_TYPE_TRACKS_TIMELINE;
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
    if (guiDragged || guiCaptured) {
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
    if (guiDragged || guiCaptured) {
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
    clipreset(dragdrop_midifile& _clip) : clip(_clip){};
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
    if (guiDragged || guiCaptured) {
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
                auto& layouts = daw.getLayouts();
                if ((kevt.mods & KB_MOD_SHIFT) == kevt.mods && ctxt.argInt0 >= 0 && ctxt.argInt0 < CtrSize(layouts)) {
                    auto index = ctxt.argInt0 % layouts.size();
                    bool store    = (kevt.mods & KB_MOD_SHIFT);
                    if (store) {
                        view->storeLayout(layouts[index]);
                        saveDawViewLayoutSnapshot(layouts[index], StringFormat("data/view%zu.layout", index));
                    } else {
                        loadLayout(layouts[index]);
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
        daw->resetClipViews();
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
        daw->resetClipViews();
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
        daw->resetClipViews();
        daw->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(localIdx == trackPtr->localIdxFlat);
        localIdx = trackPtr->localIdxFlat;
        trackPtr = nullptr;
        //UNSERIALIZE TRACK VSTs
    }

    void redo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetClipViews();
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
    resetClipViews();
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
    for (auto entry : clip->trackEntries) {
        auto it = entry->clipsGuis.find(clip);
        if (it != entry->clipsGuis.end() 
            && it->second 
            && entry->parentCtrl
            && stl_contains(dawCtrls, entry->parentCtrl)) {
            entry->parentCtrl->onGuiRemoved(it->second);
        }
    }
    resetClipViews();
    //resetMouseContext();
}

void DawInstance::preTrackDelete(track_t* track) {
    resetMouseContext();
    resetClipViews();
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
    auto ctrTracks = getTrackContainer();
    //TODO: layout settings should be handled on editor container level
    if (ctrTracks) {
        auto& settings = daw_tls::getSettings();
        settings.wndMain.dens = ctrTracks->getGrid().grid_dens;
    }
    {
        ThreadLock lock = daw.playThread.lockThread();
        //TODO: MultiLogger::removeLogger is not thread safe. This will eventually cause a race condition 
        // and a crash since not all threads and modules are synchronized here (just playthread and workerthreads)
        getMultiLogger().removeLogger(statusbarLogger.get());
        daw.unloadProject();
    }
    DawCtrl::destroy();
    daw.destroy();
}

void CompanionCtrl::destroy() {
    {
        auto ctrTracks = getTrackContainer();
        //TODO: layout settings should be handled on editor container level
        if (ctrTracks) {
            auto& settings = daw_tls::getSettings();
            settings.wndCompanion.dens = ctrTracks->getGrid().grid_dens;
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

void DawInstance::setSingleClip(clip_t* clip) {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->setSingleClip(clip);
    }
}

void DawInstance::setEditorSelection(clip_t* clip, const editor_view_selection_t& clipboardView) {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->setEditorSelection(clip, clipboardView);
    }
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

beatbar16th_t project_controller_t::toBeatBar16th(tick_t tick, bool isRelative) {
    return ::tickToBarBeat16th(tick, projectGlobals->signatureNum, projectGlobals->signatureDenom, isRelative);
}

tick_t project_controller_t::beatBarNthToTick(const beatbar16th_t& beatBarNth, bool isRelative) {
    return ::beatBarNthToTick(beatBarNth, projectGlobals->signatureNum, projectGlobals->signatureDenom, isRelative);
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

void DawInstance::setEmptyClipboard() {
    clipboardType    = CLIPBOARD_NONE;
    clipboardPlugins = std::make_shared<plugin_clipboard_t>();
    clipboardClips   = std::make_shared<clip_clipboard>();
    clipboardNotes   = std::make_shared<notes_clipboard>();
}
void DawCtrl::updateZoomLevel(float f) {
    AppCtrl::updateZoomLevel(f);
    if (view) {
        view->ctr_tempo.onGlobalZoomChanged();
        daw.tls.settings->dawsettings.globalZoom = f;
    }
}

void DawInstance::setAsyncTask(DAW::async_task_t* task) {
    asyncTask = task;
    for (auto& ctrl : dawCtrls) {
        ctrl->setAsyncTask(task);
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
                    /** validate cursor state */
                    auto ctrl = daw->getMainControl();
                    if (ctrl) {
                        if (daw->layoutsFromProjectFile.size() > 0) {
                            ctrl->loadLayout(daw->layoutsFromProjectFile[0]);
                        }
                        ctrl->view->visitEntries([f = file](SPLayoutEntry& entry) {
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
    progressOverall = (step / 2 + progressDetail) / 2.0;
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

void DawInstance::resetClipViews() {
    for (auto* dawctrl : dawCtrls) {
        dawctrl->resetClipViews();
    }
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
