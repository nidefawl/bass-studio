#pragma once
#include <nanovg_min.h>
#include "math/vec.hpp"
#include "gui/gui.hpp"
#include "event.hpp"
#include "snapshot/snapshot.hpp"
#include "str_util.hpp"
#include "color_util.hpp"
#include "gui/controls/button.hpp"
#include "gui/controls/list.hpp"
#include "gui/meter/guimeter.hpp"
#include "gui/dropdown/dropdown.hpp"
#include "gui/contextmenu/contextmenu.hpp"
#include "gui/controls/textfield.hpp"

class effectbase;
class vstplugin;
class vst3plugin;
class clapplugin;
class internalplugin;
class BaseCtrl;
class AppCtrl;
class PluginViewContainer;
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
    guibuttontoggle buttonBypassModulation;
    gui_trackmeter  guiMeter;
    float titlePosX        = 0;
    bool hasDragged        = false;
    bool isHorizontalTitle = true;
    bool bShowMeter = true;
    int layoutMode         = 0;

    std::vector<guibuttontoggle*> guiButtonsTitlebar;
    std::vector<guibuttontoggle*> guiButtonsSidebar;
    explicit guiplugin(effectbase* _effect);
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
    bool focusEvent(MouseHitEvt& evt, bool focused) override;

    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    void setControl(BaseCtrl* parentCtrl) override;
    bool isSelected() override;
    void addProperties(Table::tbl* table) override;
    void addPropertiesTooltip(Table::tbl& table);
    bool setScissorTransformContainer(NVGcontext* vg) override;
    virtual void makeSnapshot(plugin_ui_snapshot_t& puis, const tracksnapshot_store_opts_t& opts);
    virtual void loadSnapshot(const plugin_ui_snapshot_t& puis);
    guibase* getFocusedContainer() override {
        return parent;
    }
    void setMeterVisible(bool bVisible) {
        bShowMeter = bVisible;
        guiMeter.setVisible(bVisible);
    }
};

class guidropdown_select_program final : public guictxtmenu {
    effectbase* const plugin;

public:
    guidropdown_select_program(effectbase* _plugin);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
class guidropdownprogram final : public guidropdownbase {
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
    gui_list params;
    guibuttontoggle buttonOpenEditor;
    guibuttontoggle buttonShowParameterList;
    gui_textfield textFieldSearchBox;
    int layoutWidthParams = 200;

    /* holds view controller for internal plugins */
    std::shared_ptr<PluginViewContainer> viewCtr;
    /* holds guictrs of internal plugins */
    std::vector<guictr_base*> viewCtrs;
    /* holds size for internal plugins */
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
    virtual bool hasOwnMeter() const {
        return true;
    }
};
class guivstplugin final : public guipluginview {
public:
    guivstplugin(vstplugin* _vst);
    ~guivstplugin() override;
    vstplugin* const vst;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
class guiclapplugin final : public guipluginview {
public:
    guiclapplugin(clapplugin* _clap);
    ~guiclapplugin() override;
    clapplugin* const clap;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
class guivst3plugin final : public guipluginview {
public:
    guivst3plugin(vst3plugin* _clap);
    ~guivst3plugin() override;
    vst3plugin* const vst3;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
class guiinternalpluginview final : public guipluginview {
    internalplugin* const plugin;

public:
    guiinternalpluginview(internalplugin* _effect);
    ~guiinternalpluginview() override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    bool hasOwnMeter() const override;
};
