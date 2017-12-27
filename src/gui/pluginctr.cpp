#include <glm/vec2.hpp>
#include "guicontainer.h"
#include "button.h"
#include "plugin.h"
#include "event.h"
#include "pluginctr.h"
#include "str_util.h"
#include "../host/vst_plugin.h"
#include "../host/vst_plugin_handles.h"
#include "track_audiodata.h"
#include "../host/vst_host.h"
#include "../host/plugindatabase.h"
#include "../threads/playbackthread.h"
#include "pluginlist.h"
#include "renderresources.h"
#include "logging.h"
#include "list.h"
#include "track.h"
#include "guicontextmenu.h"
#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;

void guiplugin::handleDraggedMove(MouseEvent& evt) {
	MainCtrl::get()->objectDragMove(this, evt);
}
void guiplugin::handleDraggedRelease(MouseEvent& evt) {
	MainCtrl::get()->objectDragRelease(this, evt);
}
void guiplugin::dragMoveOn(guibase* target, ivec2 mousepos) {
	target->pluginDragMove(this, mousepos);
}
void guiplugin::dragReleaseOn(guibase* target, ivec2 mousepos) {
	target->pluginDragRelease(this, mousepos);
}

void guiplugin::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, size.x, size.y, G_RND);
	NVGcolor c;
	guibase* b = MainCtrl::get()->guiFocused;

	if (b == this) {
		c = g_guiColors[COL_BG_DRK_FOCUSED];
	}
	else {
		c = GUI_COLOR(G_S4);
	}
	nvgFillColor(vg, GUI_COLOR(G_S2));
	nvgFill(vg);
	nvgBeginPath(vg);
	nvgRoundedRectVarying(vg, 0, 0, size.x, HEIGHT_PLUGIN_TITLE, G_RND, G_RND, 0, 0);
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (this->text[0]) {
		setFont(vg, (int)(HEIGHT_PLUGIN_TITLE*0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, buttonOpenEditor.right()+INSET_TITLE, HEIGHT_PLUGIN_TITLE / 2, this->text, NULL);
	}
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, size.x, size.y, G_RND);
	nvgStrokeColor(vg, GUI_COLOR(G_S1));
	nvgStrokeWidth(vg, G_STROKE);
	nvgStroke(vg);
	buttonBypass.render(vg);
	buttonOpenEditor.render(vg);
	buttonDelete.render(vg);

	params.renderBackground(vg);
	params.render(vg);
}
bool guiplugin::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (contains(mpos)) {
		ivec2 mouseLocal = mpos - pos;
		if (buttonBypass.mouseHitTest(mouseLocal, evt)) {
			return true;
		}
		if (buttonOpenEditor.mouseHitTest(mouseLocal, evt)) {
			return true;
		}
		if (buttonDelete.mouseHitTest(mouseLocal, evt)) {
			return true;
		}
		if (params.mouseHitTest(mouseLocal, evt)) {
			return true;
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guiplugin::buttonClicked(guibase* _button) {
	if (_button == &buttonBypass) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		if (vst->bIsEnabled) {
			vst->sleep();
		} else {
			vst->resume();
		}
		if (vst->isSynth) {
			vsthost::getInstance()->sendNotesOff(vst);
		}

	}
	if (_button == &buttonOpenEditor) {
		if (vst->bEditOpen) {
			vst->close();
		} else {
			vst->show();
		}
	}
	if (_button == &buttonDelete) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    	vsthost::getInstance()->unloadPlugin(vst);
	}
}
class guictxtmenu_vstparam : public guictxtmenu_base {
	static const int32_t ID_DELETE = 1;
	static const int32_t ID_CREATE = 2;
	static const int32_t ID_REENABLE = 3;
	vstplugin* const vst;
	vst_param* const entry;
public:
	guictxtmenu_vstparam(vstplugin* _vst, vst_param* _entry) : vst(_vst), entry(_entry)
	{
		this->size.x = 240;
		automated_param_t* param = _vst->getRegisteredAutomation(_entry->idx);
		if (param) {
			if (!param->src->isActive()) {
				add(new ctxtmenu_entry("Reenable Automation", ID_REENABLE));
			}
			add(new ctxtmenu_entry("Delete Automation", ID_DELETE));
		} else {
			add(new ctxtmenu_entry("Create Automation Track", ID_CREATE));
		}
		layout();
	}
	void clicked(int _id) {
		switch (_id) {
		case ID_CREATE:
		{

			track_t* track = MainCtrl::get()->insertNewTrack(-1, TRACK_TYPE_AUTOMATION);
			trackdata_automation_t& automation = track->getAutomation();
			automated_param_t& target = automation.target;
			target.paramIdx = entry->idx;
			target.src = &automation.src;
			target.ref = &automation;
			target.plugin = NULL;
			vst->registerAutomationSrc(target);
		}
			break;
		case ID_DELETE:
			break;
		case ID_REENABLE:
			break;
		}
		MainCtrl::get()->closeContextMenu();
	}
};

