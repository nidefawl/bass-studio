#include <deque>
#include <memory>
#include "pluginctr.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "logging.h"
#include "event.h"
#include "keyboard.h"
#include "renderresources.h"
#include "gui/controls/button.h"
#include "gui/controls/list.h"
#include "gui/controls/knob.h"
#include "gui/automation/automatable.h"
#include "gui/gui.h"
#include "basectrl.h"
#include "pluginviewcontainers.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/views/pluginlist.h"
#include "edithistory.h"
#include "guifonts.h"

#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/internal_plugin.h"
#include "host/plugin/vst_plugin.h"
#include "host/plugin/vst_plugin_handles.h"
#include "host/plugindatabase.h"
#include "threads/playbackthread.h"

#include "track.h"
#include "track_impl.h"
#include "gui/tooltip/tooltip.h"
#include "str_util.h"
#include "snapshot.h"
#include "clipboard.h"
#include "gui/table/table.h"


#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/plugin/plugin.h"
#include "dragdrop.h"
#include "projectfile-snapshot.h"

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
void getSelectedEffects(plugin_selection& sel, std::vector<effectbase*>& out) {
    out.clear();
    if (sel.hasSelection()) {
        std::vector<effectbase*> tmp;
        sel.pluginCtr->getEffects(tmp);
        int n  = sel.firstSelection;
        int n2 = sel.lastSelection;
        for (auto* effect : tmp) {
            int slot = effect->getSlot();
            if (slot >= n && slot <= n2) {
                out.push_back(effect);
            }
        }
    }
}
void setDraggedPluginsUI(guictr_dragged_plugins& gui, plugin_selection& sel) {
    gui.trackImpl = sel.pluginCtr->stage;
    gui.effects.clear();
    getSelectedEffects(sel, gui.effects);
    std::vector<String> list;
    for (auto* effect : gui.effects) {
        list.push_back(effect->getAutomatableName());
    }
    gui.setStrings(list);
}

guibase* guictr_plugins::getDraggedControl() {
    if (isSelected()) {
        auto& sel = MainCtrl::get()->getPluginSel();
        setDraggedPluginsUI(sel.pluginCtr->dragged, sel);
        return &sel.pluginCtr->dragged;
    }
    return this;
}

bool guictr_plugins::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        if (MouseHitType::MOUSE_LEFT == evt.type)
            dragged.pos = parent ? parent->toScreenSpace(mpos) : mpos;
        //handle multi selection...
        ivec2 localMouse = this->toContainerSpace(mpos);
        for (guibase* gui : guis) {
            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT || evt.type == MouseHitType::MOUSE_RIGHT) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}


bool guictr_plugins::isSelected() {
    return parent && parent->isSelected();
}

bool guictr_plugins::getSelected(std::vector<effectbase*>& out) {
    auto& sel = MainCtrl::get()->getPluginSel();
    getSelectedEffects(sel, out);
    return true;
}

class guictxtmenu_pluginctr : public guictxtmenu {
public:
    static constexpr int CMD_LOAD_PLUGIN = 1;
    audio_stage_t* const stage;
    guictxtmenu_pluginctr(audio_stage_t* _stage) : stage(_stage) {
        this->size.x = 260;
        addEntry(new ctxtmenu_entry("Load plugin", CMD_LOAD_PLUGIN));
    }
    void clicked(int _id) override {
        auto window = parentCtrl->window;
        // promptUserFilePath initiates a native dialog that would close this context menu
        // so we close it before this happens
        closeContextMenu();// deletes this
                           // now we make sure not to access this-> after this point

        if (_id == CMD_LOAD_PLUGIN) {

            String path;
            if (promptUserFilePath(window, 0, vFILE_TYPE_PLUGINSNAPSHOT, path)) {
                ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
                std::shared_ptr<plugin_snapshot_t> pluginSnapshot = loadPluginSnapshot(path);
                dbgassert(pluginSnapshot);
                if (pluginSnapshot) {
                    vsthost* host = vsthost::getInstance();
                    assignFreeStageIds(host, *pluginSnapshot);
                    auto effect = loadPluginDeferred(*pluginSnapshot);
                    if (effect) {
                        effect->projectGlobalId = 0;// generate new id
                        if (!host->addDeferredEffect(effect)) {
                            log_printf("Failed loading effect\n");
                            delete effect;
                            return;
                        }
                        effect->getSnapshot().projectGlobalId = effect->projectGlobalId;
                        effect->load(host);
                        host->insertNewPlugin(stage, effect, -2);// insert at end
                        // host->activateDeferred(effect, 0);
                    }
                }
            }
        }
    }
};
void guictr_plugins::handleRightClick(MouseEvent& evt) {
    if (this->stage) {
        parentCtrl->openContextMenu(new guictxtmenu_pluginctr(this->stage), evt.mousepos);
    }
}

