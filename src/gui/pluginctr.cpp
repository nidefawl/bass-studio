#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include "str_util.h"
#include "logging.h"
#include "event.h"
#include "renderresources.h"
#include "button.h"
#include "list.h"
#include "knob.h"
#include "automatable.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "plugin.h"
#include "pluginctr.h"
#include "pluginlist.h"

#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugin/base_plugin.h"
#include "../host/plugin/internal_plugin.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "../host/plugindatabase.h"
#include "../threads/playbackthread.h"

#include "track.h"
#include "track_impl.h"
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

void guiplugin::renderBase(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, size.x, size.y, G_RND);
	NVGcolor c;
	if (MainCtrl::get()->isCtrOrChildFocused(this)) {
		c = g_guiColors[COL_BG_DRK_FOCUSED];
	} else {
		c = g_guiColors[COL_BG_BRT];
	}
	nvgFillColor(vg, GUI_COLOR(G_S2));
	nvgFill(vg);
	nvgBeginPath(vg);
	nvgRoundedRectVarying(vg, 0, 0, size.x, HEIGHT_PLUGIN_TITLE, G_RND, G_RND, 0, 0);
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (this->text[0]) {
		setFont(vg, (int)(HEIGHT_PLUGIN_TITLE*0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, titlePosX+INSET_TITLE, HEIGHT_PLUGIN_TITLE / 2, StringAsCStr(this->text), NULL);
	}
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, size.x, size.y, G_RND);
	nvgStrokeColor(vg, GUI_COLOR(G_S1));
	nvgStrokeWidth(vg, G_STROKE);
	nvgStroke(vg);
}

class guictxtmenu_vstparam : public guictxtmenu_base {
	vstplugin* const vst;
	vst_param* const entry;
public:
	guictxtmenu_vstparam(vstplugin* _vst, vst_param* _entry) : vst(_vst), entry(_entry)
	{
		this->size.x = 240;
		addContextEntries(this, vst->getTrack(), vst, entry->idx);
	}
	void clicked(int _id) {
		handleAutomatbleContextMenu(vst->getTrack(), vst, entry->idx, _id);
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
		knobTest.fnSetValue = [this] (float f) {
			automation_t* param = vst->getAutomation(entry->idx);
			if (param) {
				param->active = false;
			}
			return vst->setParamValue(entry->idx, f);
		};
		knobTest.fnFocus = [this](bool b) {focusEvent(b);};
		knobTest.parent = this;
	}
    virtual bool focusEvent(bool focused) override {
    	if (focused)
    		MainCtrl::get()->showAutomation(vst->getTrack(), vst, entry->idx);
    	return true;
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
		if (ctrl->isCtrOrChildFocused(this)) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_DRKER]);
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
		nvgTranslate(vg, -pos.x, -pos.y);
		auto at = vst->getRegisteredAutomation(entry->idx);
		if (at && at->src.isAutomated()) {
			knobTest.indColor = G_PURPLE;
		} else {
			knobTest.indColor = G_WHITE;
		}
		if (at && at->src.isActive()) {
			knobTest.valColor = G_PURPLE;
		} else {
			knobTest.valColor = G_BLUE;
		}

		knobTest.render(vg);
	}
};

