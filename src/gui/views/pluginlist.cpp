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