void guictr_plugins::onAdded() {
    if (parent) {
        setTheme(parent->theme);
    }
}
void guictr_plugins::addGui(effectbase* plugin) {
    guiplugin* base = plugin->makeGui();
    if (base) {
        add(base);
    }
}
bool plugin_selection::hasSelection() const {
    return firstSelection >= 0 && lastSelection >= 0 && pluginCtr && pluginCtr->stage;
}

void pastePluginClipboard(std::shared_ptr<plugin_clipboard_t>& clipboard, audio_stage_t* stage, int32_t pos) {
    auto daw = DawInstance::get();
    auto host = daw->getHost();
    for (plugin_snapshot_t& pluginSnapshot : clipboard->plugins) {
        assignFreeStageIds(host, pluginSnapshot);
        auto effect = loadPluginDeferred(pluginSnapshot);
        if (effect) {
            effect->projectGlobalId = 0;// generate new id
            stage->deferredEffects.push_back(effect);
            if (!host->addDeferredEffect(effect)) {
                log_printf("Failed loading effect\n");
                delete effect;
                continue;
            }
            effect->getSnapshot().projectGlobalId = effect->projectGlobalId;
            effect->load(host);
            host->insertNewPlugin(stage, effect, pos);
            //keep negative values
            if (pos >= 0)
                pos++;
            host->activateDeferred(effect, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
        } else {
            //TODO: handle
        }
    }
    stage->pluginsChanged();
    host->onTrackLayoutChange();
    daw->onPluginsChanged();
}
std::shared_ptr<plugin_clipboard_t> copyPluginSelection(plugin_selection& sel) {
    std::vector<plugin_snapshot_t> pluginSnapshots;
    std::vector<effectbase*> selection;
    std::shared_ptr<plugin_clipboard_t> clipboard = std::make_shared<plugin_clipboard_t>();
    getSelectedEffects(sel, selection);
    pluginSnapshots.reserve(selection.size());
    for (effectbase* effect : selection) {
        plugin_snapshot_t ps;
        effect->makeSnapshot(ps, tracksnapshot_store_opts_t::All());
        clipboard->plugins.push_back(std::move(ps));
        clipboard->range++;
    }
    return clipboard;
}
bool handlePluginCtrCommand(DawCtrl* ctrl, action_plugin_ctr action) {
    plugin_selection& sel = ctrl->getPluginSel();
    if (!sel.pluginCtr || !sel.pluginCtr->stage) {
        return false;
    }
    auto daw = ctrl->getDaw();
    ThreadLock lock      = ctrl->lockPlayThread();
    bool handledKeyinput = false;
    String desc          = "???";
    std::vector<effectbase*> effectChain;
    sel.pluginCtr->getEffects(effectChain);
    std::vector<effectbase*> selection;
    getSelectedEffects(sel, selection);

    switch (action) {
        case action_plugin_ctr::PLUGINS_SELECTALL: {
            sel.firstSelection = effectChain.front()->getSlot();
            sel.lastSelection  = effectChain.back()->getSlot();
            handledKeyinput    = true;
        } break;
        case action_plugin_ctr::PLUGINS_DELETE:
            if (!selection.empty()) {
                audio_stage_t* audioStage = selection[0]->getTrackLink();
                dbgassert(audioStage);
                for (effectbase* eff : selection) {
                    eff->close();
                }
                int32_t slot = selection[0]->getSlot();
                std::vector<effectbase*> effects;
                for (effectbase* eff : selection) {
                    audioStage->removePlugin(eff, false);
                    effects.push_back(eff);
                }
                auto* actionRemove = new action_remove_modules("Remove plugins", std::move(effects), audioStage->toRef(), slot);
                daw->pushHist(actionRemove);
                audioStage->pluginsChanged();
                daw->onPluginsChanged();
                handledKeyinput = true;
            }
            break;
        case action_plugin_ctr::PLUGINS_CUT:
            if (!selection.empty()) {
                std::shared_ptr<plugin_clipboard_t> clipboard = copyPluginSelection(sel);
                daw->setPluginClipboard(clipboard);
                audio_stage_t* audioStage = selection[0]->getTrackLink();
                dbgassert(audioStage);
                for (effectbase* eff : selection) {
                    eff->close();
                }
                for (effectbase* eff : selection) {
                    audioStage->removePlugin(eff, false);
                }
                audioStage->pluginsChanged();
                handledKeyinput = true;
            }
            break;
        case action_plugin_ctr::PLUGINS_COPY:
            if (!selection.empty()) {
                std::shared_ptr<plugin_clipboard_t> clipboard = copyPluginSelection(sel);
                daw->setPluginClipboard(clipboard);
                handledKeyinput = true;
            }
            break;
        case action_plugin_ctr::PLUGINS_DUPLICATE:
            if (!selection.empty()) {
                std::shared_ptr<plugin_clipboard_t> clipboard = copyPluginSelection(sel);
                pastePluginClipboard(clipboard, sel.pluginCtr->stage, selection.back()->getSlot() + 1);
                handledKeyinput = true;
            }
            break;
        case action_plugin_ctr::PLUGINS_PASTE:
            if (daw->getPluginClipboard()) {
                std::shared_ptr<plugin_clipboard_t> clipboard = daw->getPluginClipboard();
                int pluginPasteSlot = selection.empty() ? -2 : (selection.back()->getSlot() + 1);
                pastePluginClipboard(clipboard, sel.pluginCtr->stage, pluginPasteSlot);
                handledKeyinput = true;
            }
            break;
    }
    return handledKeyinput;
}
bool guictr_plugins::handleKeyInput(KeyEvent& kevt) {
    if (kevt.type != K_RELEASE) {
        plugin_selection& sel = dawCtrl->getPluginSel();
        if (!sel.pluginCtr) {
            return false;
        }
        auto daw = dawCtrl->getDaw();
        ThreadLock lock = daw->getPlayThread()->lockThread();
        bool handledKeyinput = false;
        if (kevt.type == K_PRESS) {
            if (isKC(KC_SELECTALL, kevt)) {
                handledKeyinput = handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_SELECTALL);
            }
            if (isKC(KC_DELETE, kevt)) {
                handledKeyinput = handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_DELETE);
            } else if (isKC(KC_CUT, kevt)) {
                handledKeyinput = handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_CUT);
            } else if (isKC(KC_COPY, kevt)) {
                handledKeyinput = handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_COPY);
            } else if (isKC(KC_DUPLICATE, kevt)) {
                handledKeyinput = handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_DUPLICATE);
            } else if (isKC(KC_PASTE, kevt) && DawInstance::get()->getPluginClipboard()) {
                handledKeyinput = handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_PASTE);
            }
        }
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y) {
                if (isShift(kevt.mods)) {

                } else {
                }
            } else if (dir.x) {
                if (isShift(kevt.mods)) {

                } else {
                }
            }
            handledKeyinput = true;
        }
        return handledKeyinput;
    }
    return false;
}

