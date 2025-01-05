#pragma once
#include "gui/container/container_layout_types.h"
#include "host/daw/mainctrl.h"
#include "gui/gui.h"
#include "str_util.h"
#include "gui/controls/textfield.h"
#include "gui/controls/list.h"
#include "host/plugindatabase/plugindatabase.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/modules.h"
#include "host/host_pluginmanager.h"
#include <vector>

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
class gui_pluginlibrary_entry final : public gui_pluginlist_entry {
    const pluginentry_t entry;
public:
    explicit gui_pluginlibrary_entry(const pluginentry_t& _entry) : gui_pluginlist_entry(), entry(_entry) {
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
public:
    enum GroupBy {
        GROUP_BY_FORMAT,
        GROUP_BY_VENDOR,
        GROUP_BY_NONE
    };
private:
    GroupBy groupBy = GROUP_BY_FORMAT;
    gui_textfield textField;
    gui_textfield textField2;
    gui_list pluginListCtr;
    String curquery = "";
    std::vector<pluginentry_t> pluginsLibList;
    std::map<String, bool> isFolderOpen;

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
        setCanMouseHit(true);
    }

    ~guictr_pluginlibrary() override {
        std::vector<gui_list_entry*> _newList;
        pluginListCtr.setList(_newList);
        remove(&pluginListCtr);
        remove(&textField);
        remove(&textField2);
    }

    GroupBy getGroupBy() const {
        return groupBy;
    }

    void setGroupBy(GroupBy _groupBy) {
        if (groupBy != _groupBy) {
            groupBy = _groupBy;
            update();
        }
    }

    void update();

    void buttonClicked(guibase* button) override;

    void layout() override {
        pluginListCtr.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        textField2.setVisible(!textField2.value().empty());
        ivec2 cs          = getSizeContent();
        textField.pos     = ivec2(0, 0);
        textField.size    = ivec2(cs.x, HEIGHT_DEFAULT_INPUT);
        textField2.pos    = ivec2(textField.left(), textField.bottom() + padding/2);
        textField2.size   = textField.size;
        pluginListCtr.pos = ivec2(0, textField.bottom() + padding/2);
        if (textField2.isVisible()) {
            pluginListCtr.pos = ivec2(textField2.left(), textField2.bottom() + padding/2);
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
    void handleRightClick(MouseEvent& evt) override;

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (guictr_base::mouseHitTest(mpos, evt)) {
            if (evt.type == MouseHitType::MOUSE_RIGHT) {
                if (evt.getGuiHit() == &pluginListCtr || (evt.getGuiHit() && evt.getGuiHit()->parent == &pluginListCtr)) {
                    evt.requestFocus(this);
                }
            }
            return true;
        }
        return false;
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
    explicit gui_modulelist_entry(const module_desc_t& _entry) : gui_pluginlist_entry(), entry(_entry) {
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
    gui_textfield textField;
    gui_list pluginListCtr;
    String curquery = "";
    std::vector<module_desc_t> effectEntries;

public:
    guictr_modulelibrary() : guictr_base() {
        setBackgroundRendered(true);
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
        effectEntries.clear();
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_LFO, "LFO", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_MACROS, "Macros", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_EMPTY, "Empty", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_EQ, "EQ", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_GAIN, "Gain", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_GROUP, "Group", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_LATENCY, "Latency", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SAMPLE_CRUSH, "Sample Crush", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SAMPLE_DELAY, "Sample Delay", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_STEREO_WIDTH, "Stereo Width", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SYNTH, "Synth", true });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SYNTH_MONO, "Synth Mono", true });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SYNTH_SHAPER, "Synth Shaper", true });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SYNTH_GPU, "Synth GPU", true });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_SYNTH_KICKXP, "Kick XP", true });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_VISUALIZER, "Visualizer", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_TAPE_DELAY, "Tape Delay", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_AIRWINDOWS_GALACTIC_1, "Galactic 1 Reverb", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_AIRWINDOWS_GALACTIC_2, "Galactic 2 Reverb", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_AIRWINDOWS_GALACTIC_3, "Galactic 3 Reverb", false });
        effectEntries.push_back(module_desc_t{ MODULE_TYPE_INTERNAL_EFFECT, PLUGIN_TYPE_AIRWINDOWS_MATRIXVERB, "MatrixVerb", false });

        auto daw = dawCtrl->getDaw();
        auto pluginMgr = daw->getPluginManager();
        std::vector<DAW::Host::builtin_module_reg_t>& vecReg = pluginMgr->getBuiltinModuleRegistry();
        for (auto& reg : vecReg) {
            effectEntries.push_back(module_desc_t{ MODULE_TYPE_VST2, reg.id, reg.name, reg.isSynth });
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
        textField.size     = ivec2(cs.x, HEIGHT_DEFAULT_INPUT);
        textField.pos      = ivec2(0, 0);
        pluginListCtr.pos  = ivec2(0, textField.bottom() + padding/2);
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
