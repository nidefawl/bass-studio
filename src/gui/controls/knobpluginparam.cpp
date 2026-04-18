#include "knobpluginparam.hpp"
#include "gui/table/table.hpp"
#include "gui/tooltip/tooltip.hpp"

using Table::table_entry_t;
using Table::tbl_row_t;
using Table::tblfloat;

template<>
void guitooltip<guiknob_pluginparam>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    auto eff = ptr->getEffectInstance();
    auto paramIdx = ptr->getParamIdx();
    if (eff)
        eff->addPropertiesParameterTooltip(table, paramIdx);
    if (table.rows.empty()) {
        auto cell = Table::tblfloat{ ptr->getValue() };
        table.tableWidth = 20;
        if (table.strW) {
            table.tableWidth = table.strW->getStringWidth(StringAsCStr(StringFormat("%f", cell.f)));
        }
        Table::tbl_row_t row{{cell}};
        table.rows.push_back(std::move(row));
    }
}

guictxtmenu_base* guiknob_pluginparam::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<guiknob_pluginparam>(this);
    return tooltip;
}