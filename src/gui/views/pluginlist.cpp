#include "pluginlist.hpp"
#include "gui/table/table.hpp"
#include "guicolors.hpp"
#include <nanovg.h>

template<>
void guitooltip<gui_pluginlibrary_entry>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    using Table::tblint;
    using Table::tblString;
    table.tableWidth = 80;
    auto entry = ptr->getEntry();
    table.rows.push_back({ { tblString{ entry.path } } });
    determine_string_width strw(parentCtrl, theme);
    for (auto str : {&entry.path}) {
        auto widthLabel = strw.getStringWidth(*str, table.rowHeight, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        table.tableWidth = math::max(table.tableWidth, (widthLabel + INSET_TABLE_CELL_PADDING * 3) * 1.05f);
    }
}

guictxtmenu_base* gui_pluginlibrary_entry::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<gui_pluginlibrary_entry>(this);
    return tooltip;
}

void gui_pluginlist_entry::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
    //        mousepos += dragOffset;
    // // mousepos -= pos;
    // mousepos.x -= size.x / 2;
    nvgTranslate(vg, mousepos.x+20, mousepos.y+20);
    ivec2 inset                    = { 2, 2 };
    theme->bindFont(vg, UIFont::FONT_DEFAULT);
    nvgFillColor(vg, THEMECOL_TEXT);
    auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
    String text = "Insert " + label;
    float w = renderTextLabel(vg,
                    vec2(3.0f, size.y * 0.5f),
                    vec2(size.x - 6.0f, size.y),
                    text,
                    theme,
                    fontSizeScaled,
                    theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                    NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    auto bgSize = ivec2(w+12, size.y);
    drawBackground(vg, theme, -ivec2(bgSize.x/2, 0), bgSize, 0, false);
    w = renderTextLabel(vg,
                    vec2(3.0f, size.y * 0.5f),
                    vec2(size.x - 6.0f, size.y),
                    text,
                    theme,
                    fontSizeScaled,
                    theme->getColor(GuiColor::COL_LABEL_ACTIVE),
                    NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
}

void gui_pluginlist_entry::drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset) {
    posInset -= ivec2(margin);
    sizeInset += ivec2(margin) * 2;
    if (sizeInset.y > 0 && sizeInset.x > 0) {
        auto stateflags = getStateFlags();
        nvgTranslateZ(vg, -2.0f);
        nvgShapeAntiAlias(vg, 0);
        nvgBeginPath(vg);
        nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
        NVGcolor bg = theme->getColor(getBackgroundColorFromState(stateflags));
        nvgFillColor(vg, bg);
        nvgFill(vg);
        nvgShapeAntiAlias(vg, 1);
        nvgTranslateZ(vg, -2.0f);
        nvgTranslateZ(vg, 3.0f);
    }
}

void guictr_pluginlibrary::update() {
    std::vector<gui_list_entry*> _newList;
    pluginsLibList.clear();
    std::map<String, std::vector<pluginentry_t>> perFolder;
    std::map<int32_t, String> formatNames;
    formatNames[2] = "VST3";
    formatNames[1] = "Clap";
    formatNames[0] = "VST2";
    
    try {
        dawCtrl->getDaw()->getPluginDatabase().query(curquery, pluginsLibList);
        for (pluginentry_t& entry : pluginsLibList) {
            switch (groupBy) {
                case GROUP_BY_FORMAT:
                    perFolder[formatNames[entry.moduleFormat]].push_back(entry);
                    break;
                case GROUP_BY_VENDOR:
                    perFolder[entry.vendorName].push_back(entry);
                    break;
                case GROUP_BY_NONE:
                    _newList.push_back(new gui_pluginlibrary_entry(entry));
                    break;
            }
        }
        textField2.setValue("");
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "db->query: %s\n", e.what());
        String strValue = e.what();
        textField2.setValue(strValue);
    }

    if (groupBy == GROUP_BY_NONE) {
        pluginListCtr.setList(_newList);
    } else {
        for (auto entry : perFolder) {
            gui_list_folder_entry* folder = new gui_list_folder_entry(entry.first);
            folder->setIsOpened(isFolderOpen[folder->getLabel()]);
            _newList.push_back(folder);
            if (folder->isOpened()) {
                for (pluginentry_t& plugin : entry.second) {
                    gui_pluginlibrary_entry* g = new gui_pluginlibrary_entry(plugin);
                    g->setDepth(1);
                    _newList.push_back(g);
                }
            }
        }

        pluginListCtr.setList(_newList);
    }
    layout();
}

void guictr_pluginlibrary::buttonClicked(guibase* button) {
    guictr_base::buttonClicked(button);
    if (button->parent == &pluginListCtr) {
        auto gui = gui_cast<gui_list_folder_entry, gui_type::GUI_TYPE_LIST_FOLDER>(button);
        if (gui) {
            bool bIsOpened = gui->isOpened();
            gui->setIsOpened(!bIsOpened);
            isFolderOpen[gui->getLabel()] = !bIsOpened;
            update();
        }
    }
}

class ctxtmenu_entry_plugin_group_by_select final : public ctxtmenu_enum_option_select_base<ctxmenu_enum_select_entry> {
    SafeRef<guibase> safeRefPluginLib;
public:
    ctxtmenu_entry_plugin_group_by_select(guictr_pluginlibrary* _parent, String _title, int _id)
        : ctxtmenu_enum_option_select_base(_id, std::move(_title)), safeRefPluginLib(_parent->makeSafeRef())
    {
        entries.push_back({ 0, "Format" });
        entries.push_back({ 1, "Vendor" });
        entries.push_back({ 2, "None" });
    }
    bool isEntrySelected(ctxmenu_enum_select_entry& e) const override {
        auto pluginLib = static_cast<guictr_pluginlibrary*>(safeRefGet(safeRefPluginLib));
        return pluginLib->getGroupBy() == e.id;
    }
};

class guictr_pluginlibrary_context_menu final : public guictxtmenu {
    SafeRef<guibase> safeRefPluginLib;
public:
    explicit guictr_pluginlibrary_context_menu(guictr_pluginlibrary* _parent)
        : guictxtmenu(), safeRefPluginLib(_parent->makeSafeRef())
    {
        maxHeight = 0;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 100) {
            int clicked = _id - 100;
            auto pluginLib = static_cast<guictr_pluginlibrary*>(safeRefGet(safeRefPluginLib));
            pluginLib->setGroupBy(static_cast<guictr_pluginlibrary::GroupBy>(clicked));
        }
        closeContextMenu();
        return true;
    }
};

void guictr_pluginlibrary::handleRightClick(MouseEvent& evt) {

    auto ctxtMenu     = new guictr_pluginlibrary_context_menu(this);
    ctxtMenu->dawCtrl = dawCtrl;
    ctxtMenu->addEntry(new ctxtmenu_entry_plugin_group_by_select(this, "Group by", 100));
    parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
}