void guictr_plugins::hideTrack(audio_stage_t* _track) {
    if (this->stage == _track) {
        this->stage->m_pluginCtr = nullptr;
        this->track            = nullptr;
        this->stage            = nullptr;
        removeGuis();
        layout();
    }
}
void guictr_plugins::onSelected(MouseEvent& evt, guiplugin* plugin) {
    plugin_selection& sel = MainCtrl::get()->getPluginSel();
    if (isShift(evt.kbmods)) {
        if (sel.hasSelection() && sel.pluginCtr == this) {
            if (plugin->effect->getSlot() > sel.lastSelection) {
                sel.lastSelection = plugin->effect->getSlot();
            }
            if (plugin->effect->getSlot() < sel.firstSelection) {
                sel.firstSelection = plugin->effect->getSlot();
            }
        }
    } else {
        sel.pluginCtr      = this;
        sel.firstSelection = plugin->effect->getSlot();
        sel.lastSelection  = plugin->effect->getSlot();
    }
    dawCtrl->onPluginSelected();
}
void guictr_plugins::onChildLayoutChanged(guibase* g) {
    if (!isDefaultPluginCtr) {
        parent->onChildLayoutChanged(this);
    } else {
        showTrack(this->stage);
    }
}
void guictr_plugins::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    guibase* lastGui = NULL;
    int32_t slot     = 0;
    nvgTranslate(vg, -scrolloffset, 0);
    dragdrop_target_indicator_t& target = dawCtrl->getDragDropTarget();
    for (guibase* gui : guis) {
        if (target.dst == this && target.slotIdx == slot) {
            ivec2 posHL(gui->pos.x + (isDefaultPluginCtr ? -4 : 4), 0);
            verticalLineAt(vg, posHL);
            nvgTranslate(vg, 8, 0);
        }
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
        slot++;
        lastGui = gui;
    }
    nvgResetScissor(vg);
    if (target.dst == this) {
        if (target.slotIdx == slot) {

            ivec2 posHL(4, 0);
            if (lastGui) posHL.x += lastGui->right();
            verticalLineAt(vg, posHL);
        }
    }
    nvgResetTransform(vg);
}
void guictr_plugins::relayout() {
    showTrack(this->stage);
}
void guictr_plugins::getEffects(std::vector<effectbase*>& out) {
    if (!this->stage) {
        out.clear();
        log_lf(Log::L_WARN, "Access into dangling guictr_plugins\n");
        return;
    }
    out = this->stage->effects;// copy
}
void guictr_plugins::showTrack(audio_stage_t* audio) {
    removeGuis();
    this->track = audio ? audio->getTrack() : nullptr;
    this->stage = audio;
    if (audio && this->track) {
        audio->m_pluginCtr = this;
        dbgassert(audio->parent || MainCtrl::getPluginCtr() == this);
        if (!audio->effects.empty()) {
            for (effectbase* vst : audio->effects) {
                addGui(vst);
            }
        } else
            add(&placeholder);
        switch (track->type) {
            case TRACK_TYPE_MIDI:
                placeholder.message = "Drop Instruments here";
                break;
            default:
                placeholder.message = "Drop Effects here";
                break;
        }
    }

    layout();
    if (track && isDefaultPluginCtr) {
        setScrolloffset(this->track->scrolloffset);
    }
}

