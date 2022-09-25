#include "types.h"
#include "empty.h"
#include "event.h"
#include "str_util.h"
#include "renderresources.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginctr.h"
#include "gui/views/pluginlist.h"

#include "base_plugin.h"
#include "internal_plugin.h"

#include "host/mainctrl.h"
#include "host/host_pluginmanager.h"
#include "host/plugindatabase.h"
#include "threads/playbackthread.h"

#include "track.h"
#include "track_impl.h"
#include "gui/plugin/plugin.h"


class guimodule_empty : public guiplugin {
public:
    module_empty* const module;
    explicit guimodule_empty(module_empty* _vst);
    void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
    }
};

guimodule_empty::guimodule_empty(module_empty* _vst)
    : guiplugin(_vst),
      module(_vst) {
}

struct module_empty::internal_handles_t {
    std::unique_ptr<guimodule_empty> gui;
};

module_empty::module_empty(int32_t _projectGlobalId, IHostCallback* _hostCallback)
    : internalplugin("Empty", PLUGIN_TYPE_EMPTY, _projectGlobalId, _hostCallback),
      handle(new module_empty::internal_handles_t{ nullptr }) {
}

module_empty::~module_empty() {
    delete handle;
}

guiplugin* module_empty::makeGui() {
    if (!handle->gui) {
        handle->gui = std::make_unique<guimodule_empty>(this);
        handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
    }
    return handle->gui.get();
}

guiplugin* module_empty::getGui() {
    return handle->gui.get();
}

void module_empty::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    dbgassert(in->samples == format.blockSize
              && out->samples == format.blockSize
              && format.blockSize > 0
              && format.sampleRate > 0);
    out->copyFrom(in);
}

template<>
effectbase* makeInstance<module_empty>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new module_empty(_projectGlobalId, _hostCallback);
}
