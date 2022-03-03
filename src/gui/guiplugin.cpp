#include "guiplugin.h"
#include <nanovg.h>
#include <memory>
#include "snapshot.h"
#include "str_util.h"
#include "logging.h"
#include "event.h"
#include "keyboard.h"
#include "edithistory.h"
#include "renderresources.h"

#include "gui.h"
#include "button.h"
#include "knob.h"
#include "list.h"
#include "theme.h"
#include "table.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "guitooltip.h"
#include "pluginviewcontainers.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu_daw.h"
#include "pluginctr.h"
#include "pluginlist.h"
#include "trackcontent.h"

#include "basectrl.h"

#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugin/base_plugin.h"
#include "../host/plugin/internal_plugin.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "../host/vst_window.h"
#include "automatable.h"
#include "debugproperties.h"
#include "projectfile-snapshot.h"

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

void setDraggedPluginsUI(guictr_dragged_plugins& gui, plugin_selection& sel);

guiplugin::~guiplugin() {
    remove(&buttonLayout);
    remove(&buttonDelete);
    remove(&buttonBypass);
    remove(&buttonSave);
    remove(&meter);
    for (auto g : guiButtonsTitlebar) {
        dbgassert(!stl_contains(guis, g));
    }
}
void guiplugin::addGuiBtnTitlebar(guibuttontoggle* btn) {
    guiButtonsTitlebar.push_back(btn);
    add(btn);
}

void guiplugin::render(NVGcontext* vg) {
    renderBase(vg);
    for (guibase* gui : guis) {
        if (!gui->isVisible())
            continue;
        if (gui->size.x < 0) {
            log_printf("gui size x %d %s\n", gui->size.x, StringAsCStr(gui->label));
            continue;
        }
        gui->render(vg);
    }
}
void guiplugin::prerender(NVGcontext* vg) {
    guictr_base::prerender(vg);
    if (effect->getParamUnchecked(PARAM_ENABLE)) {
        auto at = effect->getRegisteredAutomation(PARAM_ENABLE);
        if (at && at->isAutomated()) {
            buttonBypass.colorActive = GuiColor::COL_AUTOMATED;
        } else {
            buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
        }
    }
}
void guiplugin::determineSize(ivec2& prefSize) {
    if (layoutMode == 1) {
        //dbgassert(module->getAudioStage());
        //
        const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
        //int32_t meterW = math::max(16, (int32_t)(theme->get(GuiConstant::CONST_METER_WIDTH)*hpt/32.0));
        //prefSize.x = hpt+ctr.size.x+meterW;
        prefSize.x = hpt;
    } else {

        prefSize.x = prefSize.y;
    }
}