void guictr_plugins::pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) {
    dawCtrl->getDragDropTarget().reset();
    if (!track) return;
    if (g->isSynth()) {
        if (track->type != TRACK_TYPE_MIDI) {
            return;
        }
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{ dragdrop_target_indicator_t::slot_line_vertical, 0, this, this, this->pos };
        return;
    }

    auto slot = slotFromCoord(mousepos);

    dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{ dragdrop_target_indicator_t::slot_line_vertical, slot, this, this, this->pos };
}
int guictr_plugins::slotFromCoord(ivec2 _pos) {
    if (stage->effects.empty())
        return 0;
    int slot = 0;
    for (guibase* gui : guis) {
        if (_pos.x < gui->pos.x + gui->size.x / 2) {
            break;
        }
        slot++;
    }
    return slot;
}
effectbase* gui_vstpluginlist_entry::makeInstance() {
    vstpluginloadres res = vsthost::getInstance()->loadPlugin(entry.path, entry.uid, 0, entry.bugfixFlags);
    if (res.result == 0) {
        res.plugin->localDbId = entry.localDbId;
        return res.plugin;
    }
    return nullptr;
}
effectbase* gui_modulelist_entry::makeInstance() {
    effectbase* instance = vsthost::getInstance()->makeModuleInstance(entry.moduleType, entry.moduleId, -1);
    return instance;
}
class action_insert_effect : public action_base {
    effectbase* effect;
    audio_stage_ref_t ref;
    int32_t dstSlot;
    bool weOwn = false;

protected:
public:
    action_insert_effect(String s, effectbase* _effect, audio_stage_ref_t _ref, int32_t _dst)
        : action_base(), effect(_effect), ref(_ref), dstSlot(_dst) {
        desc = s;
    }
    void releaseResources(DawInstance* daw) override {
        if (weOwn) {
            daw->getHost()->unloadPlugin(this->effect, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
            effect = nullptr;
            weOwn  = false;
        }
    }
    void undo(DawInstance* daw) override {
        ThreadLock lock      = MainCtrl::getPlayThread()->lockThread();
        audio_stage_t* stage = daw->getHost()->getAudioStage(ref);
        if (!stage) {
            setError("missing trackimpl");
            return;
        }
        effect->close();
        vsthost::getInstance()->removePlugin(effect);
        MainCtrl::getPluginCtr()->relayout();
        weOwn = true;
    }
    void redo(DawInstance* daw) override {
        ThreadLock lock      = MainCtrl::getPlayThread()->lockThread();
        audio_stage_t* stage = daw->getHost()->getAudioStage(ref);
        if (!stage) {
            setError("missing trackimpl");
            return;
        }
        vsthost::getInstance()->insertNewPlugin(stage, effect, dstSlot);
        vsthost::getInstance()->postPluginLoaded(stage, effect);
        MainCtrl::getPluginCtr()->relayout();
        weOwn = false;
    }
};

void guictr_plugins::pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) {
    auto const daw = dawCtrl->getDaw();
    auto const host = daw->getHost();
    auto& dragDropTarget = dawCtrl->getDragDropTarget();
    int32_t dstSlot = dragDropTarget.slotIdx;
    dragDropTarget.reset();
    if (!this->stage) return;
    ThreadLock lock    = daw->lockPlayThread();
    effectbase* effect = g->makeInstance();
    if (effect) {
        log_printf("Insert effect on %s, parent %s\n", StringAsCStr(getClassName()), parent ? StringAsCStr(parent->getClassName()) : "<null>");
        host->insertNewPlugin(stage, effect, dstSlot);
        effect->onEnable();
        audio_stage_ref_t refdst = stage->toRef();
        auto* track_action = new action_insert_effect("Insert plugin", effect, refdst, dstSlot);
        daw->pushHist(track_action);
        host->postPluginLoaded(stage, effect);
        //    if (res.result == 0 && res.plugin) {
        //        res.plugin->onEnable();
        //    }
    }
    showTrack(stage);
    if (this->parent) {
        this->parent->onChildLayoutChanged(this);
    }
}
void guictr_dragged_plugins::handleDraggedRelease(MouseEvent& evt) {
    dawCtrl->objectDragRelease(this, evt);
}

