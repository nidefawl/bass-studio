#pragma once
#include <nanovg_min.h>
#include "math/vec.h"
#include "gui/gui.h"
#include "event.h"
#include "snapshot/snapshot.h"
#include "str_util.h"
#include "color_util.h"
#include "gui/controls/button.h"
#include "gui/controls/list.h"
#include "gui/meter/guimeter.h"
#include "gui/dropdown/dropdown.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/controls/textfield.h"

class effectbase;
class vstplugin;
class clapplugin;
class internalplugin;
class BaseCtrl;
class AppCtrl;
class PluginViewContainers;
class guictxtmenu_base;
struct plugin_ui_snapshot_t;

class guiplugin : public guictr_base {
public:
    effectbase* const effect;
    String text;
    guibuttontoggle buttonBypass;
    guibuttontoggle buttonDelete;
    guibuttontoggle buttonLayout;
    guibuttontoggle buttonSave;
    gui_trackmeter  guiMeter;
    float titlePosX        = 0;
    bool hasDragged        = false;
    bool isHorizontalTitle = true;
    int layoutMode         = 0;

    std::vector<guibuttontoggle*> guiButtonsTitlebar;
    std::vector<guibuttontoggle*> guiButtonsSidebar;
    guiplugin(effectbase* _effect);
    ~guiplugin() override;
    void addGuiBtnTitlebar(guibuttontoggle* btn);
    void render(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    void buttonClicked(guibase* _button) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    virtual void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) = 0;
    void determineSize(ivec2& prefSize) override;
    effectbase* getModule() {
        return effect;
    }
    void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override {
        mousepos += dragOffset;
        mousepos -= pos;
        nvgTranslate(vg, mousepos.x, mousepos.y);
        render(vg);
    }
    virtual void renderBase(NVGcontext* vg);

    void layout() override;

    void rightClicked(MouseEvent& evt, guibase* button) override;
    void handleRightClick(MouseEvent& evt) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void dragMoveOn(guibase* target, ivec2 mousepos) override;
    void dragReleaseOn(guibase* target, ivec2 mousepos) override;
    virtual void setLayoutMode(int32_t layoutMode);
    void setTitle(String _text) {
        text = _text;
    }
    void setState(bool state) {
    }
    bool isDragMoveable() override {
        return true;
    }
    bool focusEvent(MouseHitEvt& evt, bool focused) override;

    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    guibase* getDraggedControl() override;
    void setControl(BaseCtrl* parentCtrl) override;
    bool isSelected() override;
    void addProperties(Table::tbl* table) override;
    void addPropertiesTooltip(Table::tbl& table);
    bool setScissorTransformContainer(NVGcontext* vg) override;
    virtual void makeSnapshot(plugin_ui_snapshot_t& puis, const tracksnapshot_store_opts_t& opts);
    virtual void loadSnapshot(const plugin_ui_snapshot_t& puis);
};

class guidropdown_select_program : public guictxtmenu {
    effectbase* const plugin;

public:
    guidropdown_select_program(effectbase* _plugin);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
class guidropdownprogram : public guidropdownbase {
    effectbase* plugin = nullptr;

public:
    guidropdownprogram(effectbase* _plugin) : guidropdownbase(), plugin(_plugin) {
    }
    String getString() override;
    void handleDraggedRelease(MouseEvent& evt) override;
    int32_t getSelectIndex() override;
    int32_t getLastIndex() override;
    void setSelectedIndex(int32_t idx) override;
};
class guipluginview : public guiplugin {
public:
    guipluginview(effectbase* _effect);
    ~guipluginview() override;
    effectbase* const effect;
    guidropdownprogram dropdownProgram;
    gui_list params;                    //TODO: use add() on control
    guibuttontoggle buttonOpenEditor;   //TODO: use add() on controls
    guibuttontoggle buttonShowParameterList;// TODO: use add() on controls;
    gui_textfield textFieldSearchBox;
    int layoutWidthParams = 200;

    /* holds view controller for internal vstplugins with custom gui (non-steinberg api) */
    std::shared_ptr<PluginViewContainers> viewCtr;
    /* holds guictrs of internal vstplugins with custom gui (non-steinberg api) */
    std::vector<guictr_base*> viewCtrs;
    /* holds size for internal vstplugins with custom gui (non-steinberg api) */
    ivec2 sizeCtrs{};
    bool bParamListVisible = true;
    void updateParamList(const String& strParamNameFilter);

    void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override;
    void render(NVGcontext* vg) override;
    void buttonClicked(guibase* _button) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void setControl(BaseCtrl* parentCtrl) override;
    void determineSize(ivec2& prefSize) override;
    void prerender(NVGcontext* vg) override;
    void setLayoutMode(int32_t layoutMode) override;
    void onAdded() override;
    void onRemove() override;
    void onTick(AppCtrl* ctrl) override;
    void makeSnapshot(plugin_ui_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_ui_snapshot_t& puis) override;
};
class guivstplugin : public guipluginview {
public:
    guivstplugin(vstplugin* _vst);
    ~guivstplugin() override;
    vstplugin* const vst;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
class guiclapplugin : public guipluginview {
public:
    guiclapplugin(clapplugin* _clap);
    ~guiclapplugin() override;
    clapplugin* const clap;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
class guiinternalpluginview : public guipluginview {
    internalplugin* const plugin;

public:
    guiinternalpluginview(internalplugin* _effect);
    ~guiinternalpluginview() override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
