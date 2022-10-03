#pragma once
#include "str_util.h"
#include <functional>
#include <memory>
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/dropdown/dropdown.h"
#include "threads/threadlock.h"
#include "util/presetmanager.h"

class guidropdown_select_preset_file : public guictxtmenu {
public:
struct select_preset_cb_t {
    std::function<void(const String& path)> loadPreset;
    bool bValid = false;
};
private:

    std::shared_ptr<select_preset_cb_t> cb;
    // PluginVST2_Synth* plugin;
    PresetManager presetManager;

    class ctxtmenu_entry_folder : public ctxtmenu_entry {
        String path;

    public:
        bool isFolder() const { return true; }
        String getPath() const { return path; }
        ctxtmenu_entry_folder(const String& _title, const String& _path, int id)
            : ctxtmenu_entry(_title, id), path(_path) {
        }
        void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
            if (contains(ctxtSize, mouse)) {
                nvgBeginPath(vg);
                nvgRect(vg, 0, y, ctxtSize.x, height);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }

            renderTextLabel(vg,
                            vec2(leftOffset(), y + height * 0.5f),
                            vec2(width - leftOffset(), height),
                            title,
                            theme,
                            fontSize,
                            THEMECOL_TEXT,
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            String rightSide = ">";
            if (rightSide.length()) {
                auto defoffset = this->fontSize / 2.4f;
                renderTextLabel(vg,
                                vec2(width - defoffset, y + height * 0.5f),
                                vec2(width, height),
                                rightSide,
                                theme,
                                fontSize,
                                THEMECOL_TEXT,
                                NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            }
        }
    };
    class ctxtmenu_entry_preset : public ctxtmenu_entry {
        const PresetManager::Preset& preset;

    public:
        bool isFolder() const { return false; }
        String getPath() const { return preset.path; }
        String getName() const { return preset.name; }
        ctxtmenu_entry_preset(const PresetManager::Preset& _preset, int id)
            : ctxtmenu_entry(_preset.name, id),
                preset(_preset) {
        }
        void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
            if (contains(ctxtSize, mouse)) {
                nvgBeginPath(vg);
                nvgRect(vg, 0, y, ctxtSize.x, height);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }

            renderTextLabel(vg,
                            vec2(leftOffset(), y + height * 0.5f),
                            vec2(width - leftOffset(), height),
                            title,
                            theme,
                            fontSize,
                            THEMECOL_TEXT,
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    };

public:
    explicit guidropdown_select_preset_file(std::shared_ptr<select_preset_cb_t> _cb, PresetManager _presetManager, const String& presetPath, int lvl = 0)
        : cb(std::move(_cb)), presetManager(std::move(_presetManager)) {
            
        int32_t idx = 0;
        std::vector<String> paths;
        std::vector<ctxtmenu_entry_preset*> presetsCurrent;
        for (auto& preset : presetManager.getPresets()) {
            if (StrStartsWith(preset.path, presetPath)) {
                String partPath = presetPath.length() + 1 < preset.path.length() ? preset.path.substr(presetPath.length() + 1) : preset.path;
                String presetSubPath;
                SplitPath(partPath, &presetSubPath, nullptr, nullptr);
                String folderName;
                SplitPath(presetSubPath, nullptr, &folderName, nullptr);

                if (folderName.length() && folderName == presetSubPath && !stl_contains(paths, presetSubPath)) {
                    paths.push_back(presetSubPath);
                    addEntry(new ctxtmenu_entry_folder(folderName, presetPath + FILE_PATHSEP_STR + presetSubPath, (idx++) << 1 | 1));
                }
                if (presetSubPath.empty())
                    presetsCurrent.push_back(new ctxtmenu_entry_preset(preset, (idx++) << 1));
            }
        }
        for (auto preset : presetsCurrent) {
            addEntry(preset);
        }
    }

    void clickedElement(ctxtmenu_entry* e, int _id) override {
        auto appCtrlParent = parentCtrl->getParentCtrl();
        if (appCtrlParent) appCtrlParent->closeAllContextMenus();
        if ((_id & 1) == 0) {
            auto const ctxtEndpointEntry = static_cast<ctxtmenu_entry_preset*>(e);
            if (!ctxtEndpointEntry->isFolder()) {
                if (cb && cb->bValid)
                    cb->loadPreset(ctxtEndpointEntry->getPath());
            }
        }
    }

    guictxtmenu* createPopupForEntry(ctxtmenu_entry* entry, int lvl) override {
        guictxtmenu* popup = nullptr;
        auto folderEntry = dynamic_cast<ctxtmenu_entry_folder*>(entry);
        if (folderEntry) {
            popup = new guidropdown_select_preset_file(cb, presetManager, folderEntry->getPath(), lvl + 1);
        }
        return popup;
    }
};

class guidropdown_select_preset : public guidropdownbase {
    PresetManager presetManager;
    std::shared_ptr<guidropdown_select_preset_file::select_preset_cb_t> cb;
    String selected;
public:
    explicit guidropdown_select_preset()
        : guidropdownbase()
    {
    }
    void setPresetManager(PresetManager presetManager) {
        this->presetManager = std::move(presetManager);
    }
    void setCallback(std::function<void(const String&)> cbFn) {
        if (cb) cb->bValid = false;
        cb = std::make_shared<guidropdown_select_preset_file::select_preset_cb_t>();
        cb->loadPreset = std::move(cbFn);
        // cb->loadPreset = [this, outerCb=std::move(cbFn)](const String& path) {
        //     outerCb(path);
        //     this->setString(path);
        // };
        cb->bValid = true;
    }
    void onRemove() override {
        if (cb)
            cb->bValid = false;
        guidropdownbase::onRemove();
    }
    String getString() override {
        return selected;
    }
    void setString(const String& str) {
        selected = str;
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        if (!cb) {
            return;
        }
        presetManager.reload();
        auto* popup         = new guidropdown_select_preset_file(cb, presetManager, presetManager.getPresetPath());
        popup->size         = size;
        auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
        popup->setFontSize(fontSizeScaled);
        popup->size.x      = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
        parentCtrl->openAppMenu(0, popup, toScreenSpace({0, size.y}) + ivec2(0, 1), WINDOW_IS_BORDERLESS | WINDOW_POS_RELATIVE);
    }
};