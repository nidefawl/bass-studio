#pragma once
#include "types.h"
#include "edithistory.h"
#include "plugin/base_plugin.h"
#include "mainctrl.h"
#include "midiarp.h"

struct parameter_ref_t {
    const int32_t trackIdx;
    const int32_t type;
    const int32_t slot;
    const int32_t paramIdx;
};

class action_modify_effect_parameter : public action_base {
    parameter_ref_t ref{};
    float valBefore = 0;
    float valAfter  = 0;

public:
    action_modify_effect_parameter() = default;

    action_modify_effect_parameter(String _desc, parameter_ref_t _ref, float _oldVal, float _newVal)
        : action_base(),
          ref(_ref),
          valBefore(_oldVal),
          valAfter(_newVal) {
        this->desc = std::move(_desc);

        automatable_t* at = tryGetAt(DawInstance::get());
        dbgassert(at);
        log_lf(Log::L_DEBUG, "Undo modify parameter task: set %s::%s (idx %d) from %f to %f\n",
                   StringAsCStr(at->getAutomatableName()),
                   StringAsCStr(at->getParamName(_ref.paramIdx)),
                   _ref.paramIdx, _oldVal, _newVal);
    }

    //TODO: this shouldn't be here
    automatable_t* tryGetAt(DawInstance* daw) {
        auto& tracks = daw->getTracks();
        if (!tracks.validTrackIdx(ref.trackIdx)) {
            setError("track missing");
            return nullptr;
        }
        track_t* tr = tracks[ref.trackIdx];
        if (!tr) {
            setError("track missing");
            return nullptr;
        }
        auto audio = tr->audio;
        if (!audio) {
            setError("track does not have audio stage");
            return nullptr;
        }
        automatable_t* at = nullptr;
        switch (ref.type) {
            case AUTOMATABLE_ARP:
                at = tr->audio->arp;
                break;
            case AUTOMATABLE_MIXER:
                at = &tr->audio->mixer;
                break;
            case AUTOMATABLE_EFFECT:
                at = tr->audio->getPluginById(ref.slot);
                break;
        }
        if (!at) {
            setError("track audio stage does not at");
            return nullptr;
        }
        return at;
    }

    void undo(DawInstance* daw) override {

        automatable_t* at = tryGetAt(daw);
        if (at) {
            log_lf(Log::L_DEBUG, "undo(): set %s::%s (idx %d) from %f to %f\n",
                       StringAsCStr(at->getAutomatableName()),
                       StringAsCStr(at->getParamName(ref.paramIdx)),
                       ref.paramIdx, valBefore, valAfter);

            at->setParamValue(ref.paramIdx, valBefore, FLG_PAR_UPDATE_UNDO);
        }
    }

    void redo(DawInstance* daw) override {
        automatable_t* at = tryGetAt(daw);
        if (at) {
            log_lf(Log::L_DEBUG, "redo(): set %s::%s (idx %d) from %f to %f\n",
                       StringAsCStr(at->getAutomatableName()),
                       StringAsCStr(at->getParamName(ref.paramIdx)),
                       ref.paramIdx, valBefore, valAfter);

            at->setParamValue(ref.paramIdx, valAfter, FLG_PAR_UPDATE_UNDO);
        }
    }
};