void guiplugin::buttonClicked(guibase* _button) {
    if (_button == &buttonLayout) {
        layoutMode        = (layoutMode + 1) % 2;
        isHorizontalTitle = layoutMode == 0;
        buttonLayout.icon = layoutMode == 0 ? ICON_ARR_RIGHT : ICON_ARR_DOWN;
        parent->onChildLayoutChanged(this);
        return;
    }
    if (_button == &buttonBypass) {
        ThreadLock lock = dawCtrl->lockPlayThread();
        toggleDeviceEnableState(effect, FLG_PAR_UPDATE_USER);
    }
    if (_button == &buttonSave) {
        ThreadLock lock = dawCtrl->lockPlayThread();

        plugin_snapshot_t ps;
        effect->makeSnapshot(ps, tracksnapshot_store_opts_t::All());
        String path;
        auto window = dawCtrl->window;
        if (promptUserFilePath(window, 1, vFILE_TYPE_PLUGINSNAPSHOT, path)) {
            savePluginSnapshot(ps, path);
        }
        return;
    }
    if (_button == &buttonDelete) {
        removePlugin(dawCtrl->getDaw(), effect);
    }
}
bool guiplugin::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (contains(mpos)) {
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
            return false;
        }
        ivec2 localMouse = this->toContainerSpace(mpos);
        for (guibase* gui : guis) {
            if (!gui->isVisible())
                continue;
            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (isShift(evt.kbmods)) {
            if (dawCtrl->getPluginSel().pluginCtr != this->parent) {
                return true;
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}
guiplugin::guiplugin(effectbase* _effect)
    : guictr_base(),
      effect(_effect),
      meter(&_effect->meter) {
    padding = 0;
    margin  = 0;
    text[0] = 0;
    buttonBypass.setLabel("Bypass");
    buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
    buttonBypass.icon        = ICON_BYPASS;
    buttonBypass.fnGetState  = [_effect]() {
        return _effect->getParamValue(PARAM_ENABLE) > 0;
    };
    //buttonBypass.setTint(0x80c040);
    buttonDelete.setLabel("Remove");
    buttonDelete.icon = ICON_CLOSE;
    buttonLayout.icon = ICON_ARR_RIGHT;
    buttonLayout.setLabel("Hide");
    buttonSave.icon = ICON_SAVE;
    buttonSave.setLabel("Save");
    add(&meter);
    addGuiBtnTitlebar(&buttonBypass);
    addGuiBtnTitlebar(&buttonLayout);
    addGuiBtnTitlebar(&buttonDelete);
    addGuiBtnTitlebar(&buttonSave);
    //buttonDelete.setTint(0x404040);
}
void guiplugin::rightClicked(MouseEvent& evt, guibase* button) {
    int32_t clickedParamIdx = -1;
    if (button == &this->buttonBypass) {
        clickedParamIdx = PARAM_ENABLE;
    }
    if (clickedParamIdx != -1) {
        auto* ctxt = new guictxtmenu_at_param(dawCtrl, effect, clickedParamIdx);
        parentCtrl->openContextMenu(ctxt, evt.mousepos);
    }
}
void guiplugin::layout() {
    const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
    int buttonSize    = hpt * 0.8;
    int32_t inset1    = (hpt - buttonSize) / 2;
    ivec2 btnPos      = { inset1, inset1 };
    buttonLayout.pos  = btnPos;
    btnPos[isHorizontalTitle ? 0 : 1] += buttonSize;
    for (auto btn : guiButtonsTitlebar) {
        btn->size = { buttonSize, buttonSize };
        btn->setRadius(hpt / 3.f);
        if (btn == &buttonLayout) {
            continue;
        }
        if (btn == &buttonDelete) {
            continue;
        }
        btn->pos = btnPos;
        btnPos[isHorizontalTitle ? 0 : 1] += buttonSize;
    }
    if (isHorizontalTitle) {
        buttonDelete.pos = { size.x - buttonDelete.size.x - inset1, inset1 };
    } else {
        buttonDelete.pos = { inset1, size.y - buttonDelete.size.y - inset1 };
    }


    int32_t meterW = math::max(16, (int32_t) (theme->get(GuiConstant::CONST_METER_WIDTH) * hpt / 32.0));
    ivec2 contentS;
    ivec2 contentP;
    if (isHorizontalTitle) {
        contentP  = ivec2(0, hpt);
        contentS  = ivec2(size.x - meterW, size.y - hpt);
        titlePosX = btnPos.x;
    } else {
        contentP  = ivec2(hpt, 0);
        contentS  = ivec2(size.x - hpt - meterW, size.y);
        titlePosX = buttonDelete.top();
    }
    meter.pos  = ivec2(size.x - meterW, hpt);
    meter.size = ivec2(meterW, size.y - hpt);
    layoutModule(contentP, contentS, inset1);
    for (auto btn : guis) {
        btn->layout();
    }
}
void guiplugin::renderBase(NVGcontext* vg) {
    if (!setScissorTransformContainer(vg)) {
        return;
    }
    renderFrameBase(vg);
    int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
    if (isSelected()) {
        flags |= FLAG_SELECTED;
    }
    renderTitleBar(vg, getSizeContent(), this->text, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, titlePosX, flags, isHorizontalTitle);
    renderFrameOutline(vg);
}

void guiplugin::handleDraggedMove(MouseEvent& evt) {
    hasDragged = false;
    if (isSelected()) {
        auto& sel = dawCtrl->getPluginSel();
        if (sel.hasSelection()) {
            setDraggedPluginsUI(sel.pluginCtr->dragged, sel);
            parentCtrl->setDragged(&sel.pluginCtr->dragged);
            hasDragged = true;
        }
    } else {
        dawCtrl->objectDragMove(this, evt);
    }
}
void guiplugin::handleDraggedRelease(MouseEvent& evt) {
    dawCtrl->objectDragRelease(this, evt);
    if (hasDragged) {
        assert(0);
        return;
    }
    if (isSelected()) {
        static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
    }
}
void guiplugin::handleDraggedBegin(MouseEvent& evt) {
    hasDragged = false;
    if (!isSelected()) {
        //hasDragged = true;
        static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
    }
}

//enum action_plugin_ctr {
//SELECTALL, DELETE, CUT, COPY, PASTE, DUPLICATE
//};
//bool handlePluginCtrCommand(action_plugin_ctr action);
debugproperties* makeUniquePropertiesCtr();
class guictxtmenu_plugin : public guictxtmenu {
    effectbase* const effect;
public:
    static constexpr int CMD_SHOW_AUTOMATION = 1;
    static constexpr int CMD_SHOW_PARAM_LIST = 2;
    static constexpr int CMD_DUPLICATE       = 3;
    static constexpr int CMD_DELETE          = 4;
    static constexpr int CMD_COPY            = 5;
    static constexpr int CMD_CUT             = 6;
    static constexpr int CMD_PASTE           = 7;
    guictxtmenu_plugin(DawCtrl* _dawCtrl, effectbase* _effect)
        : effect(_effect)
    {
        this->dawCtrl = _dawCtrl;
        this->size.x = 260;
        addEntry(new ctxtmenu_entry("Show all automation", CMD_SHOW_AUTOMATION));
        addEntry(new ctxtmenu_entry("Show parameter list", CMD_SHOW_PARAM_LIST));
        addEntry(new ctxtmenu_splitter());
        addEntry(new ctxtmenu_entry("Copy", CMD_COPY));
        addEntry(new ctxtmenu_entry("Cut", CMD_CUT));
        addEntry(new ctxtmenu_entry("Paste", CMD_PASTE));
        addEntry(new ctxtmenu_entry("Duplicate", CMD_DUPLICATE));
        addEntry(new ctxtmenu_entry("Delete", CMD_DELETE));
    }
    void clicked(int _id) override {
        ThreadLock lock = dawCtrl->lockPlayThread();
        if (_id == CMD_SHOW_PARAM_LIST) {
            auto* gui = effect->getGui();
            if (gui) {
                debugproperties* dbgPropertiesCtrPopup = makeUniquePropertiesCtr();
                guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
                ctxtMenu->size = { 640, 480 };
                ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
                ivec2 wndPos{ 0 };
                this->parentCtrl->window->getPos(&wndPos);
                dbgPropertiesCtrPopup->setDebugPropertyHandle(gui);
                dawCtrl->openContextMenu(ctxtMenu, wndPos, 2);
                return;
            }
        }
        if (_id == CMD_DELETE) {
            handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_DELETE);
        }
        if (_id == CMD_COPY) {
            handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_COPY);
        }
        if (_id == CMD_CUT) {
            handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_CUT);
        }
        if (_id == CMD_PASTE) {
            handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_PASTE);
        }
        if (_id == CMD_PASTE) {
            handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_COPY);
        }
        if (_id == CMD_DUPLICATE) {
            handlePluginCtrCommand(dawCtrl, action_plugin_ctr::PLUGINS_DUPLICATE);
        }
        if (_id == CMD_SHOW_AUTOMATION) {
            auto tr = effect->getTrack();
            auto trCtr = dawCtrl->getTrackContainer();
            gui_track_automationlane* gtr_at = nullptr;
            if (tr) {
                track_gui_entry_t* entry = nullptr;
                if (!trCtr->getTrackEntry(tr, &entry)) {
                    dbgassert(0);
                } else {
                    entry->layout.hideTrack     = false;
                    entry->layout.hideSubtracks = false;
                    updateStoreLoadSubtracks(trCtr, entry);

                    std::vector<int32_t> automated;
                    effect->getAutomated(automated);
                    for (int32_t param : automated) {
                        auto lane = trCtr->addAutomationLane(entry, effect, param, true);
                        if (!gtr_at) {
                            gtr_at = lane;
                        }
                    }
                }
            }
            if (trCtr && gtr_at) {
                dawCtrl->updateVisibleTrackContents();
                trCtr->scrollTo(gtr_at);
            }
        }
        closeContextMenu();
    }
};
void guiplugin::handleRightClick(MouseEvent& evt) {
    handleDraggedBegin(evt);
    const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
    bool b            = false;
    if (isHorizontalTitle) {
        b = evt.relMousepos.y < hpt;
    } else {
        b = evt.relMousepos.x < hpt;
    }
    if (b) {
        parentCtrl->openContextMenu(new guictxtmenu_plugin(dawCtrl, effect), evt.mousepos);
    }
}
void guiplugin::dragMoveOn(guibase* target, ivec2 mousepos) {
    target->pluginDragMove(this, toControlsObjectSpace(mousepos, target));
}
void guiplugin::dragReleaseOn(guibase* target, ivec2 mousepos) {
    target->pluginDragRelease(this, toControlsObjectSpace(mousepos, target));
}

