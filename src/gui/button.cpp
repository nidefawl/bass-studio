#include "button.h"
#include "gui.h"
#include "guicontainer.h"
#include "guitooltip.h"
#include "renderresources.h"


using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<guibutton>::layout() {
    size.x          = 140;
    table.rowHeight = FONT_SIZE_TOOLTIP + INSET_TABLE_CELL_PADDING * 2;
    table.rows.clear();
    table.titleCols.clear();
    table.colSizes.clear();
    //row1.cols.push_back();
    {
        tbl_row_t row{};
        row.cols.push_back(tblString{ptr->getTooltipText()});
        table.rows.push_back(row);
    }
    Table::AdjustColSizes(table, getSizeContent() - ivec2(INSET_TABLE << 1));
    size.y = table.rows.size() * table.rowHeight;
}

void guibutton::renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
    if (drawFn || str.length()) {
        nvgSave(vg);
        setScissorTransform(vg);

        ivec2 renderPos(0);
        if (str.length() > 0) {
            auto fontScale = (this->fontSize > 0 ? this->fontSize : math::min(size.y, size.x)) * fFontScale;
            GuiColor::constant_t c = (stateFlags & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
            renderCenteredMultilineText(vg, theme, str, fontScale, c, renderPos, size);
        }

        if (drawFn) {
            int drawParm2 = isFlag(FLG_RENDER_BUTTON_WITH_LED) ? (getState() ? IMG_LED : IMG_LED_OFF) : getState();
            drawFn(vg, renderPos, size, getBackgroundColor(getStateFlags()), drawParm, drawParm2);
        }
        nvgRestore(vg);
    }
}
guictxtmenu_base* guibutton::getTooltip(AppCtrl* appctrl) {
    if (!label.empty()) {

        auto tooltip = new guitooltip<guibutton>(this);
        return tooltip;
    }
    return nullptr;
}
