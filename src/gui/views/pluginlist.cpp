#include "pluginlist.h"
#include "gui/table/table.h"
#include "guicolors.h"
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
    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
    UIFont::bindFont(vg, instance);
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
        nvgShapeAntiAlias(vg, USE_NANOVG_AA);
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
        log_lf(Log::L_ERROR, "Error: %s\n", e.what());
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
        log_printf("Selected %s\n", StringAsCStr(button->getLabel()));
        auto gui = gui_cast<gui_list_folder_entry, gui_type::GUI_TYPE_LIST_FOLDER>(button);
        if (gui) {
            bool bIsOpened = gui->isOpened();
            gui->setIsOpened(!bIsOpened);
            isFolderOpen[gui->getLabel()] = !bIsOpened;
            update();
        }
    }
}

class ctxtmenu_entry_plugin_group_by_select final : public ctxtmenu_entry {
    SafeRef<guibase> safeRefPluginLib;

    struct _group_by_entry {
        int id;
        String name;
        int x = 0;
        int y = 0;
        int w = 0;
    };
    std::vector<_group_by_entry> entries;

public:
    const int pad   = 10;
    const int inset = 5;
public:
    ctxtmenu_entry_plugin_group_by_select(guictr_pluginlibrary* _parent, String _title, int _id)
        : ctxtmenu_entry(std::move(_title), _id), safeRefPluginLib(_parent->makeSafeRef())
    {
        entries.push_back({ 0, "Format" });
        entries.push_back({ 1, "Vendor" });
        entries.push_back({ 2, "None" });
    }

    void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
        width = size.x;
        this->fontSize = _fontSize;
        const int h    = math::roundfS32(_fontSize);
        layoutE(width, h, 3);
    }

    void layoutE(int tw, int h, int perRow) {
        int iX      = inset;
        int iY      = h + 2;
        int elW     = (tw - inset * 2) / perRow;
        for (_group_by_entry& e : entries) {
            this->height = iY + h;
            e.x = iX;
            e.y = iY;
            e.w = elW;
            iX += e.w;
            if (iX >= tw - inset * 2) {
                iX = inset;
                iY += h;
            }
        }
    }


    void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
        auto h = fontSize * 1.1f;

        auto pluginLib = static_cast<guictr_pluginlibrary*>(safeRefGet(safeRefPluginLib));
        auto selected = pluginLib->getGroupBy();
        for (_group_by_entry& e : entries) {
            if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                nvgBeginPath(vg);
                nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }
            if (e.id == selected) {
                nvgBeginPath(vg);
                nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                nvgFill(vg);
            }
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + h * 0.5f),
                        vec2(width, h),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (_group_by_entry& e : entries) {
            renderTextLabel(vg,
                            vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                            vec2(width, h),
                            e.name,
                            theme,
                            fontSize * 0.9f,
                            theme->getColor(GuiColor::COL_TEXT),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }

    bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
        return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
    }

    int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
        if (contains(ctxtSize, mouse)) {
            const auto h = this->fontSize;
            for (_group_by_entry& e : entries) {
                if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                    return 100 + e.id;
                }
            }
        }
        return -1;
    }
};

class guictr_pluginlibrary_context_menu final : public guictxtmenu {
    SafeRef<guibase> safeRefPluginLib;
public:
    explicit guictr_pluginlibrary_context_menu(guictr_pluginlibrary* _parent)
        : guictxtmenu(), safeRefPluginLib(_parent->makeSafeRef())
    {
        this->size.x   = 270;
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
    ctxtMenu->addEntry(new ctxtmenu_entry_plugin_group_by_select(this, "Group by", 0));
    parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
}
