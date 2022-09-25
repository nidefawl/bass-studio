#include "plugin.h"
#include <nanovg.h>
#include <memory>
#include "assert_dbg.h"
#include "automation.h"
#include "fileio.h"
#include "guiglobals.h"
#include "platform.h"
#include "snapshot.h"
#include "str_util.h"
#include "logging.h"
#include "event.h"
#include "keyboard.h"
#include "edithistory.h"
#include "renderresources.h"

#include "gui/gui.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/controls/list.h"
#include "theme.h"
#include "gui/table/table.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/tooltip/tooltip.h"
#include "pluginviewcontainers.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "pluginctr.h"
#include "gui/views/pluginlist.h"
#include "gui/track/trackcontent.h"

#include "basectrl.h"

#include "host/mainctrl.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/internal_plugin.h"
#include "host/plugin/vst_plugin.h"
#include "host/plugin/vst_plugin_handles.h"
#include "host/host_plugin_window.h"
#include "gui/automation/automatable.h"
#include "gui/properties/properties_table.h"
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
    remove(&guiMeter);
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
        setLayoutMode((layoutMode + 1) % 2);
        parent->onChildLayoutChanged(this);
        return;
    }
    if (_button == &buttonBypass) {
        ThreadLock lock = dawCtrl->lockPlayThread();
        toggleDeviceEnableState(effect, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    }
    if (_button == &buttonSave) {
        ThreadLock lock = dawCtrl->lockPlayThread();

        plugin_snapshot_t ps;
        effect->makeSnapshot(ps, tracksnapshot_store_opts_t::All());
        CreateDirectoryIfNotExists(App::Platform::toUserdataPath("presets"));
        String defaultPresetPath = App::Platform::toUserdataPath("presets/" + effect->getName());
        CreateDirectoryIfNotExists(defaultPresetPath);
        String path;
        auto window = dawCtrl->window;
        if (promptUserFilePath(window, 1, vFILE_TYPE_PLUGINSNAPSHOT, path, defaultPresetPath)) {
            String ext;
            SplitPath(path, nullptr, nullptr, &ext);
            if (ext.empty()) {
                path += "." + vFILE_TYPE_PLUGINSNAPSHOT[0].ext;
            }
            savePluginSnapshot(ps, path);
        }
        return;
    }
    if (_button == &buttonDelete) {
        DAW::removePlugin(dawCtrl->getDaw(), effect);
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
    : guictr_base(gui_type::CTR_TYPE_PLUGIN),
      effect(_effect),
      guiMeter(&_effect->meter) {
    isHorizontalTitle = false;
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
    add(&guiMeter);
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

    const auto heightTitle = static_cast<float>(theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT));
    const auto hpt = isHorizontalTitle ? heightTitle : 0;
    const auto wpt = isHorizontalTitle ? 0 : heightTitle;
    auto buttonSize = heightTitle * 0.8f;
    auto inset1 = (heightTitle - buttonSize) * 0.5f;
    auto btnPos = ivec2(math::roundfS32(inset1));
    auto iButtonSize = math::roundfS32(buttonSize);
    buttonLayout.pos  = btnPos;
    btnPos[isHorizontalTitle ? 0 : 1] += iButtonSize;
    for (auto btn : guiButtonsTitlebar) {
        btn->size = { iButtonSize, iButtonSize };
        btn->setRadius(heightTitle / 3.f);
        if (btn == &buttonLayout) {
            continue;
        }
        if (btn == &buttonDelete) {
            continue;
        }
        btn->pos = btnPos;
        btnPos[isHorizontalTitle ? 0 : 1] += iButtonSize;
    }
    if (isHorizontalTitle) {
        buttonDelete.pos = { size.x - buttonDelete.size.x - inset1, inset1 };
    } else {
        buttonDelete.pos = { inset1, size.y - buttonDelete.size.y - inset1 };
    }


    int32_t meterW = math::max(16, (int32_t) (theme->get(GuiConstant::CONST_METER_WIDTH) * heightTitle / 32.0));
    auto contentP  = ivec2(wpt, hpt);
    auto contentS  = ivec2(size.x - wpt - meterW, size.y - hpt);
    if (isHorizontalTitle) {
        titlePosX = btnPos.x;
    } else {
        titlePosX = buttonDelete.top();
    }
    guiMeter.pos  = ivec2(size.x - meterW, hpt);
    guiMeter.size = ivec2(meterW, size.y - hpt);
    layoutModule(contentP, contentS, inset1);
    for (auto btn : guis) {
        btn->layout();
    }
}
bool guiplugin::setScissorTransformContainer(NVGcontext* vg) {
    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return false;
    }
    nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
    nvgTranslate(vg, pos.x, pos.y);
    return true;
}

