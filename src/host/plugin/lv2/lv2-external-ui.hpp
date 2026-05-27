#pragma once

#include <lv2/ui/ui.h>

/** KXStudio / Nedko LV2 External UI — host side (plugin UI is external-ui#Widget). */
#define LV2_EXTERNAL_UI__Host "http://kxstudio.sf.net/ns/lv2ext/external-ui#Host"
#define LV2_EXTERNAL_UI_DEPRECATED_HOST_URI "http://nedko.arnaudov.name/lv2/externalui#Host"
/** Alias used by Carla and older plugins. */
#define LV2_EXTERNAL_UI_DEPRECATED_URI LV2_EXTERNAL_UI_DEPRECATED_HOST_URI

typedef struct {
    void (*ui_closed)(LV2UI_Controller controller);
    const char* plugin_human_id;
} LV2_External_UI_Host;
