#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include "str_util.h"
#include "logging.h"
#include "event.h"
#include "keyboard.h"
#include "renderresources.h"
#include "button.h"
#include "list.h"
#include "knob.h"
#include "automatable.h"
#include "gui.h"
#include "basectrl.h"
#include "pluginviewcontainers.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "pluginctr.h"
#include "pluginlist.h"
#include "edithistory.h"

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
#include "guitooltip.h"
#include "str_util.h"
#include "snapshot.h"
#include "table.h"

#include "leak_detect.h"

#include "guicontextmenu_daw.h"
#include "guiplugin.h"


using glm::vec2;
using glm::ivec2;
using Table::tbl;
using Table::tbl_row_t;
using Table::table_entry_t;
using Table::tblint;
using Table::tblfloat;
using Table::tblstr;
void getSelectedEffects(plugin_selection& sel, std::vector<effectbase*>& out) {
	out.clear();
	if (sel.hasSelection()) {
		std::vector<effectbase*> tmp;
		sel.pluginCtr->getEffects(tmp);
		int n = sel.firstSelection;
		int n2 = sel.lastSelection;
		for (auto* effect : tmp) {
			int slot = effect->getSlot();
			if (slot >= n && slot <= n2) {
				out.push_back(effect);
			}
		}
	}
}
void setDraggedPluginsUI(guictr_dragged_plugins& gui, plugin_selection& sel) {
	gui.trackImpl = sel.pluginCtr->stage;
	gui.effects.clear();
	getSelectedEffects(sel, gui.effects);
	std::vector<String> list;
	for (auto* effect : gui.effects) {
		list.push_back(effect->getAutomatableName());
	}
	gui.setStrings(list);
}

guibase* guictr_plugins::getDraggedControl() {
	if (isSelected()) {
		auto& sel = MainCtrl::get()->getPluginSel();
		setDraggedPluginsUI(sel.pluginCtr->dragged, sel);
		return &sel.pluginCtr->dragged;
	}
	return this;
}

bool guictr_plugins::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		if (MouseHitType::MOUSE_LEFT == evt.type)
			dragged.pos = parent ? parent->toScreenSpace(mpos) : mpos;
		//handle multi selection...
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			evt.requestFocus(this);
			return true;
		}
	}
	return false;
}


bool guictr_plugins::isSelected() {
	return parent && parent->isSelected();
}