bool guiplugin::focusEvent(MouseHitEvt& evt, bool focused) {
    return true;
}
void guiplugin::setControl(BaseCtrl* parentCtrl) {
    guictr_base::setControl(parentCtrl);
}
guibase* guiplugin::getDraggedControl() {
    return this;
}
bool guiplugin::isSelected() {
    dbgassert(this->parentCtrl);
    if (!this->parentCtrl->guiCtrFocused) {
        return false;
    }
    auto& sel = dawCtrl->getPluginSel();
    if (!sel.hasSelection())
        return false;
    if (sel.pluginCtr == this->parent) {
        if (this->effect->getSlot() >= sel.firstSelection &&
            this->effect->getSlot() <= sel.lastSelection) {
            return isChildOf(this->parentCtrl->guiCtrFocused);
        }
        return false;
    }
    if (this->parent) {
        return this->parent->isSelected();
    }
    return false;
}
void effectbase::addPropertiesParameterList(Table::tbl& table) {
    table.tableWidth = 450;
    table.colSizes.push_back(180);
    table.colSizes.push_back(30);
    table.colSizes.push_back(60);
    table.colSizes.push_back(60);
    std::vector<tbl_row_t>& rows = table.rows;
    std::vector<automatable_param_t*> sortedParams;
    this->getSortedParams(sortedParams);
    rows.push_back({{tblString{"Name"}, tblString{"Unit"}, tblString{"Value"}, tblString{"idx"}, tblString{"internalIdx"}, tblString{"flags"}, tblString{"category"}, tblString{"Step"}}});
    for (automatable_param_t* param : sortedParams) {
        tbl_row_t row;
        row.cols.push_back(tblString{param->name});
        row.cols.push_back(tblString{param->unit});
        row.cols.push_back(tblString{StringFormat("%0.4f", param->value)});
        row.cols.push_back(tblint{param->idx});
        row.cols.push_back(tblint{param->internalIdx});
        row.cols.push_back(tblint{param->flags});
        row.cols.push_back(tblint{param->category});
        if (param->flags & ParamUsesFloatStep) {
            row.cols.push_back(tblString{StringFormat("Float %f %f %f", param->stepSmall.valFloat, param->step.valFloat, param->stepLarge.valFloat)});
        } else if (param->flags & ParamUsesIntStep) {
            row.cols.push_back(tblString{StringFormat("Int %d %d %d", param->stepSmall.valInt, param->step.valInt, param->stepLarge.valInt)});
        } else {
            row.cols.push_back(tblString{"None"});
        }
        rows.push_back(row);
    }
}
void effectbase::addPropertiesTooltip(Table::tbl& table) {
    table.tableWidth = 350;
    table.colSizes.push_back(150);
    table.rows.push_back({ { String("projectGlobalId"), (int) this->projectGlobalId } });
    table.rows.push_back({ { tblstr{ "track" }, tblint{ (int64_t) this->getTrack(), "%12x" } } });
    table.rows.push_back({ { tblstr{ "tracklink" }, tblint{ (int64_t) this->getTrackLink(), "%12x" } } });
    table.rows.push_back({ { tblstr{ "bIsSetup" }, tblint{ this->bIsSetup } } });
    table.rows.push_back({ { tblstr{ "bIsEnabled" }, tblint{ this->bIsEnabled } } });
    table.rows.push_back({ { tblstr{ "PARAM_ENABLE" }, tblfloat{ this->getParamValue(PARAM_ENABLE) } } });
}
template<>
void guitooltip<guiplugin>::setContent() {
    ptr->effect->addPropertiesTooltip(table);
}

