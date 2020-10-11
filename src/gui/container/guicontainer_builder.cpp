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
			return std::make_shared<guidialog_settings>();
		};
		containerFactory[container_type::CTR_TYPE_EFFECTLIBRARY] = []() {
			return std::shared_ptr<guictr_base>(makeGuiEffectLibrary());
		};
		containerFactory[container_type::CTR_TYPE_PLUGINSLOADED] = []() {
			return std::shared_ptr<guictr_base>(makeGuiPluginsLoadedList());
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
std::shared_ptr<guictr_layout_entry> createGuiCtrLayoutEntry(std::shared_ptr<guictr_base> ctr) {
	std::shared_ptr<guictr_layout_entry> entry1 = std::make_shared<guictr_layout_entry>(ctr->getLabel(),  ctr);
	return entry1;
}