void guictr_plugins::onAdded() {
	if (parent) {
		theme = parent->theme;
	}
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

void pastePluginClipboard(std::shared_ptr<plugin_clipboard_t>& clipboard, audio_stage_t* stage, int32_t pos) {
	for (const plugin_snapshot_t& pluginSnapshot : clipboard->plugins) {
		effectbase* effect = loadEffectModule(pluginSnapshot);
		if (effect) {
			loadEffectParamsFromSnapshot(pluginSnapshot, effect);
			stage->insertEffect(pos, effect);
//						host->insertNewPlugin(this, effect, pluginSnapshot.slot);
			effect->loadSnapshot(pluginSnapshot);
			loadEffectAutomationFromSnapshot(pluginSnapshot, effect);
			if (pluginSnapshot.enabled) {
				effect->resume();
			}
		} else {
			//TODO: handle
		}
	}
	stage->pluginsChanged();
	MainCtrl::getPluginCtr()->relayout();
}
std::shared_ptr<plugin_clipboard_t> copyPluginSelection(plugin_selection& sel) {
	std::vector<plugin_snapshot_t> pluginSnapshots;
	std::vector<effectbase*> selection;
	std::shared_ptr<plugin_clipboard_t> clipboard = std::make_shared<plugin_clipboard_t>();
	getSelectedEffects(sel, selection);
	pluginSnapshots.reserve(selection.size());
	for (effectbase* effect : selection) {
		plugin_snapshot_t ps;
		effect->makeSnapshot(ps, true);
		clipboard->plugins.push_back(std::move(ps));
		clipboard->range++;
	}
	return clipboard;
}
bool guictr_plugins::handleKeyInput(KeyEvent& kevt) {
	if (kevt.type != K_RELEASE) {
		plugin_selection& sel = MainCtrl::get()->getPluginSel();
		if (!sel.pluginCtr) {
			return false;
		}
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		bool modified = false;
		bool handledKeyinput = false;
		String desc = "???";
		std::vector<effectbase*> effectChain;
		sel.pluginCtr->getEffects(effectChain);
		std::vector<effectbase*> selection;
		getSelectedEffects(sel, selection);

		bool clipboard = false;
		if (kevt.type == K_PRESS) {
			if (isKC(KC_SELECTALL, kevt)) {
				sel.firstSelection = effectChain.front()->getSlot();
				sel.lastSelection = effectChain.back()->getSlot();
				handledKeyinput = true;
			}
			if (isKC(KC_DELETE, kevt) && selection.size()) {
				audio_stage_t* audioStage = selection[0]->getTrackLink();
				assert(audioStage);
				for (effectbase* eff : selection) {
					eff->close();
				}
				for (effectbase* eff : selection) {
					audioStage->removePlugin(eff, false);
				}
				audioStage->pluginsChanged();
//				auto* actionRemove = new action_remove_module("Remove plugin", module, audioStage->toRef(), module->getSlot());
//				MainCtrl::get()->pushHist(actionRemove);
				handledKeyinput = true;
			}
			else if (isKC(KC_CUT, kevt) && selection.size()) {
				std::shared_ptr<plugin_clipboard_t> clipboard = copyPluginSelection(sel);
				MainCtrl::get()->setPluginClipboard(clipboard);
				audio_stage_t* audioStage = selection[0]->getTrackLink();
				assert(audioStage);
				for (effectbase* eff : selection) {
					eff->close();
				}
				for (effectbase* eff : selection) {
					audioStage->removePlugin(eff, false);
				}
				audioStage->pluginsChanged();
				handledKeyinput = true;
			}
			else if (isKC(KC_COPY, kevt) && selection.size()) {
				std::shared_ptr<plugin_clipboard_t> clipboard = copyPluginSelection(sel);
				MainCtrl::get()->setPluginClipboard(clipboard);
				handledKeyinput = true;
			}
			else if (isKC(KC_DUPLICATE, kevt) && selection.size()) {
				std::shared_ptr<plugin_clipboard_t> clipboard = copyPluginSelection(sel);
				pastePluginClipboard(clipboard, sel.pluginCtr->stage, selection.back()->getSlot()+1);
				handledKeyinput = true;
			}
			else if (isKC(KC_PASTE, kevt) && MainCtrl::get()->getPluginClipboard()) {
				std::shared_ptr<plugin_clipboard_t> clipboard = MainCtrl::get()->getPluginClipboard();
				pastePluginClipboard(clipboard, sel.pluginCtr->stage, selection.back()->getSlot()+1);
				handledKeyinput = true;
			}
		} else {
		}
		if (isArrowKey(kevt.keyCode)) {
			ivec2 dir;
			arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
			if (dir.y) {
				if (isShift(kevt.mods)) {

				} else {

				}
			} else if (dir.x) {
				if (isShift(kevt.mods)) {

				} else {

				}
			}
			handledKeyinput = true;
//			desc = "Move notes";
		}
//		if (modified) {
//			action_modify_track* track_action = new action_modify_track(desc, resizePreModifyState.copy());
//			MainCtrl::get()->pushHist(track_action);
//
//		}
//		if (handledKeyinput) {
//			updateVisibleTrackContents();
//		}
		return handledKeyinput;
	}
	return false;
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
void guictr_plugins::onSelected(MouseEvent& evt, guiplugin* plugin) {
	plugin_selection& sel = MainCtrl::get()->getPluginSel();
	if (isShift(evt.kbmods)) {
		if (sel.hasSelection() && sel.pluginCtr == this) {
			if (plugin->effect->getSlot() > sel.lastSelection) {
				sel.lastSelection = plugin->effect->getSlot();
			}
			if (plugin->effect->getSlot() < sel.firstSelection) {
				sel.firstSelection = plugin->effect->getSlot();
			}
		}
	} else {
		sel.pluginCtr = this;
		sel.firstSelection = plugin->effect->getSlot();
		sel.lastSelection = plugin->effect->getSlot();
	}
}
void guictr_plugins::onChildLayoutChanged(guibase* g) {
//	layout();
	if (this->parent != NULL) {
		this->parent->onChildLayoutChanged(this);
	}
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
void guictr_plugins::relayout() {
	showTrack(this->stage);
}
void guictr_plugins::getEffects(std::vector<effectbase*>& out) {
	out = this->stage->effects; // copy
}
void guictr_plugins::showTrack(audio_stage_t* audio) {
	removeGuis();
	this->track = audio ? audio->getTrack() : nullptr;
	this->stage = audio;
//	my_printf("%d %d\n", myNumber1, myNumber2);
//	my_printf("showTrack %s\n", (isDefaultPluginCtr ? "default" : "group"));
	if (audio && this->track) {
		audio->pluginCtr = this;
		if (!audio->pluginCtr->parent) {
			if (MainCtrl::getPluginCtr() != audio->pluginCtr)
				assert(0);
//			my_printf("plugin ctr with parent == null\n", 0);
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
	effectbase* instance = makeModuleInstance(entry.moduleType, entry.moduleId, -1);
	return instance;
}
class action_insert_effect : public action_base {
	effectbase* effect;
	audio_stage_ref_t ref;
	int32_t dstSlot;
	bool weOwn = false;
	protected:
	public:
		action_insert_effect(String s, effectbase* _effect, audio_stage_ref_t _ref, int32_t _dst)
			: action_base(), effect(_effect), ref(_ref), dstSlot(_dst) {
			desc = s;
		}
		~action_insert_effect() {
			if (weOwn) {
				vsthost::getInstance()->unloadPlugin(this->effect);
			}
		}
		void undo(MainCtrl* ctrl) override {
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			audio_stage_t* stage = vsthost::getInstance()->getAudioStage(ref);
			if (!stage) {
				setError("missing trackimpl");
				return;
			}
			effect->close();
			vsthost::getInstance()->removePlugin(effect);
			MainCtrl::getPluginCtr()->relayout();
			weOwn = true;
		}
		void redo(MainCtrl* ctrl) override {
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			audio_stage_t* stage = vsthost::getInstance()->getAudioStage(ref);
			if (!stage) {
				setError("missing trackimpl");
				return;
			}
			vsthost::getInstance()->insertNewPlugin(stage, effect, dstSlot);
			MainCtrl::getPluginCtr()->relayout();
			weOwn = false;
		}
};

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
		audio_stage_ref_t refdst = stage->toRef();
		auto* track_action = new action_insert_effect("Insert plugin", effect, refdst, dstSlot);
		MainCtrl::get()->pushHist(track_action);
//	if (res.result == 0 && res.plugin) {
//		res.plugin->resume();
//	}
	}
	showTrack(stage);
	if (this->parent) {
		this->parent->onChildLayoutChanged(this);
	}
}
void guictr_plugins::pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) {
	MainCtrl::get()->getDragDropTarget().reset();
	if (!this->stage) return;
	audio_stage_t* srcStage = g->getTrackLink();
	for (auto* ptr : g->effects) {
		assert(ptr->getTrackLink() == srcStage);
	}

	int highlightSlot = slotFromCoord(mousepos);
	if (this->stage == srcStage){
		int first = g->effects.front()->getSlot();
		int last = g->effects.back()->getSlot();
		if (highlightSlot >= first && highlightSlot <= last) {
			return;
		}
	} else {
		//prevent dragging onto if any of the effects is parent of this
		audio_stage_t* p = this->stage;
		while (p) {
			if (p->owner && std::find(g->effects.begin(), g->effects.end(), p->owner) != g->effects.end()) {
				return;
			}
			p = p->parent;
		}
//		auto p = this->stage;
//		while (p) {
//
//			p = p->parent;
//		}
//		if (this->stage)
//		for (auto* ptr : g->effects) {
//			ptr->is
//			auto ptr1 = ptr->getTrackLink();
//			if (ptr1 == this->stage) {
//				return;
//			}
//			if (isAudioStageChildOf(ptr1, this->stage)) {
//				return;
//			}
//		}
	}
//	if (abs((evt.dragStart - evt.mousepos).x) > getSizeContent().y / 4) {
		MainCtrl::get()->getDragDropTarget().set(this, highlightSlot);
//	}
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
class action_move_modules : public action_base {
	audio_stage_ref_t refdst;
	audio_stage_ref_t refsrc;
	int32_t dst;
	int32_t src;
	int32_t len;
	protected:
	public:
		action_move_modules(String s, audio_stage_ref_t _refdst, audio_stage_ref_t _refsrc, int32_t _dst, int32_t _src, int32_t _len)
			: action_base(), refdst(_refdst), refsrc(_refsrc), dst(_dst), src(_src), len(_len) {
			desc = s;
		}
		void undo(MainCtrl* ctrl) override {
			audio_stage_t* dstStage = vsthost::getInstance()->getAudioStage(refdst);
			audio_stage_t* srcStage = vsthost::getInstance()->getAudioStage(refsrc);
			if (!dstStage || !srcStage) {
				setError("missing trackimpl");
				return;
			}
			vsthost::getInstance()->movePlugins(srcStage, dstStage, dst, src, len);
			MainCtrl::getPluginCtr()->relayout();
		}
		void redo(MainCtrl* ctrl) override {
			audio_stage_t* dstStage = vsthost::getInstance()->getAudioStage(refdst);
			audio_stage_t* srcStage = vsthost::getInstance()->getAudioStage(refsrc);
			if (!dstStage || !srcStage) {
				setError("missing trackimpl");
				return;
			}
			vsthost::getInstance()->movePlugins(dstStage, srcStage, src, dst, len);
			MainCtrl::getPluginCtr()->relayout();
		}
};
class action_shift_modules : public action_base {
	audio_stage_ref_t ref;
	int32_t dst;
	int32_t src;
	int32_t len;
	protected:
	public:
	action_shift_modules(String s, audio_stage_ref_t _ref, int32_t _dst, int32_t _src, int32_t _len)
			: action_base(), ref(_ref), dst(_dst), src(_src), len(_len) {
			desc = s;
		}
		void undo(MainCtrl* ctrl) override {
			audio_stage_t* stage = vsthost::getInstance()->getAudioStage(ref);
			if (!stage) {
				setError("missing trackimpl");
				return;
			}
			vsthost::getInstance()->moveEffects(stage, dst, src, len);
			MainCtrl::getPluginCtr()->relayout();
		}
		void redo(MainCtrl* ctrl) override {
			audio_stage_t* stage = vsthost::getInstance()->getAudioStage(ref);
			if (!stage) {
				setError("missing trackimpl");
				return;
			}
			vsthost::getInstance()->moveEffects(stage, src, dst, len);
			MainCtrl::getPluginCtr()->relayout();
		}
};

class action_remove_module : public action_base {
	effectbase* effect;
	audio_stage_ref_t ref;
	int32_t dstSlot;
	bool weOwn = true;
	protected:
	public:
		action_remove_module(String s, effectbase* _effect, audio_stage_ref_t _ref, int32_t _dst)
			: action_base(), effect(_effect), ref(_ref), dstSlot(_dst) {
			desc = s;
		}
		~action_remove_module() {
			if (weOwn) {
				vsthost::getInstance()->unloadPlugin(this->effect);
			}
		}
		void undo(MainCtrl* ctrl) override {
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			audio_stage_t* stage = vsthost::getInstance()->getAudioStage(ref);
			if (!stage) {
				setError("missing trackimpl");
				return;
			}
			vsthost::getInstance()->insertNewPlugin(stage, effect, dstSlot);
			assert(effect->getSlot() == dstSlot);
			MainCtrl::getPluginCtr()->relayout();
			weOwn = false;
		}
		void redo(MainCtrl* ctrl) override {
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			audio_stage_t* stage = vsthost::getInstance()->getAudioStage(ref);
			if (!stage) {
				setError("missing trackimpl");
				return;
			}
			assert(effect->getSlot() == dstSlot);
			effect->close();
			vsthost::getInstance()->removePlugin(effect);
			MainCtrl::getPluginCtr()->relayout();
			weOwn = true;
		}
};

void removePlugin(effectbase* module) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	audio_stage_t* audioStage = module->getTrackLink();
	assert(audioStage);
	module->close();
	audioStage->removePlugin(module, true);
	auto* actionRemove = new action_remove_module("Remove plugin", module, audioStage->toRef(), module->getSlot());
	MainCtrl::get()->pushHist(actionRemove);
	audioStage->pluginsChanged();
}
void guictr_plugins::pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) {
	int32_t dstSlot = MainCtrl::get()->getDragDropTarget().idx;
	MainCtrl::get()->getDragDropTarget().reset();
	if (!this->stage) return;
	assert(g->effects.size());
	audio_stage_t* srcStage = g->getTrackLink();
	audio_stage_t* p = this->stage;
	while (p) {
		if (std::find(g->effects.begin(), g->effects.end(), p->owner) != g->effects.end()) {
			return;
		}
		p = p->parent;
	}
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	int first = g->effects.front()->getSlot();
	int last = g->effects.back()->getSlot();

	my_printf("move %d plugins from %s:%d to %s:%d\n",
			(int)g->effects.size(),
			StringAsCStr(srcStage->getTrack()->name), first,
			StringAsCStr(this->stage->getTrack()->name), dstSlot);
	int targetslot = slotFromCoord(mousepos);
	if (srcStage == this->stage) {
		int first = g->effects.front()->getSlot();
		int last = g->effects.back()->getSlot();
		if (targetslot >= first && targetslot <= last) {
			return;
		}
	}

	if (targetslot >= 0) {
		if (srcStage != this->stage) {
			vsthost::getInstance()->movePlugins(this->stage, srcStage, first, targetslot, last-first+1);

			audio_stage_ref_t refsrc = srcStage->toRef();
			audio_stage_ref_t refdst = stage->toRef();
			auto* track_action = new action_move_modules("Move plugin", refdst, refsrc, targetslot, first, last-first+1);
			MainCtrl::get()->pushHist(track_action);
		} else {
			int first = g->effects.front()->getSlot();
			int last = g->effects.back()->getSlot();
			if (targetslot > first) targetslot-=g->effects.size();
//			if (targetslot > curSlot) targetslot--;
			if (first == targetslot)
				return;
			vsthost::getInstance()->moveEffects(this->stage, first, targetslot, last-first+1);
			audio_stage_ref_t ref = this->stage->toRef();
			auto* track_action = new action_shift_modules("Move plugin", ref, targetslot, first, last-first+1);
//			MainCtrl::get()->pushHist(track_action);
		}
		if (this->parent) {
			this->parent->onChildLayoutChanged(this);
		}
		showTrack(stage);
	}
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
			vsthost::getInstance()->movePlugins(this->stage, trp, curSlot, targetslot, 1);

			audio_stage_ref_t refsrc = trp->toRef();
			audio_stage_ref_t refdst = stage->toRef();
			auto* track_action = new action_move_modules("Move plugin", refdst, refsrc, targetslot, curSlot, 1);
			MainCtrl::get()->pushHist(track_action);

		} else {
			if (targetslot > curSlot) targetslot--;
			vsthost::getInstance()->moveEffects(trp, curSlot, targetslot, 1);
			audio_stage_ref_t ref = trp->toRef();
			auto* track_action = new action_shift_modules("Move plugin", ref, targetslot, curSlot, 1);
			MainCtrl::get()->pushHist(track_action);
		}
		if (this->parent) {
			this->parent->onChildLayoutChanged(this);
		}
		showTrack(stage);
	} else {

		my_printf("targetslot < 0 %d\n", targetslot);
	}
	return;
}