guiplugin::guiplugin(effectbase* _effect)
: guictr_base(GUI_PLUGIN),
  effect(_effect),
  buttonBypass(12),
  buttonDelete(12),
  meter(&_effect->meter) {
	padding = 0;
	margin = 0;
	text[0] = 0;
	buttonBypass.icon = ICON_BYPASS;
	buttonBypass.state = &_effect->bIsEnabled;
	buttonBypass.parent = this;
	buttonBypass.setColor(0x80c040);
	buttonDelete.icon = ICON_CLOSE;
	static bool closeEnabled = true;
	buttonDelete.state = &closeEnabled;
	buttonDelete.parent = this;
	buttonDelete.setColor(0x404040);
}
void guictr_plugins::onAdded() {
//	guibase* g = this;
//	while (g != nullptr) {
////		if (g->guiType == GUI_PLUGIN_VIEW) {
////			guictr_pluginview* pv = static_cast<guictr_pluginview*>(g);
//////			pv->pluginContainers.push_back(this);
////		}
//		g = g->parent;
//	}
}
void guictr_plugins::addGui(effectbase* plugin) {
	guiplugin* base = plugin->makeGui();
	if (base) {
		add(base);
	}
}
void guictr_plugins::hideTrack(audio_stage_t* _track) {
	if (this->stage == _track) {
		this->stage->pluginCtr = nullptr;
		this->track = nullptr;
		this->stage = nullptr;
		removeGuis();
		layout();
	}
}
void guictr_plugins::onChildLayoutChanged(guibase* g) {
	layout();
}
void guictr_plugins::render(NVGcontext* vg) {
//	if (isDefaultPluginCtr) {
//		renderBackground(vg);
//	} else {
//		drawInsetBackground(vg, getPosContent(), getSizeContent());
//	}
	renderBackground(vg);
	if (!setScissorTransform(vg)) {
		return;
	}
	guibase * lastGui = NULL;
	int32_t slot = 0;
	nvgTranslate(vg, -scrolloffset, 0);
	dragdrop_target_indicator& target = MainCtrl::get()->getDragDropTarget();
	for (guibase* gui : guis) {
		if (target.ptr == this && target.idx == slot) {
			ivec2 posHL(gui->pos.x + (isDefaultPluginCtr?-4:4), 0);
			verticalLineAt(vg, posHL);
			nvgTranslate(vg, 8, 0);
		}
		nvgSave(vg);
		gui->render(vg);
		nvgRestore(vg);
		slot++;
		lastGui = gui;
	}
	nvgResetScissor(vg);
	if (target.ptr == this) {
		if (target.idx == slot) {

			ivec2 posHL(4, 0);
			if (lastGui) posHL.x += lastGui->right();
			verticalLineAt(vg, posHL);
		}
	}
	nvgResetTransform(vg);
}
void guictr_plugins::showTrack(audio_stage_t* audio) {
	removeGuis();
	this->track = audio ? audio->getTrack() : nullptr;
	this->stage = audio;
//	my_printf("%d %d\n", myNumber1, myNumber2);
	my_printf("showTrack %s\n", (isDefaultPluginCtr ? "default" : "group"));
	if (audio && this->track) {
		audio->pluginCtr = this;
		if (!audio->pluginCtr->parent) {
			my_printf("plugin ctr with parent == null\n", 0);
		}
		if (audio && !audio->effects.empty()) {
			for (effectbase* vst : audio->effects) {
				addGui(vst);
			}
		} else
			add(&placeholder);
		switch (track->type) {
		case TRACK_TYPE_MIDI:
			placeholder.message = "Drop Instruments here";
			break;
		default:
			placeholder.message = "Drop Effects here";
			break;
		}
	}

	layout();
	if (track && isDefaultPluginCtr) {
		setScrolloffset(this->track->scrolloffset);
	}
}

