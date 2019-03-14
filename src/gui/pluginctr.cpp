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
#include "pluginviewcontainers.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "contextmenus.h"
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
#include "guiplugin.h"


using glm::vec2;
using glm::ivec2;
void getSelectedEffects(plugin_selection& sel, std::vector<effectbase*>& out) {
	out.clear();
	std::vector<effectbase*> tmp;
	sel.pluginCtr->getEffects(tmp);
	int n = sel.firstSelection->effect->getSlot();
	int n2 = sel.lastSelection->effect->getSlot();
	for (auto* effect : tmp) {
		int slot = effect->getSlot();
		if (slot >= n && slot <= n2) {
			out.push_back(effect);
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
void guiplugin::handleDraggedMove(MouseEvent& evt) {
	if (isSelected()) {
		auto& sel = MainCtrl::get()->getPluginSel();
		setDraggedPluginsUI(sel.pluginCtr->dragged, sel);
		MainCtrl::get()->setDragged(&sel.pluginCtr->dragged);
		hasDragged = true;
	} else {
		hasDragged = false;
		MainCtrl::get()->objectDragMove(this, evt);
	}
}
void guiplugin::handleDraggedRelease(MouseEvent& evt) {
	MainCtrl::get()->objectDragRelease(this, evt);
	if (hasDragged) {
		return;
	}
	if (isSelected()) {
		static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
	}
}
void guiplugin::handleDraggedBegin(MouseEvent& evt) {
	hasDragged = false;
	if (!isSelected()) {
//		hasDragged = true;
		static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
	}
}
void guiplugin::dragMoveOn(guibase* target, ivec2 mousepos) {
	target->pluginDragMove(this, mousepos);
}
void guiplugin::dragReleaseOn(guibase* target, ivec2 mousepos) {
	target->pluginDragRelease(this, mousepos);
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

guibase* guiplugin::getDraggedControl() {
	return this;
}
bool guiplugin::isSelected() {
	auto& sel = MainCtrl::get()->getPluginSel();
	if (sel.pluginCtr == this->parent) {
		if (sel.firstSelection) {
			if (this->effect->getSlot() >= sel.firstSelection->effect->getSlot() &&
					this->effect->getSlot() <= sel.lastSelection->effect->getSlot()) {
				return true;
			}
		}
	}
	return this->parent && this->parent->isSelected();
}

void guiplugin::setControl(BaseCtrl* parentCtrl) {
	guictr_base::setControl(parentCtrl);
	buttonBypass.setControl(parentCtrl);
	buttonDelete.setControl(parentCtrl);
	meter.setControl(parentCtrl);
}
bool guictr_plugins::isSelected() {
	return parent && parent->isSelected();
}
void guiplugin::renderBase(NVGcontext* vg) {
	if (!setScissorTransformContainer(vg)) {
		return;
	}
	renderFrameBase(vg);
	int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
	if (isSelected()) {
		flags |= FLAG_SELECTED;
	}
	renderTitleBarHorizontal(vg, this->text, titlePosX, flags);
	renderFrameOutline(vg);
}

template <>
void guitooltip<guiplugin>::layout()  {
	size.x = 250;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	{
		table.rows.push_back({{tblstr{"track"}, tblint{(int64_t)ptr->effect->getTrack(), "%12x"}}});
		table.rows.push_back({{tblstr{"tracklink"}, tblint{(int64_t)ptr->effect->getTrackLink(), "%12x"}}});
		table.rows.push_back({{tblstr{"bIsSetup"}, tblint{ptr->effect->bIsSetup}}});
		table.rows.push_back({{tblstr{"bIsEnabled"}, tblint{ptr->effect->bIsEnabled}}});
		table.rows.push_back({{tblstr{"PARAM_ENABLE"}, tblfloat{ptr->effect->getParamValue(PARAM_ENABLE)}}});
	}
	adjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guiplugin::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guiplugin>(this);
	return tooltip;
}

bool guiplugin::focusEvent(MouseHitEvt& evt, bool focused) {
	return true;
}


class gui_plugin_paramlist_entry : public gui_list_entry {

	const float spacing = INSET_TITLE;
public:
	effectbase* const effect;
	automatable_param_t* const entry;
	guiknob knobTest;
	gui_plugin_paramlist_entry(effectbase* _effect, automatable_param_t* _entry)
		: gui_list_entry(),
		  effect(_effect),
		  entry(_entry),
		  knobTest(false)
	{
		icon = 0;
		knobTest.setAutomationRef(effect, entry->idx);
		knobTest.setAutomationHandlers();
		knobTest.fnFocus = [this](MouseHitEvt& evt, bool focused) {focusEvent(evt, focused);};
		knobTest.parent = this;
	}
    virtual bool focusEvent(MouseHitEvt& evt, bool focused) override {
    	if (focused)
    		MainCtrl::get()->showAutomation(effect->getTrack(), effect, entry->idx);
    	return true;
    }
	void handleRightClick(MouseEvent& evt) override {
		guictxtmenu_base* ctxt = new guictxtmenu_vstparam(effect, entry);
		MainCtrl::get()->openContextMenu(ctxt, evt.mousepos);
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
	virtual void setControl(BaseCtrl* parentCtrl) {
		guibase::setControl(parentCtrl);
		knobTest.setControl(parentCtrl);
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
			nvgFillColor(vg, theme->getColor(COL_BG_DRKER));
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
		nvgTranslate(vg, -pos.x, -pos.y);

		knobTest.render(vg);
	}
};

guiplugin::guiplugin(effectbase* _effect)
: guictr_base(GUI_PLUGIN),
  effect(_effect),
  buttonBypass(32),
  buttonDelete(32),
  meter(&_effect->meter) {
	padding = 0;
	margin = 0;
	text[0] = 0;
	buttonBypass.icon = ICON_BYPASS;
	buttonBypass.getState = [_effect]() {
		return _effect->getParamValue(PARAM_ENABLE)>0;
	};
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
				sel.firstSelection = effectChain.front()->getGui(); //UGLY
				sel.lastSelection = effectChain.back()->getGui(); //UGLY
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
		if (sel.pluginCtr == this) {
			if (sel.lastSelection && plugin->effect->getSlot() > sel.lastSelection->effect->getSlot()) {
				sel.lastSelection = plugin;
			}
			if (sel.firstSelection && plugin->effect->getSlot() < sel.firstSelection->effect->getSlot()) {
				sel.firstSelection = plugin;
			}
		}
	} else {
		sel.pluginCtr = this;
		sel.firstSelection = plugin;
		sel.lastSelection = plugin;
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
	assert(g->effects.size());
	audio_stage_t* srcStage = g->getTrackLink();
	int highlightSlot = slotFromCoord(mousepos);
//	if (abs((evt.dragStart - evt.mousepos).x) > getSizeContent().y / 4) {
		audio_stage_t* p = this->stage;
		while (p) {
			if (std::find(g->effects.begin(), g->effects.end(), p->owner) != g->effects.end()) {
				return;
			}
			p = p->parent;
		}
		if (srcStage == this->stage) {
			int first = g->effects.front()->getSlot();
			int last = g->effects.back()->getSlot();
			if (highlightSlot >= first && highlightSlot <= last) {
				return;
			}
		}
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


guivstplugin::guivstplugin(vstplugin * _vst)
: guiplugin(_vst),
  vst(_vst),
  params(48),
  buttonOpenEditor(32)
{
	buttonOpenEditor.icon = ICON_ADJUST;
	buttonOpenEditor.state = &_vst->bEditOpen;
	buttonOpenEditor.parent = this;
	buttonOpenEditor.setColor(0x40ABC0);
	params.parent = this;
	meter.parent = this;
	std::vector<gui_list_entry*> _newList;
	for (automatable_param_t& param : _vst->params) {
		if (param.internalIdx >= 0)
			_newList.push_back(new gui_plugin_paramlist_entry(_vst, &param));
	}
	params.setList(_newList);
}

guivstplugin::~guivstplugin() {
}
void guivstplugin::setControl(BaseCtrl* parentCtrl) {
	guiplugin::setControl(parentCtrl);
	buttonOpenEditor.setControl(parentCtrl);
	params.setControl(parentCtrl);
	for (auto* ctr : viewCtrs) {
		ctr->setControl(parentCtrl);
	}
}

void guivstplugin::determineSize() {
	if (this->viewCtr) {
		this->viewCtr->getFixedSize(&sizeCtrs.x, &sizeCtrs.y);
		if (size.y > sizeCtrs.y) {
			int width = (int)((sizeCtrs.x/(float)sizeCtrs.y)*size.y);
			sizeCtrs.x = width;
			sizeCtrs.y = size.y;
			size.y = std::max(sizeCtrs.y, size.y);
		}
		size.y = std::max(sizeCtrs.y, size.y);
		size.x += sizeCtrs.x;
	} else {
		sizeCtrs = {0, 0};
	}
}
void guivstplugin::render(NVGcontext* vg) {
	renderBase(vg);
	buttonBypass.render(vg);
	buttonOpenEditor.render(vg);
	buttonDelete.render(vg);
	for (auto* ctr : viewCtrs) {
		nvgSave(vg);
		ctr->render(vg);
		nvgRestore(vg);
	}
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
		for (auto* ctr : viewCtrs) {
			if (ctr->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (params.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (isShift(evt.kbmods)) {
			if (MainCtrl::get()->getPluginSel().pluginCtr != this->parent) {
				return true;
			}
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guivstplugin::buttonClicked(guibase* _button) {
	if (_button == &buttonBypass) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    	float f = vst->getParamValue(PARAM_ENABLE);
    	float f2 = f > 0.5 ? 0 : 1;
    	vst->setParamValue(PARAM_ENABLE, f2, 2);
		vst->postSetParameter(PARAM_ENABLE, f, f2, 2);
	}
	if (_button == &buttonOpenEditor) {
		if (vst->bEditOpen) {
			vst->close();
		} else {
			vst->show();
		}
	}
	if (_button == &buttonDelete) {
    	removePlugin(vst);
	}
}
void guivstplugin::layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) {
	layoutButtons();
	const int32_t hpt = theme->get(G_PLUGIN_TITLE_HEIGHT);
	buttonOpenEditor.setRadius(buttonBypass.radius);
	buttonOpenEditor.pos.y = inset1;
	buttonOpenEditor.pos.x = buttonBypass.right();
	titlePosX = buttonOpenEditor.right();
	int32_t insetCtrls = INSET_TITLE;
	int rowHeight = 64;
	while (contentS.y < rowHeight * 8 && rowHeight > 8) {
		rowHeight -= 4;
	}
	int paramsW = contentS.x - sizeCtrs.x;
	params.setRowHeight(rowHeight);
	params.pos = ivec2(insetCtrls, insetCtrls + hpt);
	params.size = ivec2(paramsW, contentS.y) - ivec2(insetCtrls*2);
	params.layout();
	if (viewCtrs.size()) {
		int left = params.right() + INSET_TITLE;
		for (auto* ctr : viewCtrs) {
			ctr->pos = ivec2(left, 0) + ivec2(insetCtrls, insetCtrls + hpt);
			ctr->size = ivec2(sizeCtrs.x, contentS.y) - ivec2(insetCtrls*2);
			ctr->determineSize();
			ctr->layout();
			left = ctr->right() + INSET_TITLE;
		}
	}
}


template <>
void guitooltip<guivstplugin>::layout()  {
	size.x = 250;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	{
		table.rows.push_back({{String("isSynth"), (int)ptr->vst->isSynth}});
		table.rows.push_back({{tblstr{"bIsEnabled"}, tblint{ptr->vst->bIsEnabled}}});
		table.rows.push_back({{tblstr{"PARAM_ENABLE"}, tblfloat{ptr->vst->getParamValue(PARAM_ENABLE)}}});
	}
	adjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guivstplugin::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guivstplugin>(this);
	return tooltip;
}