guictxtmenu_base* guiplugin::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<guiplugin>(this);
    return tooltip;
}


class gui_plugin_paramlist_entry : public gui_list_entry {

    const float spacing = INSET_TITLE;

public:
    effectbase* const effect;
    automatable_param_t* const entry;
    guiknob knobTest;
    gui_plugin_paramlist_entry(effectbase* _effect, automatable_param_t* _entry)
        : gui_list_entry(),
          effect(_effect),
          entry(_entry),
          knobTest(false) {
        icon = 0;
        knobTest.setAutomationRef(effect, entry->idx);
        knobTest.setAutomationHandlers();
        knobTest.setParent(this);
    }
    void handleRightClick(MouseEvent& evt) override {
        parentCtrl->openContextMenu(new guictxtmenu_at_param(dawCtrl, effect, entry->idx), evt.mousepos);
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            if (evt.type != MouseHitType::MOUSE_RIGHT) {
                if (knobTest.mouseHitTest(mpos, evt)) {
                    return true;
                }
            }
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guibase::setControl(parentCtrl);
        knobTest.setControl(parentCtrl);
    }
    void setParent(guibase* parent) override {
        guibase::setParent(parent);
        dbgassert(knobTest.parent == this);
    }
    String getText() override {
        return entry->name;
    }
    void layout() override {
        knobTest.pos  = pos + ivec2(spacing);
        knobTest.size = ivec2(size.y, size.y) - ivec2(spacing * 2);
    }
    void render(NVGcontext* vg) override {
        float rowHeight = size.y;
        float x         = knobTest.right() + spacing;
        if (dawCtrl->isCtrOrChildFocused(this)) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
            nvgFill(vg);
        }
        nvgTranslate(vg, pos.x, pos.y);
        if (rowHeight > 32) {
            setFont(vg, (int) (rowHeight * 0.4), G_WHITE, G_TITLE_ALIGN);
            nvgText(vg, x, rowHeight * 0.25, StringAsCStr(getText()), nullptr);
            String sValue = effect->getParamValueDisplay(entry->idx);
            nvgText(vg, x, rowHeight * 0.5 + rowHeight * 0.25, StringAsCStr(sValue), nullptr);
        } else {
            setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
            nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), nullptr);
        }
        nvgTranslate(vg, -pos.x, -pos.y);

        if (knobTest.size.y >= 4) {
            knobTest.render(vg);
        }
    }
};