void guictr_dragged_plugins::handleDraggedMove(MouseEvent& evt) {
    dawCtrl->objectDragMove(this, evt);
}

void guictr_dragged_plugins::dragMoveOn(guibase* target, ivec2 mousepos) {
    target->pluginMultiDragMove(this, toControlsObjectSpace(mousepos, target));
}

void guictr_dragged_plugins::dragReleaseOn(guibase* target, ivec2 mousepos) {
    target->pluginMultiDragRelease(this, toControlsObjectSpace(mousepos, target));
}
void guictr_dragged_plugins::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
    //        mousepos += dragOffset;
    mousepos -= pos;
    mousepos.x -= size.x / 2;
    nvgTranslate(vg, mousepos.x, mousepos.y);
    drawBackground(vg, theme, pos, size, 0, true, false);
    ivec2 inset = { 2, 2 };
    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
    UIFont::bindFont(vg, instance);
    nvgFillColor(vg, THEMECOL_TEXT);
    Table::DrawTableNVG(this->table, vg, theme, pos + inset, size - inset * 2, HEIGHT_ENTRY - 4);
}
void guictr_dragged_plugins::setStrings(std::vector<String>& list) {
    table.tableWidth  = 200 - (INSET_TABLE<<1);
    table.titleHeight = HEIGHT_ENTRY;
    table.rowHeight   = HEIGHT_ENTRY;
    table.rows.clear();
    for (String s : list) {
        Table::tbl_row_t row;
        row.cols.push_back(s);
        table.rows.push_back(row);
    }
    Table::AdjustColSizes(table);
    size = ivec2(table.tableWidth, table.rows.size() * table.rowHeight) + ivec2(INSET_TABLE << 1);
}
void guictr_plugins::pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) {
    //    log_printf("pluginMultiDragMove %d %d on guictr_plugins %12X\n", mousepos.x, mousepos.y, (int64_t)this);
    dawCtrl->getDragDropTarget().reset();
    if (!this->stage) return;
    audio_stage_t* srcStage = g->getTrackLink();
    for (auto* ptr : g->effects) {
        dbgassert(ptr->getTrackLink() == srcStage);
    }
    dbgassert(srcStage->m_pluginCtr);

    int highlightSlot = slotFromCoord(mousepos);
    if (this->stage == srcStage) {
        int first = g->effects.front()->getSlot();
        int last  = g->effects.back()->getSlot();
        if (highlightSlot >= first && highlightSlot <= last) {
            return;
        }
    } else {
        //prevent dragging onto if any of the effects is parent of this
        audio_stage_t* p = this->stage;
        while (p) {
            if (p->owner && std::find(g->effects.begin(), g->effects.end(), p->owner) != g->effects.end()) {
                return;
            }
            p = p->parent;
        }
//        auto p = this->stage;
//        while (p) {
//
//            p = p->parent;
//        }
//        if (this->stage)
//            for (auto* ptr : g->effects) {
//                ptr->is auto ptr1 = ptr->getTrackLink();
//                if (ptr1 == this->stage) {
//                    return;
//                }
//                if (isAudioStageChildOf(ptr1, this->stage)) {
//                    return;
//                }
//            }
    }
    //  if (abs((evt.dragStart - evt.mousepos).x) > getSizeContent().y / 4) {
    dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
        dragdrop_target_indicator_t::target_area,
        highlightSlot,
        srcStage->m_pluginCtr,
        this,
        { -1, -1 }
    };

    //  }
}
void guictr_plugins::pluginDragMove(guiplugin* g, ivec2 mousepos) {
    dawCtrl->getDragDropTarget().reset();
    if (!this->stage) return;
    effectbase* effect = g->getModule();
    audio_stage_t* trp = effect->getTrackLink();
    dbgassert(trp);
    dbgassert(trp->m_pluginCtr);
    int highlightSlot = slotFromCoord(mousepos);
    //  if (abs((evt.dragStart - evt.mousepos).x) > getSizeContent().y / 4) {
    int curSlot = trp == stage ? (effect->getSlot()) : -2;
    if (trp == this->stage && (curSlot == highlightSlot || curSlot + 1 == highlightSlot)) {
        return;
    }
    dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
        dragdrop_target_indicator_t::target_area,
        highlightSlot,
        trp->m_pluginCtr,
        this,
        { -1, -1 }
    };
    //  }
}
class action_move_modules : public action_base {
    audio_stage_ref_t refdst;
    audio_stage_ref_t refsrc;
    int32_t dst;
    int32_t src;
    int32_t len;

protected:
public:
    action_move_modules(String s, audio_stage_ref_t _refdst, audio_stage_ref_t _refsrc, int32_t _dst, int32_t _src, int32_t _len)
        : action_base(), refdst(_refdst), refsrc(_refsrc), dst(_dst), src(_src), len(_len) {
        desc = s;
    }
    void undo(DawInstance* daw) override {
        audio_stage_t* dstStage = daw->getHost()->getAudioStage(refdst);
        audio_stage_t* srcStage = daw->getHost()->getAudioStage(refsrc);
        if (!dstStage || !srcStage) {
            setError("missing trackimpl");
            return;
        }
        daw->getHost()->movePlugins(srcStage, dstStage, dst, src, len);
        MainCtrl::getPluginCtr()->relayout();
        daw->getHost()->onTrackLayoutChange();
    }
    void redo(DawInstance* daw) override {
        audio_stage_t* dstStage = daw->getHost()->getAudioStage(refdst);
        audio_stage_t* srcStage = daw->getHost()->getAudioStage(refsrc);
        if (!dstStage || !srcStage) {
            setError("missing trackimpl");
            return;
        }
        daw->getHost()->movePlugins(dstStage, srcStage, src, dst, len);
        MainCtrl::getPluginCtr()->relayout();
        daw->getHost()->onTrackLayoutChange();
    }
};
class action_shift_modules : public action_base {
    audio_stage_ref_t ref;
    int32_t dst;
    int32_t src;
    int32_t len;

protected:
public:
    action_shift_modules(String s, audio_stage_ref_t _ref, int32_t _dst, int32_t _src, int32_t _len)
        : action_base(), ref(_ref), dst(_dst), src(_src), len(_len) {
        desc = s;
    }
    void undo(DawInstance* daw) override {
        audio_stage_t* stage = daw->getHost()->getAudioStage(ref);
        if (!stage) {
            setError("missing trackimpl");
            return;
        }
        daw->getHost()->moveEffects(stage, dst, src, len);
        MainCtrl::getPluginCtr()->relayout();
    }
    void redo(DawInstance* daw) override {
        audio_stage_t* stage = daw->getHost()->getAudioStage(ref);
        if (!stage) {
            setError("missing trackimpl");
            return;
        }
        daw->getHost()->moveEffects(stage, src, dst, len);
        MainCtrl::getPluginCtr()->relayout();
    }
};