GuiColor::constant_t COL_BTN_BG_DEFAULT_INACTIVE("COL_BTN_BG_DEFAULT_INACTIVE", 0xff40ABC0);
GuiColor::constant_t COL_BTN_BG_DEFAULT_ACTIVE("COL_BTN_BG_DEFAULT_ACTIVE", 0xff40ABC0);
GuiColor::constant_t COL_BTN_BG_BYPASS_ACTIVE("COL_BTN_BG_BYPASS_ACTIVE", 0xff40ABC0);
GuiColor::constant_t COL_BTN_BG_SHOW_ACTIVE("COL_BTN_BG_SHOW_ACTIVE", 0xff40ABC0);

namespace GuiColor {
constant_t COL_PLUGIN_VIEW_FRAME("COL_PLUGIN_VIEW_FRAME", 0x7fffffff);
}
void guictr_pluginview::render(NVGcontext* vg) {
	ivec2 cp = this->getPosContent();
	ivec2 cs = this->getSizeContent();
	if (MainCtrl::get()->isPluginViewVisible()) {
		drawAttachedBackground(vg, theme, cp, cs, margin);
	} else {
		drawBackground(vg, theme, cp, cs, margin, false);
	}
	ivec2 csp = ctr_plugins->getSizeContent();
	int32_t w = ctr_plugins->getTotalWidth();
	if (cs.x > 0 && cs.y > 0 && csp.x > 0 && csp.y > 0) {
		float scY = cs.y / (float) csp.y;
		float scContent = min(1.0f, csp.x / (float) w);
		float minScale = min((cs.x / (float) max(csp.x, w)), scY);
		nvgSave(vg);
		if (setScissorTransform(vg)) {
			nvgScale(vg, minScale, scY);
			for (guibase* gui : ctr_plugins->guis) {
				nvgSave(vg);
				gui->render(vg);
				nvgRestore(vg);
			}
		}
		nvgRestore(vg);
		nvgBeginPath(vg);
		nvgRect(vg, cp.x + ctr_plugins->scrolloffset * minScale, cp.y, cs.x * scContent, cs.y);
		nvgStrokeWidth(vg, 3);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLUGIN_VIEW_FRAME));
		nvgStroke(vg);

	}
}
void guictr_plugins::onTick(AppCtrl* ctrl) {
#define SCROLL_START_X 30
	if (isDefaultPluginCtr && ctrl->guiDragged != NULL && ctrl->guiDragged->parent == this) {
		if (ctrl->m_mousePos.x < SCROLL_START_X && scrolloffset > 0) {
			setScrolloffset(scrolloffset - (int) ((TIMER_MS / 50.0) * 40));
		}
		if (ctrl->m_mousePos.x > getSizeContent().x - SCROLL_START_X && scrolloffset < getTotalWidth() - getSizeContent().x) {
			setScrolloffset(scrolloffset + (int) ((TIMER_MS / 50.0) * 40));
		}
		ctrl->requestRedraw();
	}
	for (guibase* gui : guis) {
		gui->onTick(ctrl);
	}
}
void guictr_plugins::layout() {
	ivec2 sizeInset = getSizeContent();
	int32_t guiH = sizeInset.y - margin;
	int32_t titleHeight = ((guiH/8)>>1)<<1;
	theme->set(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, titleHeight);
	int32_t inset = margin / 2;
	ivec2 gPos(inset * 3, 0);
	for (guibase* gui : guis) {
		gui->pos = gPos;
		gui->size = ivec2(guiH);
		gui->determineSize();
		gui->pos.y = inset;
		gPos.x += gui->size.x + margin * 2;
		gui->layout();
	}
}