class gui_plugin_paramlist_entry : public gui_list_entry {

	const float spacing = INSET_TITLE;
public:
	vstplugin* const vst;
	vst_param* const entry;
	guiknob knobTest;
	gui_plugin_paramlist_entry(vstplugin* _vst, vst_param* _entry)
		: gui_list_entry(),
		  vst(_vst),
		  entry(_entry),
		  knobTest(false)
	{
		icon = 0;
		const int32_t paramIdx = _entry->idx;
		knobTest.fnGetValue = [_vst, paramIdx] () {
			return _vst->getParamValue(paramIdx);
		};
		knobTest.fnSetValue = [_vst, paramIdx] (float f) {
			return _vst->setParamValue(paramIdx, f);
		};
	}
	void handleRightClick(MouseEvent& evt) override {
		MainCtrl::get()->openContextMenu(new guictxtmenu_vstparam(vst, entry), evt.mousepos);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			if (evt.type != MouseHitType::MOUSE_RIGHT)
			{
				if (knobTest.mouseHitTest(mpos, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
	}
	String getText() override {
		return entry->label;
	}
	void layout() {
		knobTest.pos = pos + ivec2(spacing);
		knobTest.size = ivec2(size.y, size.y) - ivec2(spacing*2);
	}
	virtual void render(NVGcontext* vg) {
		MainCtrl* ctrl = MainCtrl::get();
		float rowHeight = size.y;
		float x = knobTest.right()+spacing;
		if (ctrl->guiFocused == this) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_DRKER]);
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
		nvgTranslate(vg, -pos.x, -pos.y);
		knobTest.render(vg);
	}
};
guiplugin::guiplugin(vstplugin* _vst)
: guibase(),
  vst(_vst),
  params(48),
  buttonBypass(ivec2(0), 12),
  buttonOpenEditor(ivec2(0), 12),
  buttonDelete(ivec2(0), 12) {
	text[0] = 0;
	buttonBypass.icon = ICON_BYPASS;
	buttonBypass.state = &vst->bIsEnabled;
	buttonBypass.parent = this;
	buttonBypass.setColor(0x80c040);
	buttonOpenEditor.icon = ICON_ADJUST;
	buttonOpenEditor.state = &vst->bEditOpen;
	buttonOpenEditor.parent = this;
	buttonOpenEditor.setColor(0x40ABC0);
	buttonDelete.icon = ICON_CLOSE;
	static bool closeEnabled = true;
	buttonDelete.state = &closeEnabled;
	buttonDelete.parent = this;
	buttonDelete.setColor(0x404040);
	std::vector<gui_list_entry*> _newList;
	for (vst_param& param : _vst->params) {
		_newList.push_back(new gui_plugin_paramlist_entry(_vst, &param));
	}
	params.setList(_newList);
}
void guictr_plugins::addGui(vstplugin* plugin) {
	if (!plugin->handle->gui) {
		plugin->handle->gui = std::make_unique<guiplugin>(plugin);
		plugin->handle->gui->setTitle(StringFormat("%s", StringAsCStr(plugin->sName)));
	}
	add(plugin->handle->gui.get());
}
void guictr_plugins::showTrack(track_t* _track) {
	this->track = _track;
	removeGuis();
	if (track) {
		track_plugins_t* audio = track->audio;
		if (audio && audio->instrument != NULL) {
			addGui(audio->instrument);
		} else {
			add(&placeholder);
		}
		if (audio && !audio->effects.empty()) {
			for (vstplugin* vst : audio->effects) {
				addGui(vst);
			}
		}
	}

	layout();
	if (track) {
		setScrolloffset(this->track->scrolloffset);
	}
}

void guictr_plugins::pluginDragMove(guiplugin* g, ivec2 mousepos) {
	if (!track) return;
	my_printf("pluginDragMove\n",0);
	highlightSlot = -1;
	track_plugins_t* trp = g->vst->handle->tr_plugins;
	if (!trp) {
		assert(0&&"TRP WAS NULL");
		return;
	}
	if (g->vst->isSynth) {
		if (trp->track == track) {
			return;
		}
		highlightSlot = 0;
		return;
	}
	my_printf("handle dragg\n",0);
//	if (abs((evt.dragStart - evt.mousepos).x) > getSizeContent().y / 4) {
		highlightSlot = slotFromCoord(mousepos);
		int curSlot = trp->track == track ? (g->vst->handle->slot) : -2;
		if (highlightSlot == 0 || curSlot == highlightSlot || curSlot + 1 == highlightSlot) {
			highlightSlot = -1;
		}
//	}
	return;
}
void guictr_plugins::pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) {
	if (!track) return;
	highlightSlot = slotFromCoord(mousepos);
}
void guictr_plugins::pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) {
	if (!track) return;
	int32_t dstSlot = highlightSlot;
	highlightSlot = -1;
	const pluginentry_t& entry = g->entry;
	vstpluginloadres res = vsthost::getInstance()->loadPlugin(entry.path);
	if (res.result == 0 && res.plugin) {
		vsthost::getInstance()->insertNewPlugin(track->audio, res.plugin, dstSlot);
	}
	showTrack(track);
}
void guictr_plugins::pluginDragRelease(guiplugin* g, ivec2 mousepos) {
	if (!track) return;
	highlightSlot = -1;
	int targetslot = slotFromCoord(mousepos);
	my_printf("pluginDragRelease %d\n",targetslot);
	track_plugins_t* trp = g->vst->handle->tr_plugins;
	if (!trp) {
		assert(0&&"TRP WAS NULL");
		return;
	}
	int curSlot = g->vst->handle->slot;
	my_printf("pluginDragRelease curSlot %d\n",curSlot);
	if (trp->track == track && (curSlot == targetslot || curSlot + 1 == targetslot)) {
		targetslot = -1;
	}
	if (g->vst->isSynth) {
		if (trp->track == track) {
			my_printf("trp->track == track\n",0);
			return;
		}
		targetslot = 0;
		curSlot = 0;
	}
	if (targetslot >= 0) {
		if (trp->track == track&&curSlot < targetslot) {
			targetslot--;
		}

		if (!g->vst->isSynth && targetslot < 1) {
			my_printf("targetslot < 1 %d\n",targetslot);
			if (!track->audio || track->audio->effects.empty()) {
				targetslot = 1;
			} else {
				return;
			}

		}
		if (trp->track != track) {
			my_printf("movePlugin %d %d\n",curSlot, targetslot);
			vsthost::getInstance()->movePlugin(track, trp, curSlot, targetslot);
		} else {
			my_printf("swapEffects %d %d\n",curSlot, targetslot);
			vsthost::getInstance()->swapEffects(trp, curSlot, targetslot);
		}
		showTrack(track);
	} else {

		my_printf("targetslot < 0 %d\n", targetslot);
	}
	return;
}