void removePlugin(DawInstance* daw, effectbase* module) {
    ThreadLock lock           = daw->getPlayThread()->lockThread();
    audio_stage_t* audioStage = module->getTrackLink();
    dbgassert(audioStage);
    module->close();
    audioStage->removePlugin(module, true);
    std::vector<effectbase*> effects;
    effects.push_back(module);
    auto* actionRemove = new action_remove_modules("Remove plugin", std::move(effects), audioStage->toRef(), module->getSlot());
    daw->pushHist(actionRemove);
    audioStage->pluginsChanged();
    daw->onPluginsChanged();
}
void guictr_plugins::pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) {
    // gui_ctr_plugins receiving list of effectbase
    int32_t dstSlot = dawCtrl->getDragDropTarget().slotIdx;
    dawCtrl->getDragDropTarget().reset();
    if (!this->stage) return;
    dbgassert(g->effects.size());

    audio_stage_t* srcStage          = g->getTrackLink();
    audio_stage_t* thisStageOrParent = this->stage;

    // make sure this pluginctrs stage-owner and all parents stage-owners aren't in the list of dragged effectbase instances
    while (thisStageOrParent) {
        if (!thisStageOrParent->parent) {
            dbgassert(thisStageOrParent->owner == nullptr);
        }
        if (thisStageOrParent->parent && std::find(g->effects.begin(), g->effects.end(), thisStageOrParent->owner) != g->effects.end()) {
            return;
        }
        thisStageOrParent = thisStageOrParent->parent;
    }


    ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    int first       = g->effects.front()->getSlot();
    int last        = g->effects.back()->getSlot();

    log_printf("move %d plugins from %s:%d to %s:%d\n",
              (int) g->effects.size(),
              StringAsCStr(srcStage->getTrack()->name), first,
              StringAsCStr(this->stage->getTrack()->name), dstSlot);
    int targetslot = slotFromCoord(mousepos);
    if (srcStage == this->stage) {
        if (targetslot >= first && targetslot <= last) {
            return;
        }
    }

    if (targetslot >= 0) {
        if (srcStage != this->stage) {
            vsthost::getInstance()->movePlugins(this->stage, srcStage, first, targetslot, last - first + 1);

            audio_stage_ref_t refsrc = srcStage->toRef();
            audio_stage_ref_t refdst = stage->toRef();
            auto* track_action       = new action_move_modules("Move plugin", refdst, refsrc, targetslot, first, last - first + 1);
            DawInstance::get()->pushHist(track_action);
        } else {
            if (targetslot > first) targetslot -= g->effects.size();
            //if (targetslot > curSlot) targetslot--;
            if (first == targetslot)
                return;
            vsthost::getInstance()->moveEffects(this->stage, first, targetslot, last - first + 1);
            // audio_stage_ref_t ref = this->stage->toRef();
            //auto* track_action    = new action_shift_modules("Move plugin", ref, targetslot, first, last - first + 1);
            //TODO: make this work
            //DawInstance::get()->pushHist(track_action);
        }
        if (this->parent) {
            this->parent->onChildLayoutChanged(this);
        }
        showTrack(stage);
    }
}
void guictr_plugins::pluginDragRelease(guiplugin* g, ivec2 mousepos) {
    dawCtrl->getDragDropTarget().reset();
    if (!this->stage) return;
    int targetslot     = slotFromCoord(mousepos);
    effectbase* effect = g->getModule();
    audio_stage_t* trp = effect->getTrackLink();
    if (!trp) {
        dbgassert(0 && "TRP WAS NULL");
        return;
    }
    int curSlot = effect->getSlot();
    if (trp == this->stage && (curSlot == targetslot || curSlot + 1 == targetslot)) {
        return;
    }

    if (targetslot >= 0) {
        if (trp != this->stage) {
            vsthost::getInstance()->movePlugins(this->stage, trp, curSlot, targetslot, 1);

            audio_stage_ref_t refsrc = trp->toRef();
            audio_stage_ref_t refdst = stage->toRef();
            auto* track_action       = new action_move_modules("Move plugin", refdst, refsrc, targetslot, curSlot, 1);
            DawInstance::get()->pushHist(track_action);

        } else {
            if (targetslot > curSlot) targetslot--;
            vsthost::getInstance()->moveEffects(trp, curSlot, targetslot, 1);
            audio_stage_ref_t ref = trp->toRef();
            auto* track_action    = new action_shift_modules("Move plugin", ref, targetslot, curSlot, 1);
            DawInstance::get()->pushHist(track_action);
        }
        if (this->parent) {
            this->parent->onChildLayoutChanged(this);
        }
        showTrack(stage);
    } else {
        log_printf("targetslot < 0 %d\n", targetslot);
    }
}

