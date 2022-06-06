#include "knobpluginparam.h"
#include "gui/table/table.h"
#include "gui/tooltip/tooltip.h"

using Table::table_entry_t;
using Table::tbl_row_t;
using Table::tblfloat;

template<>
void guitooltip<guiknob_pluginparam>::setContent() {
    auto eff = ptr->getEffectInstance();
    auto paramIdx = ptr->getParamIdx();
    if (eff)
        eff->addPropertiesParameterTooltip(table, paramIdx);
    if (table.rows.empty()) {
        table.tableWidth = 100;
        table.rows.push_back({ { tblfloat{ ptr->getValue() } } });
    }
}

guictxtmenu_base* guiknob_pluginparam::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<guiknob_pluginparam>(this);
    return tooltip;
}