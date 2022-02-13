#include "guicontainer_dnd_layout.h"
#include "basectrl.h"
#include "gui/guicontainer.h"
#include "gui/container/guicontainer_layout_types.h"
#include "gui/guishaderview.h"
#include "gui/dialog_io.h"
#include "gui/debugctr.h"

guictr_base* makeCtrProperties();
guictr_base* makeCtrTheme();
guictr_base* makeCtrHistory();
guictr_base* makeGuiPluginsLoadedList();
guictr_base* makeGuiEffectLibrary();
guictr_base* makeGuiPerformance();
guictr_base* makeGuiExport();
guictr_base* makeGuiClipEditor();

bool getContainerLabel(container_type type, String& out) {
    switch (type) {
        case CTR_TYPE_LAYOUT:
            out = "Layout";
            return true;
        case CTR_TYPE_BASE:
            out = "Base";
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
        default:
            break;
    }
    return false;
}
std::map<container_type, ContainerBuilder>& getContainerFactory() {
    static bool init = false;
    static std::map<container_type, ContainerBuilder> containerFactory;
    if (!init) {
#if BUILD_VSTHOST
        containerFactory[container_type::CTR_TYPE_DEBUG_0] = []() {
            return std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_0);
        };
        containerFactory[container_type::CTR_TYPE_DEBUG_1] = []() {
            return std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_1);
        };
        containerFactory[container_type::CTR_TYPE_DEBUG_2] = []() {
            return std::make_shared<gui_ctr_debug>(gui_ctr_debug::gui_ctr_debug_type_i32::TYPE_2);
        };
        containerFactory[container_type::CTR_TYPE_HISTORY] = []() {
            return std::shared_ptr<guictr_base>(makeCtrHistory());
        };
        containerFactory[container_type::CTR_TYPE_SHADERVIEW] = []() {
            return std::make_shared<gui_shaderview>();
        };
        containerFactory[container_type::CTR_TYPE_SETTINGS] = []() {
            return std::make_shared<DialogSettings::guidialog_settings>();
        };
        containerFactory[container_type::CTR_TYPE_EFFECTLIBRARY] = []() {
            return std::shared_ptr<guictr_base>(makeGuiEffectLibrary());
        };
        containerFactory[container_type::CTR_TYPE_PLUGINSLOADED] = []() {
            return std::shared_ptr<guictr_base>(makeGuiPluginsLoadedList());
        };
        containerFactory[container_type::CTR_TYPE_PERFORMANCE] = []() {
            return std::shared_ptr<guictr_base>(makeGuiPerformance());
        };
        containerFactory[container_type::CTR_TYPE_EXPORT] = []() {
            return std::shared_ptr<guictr_base>(makeGuiExport());
        };
        containerFactory[container_type::CTR_TYPE_CLIPEDITOR] = []() {
            return std::shared_ptr<guictr_base>(makeGuiClipEditor());
        };
#endif
        containerFactory[container_type::CTR_TYPE_PROPERTIES] = []() {
            return std::shared_ptr<guictr_base>(makeCtrProperties());
        };
        containerFactory[container_type::CTR_TYPE_THEME] = []() {
            return std::shared_ptr<guictr_base>(makeCtrTheme());
        };
        containerFactory[container_type::CTR_TYPE_LAYOUT] = []() {
            return std::make_shared<guictr_layout>();
        };
    }
    return containerFactory;
}
bool makeContainer(container_type type, std::shared_ptr<guictr_base>& out) {
    auto& fac = getContainerFactory();
    out       = nullptr;
    if (fac.count(type)) {
        ContainerBuilder& builder = fac[type];
        std::shared_ptr<guictr_base> sharedContainer = builder();
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
