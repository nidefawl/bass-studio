#pragma once
#include "logging.h"
#include "nanovg/nanovg.h"
#include "host/daw/mainctrl.h"
#include "gui/gui.h"
#include "str_util.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "gui/controls/textfield.h"
#include "gui/controls/list.h"
#include "host/plugindatabase/plugindatabase.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/modules.h"
#include "gui/tooltip/tooltip.h"
#include "host/host_pluginmanager.h"

class effectbase;
class gui_pluginlist_entry : public gui_list_entry {
public:
    gui_pluginlist_entry() { 
        setGuiType(gui_type::CTR_TYPE_PLUGINS_LIST_ENTRY);
        setDragRendered(true);
    };
    ~gui_pluginlist_entry() override = default;
    virtual effectbase* makeInstance() = 0;
    virtual bool isSynth()             = 0;
    void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;

    void drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset);
};
class gui_vstpluginlist_entry final : public gui_pluginlist_entry {
    const pluginentry_t entry;
public:
    gui_vstpluginlist_entry(const pluginentry_t _entry) : gui_pluginlist_entry(), entry(_entry) {
        icon = _entry.isSynth ? ICON_SYNTH : ICON_EFFECT;
        label = _entry.name;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
        target->pluginEntryDragMove(this, toControlsObjectSpace(mousepos, target));
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
        target->pluginEntryDragRelease(this, toControlsObjectSpace(mousepos, target));
    }
    String getText() override {
        return entry.name;
    }
    effectbase* makeInstance() override;
    bool isSynth() override {
        return entry.isSynth;
    }
    const pluginentry_t& getEntry() const {
        return entry;
    }

    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};
class guictr_pluginlibrary final : public guictr_base {
    const int32_t heightTextField = HEIGHT_DEFAULT_INPUT;
    gui_textfield textField;
    gui_textfield textField2;
    gui_list pluginListCtr;
    String curquery = "";
    std::vector<pluginentry_t> pluginsLibList;

public:
    guictr_pluginlibrary() : guictr_base() {
        setBackgroundRendered(true);
        pluginListCtr.padding = 0;
        pluginListCtr.setBackgroundRendered(false);
        add(&textField);
        add(&textField2);
        add(&pluginListCtr);
        textField.setChangeCallback([this](const std::string& str) {
            curquery = str;
            update();
            return true;
        });
        textField.setPlaceholder("Search");
        textField2.setPlaceholder("SQL error");
    }
    ~guictr_pluginlibrary() override {
        std::vector<gui_list_entry*> _newList;
        pluginListCtr.setList(_newList);
        remove(&pluginListCtr);
        remove(&textField);
        remove(&textField2);
    }
    void update() {
        std::vector<gui_list_entry*> _newList;
        pluginsLibList.clear();
        try {
            dawCtrl->getDaw()->getPluginDatabase().query(curquery, pluginsLibList);
            for (pluginentry_t& entry : pluginsLibList) {
                gui_pluginlist_entry* g = new gui_vstpluginlist_entry(entry);
                _newList.push_back(g);
            }
            textField2.setValue("");
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Error: %s\n", e.what());
            String strValue = e.what();
            textField2.setValue(strValue);
        }

        pluginListCtr.setList(_newList);
        layout();
    }
    void layout() override {
        pluginListCtr.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        textField2.setVisible(!textField2.value().empty());
        ivec2 cs          = getSizeContent();
        textField.pos     = ivec2(0, 0);
        textField.size    = ivec2(cs.x, heightTextField);
        textField2.pos    = ivec2(textField.left(), textField.bottom());
        textField2.size   = textField.size;
        pluginListCtr.pos = ivec2(0, textField.bottom());
        if (textField2.isVisible()) {
            pluginListCtr.pos = ivec2(textField2.left(), textField2.bottom());
        }
        pluginListCtr.size = ivec2(cs.x, cs.y - pluginListCtr.pos.y);
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        textField.render(vg);
        if (textField2.isVisible()) {
            textField2.render(vg);
        }
        pluginListCtr.render(vg);
    }
};
struct module_desc_t {
    int moduleType;
    int moduleId;
    String name;
    bool isSynth;
};
class gui_modulelist_entry final : public gui_pluginlist_entry {
public:
    const module_desc_t entry;
    gui_modulelist_entry(const module_desc_t _entry) : gui_pluginlist_entry(), entry(_entry) {
        icon = _entry.isSynth ? ICON_SYNTH : ICON_EFFECT;
        label = _entry.name;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
        target->pluginEntryDragMove(this, toControlsObjectSpace(mousepos, target));
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
        target->pluginEntryDragRelease(this, toControlsObjectSpace(mousepos, target));
    }
    String getText() override {
        return entry.name;
    }
    effectbase* makeInstance() override;
    bool isSynth() override {
        return entry.isSynth;
    }
};
class guictr_modulelibrary final : public guictr_base {
    const int32_t heightTextField = 30;
    gui_textfield textField;
    gui_list pluginListCtr;
    String curquery = "";
    std::vector<module_desc_t> effectEntries;

public:
    guictr_modulelibrary() : guictr_base() {
        setBackgroundRendered(true);
        padding = 4;
        margin = 2;
        pluginListCtr.padding = 0;
        pluginListCtr.setBackgroundRendered(false);
        add(&textField);
        add(&pluginListCtr);
        textField.setChangeCallback([this](const std::string& str) {
            curquery = str;
            update();
            return true;
        });
        textField.setPlaceholder("Search");
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        if (!parentCtrl || !dawCtrl) {
            return;
        }
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_LFO, 0, "LFO", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_MACROS, 0, "Macros", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_EMPTY, 0, "Empty", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_GAIN, 0, "Gain", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_GROUP, 0, "Group", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_LATENCY, 0, "Latency", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_SAMPLE_CRUSH, 0, "Sample Crush", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_SAMPLE_DELAY, 0, "Sample Delay", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_STEREO_WIDTH, 0, "Stereo Width", false });
        effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_SYNTH, 0, "Synth", true });
        auto daw = dawCtrl->getDaw();
        auto pluginMgr = daw->getPluginManager();
        std::vector<DAW::Host::builtin_module_reg_t>& vecReg = pluginMgr->getBuiltinModuleRegistry();
        for (auto& reg : vecReg) {
            effectEntries.push_back(module_desc_t{ PLUGIN_TYPE_INTERNAL_EFFECT, reg.id, reg.name, reg.isSynth });
            break;
        }
    }
    ~guictr_modulelibrary() override {
        removeGuis();
    }
    void update() {
        std::vector<gui_list_entry*> _newList;
        for (auto& t : effectEntries) {
            if (StringContainsCI(t.name, curquery) >= 0) {
                gui_modulelist_entry* g = new gui_modulelist_entry(t);
                _newList.push_back(g);
            }
        }
        pluginListCtr.setList(_newList);
        layout();
    }
    void layout() override {
        pluginListCtr.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        ivec2 cs           = getSizeContent();
        textField.size     = ivec2(cs.x, heightTextField);
        textField.pos      = ivec2(0, 0);
        pluginListCtr.pos  = ivec2(0, textField.bottom()+padding);
        pluginListCtr.size = ivec2(cs.x, cs.y - pluginListCtr.top());
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        textField.render(vg);
        pluginListCtr.render(vg);
    }
};