void guictr_pluginview::render(NVGcontext* vg) {
    ivec2 cp = this->getPosContent();
    ivec2 cs = this->getSizeContent();
    bool visible = dawCtrl->isPluginViewVisible();
    bool focused = visible && ctr_plugins->focused();
    if (visible) {
        int topOffset = CTR_SPACING / 2 + 1;
        drawBackground(vg, theme, cp + ivec2(0, -topOffset), cs+ivec2(0, topOffset), margin, focused, false);
    }
    drawInsetBackground(vg, theme, cp, cs);
    ivec2 csp = ctr_plugins->getSizeContent();
    int32_t w = ctr_plugins->getTotalWidth();
    if (cs.x > 0 && cs.y > 0 && csp.x > 0 && csp.y > 0) {
        float scY       = cs.y / (float) csp.y;
        float scContent = math::min(1.0f, csp.x / (float) w);
        float minScale  = math::min((cs.x / (float) math::max(csp.x, w)), scY);
        nvgSave(vg);
        if (setScissorTransform(vg)) {
            nvgScale(vg, minScale, scY);
            for (guibase* gui : ctr_plugins->guis) {
                nvgSave(vg);
                gui->render(vg);
                nvgRestore(vg);
            }
        }
        nvgRestore(vg);
        nvgBeginPath(vg);
        nvgRect(vg, cp.x + ctr_plugins->scrolloffset * minScale, cp.y, cs.x * scContent, cs.y);
        nvgStrokeWidth(vg, 3);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLUGIN_VIEW_FRAME));
        nvgStroke(vg);
    }
}
void guictr_plugins::onTick(AppCtrl* ctrl) {
#define SCROLL_START_X 30
    if (isDefaultPluginCtr && ctrl->guiDragged) {
        guictr_plugins* ctr   = MainCtrl::getPluginCtr();
        ivec2 cs              = ctr->getSizeContent();
        ivec2 screenPosMouse  = ctrl->m_mousePos;
        ivec2 screenPosCtrMin = toScreenSpace(ivec2(scrolloffset, 0));
        ivec2 screenPosCtrMax = screenPosCtrMin + cs;
        if (screenPosMouse.y >= screenPosCtrMin.y && screenPosMouse.y <= screenPosCtrMax.y) {
            if (screenPosMouse.x < screenPosCtrMin.x + SCROLL_START_X && scrolloffset > 0) {
                setScrolloffset(scrolloffset - (int) ((TIMER_MS / 50.0) * 40));
                ctrl->requestRedraw();
            } else if (screenPosMouse.x > screenPosCtrMax.x - SCROLL_START_X && scrolloffset < getTotalWidth() - cs.x) {
                setScrolloffset(scrolloffset + (int) ((TIMER_MS / 50.0) * 40));
                ctrl->requestRedraw();
            }
        }
    }
    for (guibase* gui : guis) {
        gui->onTick(ctrl);
    }
}
void guictr_plugins::layout() {
    determineSize(size);
    //    for (guibase* gui : guis) {
    //        gui->layout();
    //    }
}
void guictr_plugins::determineSize(glm::ivec2& prefSize) {

    ivec2 sizeInset     = prefSize - (paddingTL(padding) + paddingBR(padding));
    int32_t guiH        = sizeInset.y - margin;
    int32_t titleHeight = math::min(((320 / 8) >> 1) << 1, ((guiH / 8) >> 1) << 1);
    theme->set(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, titleHeight);


    int32_t inset = margin / 2;
    ivec2 gPos(inset * 3, 0);
    for (guibase* gui : guis) {
        gui->pos  = gPos;
        gui->size = { guiH, guiH };
        gui->determineSize(gui->size);
        gui->pos.y = inset;
        gPos.x += gui->size.x + margin * 2;
        gui->layout();
    }
    if (!isDefaultPluginCtr) {
        int32_t maxX = 0;
        for (guibase* gui : guis) {
            maxX = math::max(gui->right(), maxX);
        }
        prefSize.x = maxX;
    }
}
action_remove_modules::action_remove_modules(String s, std::vector<effectbase*>&& _effects, audio_stage_ref_t _ref, int32_t _dst) : action_base(), effects(_effects), ref(_ref), dstSlot(_dst) {
    desc = s;
    assert(effects.size());
}

