#include "host/plugin/base/base-plugin.h"
#include "empty.h"
#include "event.h"
#include "gui/container/container.h"
#include "gui/gui.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/views/pluginlist.h"
#include "host/host_pluginmanager.h"
#include "host/daw/mainctrl.h"
#include "host/plugindatabase/plugindatabase.h"
#include "host/plugin/internal/internal-plugin.h"
#include "renderresources.h"
#include "str_util.h"
#include "threads/playbackthread.h"
#include "host/track/track_impl.h"
#include "host/track/track.h"
#include "types.h"


class guimodule_empty final : public guiplugin {
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
    : internalplugin("Empty", _projectGlobalId, _hostCallback),
      handle(new module_empty::internal_handles_t{ nullptr }) {
}

module_empty::~module_empty() {
    delete handle;
}

std::shared_ptr<guiplugin> module_empty::createGuiPlugin(int32_t uuid) {
    auto gui = std::make_shared<guimodule_empty>(this);
    gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
    return gui;
}

void module_empty::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
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
