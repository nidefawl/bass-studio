#include "container_dnd_layout.h"
#include "basectrl.h"
#include "container.h"
#include "gui/container/container_builder.h"
#include "gui/container/container_layout_types.h"
#include "gui/gui.h"
#include "gui/views/shaderview.h"
#include "gui/dialog/dialog_io.h"
#include "gui/views/debugctr.h"
#include "gui/shape/shapeeditor.h"
#include "logging.h"
#include "tls.h"

bool getContainerLabel(gui_type type, String& out) {
    switch (type) {
        case CTR_TYPE_LAYOUT:
            out = "Layout";
            return true;
        case CTR_TYPE_UNKNOWN:
            out = "Unknown";
            return true;
        case CTR_TYPE_PROPERTIES:
            out = "Properties";
            return true;
        case CTR_TYPE_THEME:
            out = "Theme";
            return true;
        case CTR_TYPE_HISTORY:
            out = "History";
            return true;
        case CTR_TYPE_SHADERVIEW:
            out = "Shader Test";
            return true;
        case CTR_TYPE_SETTINGS:
            out = "Preferences";
            return true;
        case CTR_TYPE_EFFECTLIBRARY:
            out = "Plugin Library";
            return true;
        case CTR_TYPE_PLUGINSLOADED:
            out = "Loaded Plugins";
            return true;
        case CTR_TYPE_DEBUG_0:
            out = "Debug 0";
            return true;
        case CTR_TYPE_DEBUG_1:
            out = "Debug AppCtrl State";
            return true;
        case CTR_TYPE_DEBUG_2:
            out = "Debug 2";
            return true;
        case CTR_TYPE_PERFORMANCE:
            out = "Performance";
            return true;
        case CTR_TYPE_EXPORT:
            out = "Export Audio";
            return true;
        case CTR_TYPE_CLIPEDITOR:
            out = "Clip Editor";
            return true;
        case GUI_TYPE_UNKNOWN:
            out = "Unknown";
            return true;
        case GUI_TYPE_BUTTON:
            out = "Button";
            return true;
        case GUI_TYPE_KNOB:
            out = "Knob";
            return true;
        case GUI_TYPE_SCROLLBAR:
            out = "Scrollbar";
            return true;
        case GUI_TYPE_TEXTFIELD:
            out = "Textfield";
            return true;
        case CTR_TYPE_SHAPE_EDITOR:
            out = "Shape Editor";
            return true;
        case CTR_TYPE_KEYBINDS:
            out = "Keybinds";
            return true;
        case CTR_TYPE_MIDI_MONITOR:
            out = "Midi Monitor";
            return true;
        case CTR_TYPE_TRACKS:
            out = "Tracks";
            return true;
        case CTR_TYPE_NODES:
            out = "Nodes";
            return true;
        case CTR_TYPE_PLUGINS:
            out = "Plugins";
            return true;
        default:
            break;
    }
    return false;
}
ContainerFactory& getContainerFactory() {
    static bool init = false;
    static std::map<gui_type, ContainerBuilder> containerFactory;
    if (!init) {
#if BUILD_DAW_HOST
        containerFactory[gui_type::CTR_TYPE_DEBUG_0] = [](auto& ctxt) {
            return std::make_shared<gui_ctr_debug>(ctxt, gui_ctr_debug::DebugCtrType::TYPE_0);
        };
        containerFactory[gui_type::CTR_TYPE_DEBUG_1] = [](auto& ctxt) {
            return std::make_shared<gui_ctr_debug>(ctxt, gui_ctr_debug::DebugCtrType::DEBUG_APPCTRL);
        };
        containerFactory[gui_type::CTR_TYPE_DEBUG_2] = [](auto& ctxt) {
            return std::make_shared<gui_ctr_debug>(ctxt, gui_ctr_debug::DebugCtrType::TYPE_2);
        };
        containerFactory[gui_type::CTR_TYPE_HISTORY] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiHistoryList(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_SHADERVIEW] = [](auto& ctxt) {
            return std::make_shared<gui_shaderview>();
        };
        containerFactory[gui_type::CTR_TYPE_SETTINGS] = [](auto& ctxt) {
            return std::make_shared<DAW::DialogSettings::guidialog_settings>(ctxt.daw);
        };
        containerFactory[gui_type::CTR_TYPE_EFFECTLIBRARY] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiEffectLibrary(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_PLUGINSLOADED] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiPluginsLoadedList(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_PERFORMANCE] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiPerformance(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_EXPORT] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiExport(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_CLIPEDITOR] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiClipEditor(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_KEYBINDS] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::DialogSettings::makeKeybindsDialog(ctxt.daw));
        };
        containerFactory[gui_type::CTR_TYPE_MIDI_MONITOR] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiMidiInspect(ctxt));
        };
#endif
        containerFactory[gui_type::CTR_TYPE_PROPERTIES] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiObjectProperties(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_THEME] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(DAW::UI::makeGuiThemeEditor(ctxt));
        };
        containerFactory[gui_type::CTR_TYPE_LAYOUT] = [](auto& ctxt) {
            return std::make_shared<guictr_layout>();
        };
        containerFactory[gui_type::CTR_TYPE_SHAPE_EDITOR] = [](auto& ctxt) {
            auto shapeEditor = makeShapeEditor();
            return std::shared_ptr<guictr_base>(shapeEditor->getGuiContainer());
        };
    }
    return containerFactory;
}
ContainerRegistry& getContainerRegistry() {
    static ContainerRegistry sortedVector;
    static bool init = false;
    if (!init) {
        init = true;
        auto fac = getContainerFactory();
        for (auto& it : fac) {
            auto guiType = it.first;
            String name;
            getContainerLabel(guiType, name);
            if (name.empty())
                continue;
            sortedVector.emplace_back(guiType, name);
        }
        std::sort(sortedVector.begin(), sortedVector.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
    }
    return sortedVector;
}
bool makeContainer(ContainerInstanceContext& ctxt, gui_type type, std::shared_ptr<guictr_base>& out) {
    auto& fac = getContainerFactory();
    out       = nullptr;
    if (fac.count(type)) {
        ContainerBuilder& builder = fac[type];
        auto createContainer = create_ctr_t{ctxt.daw};
        std::shared_ptr<guictr_base> sharedContainer = builder(createContainer);
        if (!sharedContainer) {
            log_printf("Failed building container of type %d\n", type);
            return false;
        }
        getContainerLabel(type, sharedContainer->label);
        out = sharedContainer;
        return true;
    }
    return false;
}
SPLayoutEntry createGuiCtrLayoutEntry(const std::shared_ptr<guictr_layout>& ctr) {
    SPLayoutEntry entry1 = std::make_shared<GuiCtrLayoutEntry>(ctr->getLabel(), ctr);
    entry1->selfLayoutCtr = ctr;
    return entry1;
}
SPLayoutEntry createGuiCtrLayoutEntry(const std::shared_ptr<guictr_base>& ctr) {
    dbgassert(ctr->getGuiType() != gui_type::CTR_TYPE_LAYOUT);
    SPLayoutEntry entry1 = std::make_shared<GuiCtrLayoutEntry>(ctr->getLabel(), ctr);
    return entry1;
}