void guiplugin::renderBase(NVGcontext* vg) {
    if (!setScissorTransformContainer(vg)) {
        return;
    }
    renderFrameBase(vg);
    int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : 0;
    if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
    renderTitleBar(vg, size, this->text, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, titlePosX, flags, isHorizontalTitle);
    renderFrameOutline(vg);
    for (auto* btn : guiButtonsTitlebar) {
        if (btn->isVisible())
            btn->render(vg);
    }
    ivec2 posInset  = getPosContent();
    nvgTranslate(vg, posInset.x-pos.x, posInset.y-pos.y);
    nvgTranslateZ(vg, -4.0f);
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
guictr_properties_table* makeUniquePropertiesCtr();
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
                guictr_properties_table* dbgPropertiesCtrPopup = makeUniquePropertiesCtr();
                guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
                ctxtMenu->setBackgroundRendered(true);
                ctxtMenu->size = { 640, 480 };
                ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
                ivec2 wndPos{ 0 };
                dbgPropertiesCtrPopup->setDebugPropertyHandle(gui);
                dawCtrl->openContextMenu(ctxtMenu, gui->toScreenSpace({gui->size.x, 0}));
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
void effectbase::addPropertiesParameterTooltip(Table::tbl& table, int idx) {
}
void effectbase::addPropertiesTooltip(Table::tbl& table) {
    table.tableWidth = 350;
    table.colSizes.push_back(150);
    table.rows.push_back({ { String("projectGlobalId"), (int) this->projectGlobalId } });
    table.rows.push_back({ { tblstr{ "track" }, tblint{ (int64_t) this->getTrack(), "%12x" } } });
    table.rows.push_back({ { tblstr{ "tracklink" }, tblint{ (int64_t) this->getTrackLink(), "%12x" } } });
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
          knobTest(guiknob::knobtype::KNOB_UNLABELED) {
        icon = 0;
        knobTest.setAutomationRef(effect, entry->idx);
        knobTest.setKnobInternalHandlers();
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
        if (rowHeight > 24) {
            auto paramValue = effect->getParamValueDisplay(entry->idx);
            String paramValueStr = paramValue.value;
            if (!paramValue.unit.empty()) {
                paramValueStr += " " + paramValue.unit;
            }

            renderTextLabel(vg, vec2(x, rowHeight*0.25f),
                                vec2(size.x-x, rowHeight*0.5f), 
                                getText(), theme, rowHeight * 0.5f, theme->getColor(GuiColor::COL_TEXT), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            renderTextLabel(vg, vec2(x, rowHeight * 0.5f + rowHeight*0.25f),
                                vec2(size.x-x, rowHeight*0.5f), 
                                paramValueStr, theme, rowHeight * 0.5f, theme->getColor(GuiColor::COL_TEXT), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        } else {
            renderTextLabel(vg, vec2(x, rowHeight*0.5f),
                        vec2(size.x-x, rowHeight), 
                        getText(), theme, rowHeight, theme->getColor(GuiColor::COL_TEXT), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

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
        auto window = plugin->windowHost;
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
    params.setVisible(_effect->getModuleType() == PLUGIN_TYPE_VST);
    // this->isHorizontalTitle = !params.isVisible();
    params.setRowHeight(48);
    params.margin = 2;
    params.padding = 4;
    textFieldSearchBox.setParent(this);
    textFieldSearchBox.setChangeCallback([this](const String& str) {
        updateParamList(str);
        return true;
    });
    textFieldSearchBox.setPlaceholder("Search");
    buttonOpenEditor.icon = ICON_SYNTH_SMALL;
    buttonOpenEditor.setStateRef(&_effect->bEditOpen);
    buttonOpenEditor.setParent(this);
    buttonOpenEditor.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
    params.setParent(this);
    bParamListVisible = params.isVisible();
    buttonShowParameterList.icon = ICON_ADJUST;
    buttonShowParameterList.setStateRef(&bParamListVisible);
    buttonShowParameterList.setParent(this);
    buttonShowParameterList.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
    dropdownProgram.setParent(this);
    updateParamList("");
    addGuiBtnTitlebar(&buttonShowParameterList);
    addGuiBtnTitlebar(&buttonOpenEditor);
}

guipluginview::~guipluginview() {
    remove(&buttonOpenEditor);
    remove(&buttonShowParameterList);
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
        ctr->prerender(vg);
    }
}

void guipluginview::onAdded() {
    guictr_base::onAdded();
    if (viewCtr) {
        viewCtr->onGuiOpen();
    }
    for (auto* ctr : viewCtrs) {
        ctr->setParent(this);
    }
}

void guipluginview::onTick(AppCtrl* ctrl) {
    guiplugin::onTick(ctrl);
    for (auto* ctr : viewCtrs) {
        dbgassert(ctr->parent == this);
        if (ctr->isVisible()) {
            ctr->onTick(ctrl);
        }
    }
}

void guipluginview::onRemove() {
    guictr_base::onRemove();
    if (viewCtr) {
        viewCtr->onGuiClose();
    }
    for (auto* ctr : viewCtrs) {
        ctr->setParent(nullptr);
    }
}

void guipluginview::determineSize(glm::ivec2& prefSize) {
    if (layoutMode == 1) {
        guiplugin::determineSize(prefSize);
        return;
    }
    const int32_t hpt = parent->theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
    int32_t meterW    = math::max(16, (int32_t) (theme->get(GuiConstant::CONST_METER_WIDTH) * hpt / 32.0));

    auto contentSizeY = size.y - (isHorizontalTitle ? hpt : 0);
    int rowHeight     = 48;
    while (contentSizeY < rowHeight * 8 && rowHeight > 8) {
        rowHeight -= 4;
    }
    params.setRowHeight(rowHeight);
    layoutWidthParams = rowHeight*6;
    prefSize.x  = (params.isVisible()?layoutWidthParams:0) + meterW + (!isHorizontalTitle?hpt:0);
    sizeCtrs = { 0, contentSizeY };

    if (viewCtr) {
        ivec2 sizeCtr;
        viewCtr->getFixedSize(&sizeCtr.x, &sizeCtr.y);
        sizeCtr.x = (int) ((sizeCtr.x / (float) sizeCtr.y) * size.y);
        sizeCtr.y = sizeCtrs.y;
        viewCtr->layout(sizeCtr.x, sizeCtr.y);
        sizeCtrs.x += sizeCtr.x;
    }
    prefSize.y = math::max(sizeCtrs.y, prefSize.y);
    prefSize.x += sizeCtrs.x;
    auto minWidth = buttonOpenEditor.right()+buttonDelete.size.x+16;
    // if (prefSize.x < minWidth && viewCtrs.empty()) {
    //     params.size.x = (minWidth-meterW);
    // }
    prefSize.x = math::max(minWidth, prefSize.x);
    dbgassert(prefSize.x > 0);
    dbgassert(prefSize.y > 0);
}
void guipluginview::render(NVGcontext* vg) {
    renderBase(vg);
    if (layoutMode != 1) {
        for (auto* ctr : viewCtrs) {
            if (ctr->isVisible()) {
                nvgSave(vg);
                ctr->render(vg);
                nvgRestore(vg);
            }
        }
        if (guiMeter.isVisible())
            guiMeter.render(vg);
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
            effect->closeWindow();
        } else {
            bool bResetPosition = isShift(parentCtrl->lastMouseEvent.kbmods);
            effect->showWindow(bResetPosition);
        }
    }
    if (_button == &buttonShowParameterList) {
        params.setVisible(!params.isVisible());
        bParamListVisible = params.isVisible();
        this->onChildLayoutChanged(this);
    }
}
void guipluginview::layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) {
    contentS.x = math::max(64, contentS.x);
    contentS.y = math::max(64, contentS.y);
    const int32_t heightTitle = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
    const auto hpt = isHorizontalTitle ? heightTitle : 0;
    const auto wpt = isHorizontalTitle ? 0 : heightTitle;
    int32_t insetCtrls = INSET_TITLE;
    auto left = wpt;
    if (params.isVisible()) {
        textFieldSearchBox.setVisible(layoutMode == 0 && heightTitle >= 16);
        dropdownProgram.setVisible(layoutMode == 0 && effect->programNames.size() && heightTitle >= 16);
        double hDropDown = 0;

        if (dropdownProgram.isVisible()) {
            hDropDown += heightTitle * 0.7;
        }
        if (textFieldSearchBox.isVisible()) {
            hDropDown += heightTitle * 0.7;
        }
        params.pos  = ivec2(insetCtrls + wpt, insetCtrls + hpt + hDropDown);
        params.size = ivec2(layoutWidthParams, contentS.y - hDropDown) - ivec2(insetCtrls * 2);
        params.layout();
        double topOffset = 0;
        if (dropdownProgram.isVisible()) {
            dropdownProgram.pos  = ivec2(insetCtrls + wpt, insetCtrls + hpt + topOffset);
            dropdownProgram.size = ivec2(layoutWidthParams, heightTitle * 0.7) - ivec2(insetCtrls * 2, 0);
            dropdownProgram.layout();
            topOffset += heightTitle * 0.7;
        }
        if (textFieldSearchBox.isVisible()) {
            textFieldSearchBox.pos  = ivec2(insetCtrls + wpt, insetCtrls + hpt + topOffset);
            textFieldSearchBox.size = ivec2(layoutWidthParams, heightTitle * 0.7) - ivec2(insetCtrls * 2, 0);
            textFieldSearchBox.layout();
            topOffset += heightTitle * 0.7;
        }
        left = params.right();
    } else {
        dropdownProgram.setVisible(false);
        textFieldSearchBox.setVisible(false);
    }
    for (auto* ctr : viewCtrs) {
        if (ctr->isVisible()) {
            ctr->pos          = ivec2(left, 0) + ivec2(insetCtrls, insetCtrls + hpt);
            ivec2 prefSizeCtr = ctr->size - ivec2(insetCtrls * 2);
            ctr->determineSize(prefSizeCtr);
            ctr->size = prefSizeCtr;
            ctr->layout();
            left = ctr->right();
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
    if (_id >= 0 && _id < CtrSize(plugin->programNames)) {
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
    for (auto& in : this->inputChannelsDesc) {
        table.rows.push_back({ { tblString{ StringFormat("input[%d,%d]", in.offset, in.count) }, tblstr{ StringAsCStr(in.name) } } });
    }
    for (auto& out : this->outputChannelsDesc) {
        table.rows.push_back({ { tblString{ StringFormat("output[%d,%d]", out.offset, out.count) }, tblstr{ StringAsCStr(out.name) } } });
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
    (void) plugin;
    viewCtr = _effect->getViewCtr(UID_VIEW_CTR_PLUGIN_CTR);
    if (viewCtr) {
        viewCtr->addTo(viewCtrs);
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

void guidropdownprogram::setSelectedIndex(int32_t idx) {
    if (idx < getLastIndex()) {
        plugin->setCurrentProgram(idx);
    }
}

int32_t guidropdownprogram::getLastIndex() {
    uint32_t maxProgram = 0;
    plugin->getNumberOfPrograms(maxProgram);
    return static_cast<int32_t>(maxProgram);
}

int32_t guidropdownprogram::getSelectIndex() {
    uint32_t index = 0;
    plugin->getCurrentProgram(index);
    return index;
}

void guipluginview::setLayoutMode(int32_t layoutMode) {
    guiplugin::setLayoutMode(layoutMode);
    dropdownProgram.setVisible(this->layoutMode == 0 && effect->programNames.size());
    params.setVisible(this->bParamListVisible && this->layoutMode == 0);
    // bParamListVisible = params.isVisible();
    for (auto* ctr : viewCtrs) {
        ctr->setVisible(layoutMode == 0);
    }
}

void guiplugin::setLayoutMode(int32_t layoutMode) {
    this->layoutMode = layoutMode;
    guiMeter.setVisible(layoutMode == 0);
    // isHorizontalTitle = layoutMode == 0;
    buttonLayout.icon = layoutMode == 0 ? ICON_ARR_RIGHT : ICON_ARR_DOWN;
}

void guiplugin::makeSnapshot(plugin_ui_snapshot_t& puis, const tracksnapshot_store_opts_t& opts){
    if (opts.storeLayouts) {
        puis.layoutMode = layoutMode;
        puis.windowPosSizeValid = effect->getLastWindowPosSize(puis.windowPosSize);
    }
}

void guiplugin::loadSnapshot(const plugin_ui_snapshot_t& puis) {
    if (puis.layoutMode > -1) {
        setLayoutMode(puis.layoutMode);
    }
    effect->bWindowPosSizeValid = puis.windowPosSizeValid;
    effect->lastWindowPosSize = puis.windowPosSize;
}

void guipluginview::makeSnapshot(plugin_ui_snapshot_t& puis, const tracksnapshot_store_opts_t& opts) {
    guiplugin::makeSnapshot(puis, opts);
    if (opts.storeLayouts) {
        puis.isWindowOpen = effect->bEditOpen;
        puis.parameterListVisible = params.isVisible();
    }
}

void guipluginview::loadSnapshot(const plugin_ui_snapshot_t& puis) {
    guiplugin::loadSnapshot(puis);
    params.setVisible(puis.parameterListVisible);
    bParamListVisible = params.isVisible();
}
