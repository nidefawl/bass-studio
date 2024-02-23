#pragma once
class guictr_base;
class guidialog_base;
class DawInstance;

struct create_ctr_t {
    DawInstance* const daw;
};

namespace DAW::UI {
guictr_base* makeGuiObjectProperties(create_ctr_t ctxt);
guictr_base* makeGuiThemeEditor(create_ctr_t ctxt);
guictr_base* makeGuiHistoryList(create_ctr_t ctxt);
guictr_base* makeGuiPluginsLoadedList(create_ctr_t ctxt);
guictr_base* makeGuiEffectLibrary(create_ctr_t ctxt);
guictr_base* makeGuiPerformance(create_ctr_t ctxt);
guictr_base* makeGuiExport(create_ctr_t ctxt);
guictr_base* makeGuiClipEditor(create_ctr_t ctxt);
guictr_base* makeGuiMidiInspect(create_ctr_t ctxt);
guidialog_base* makeGuiExportDialog(create_ctr_t ctxt);
}