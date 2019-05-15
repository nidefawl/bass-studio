#pragma once
#include <stdint.h>
#include "edithistory.h"
#include "plugin/base_plugin.h"
#include "mainctrl.h"
#include "midiarp.h"

struct parameter_ref_t {
	int32_t trackIdx;
	int32_t type;
	int32_t slot;
	int32_t paramIdx;
};
class action_modify_effect_parameter : public action_base {
	parameter_ref_t ref{ 0 };
	float valBefore = 0;
	float valAfter = 0;
public:
	action_modify_effect_parameter() : action_base() {
	}
	//desc, clip, notesBefore, cursorBefore
	action_modify_effect_parameter(String description, parameter_ref_t _ref, float _oldVal, float _newVal) :
		action_base(),
		ref(_ref),
		valBefore(_oldVal),
		valAfter(_newVal) {
		desc = description;

		automatable_t* at = tryGetAt(MainCtrl::get());
		dbgassert(at);
		my_printf("Undo modify parameter task: set %s::%s (idx %d) from %f to %f\n", StringAsCStr(at->getAutomatableName()), StringAsCStr(at->getParamName(_ref.paramIdx)), _ref.paramIdx,  _oldVal, _newVal);
	}
	//TODO: this shouldn't be here
	automatable_t* tryGetAt(MainCtrl* ctrl) {
		track_t* tr = ctrl->getTracks()[ref.trackIdx];
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
	void undo(MainCtrl* ctrl) {

		automatable_t* at = tryGetAt(ctrl);
		if (at) {
			my_printf("undo(): set %s::%s (idx %d) from %f to %f\n", StringAsCStr(at->getAutomatableName()), StringAsCStr(at->getParamName(ref.paramIdx)), ref.paramIdx,  valBefore, valAfter);

			at->setParamValue(ref.paramIdx, valBefore, FLG_PAR_UPDATE_UNDO);
		}
	}
	void redo(MainCtrl* ctrl) {
		automatable_t* at = tryGetAt(ctrl);
		if (at) {
			my_printf("redo(): set %s::%s (idx %d) from %f to %f\n", StringAsCStr(at->getAutomatableName()), StringAsCStr(at->getParamName(ref.paramIdx)), ref.paramIdx,  valBefore, valAfter);

			at->setParamValue(ref.paramIdx, valAfter, FLG_PAR_UPDATE_UNDO);
		}
	}
};
