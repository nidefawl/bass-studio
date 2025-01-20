#include "host/plugin/base/base-plugin.hpp"
#include "empty.hpp"
#include "event.hpp"
#include "gui/container/container.hpp"
#include "gui/gui.hpp"
#include "gui/plugin/plugin.hpp"
#include "gui/plugin/pluginctr.hpp"
#include "gui/views/pluginlist.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/plugindatabase/plugindatabase.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include "renderresources.hpp"
#include "str_util.hpp"
#include "threads/playbackthread.hpp"
#include "host/track/track_impl.hpp"
#include "host/track/track.hpp"
#include "types.hpp"


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