class guipluginview_preview : public guictr_base {
    vstplugin* const plugin;
    guipluginview* const guivst;
    int tex = -1;
    ivec2 sizeTex;

public:
    guipluginview_preview(vstplugin* _plugin, guipluginview* _guivst)
        : guictr_base(), plugin(_plugin), guivst(_guivst), sizeTex{ 0, 0 } {
        padding = 0;
        margin  = 0;
    }
    ~guipluginview_preview() override = default;
    void determineSize(ivec2& prefSize) override {
        if (sizeTex.x && sizeTex.y) {
            prefSize.x = (int) ((sizeTex.x / (float) sizeTex.y) * prefSize.y);
        }
    }
    int nFrame = 0;
    void prerender(NVGcontext* vg) override {
        //TODO: resource management
        //if (nFrame++<20)
        //return;
        //nFrame = 0;
        auto window = plugin->window;
        if (window && guivst) {
            if (plugin->requestCaptureGUI >= 1) {
                plugin->requestCaptureGUI++;
                if (plugin->requestCaptureGUI >= 33) {
                    window->captureWindowFrame();
                    plugin->requestCaptureGUI = -1;
                }
            } else if (plugin->requestCaptureGUI == -1) {
                plugin->requestCaptureGUI = 0;
                auto& frame               = window->capturedFrame;
                if (frame.w && frame.h && frame.bytes.size()) {
                    if (tex > 0 && (frame.w != sizeTex.x || frame.h != sizeTex.y)) {
                        nvgDeleteImage(vg, tex);
                        tex = -1;
                    }
                    if (tex < 0) {
                        tex     = nvgCreateImageRGBA(vg, frame.w, frame.h, 0, (const unsigned char*) nullptr);
                        sizeTex = { frame.w, frame.h };
                    }
                    std::vector<uint8_t> tmpData = frame.bytes;
////                    tmpData.resize(frame.w * frame.h * 4);
//                    for (int _x = 0; _x < frame.w; _x++) {
//
//                        for (int _y = 0; _y < frame.h; _y++) {
//                            int idx              = _x * frame.h + _y;
//                            tmpData[idx * 4 + 0] = 0xff;
////                            tmpData[idx * 4 + 1] = 0xff;
////                            tmpData[idx * 4 + 2] = 0xff;
//                            tmpData[idx * 4 + 3] = 0xff;
//                        }
//                    }
                    nvgUpdateImage(vg, tex, frame.bytes.data());
                    MainCtrl::getPluginCtr()->relayout();
                } else if (tex > 0) {
                    nvgDeleteImage(vg, tex);
                    tex     = -1;
                    sizeTex = { frame.w, frame.h };
                }
            }
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        if (tex > 0) {
            drawImage(vg, tex, 1.0f, 0, 0, sizeTex.x, sizeTex.y, 0, 0, size.x, size.y);
        }
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
};
void guipluginview::updateParamList(const String& strParamNameFilter) {
    std::vector<automatable_param_t*> paramsAutomated;
    std::vector<automatable_param_t*> paramsRest;
    effect->getSortedParamsSeperate(paramsAutomated, paramsRest);
    std::vector<gui_list_entry*> listEntries;
    paramsAutomated.insert(paramsAutomated.end(), paramsRest.cbegin(), paramsRest.cend());
    listEntries.reserve(paramsAutomated.size());
    std::for_each(paramsAutomated.begin(), paramsAutomated.end(), [&listEntries, eff = this->effect, &strParamNameFilter](auto* param) {
        if (strParamNameFilter.empty() || StringContainsCI(param->name, strParamNameFilter) >= 0) {
            listEntries.push_back(new gui_plugin_paramlist_entry(eff, param));
        }
    });
    params.setList(listEntries);
}
guipluginview::guipluginview(effectbase* _effect)
    : guiplugin(_effect), effect(_effect), dropdownProgram(_effect) {
    params.setRowHeight(48);
    textFieldSearchBox.setParent(this);
    textFieldSearchBox.setChangeCallback([this](const String& str) {
        updateParamList(str);
        return true;
    });
    textFieldSearchBox.setPlaceholder("Search");
    buttonOpenEditor.icon = ICON_ADJUST;
    buttonOpenEditor.setStateRef(&_effect->bEditOpen);
    buttonOpenEditor.setParent(this);
    buttonOpenEditor.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
    addGuiBtnTitlebar(&buttonOpenEditor);
    params.setParent(this);
    buttonShowInlineGUI.icon = ICON_ADJUST;
    buttonShowInlineGUI.setStateRef(&_effect->bCaptureGUI);
    buttonShowInlineGUI.setParent(this);
    buttonShowInlineGUI.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
    dropdownProgram.setParent(this);
    updateParamList("");
    if (_effect->pluginType == PLUGIN_TYPE_VST) {
        dbgassert(dynamic_cast<vstplugin*>(_effect));
        ctrPreview = new guipluginview_preview(dynamic_cast<vstplugin*>(_effect), this);
        ctrPreview->setVisible(false);
        viewCtrs.push_back(ctrPreview);
        addGuiBtnTitlebar(&buttonShowInlineGUI);
    }
}

guipluginview::~guipluginview() {
    remove(&buttonOpenEditor);
    //will propably fall on the nose with accessing _effect in the destructor here
    if (effect->pluginType == PLUGIN_TYPE_VST) {
        remove(&buttonShowInlineGUI);
    }
    if (ctrPreview) {
        delete ctrPreview;
    }
}
void guipluginview::setControl(BaseCtrl* parentCtrl) {
    guiplugin::setControl(parentCtrl);
    params.setControl(parentCtrl);
    dropdownProgram.setControl(parentCtrl);
    textFieldSearchBox.setControl(parentCtrl);
    for (auto* ctr : viewCtrs) {
        ctr->setControl(parentCtrl);
    }
}

void guipluginview::prerender(NVGcontext* vg) {
    guiplugin::prerender(vg);
    for (auto* ctr : viewCtrs) {
        assert(!ctr->parent);
        ctr->prerender(vg);
    }
}
void guipluginview::determineSize(glm::ivec2& prefSize) {
    if (layoutMode == 1) {
        guiplugin::determineSize(prefSize);
        return;
    }
    const int32_t hpt = parent->theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
    int32_t meterW    = math::max(16, (int32_t) (theme->get(GuiConstant::CONST_METER_WIDTH) * hpt / 32.0));
    ivec2 contentS;
    ivec2 contentP;

    if (isHorizontalTitle) {
        contentP = ivec2(0, hpt);
        contentS = ivec2(prefSize.x - meterW, size.y - hpt);
    } else {
        contentP = ivec2(hpt, 0);
        contentS = ivec2(size.x - hpt - meterW, size.y);
    }
    sizeCtrs = { 0, contentS.y };

    if (viewCtr) {
        ivec2 sizeCtr;
        viewCtr->getFixedSize(&sizeCtr.x, &sizeCtr.y);
        sizeCtr.x = (int) ((sizeCtr.x / (float) sizeCtr.y) * contentS.y);
        sizeCtr.y = sizeCtrs.y;
        viewCtr->layout(sizeCtr.x, sizeCtr.y);
        sizeCtrs.x += sizeCtr.x;
    }
    if (ctrPreview && ctrPreview->isVisible()) {
        ivec2 sizeCtr{ sizeCtrs.y, sizeCtrs.y };
        ctrPreview->determineSize(sizeCtr);
        sizeCtrs.x += sizeCtr.x;
    }
    prefSize.y = math::max(sizeCtrs.y, prefSize.y);
    prefSize.x += sizeCtrs.x;
}
void guipluginview::render(NVGcontext* vg) {
    renderBase(vg);
    for (auto* btn : guiButtonsTitlebar) {
        if (btn->isVisible())
            btn->render(vg);
    }
    if (layoutMode != 1) {
        for (auto* ctr : viewCtrs) {
            if (ctr->isVisible()) {
                nvgSave(vg);
                ctr->render(vg);
                nvgRestore(vg);
            }
        }
        if (meter.isVisible())
            meter.render(vg);
        if (dropdownProgram.isVisible()) {
            dropdownProgram.render(vg);
        }
        if (textFieldSearchBox.isVisible()) {
            textFieldSearchBox.render(vg);
        }
        if (params.isVisible()) {
            if (params.isBackgroundRendered()) {
                params.renderBackground(vg);
            }
            params.render(vg);
        }
    }
}
bool guipluginview::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
        return false;
    }
    if (contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        for (auto* btn : guiButtonsTitlebar) {
            if (btn->isVisible() && btn->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        for (auto* ctr : viewCtrs) {
            if (ctr->isVisible() && ctr->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (params.isVisible() && params.mouseHitTest(localMouse, evt)) {
            return true;
        }
        if (dropdownProgram.isVisible() && dropdownProgram.mouseHitTest(localMouse, evt)) {
            return true;
        }
        if (textFieldSearchBox.isVisible() && textFieldSearchBox.mouseHitTest(localMouse, evt)) {
            return true;
        }
        if (isShift(evt.kbmods)) {
            if (MainCtrl::get()->getPluginSel().pluginCtr != this->parent) {
                return true;
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}
void guipluginview::buttonClicked(guibase* _button) {
    guiplugin::buttonClicked(_button);
    if (_button == &buttonOpenEditor) {
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        if (effect->bEditOpen) {
            effect->close();
        } else {
            effect->show();
        }
    }
    if (_button == &buttonShowInlineGUI) {
        effect->bCaptureGUI = !effect->bCaptureGUI;
        if (effect->bCaptureGUI) {
            ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
            if (effect->bEditOpen) {
                effect->close();
            }
            effect->requestCaptureGUI = 1;
            effect->show();
        }
    }
    if (ctrPreview) {
        ctrPreview->setVisible(effect->bCaptureGUI);
    }

    dropdownProgram.setVisible(layoutMode == 0 && effect->programNames.size());
    params.setVisible(layoutMode == 0);
    meter.setVisible(layoutMode == 0);
    this->onChildLayoutChanged(this);
}
void guipluginview::layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) {
    const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
//    buttonOpenEditor.size = buttonBypass.size;
//    buttonOpenEditor.setRadius(buttonBypass.radius);
//    buttonOpenEditor.pos.y = inset1;
//    buttonOpenEditor.pos.x = buttonBypass.right();
//    titlePosX              = buttonOpenEditor.right();
    contentS.x         = math::max(64, contentS.x);
    contentS.y         = math::max(64, contentS.y);
    int32_t insetCtrls = INSET_TITLE;
    int rowHeight      = 64;
    while (contentS.y < rowHeight * 8 && rowHeight > 8) {
        rowHeight -= 4;
    }
    int heightRow = hpt * 0.7;
    textFieldSearchBox.setVisible(layoutMode == 0 && heightRow >= 20);
    dropdownProgram.setVisible(layoutMode == 0 && effect->programNames.size() && heightRow >= 20);
    int hDropDown = 0;

    if (dropdownProgram.isVisible()) {
        hDropDown += hpt * 0.7;
    }
    if (textFieldSearchBox.isVisible()) {
        hDropDown += hpt * 0.7;
    }
    int paramsW = contentS.x - sizeCtrs.x;
    params.setRowHeight(rowHeight);
    params.pos  = ivec2(insetCtrls, insetCtrls + hpt + hDropDown);
    params.size = ivec2(paramsW, contentS.y - hDropDown) - ivec2(insetCtrls * 2);
    params.layout();
    int topOffset = 0;
    if (dropdownProgram.isVisible()) {
        dropdownProgram.pos  = ivec2(insetCtrls * 2, insetCtrls + hpt + topOffset);
        dropdownProgram.size = ivec2(paramsW, hpt * 0.7) - ivec2(insetCtrls * 4, 0);
        dropdownProgram.layout();
        topOffset += hpt * 0.7;
    }
    if (textFieldSearchBox.isVisible()) {
        textFieldSearchBox.pos  = ivec2(insetCtrls * 2, insetCtrls + hpt + topOffset);
        textFieldSearchBox.size = ivec2(paramsW, hpt * 0.7) - ivec2(insetCtrls * 4, 0);
        textFieldSearchBox.layout();
        topOffset += hpt * 0.7;
    }
    int left = params.right() + INSET_TITLE;
    if (viewCtrs.size()) {
        for (auto* ctr : viewCtrs) {
            if (ctr->isVisible()) {
                ctr->pos          = ivec2(left, 0) + ivec2(insetCtrls, insetCtrls + hpt);
                ivec2 prefSizeCtr = ivec2(ctr->size.x, contentS.y) - ivec2(insetCtrls * 2);
                ctr->determineSize(prefSizeCtr);
                ctr->size = prefSizeCtr;
                ctr->layout();
                left = ctr->right() + INSET_TITLE;
            }
        }
    }
}


guidropdown_select_program::guidropdown_select_program(effectbase* _plugin) : plugin(_plugin) {
    this->size.x   = 120;
    this->fontSize = FONT_SIZE_CTXT_SMALL;
    this->paddingV = 0;
    int32_t idx    = 0;
    for (auto str : plugin->programNames) {
        addEntry(new ctxtmenu_entry(str, idx));
        idx++;
    }
}

void guidropdown_select_program::clicked(int _id) {
    closeContextMenu();
    if (_id >= 0 && _id < plugin->programNames.size()) {
        plugin->setCurrentProgram(_id);
    }
}

String guidropdownprogram::getString() {
    String s = "";
    plugin->getCurrentProgramName(s);
    return s;
}

void guidropdownprogram::handleDraggedRelease(MouseEvent& evt) {
    if (plugin) {
        guictxtmenu_base* popup = new guidropdown_select_program(plugin);
        popup->size             = size;
        popup->setFontSize(size.y);
        this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
}

void vstplugin::addPropertiesTooltip(Table::tbl& table) {
    table.tableWidth = 350;
    table.colSizes.push_back(150);
    auto const aeffect = this->handle->aeffect;
    table.rows.push_back({ { String( "projectGlobalId"), (int) this->projectGlobalId } });
    table.rows.push_back({ { String( "isSynth"), (int) this->isSynth } });
    table.rows.push_back({ { tblstr{ "numInputs" }, tblint{ aeffect->numInputs } } });
    table.rows.push_back({ { tblstr{ "numOutputs" }, tblint{ aeffect->numOutputs } } });
    table.rows.push_back({ { tblstr{ "numParams" }, tblint{ aeffect->numParams } } });
    table.rows.push_back({ { tblstr{ "numPrograms" }, tblint{ aeffect->numPrograms } } });
    table.rows.push_back({ { tblstr{ "uniqueID" }, tblint{ aeffect->uniqueID, "%8X" } } });
    table.rows.push_back({ { tblstr{ "version" }, tblint{ aeffect->version } } });
    table.rows.push_back({ { tblstr{ "bIsEnabled" }, tblint{ this->bIsEnabled } } });
    table.rows.push_back({ { String( "bCanReceiveMidi"), (int) this->bCanReceiveMidi } });
    table.rows.push_back({ { String( "midiEventsDispatched"), (int) this->midiEventsDispatched } });
    table.rows.push_back({ { tblstr{ "PARAM_ENABLE" }, tblfloat{ this->getParamValue(PARAM_ENABLE) } } });
    table.rows.push_back({ { tblstr{ "flags" }, tblint{ aeffect->flags } } });
    table.rows.push_back({ { tblstr{ "initialDelay" }, tblint{ aeffect->initialDelay } } });
    table.rows.push_back({ { tblstr{ "magic" }, tblint{ aeffect->magic } } });
    table.rows.push_back({ { tblstr{ "offQualities" }, tblint{ aeffect->offQualities } } });
    table.rows.push_back({ { tblstr{ "realQualities" }, tblint{ aeffect->realQualities } } });
    int n = 0;
    for (auto& in : this->inputNames) {
        table.rows.push_back({ { tblString{ StringFormat("input[%d]", n) }, tblstr{ StringAsCStr(in) } } });
        n++;
    }
    n = 0;
    for (auto& out : this->outputNames) {
        table.rows.push_back({ { tblString{ StringFormat("output[%d]", n) }, tblstr{ StringAsCStr(out) } } });
        n++;
    }
}
template<>
void guitooltip<guivstplugin>::setContent() {
    ptr->effect->addPropertiesTooltip(table);
}

guivstplugin::guivstplugin(vstplugin* _effect) : guipluginview(_effect), vst(_effect) {
}
guivstplugin::~guivstplugin() {
    if (viewCtr) {
        viewCtr->setFree();
    }
}
guictxtmenu_base* guivstplugin::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<guivstplugin>(this);
    return tooltip;
}
guictxtmenu_base* guiinternalpluginview::getTooltip(AppCtrl* appctrl) {
    //auto tooltip = new guitooltip<guiinternalpluginview>(this);
    //return tooltip;
    return nullptr;
}
guiinternalpluginview::guiinternalpluginview(internalplugin* _effect) : guipluginview(_effect), plugin(_effect) {
    viewCtr = _effect->createInternalView();
    if (viewCtr) {
        viewCtr->addTo(viewCtrs);
        viewCtr->onGuiOpen(nullptr);
//        this->viewCtr->setVSTPlugin(this);
//        handleInt->viewForInternalVst2 = this->viewCtr;
    }
}
guiinternalpluginview::~guiinternalpluginview() {
    if (viewCtr) {
        viewCtr->setFree();
    }
}

void guiplugin::addProperties(Table::tbl* table) {
    effect->addPropertiesParameterList(*table);
}

void guidropdownprogram::setSelectedIndex(uint32_t idx) {
    if (idx < getLastIndex()) {
        plugin->setCurrentProgram(idx);
    }
}

uint32_t guidropdownprogram::getLastIndex() {
    uint32_t maxProgram = 0;
    plugin->getNumberOfPrograms(maxProgram);
    return maxProgram;
}

uint32_t guidropdownprogram::getSelectIndex() {
    uint32_t index = 0;
    plugin->getCurrentProgram(index);
    return index;
}