void guictr_plugins::pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) {
	MainCtrl::get()->getDragDropTarget().reset();
	if (!track) return;
	if (g->isSynth()) {
		if (track->type != TRACK_TYPE_MIDI) {
			return;
		}
		MainCtrl::get()->getDragDropTarget().set(this, 0);
		return;
	}
	MainCtrl::get()->getDragDropTarget().set(this, slotFromCoord(mousepos));
}
void guictr_plugins::determineSize() {
	int32_t maxX = 0;
	for (guibase* gui : guis) {
		maxX = std::max(gui->right(), maxX);
	}
	size.x = maxX;
}
int guictr_plugins::slotFromCoord(ivec2 _pos) {
	if (stage->effects.empty())
		return 0;
	int slot = 0;
	for (guibase* gui : guis) {
		if (_pos.x < gui->pos.x + gui->size.x / 2) {
			break;
		}
		slot++;
	}
	return slot;
}
effectbase* gui_vstpluginlist_entry::makeInstance() {
	vstpluginloadres res = vsthost::getInstance()->loadPlugin(entry.path);
	return res.result == 0 ? res.plugin : nullptr;
}
effectbase* gui_modulelist_entry::makeInstance() {
	effectbase* instance = makeModuleInstance(entry.uid, -1);
	return instance;
}
void guictr_plugins::pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) {
	int32_t dstSlot = MainCtrl::get()->getDragDropTarget().idx;
	MainCtrl::get()->getDragDropTarget().reset();
	if (!this->stage) return;
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	effectbase* effect = g->makeInstance();
	if (effect) {
		my_printf("Insert effect on %s, parent %s\n", StringAsCStr(getClassName()), parent? StringAsCStr(parent->getClassName()) : "<null>");
		vsthost::getInstance()->insertNewPlugin(stage, effect, dstSlot);
		effect->resume();
		//	if (res.result == 0 && res.plugin) {
		//		res.plugin->resume();
		//	}
	}
	showTrack(stage);
}
void guictr_plugins::pluginDragMove(guiplugin* g, ivec2 mousepos) {
	MainCtrl::get()->getDragDropTarget().reset();
	if (!this->stage) return;
	effectbase* effect = g->getModule();
	audio_stage_t* trp = effect->getTrackLink();
	if (!trp) {
		assert(0&&"TRP WAS NULL");
		return;
	}
	int highlightSlot = slotFromCoord(mousepos);
//	if (abs((evt.dragStart - evt.mousepos).x) > getSizeContent().y / 4) {
		int curSlot = trp == stage ? (effect->getSlot()) : -2;
		if (trp == this->stage && (curSlot == highlightSlot || curSlot + 1 == highlightSlot)) {
			return;
		}
		MainCtrl::get()->getDragDropTarget().set(this, highlightSlot);
//	}
}
void guictr_plugins::pluginDragRelease(guiplugin* g, ivec2 mousepos) {
	MainCtrl::get()->getDragDropTarget().reset();
	if (!this->stage) return;
	int targetslot = slotFromCoord(mousepos);
	effectbase* effect = g->getModule();
	audio_stage_t* trp = effect->getTrackLink();
	if (!trp) {
		assert(0&&"TRP WAS NULL");
		return;
	}
	int curSlot = effect->getSlot();
	if (trp == this->stage && (curSlot == targetslot || curSlot + 1 == targetslot)) {
		return;
	}

	if (targetslot >= 0) {
		if (trp != this->stage) {
			vsthost::getInstance()->movePlugin(this->stage, trp, curSlot, targetslot);
		} else {
			if (targetslot > curSlot) targetslot--;
			vsthost::getInstance()->moveEffect(trp, curSlot, targetslot);
		}
		showTrack(stage);
	} else {

		my_printf("targetslot < 0 %d\n", targetslot);
	}
	return;
}


guivstplugin::guivstplugin(vstplugin * _vst)
: guiplugin(_vst),
  vst(_vst),
  params(48),
  buttonOpenEditor(12) {
	buttonOpenEditor.icon = ICON_ADJUST;
	buttonOpenEditor.state = &_vst->bEditOpen;
	buttonOpenEditor.parent = this;
	buttonOpenEditor.setColor(0x40ABC0);
	params.parent = this;
	meter.parent = this;
	std::vector<gui_list_entry*> _newList;
	for (vst_param& param : _vst->params) {
		_newList.push_back(new gui_plugin_paramlist_entry(_vst, &param));
	}
	params.setList(_newList);
}

guivstplugin::~guivstplugin() {
}
void guivstplugin::render(NVGcontext* vg) {
	renderBase(vg);
	buttonBypass.render(vg);
	buttonOpenEditor.render(vg);
	buttonDelete.render(vg);

	meter.render(vg);
	params.renderBackground(vg);
	params.render(vg);
}
bool guivstplugin::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
		return false;
	}
	if (contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		if (buttonBypass.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (buttonOpenEditor.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (buttonDelete.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (params.mouseHitTest(localMouse, evt)) {
			return true;
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guivstplugin::buttonClicked(guibase* _button) {
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