void action_remove_modules::releaseResources(DawInstance* daw) {
    if (weOwn) {
        for (effectbase* eff : effects) {
            daw->getHost()->unloadPlugin(eff, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
        }
        effects.clear();
        weOwn = false;
    }
}

void action_remove_modules::undo(DawInstance* daw) {
    ThreadLock lock      = MainCtrl::getPlayThread()->lockThread();
    audio_stage_t* stage = daw->getHost()->getAudioStage(ref);
    if (!stage) {
        setError("missing trackimpl");
        return;
    }
    int32_t slot = 0;
    for (effectbase* eff : effects) {
        daw->getHost()->insertNewPlugin(stage, eff, dstSlot + slot);
        daw->getHost()->postPluginLoaded(stage, eff);
        dbgassert(eff->getSlot() == dstSlot + slot);
        slot++;
    }
    MainCtrl::getPluginCtr()->relayout();
    weOwn = false;
}

void action_remove_modules::redo(DawInstance* daw) {
    ThreadLock lock      = MainCtrl::getPlayThread()->lockThread();
    audio_stage_t* stage = daw->getHost()->getAudioStage(ref);
    if (!stage) {
        setError("missing trackimpl");
        return;
    }
    dbgassert(effects[0]->getSlot() == dstSlot);
    for (effectbase* eff : effects) {
        eff->close();
        daw->getHost()->removePlugin(eff);
    }
    MainCtrl::getPluginCtr()->relayout();
    weOwn = true;
}
