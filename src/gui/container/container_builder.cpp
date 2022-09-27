#include "container_dnd_layout.h"
#include "basectrl.h"
#include "container.h"
#include "gui/container/container_layout_types.h"
#include "gui/gui.h"
#include "gui/views/shaderview.h"
#include "gui/dialog/dialog_io.h"
#include "gui/views/debugctr.h"
#include "gui/shape/shapeeditor.h"

guictr_base* makeCtrProperties();
guictr_base* makeCtrTheme();
guictr_base* makeCtrHistory();
guictr_base* makeGuiPluginsLoadedList();
guictr_base* makeGuiEffectLibrary();
guictr_base* makeGuiPerformance();
guictr_base* makeGuiExport();
guictr_base* makeGuiClipEditor();

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
            out = "Settings";
            return true;
        case CTR_TYPE_EFFECTLIBRARY:
            out = "Plugins";
            return true;
        case CTR_TYPE_PLUGINSLOADED:
            out = "Instances";
            return true;
        case CTR_TYPE_DEBUG_0:
            out = "Debug 0";
            return true;
        case CTR_TYPE_DEBUG_1:
            out = "Debug 1";
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
            return std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_0);
        };
        containerFactory[gui_type::CTR_TYPE_DEBUG_1] = [](auto& ctxt) {
            return std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_1);
        };
        containerFactory[gui_type::CTR_TYPE_DEBUG_2] = [](auto& ctxt) {
            return std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2);
        };
        containerFactory[gui_type::CTR_TYPE_HISTORY] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeCtrHistory());
        };
        containerFactory[gui_type::CTR_TYPE_SHADERVIEW] = [](auto& ctxt) {
            return std::make_shared<gui_shaderview>();
        };
        containerFactory[gui_type::CTR_TYPE_SETTINGS] = [](auto& ctxt) {
            return std::make_shared<DAW::DialogSettings::guidialog_settings>(ctxt.daw);
        };
        containerFactory[gui_type::CTR_TYPE_EFFECTLIBRARY] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeGuiEffectLibrary());
        };
        containerFactory[gui_type::CTR_TYPE_PLUGINSLOADED] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeGuiPluginsLoadedList());
        };
        containerFactory[gui_type::CTR_TYPE_PERFORMANCE] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeGuiPerformance());
        };
        containerFactory[gui_type::CTR_TYPE_EXPORT] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeGuiExport());
        };
        containerFactory[gui_type::CTR_TYPE_CLIPEDITOR] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeGuiClipEditor());
        };
#endif
        containerFactory[gui_type::CTR_TYPE_PROPERTIES] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeCtrProperties());
        };
        containerFactory[gui_type::CTR_TYPE_THEME] = [](auto& ctxt) {
            return std::shared_ptr<guictr_base>(makeCtrTheme());
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
bool makeContainer(ContainerInstanceContext& ctxt, gui_type type, std::shared_ptr<guictr_base>& out) {
    auto& fac = getContainerFactory();
    out       = nullptr;
    if (fac.count(type)) {
        ContainerBuilder& builder = fac[type];
        std::shared_ptr<guictr_base> sharedContainer = builder(ctxt);
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
std::shared_ptr<guictr_layout_entry> createGuiCtrLayoutEntry(std::shared_ptr<guictr_base> ctr) {
    std::shared_ptr<guictr_layout_entry> entry1 = std::make_shared<guictr_layout_entry>(ctr->getLabel(), ctr);
    return entry1;
}
