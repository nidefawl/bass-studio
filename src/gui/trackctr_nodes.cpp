#include <nanovg.h>
#include "trackctr.h"
#include "math/seq_math.h"
#include "gui.h"
#include "guicontainer.h"
#include "exceptions.h"
#include "theme.h"
#include "automation.h"
#include "track.h"
#include "trackcontent.h"
#include "renderresources.h"
#include "trackcontrols.h"
#include "track.h"
#include "track_impl.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "button.h"
#include "trackctr_nodes.h"
#include "guimeter.h"

void guictr_nodes::scrollTo(guibase* g) {
//	int32_t y = g->pos.y;
//	ivec2 cs = getSizeContent() - graph.size;
//	int32_t scrOffsetX = math::max(0.0f, scrollbar.scrollOffset*cs.x);
//	scrollbar.scrollVisible(y+scrOffsetX, g->size.y);
}
void gui_graph_entry::handleDraggedMove(MouseEvent& evt) {
	parentCtrl->objectDragMove(this, evt);
}

void gui_graph_entry::handleDraggedRelease(MouseEvent& evt) {
	parentCtrl->objectDragRelease(this, evt);
}

void gui_graph_entry::render(NVGcontext* vg) {

	if (!setScissorTransformContainer(vg)) {
		return;
	}
	renderFrameBase(vg);
	String text = getText();
	int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
	ivec2 sizeContent = getSizeContent();
	renderTitleBar(vg, sizeContent, text, GuiConstant::CONST_FIXED_TITLE_HEIGHT, 0, flags, true);
	renderFrameOutline(vg);
	for (guibase* gui : guis) {
		nvgSave(vg);
		gui->render(vg);
		nvgRestore(vg);
	}
	nvgSave(vg);
//	const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
//	if (hpt <= 0) {
//		return;
//	}
//	nvgTranslate(vg, 0, 0);
	//	if (icon > -1) {
	//		RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
	//		drawIcon(vg, size, &image);
	//	}
	int32_t inset = 4;
	int32_t i2 = inset * 2;
	const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
	int32_t h = TRACK_HEIGHT_STEP-i2;
	setFont(vg, G_FONT_SCALE(h), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	for (guibase* gui : guis) {

//			gui->render(vg);
		nvgText(vg, i2, gui->top() + G_FONT_MIDDLE_OFFSET(gui->size.y), StringAsCStr(gui->label), nullptr);
	}
	nvgRestore(vg);

//
//	BaseCtrl* ctrl = parentCtrl;
//	float spacing = INSET_TITLE;
//	float x = spacing;
//	if (icon > -1) {
//		x += rowHeight + spacing;
//	}
//	bool focused = ctrl->isCtrOrChildFocused(this);
//	auto color = theme->getColor(GuiColor::COL_BG_DRKER2);
//	if (focused || selected) {
//		color = theme->getColor(focused ? GuiColor::COL_BG_DRKER : GuiColor::COL_BG_DRK);
//	}
//	nvgBeginPath(vg);
//	nvgRect(vg, pos.x, pos.y, size.x, size.y);
//	nvgFillColor(vg, color);
//	nvgFill(vg);
//	nvgTranslate(vg, pos.x, pos.y);
//	if (icon > -1) {
//		RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
//		drawIcon(vg, size, &image);
//	}
//	setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
//	nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
//	nvgTranslate(vg, -pos.x, -pos.y);
}

void gui_graph_entry::handleDraggedBegin(MouseEvent& evt) {
	if (parent) parent->buttonClicked(this);
}
class gui_graph_n : public gui_graph_entry {
	DAW::processing_track_node_t* const node;
	vec2 portInputPos;
	vec2 portOutputPos;
public:
	gui_graph_n(DAW::processing_track_node_t* const _node) : gui_graph_entry(), node(_node) {
		padding = 0;
	}
	const DAW::processing_track_node_t* getProcessingNode() const {
		return node;
	}
	vec2 getPortOutputPos() const {
		return portOutputPos;
	}
	vec2 getPortInputPos() const {
		return portInputPos;
	}

	virtual ~gui_graph_n() {
		destroyGuis();
	}
	void layout() override {
		ivec2 pos = getPosContent();
		ivec2 size = getSizeContent();
		const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
		pos.y+=hpt/2;
		portInputPos = pos;
		portOutputPos = vec2(pos.x+size.x, pos.y);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	bool wasDragReleaseOnGuiCtrNodes = false;
	virtual void handleDraggedBegin(MouseEvent& evt) {
		gui_graph_entry::handleDraggedBegin(evt);
		if (node->trackOptional)
			MainCtrl::get()->setSelectedTrack(node->trackOptional);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		parentCtrl->objectDragRelease(this, evt);
		if (wasDragReleaseOnGuiCtrNodes) {
			auto screenPos = evt.mousepos + evt.dragOffset;
			ivec2 localPos = toControlsObjectSpace(screenPos, parent);
			pos = localPos;
			layout();
		}
		wasDragReleaseOnGuiCtrNodes = false;
	}
	virtual void dragMoveOn(guibase* target, ivec2 mousepos) {
		wasDragReleaseOnGuiCtrNodes = false;
		log_printf("dragMoveOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
	};
	virtual void dragReleaseOn(guibase* target, ivec2 mousepos) {
		log_printf("dragReleaseOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
		if (target == parent->parent) {
			wasDragReleaseOnGuiCtrNodes = true;
		}
	};
	virtual String getText() {
		track_t* const tr = node->trackOptional;
		if (tr)
			return tr->name;
		return StringFormat("Stage id %d", node->stageId);
	}

	void render(NVGcontext* vg) override {
		if (parentCtrl&&parentCtrl->guiDragged == this) {
			nvgGlobalAlpha(vg, 0.5f);
		}
		nvgSave(vg);
		gui_graph_entry::render(vg);
		const float nodePortRadius = 6.0f;
		nvgRestore(vg);
		nvgBeginPath(vg);
		nvgCircle(vg, portInputPos.x, portInputPos.y, nodePortRadius);
		nvgCircle(vg, portOutputPos.x, portOutputPos.y, nodePortRadius);
		nvgFillColor(vg, theme->getFrameColorBase());
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgCircle(vg, portInputPos.x, portInputPos.y, nodePortRadius);
		nvgCircle(vg, portOutputPos.x, portOutputPos.y, nodePortRadius);
		nvgStrokeColor(vg, theme->getFrameColorOutline());
		nvgStrokeWidth(vg, 2.0);
		nvgStroke(vg);
		if (parentCtrl&&parentCtrl->guiDragged == this) {
			nvgGlobalAlpha(vg, 1.0f);
		}
	}
	void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override {
		mousepos += dragOffset;
		mousepos -= pos;
		nvgTranslate(vg, mousepos.x, mousepos.y);
		gui_graph_entry::render(vg);
	}
};

class guictr_nodes::guictr_nodes_impl
{
	friend class guictr_nodes;
	int refreshQueued = true;
public:
	guictr_nodes_impl() {

	}
};
class gui_graph::guictr_graph_impl
{
public:
	struct edge_t {
		gui_graph_n* a;
		gui_graph_n* b;
	};
	std::shared_ptr<DAW::processing_graph_t> procList;
	std::vector<gui_graph_entry*> listGuis;
	std::vector<gui_graph_n*> listNodes;
	int updateTick = 2220;
	std::vector<edge_t> edgeList;
	guictr_graph_impl() {

	}
};
class gui_path : public guibase {
	struct segment {
		vec2 begin;
		vec2 end;
	};
	std::vector<segment> segments;
public:
	gui_path() {

	}
	void render(NVGcontext* vg) {
		nvgSave(vg);
		nvgRestore(vg);
	}
	void setFrom() {

	}
};
gui_graph::gui_graph() : guictr_base(), impl(new gui_graph::guictr_graph_impl) {
	setBackgroundRendered(true);
}
void gui_graph::refresh() {
	reset();
	updateList(false);
}
void gui_graph::reset() {
	impl->edgeList.clear();
	impl->listGuis.clear();
	impl->listNodes.clear();
	destroyGuis();
	impl->procList = nullptr;
	impl->updateTick = 2220;
}
void gui_graph::render(NVGcontext* vg) {
	if (isBackgroundRendered()) {
		renderBackground(vg);
	}
	if (!setScissorTransform(vg)) {
		return;
	}
	for (gui_graph::guictr_graph_impl::edge_t& edge : impl->edgeList) {

		gui_graph_n* graphNode = edge.a;
		gui_graph_n* gTarget = edge.b;
		auto portInputPos = graphNode->getPortInputPos();
		auto portOutputPos = gTarget->getPortOutputPos();
		nvgBeginPath(vg);
		nvgMoveTo(vg, portInputPos.x, portInputPos.y);
		vec2 dInOut = vec2(portOutputPos)-vec2(portInputPos);
		int steps = 32;
		bool b = false;
		int d = 1;
		for (int i = d; i < steps-d; i++) {
			float t = i/(float)(steps-1);
			float f = t * t * (3.0 - 2.0 * t);
			vec2 pos = vec2(portInputPos) + vec2(dInOut.x*t, dInOut.y*f);
			nvgLineTo(vg, pos.x, pos.y);
		}
		nvgLineTo(vg, portOutputPos.x, portOutputPos.y);
		nvgStrokeColor(vg, theme->getFrameColorOutline());
		nvgStrokeWidth(vg, 4.0);
		nvgStroke(vg);
	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
	for (gui_graph::guictr_graph_impl::edge_t& edge : impl->edgeList) {

		gui_graph_n* graphNode = edge.a;
		gui_graph_n* gTarget = edge.b;
		auto portInputPos = graphNode->getPortInputPos();
		auto portOutputPos = gTarget->getPortOutputPos();
		auto edgeLabelPos = ivec2(portInputPos+vec2(portOutputPos-portInputPos)*0.5f);
		auto procNode = gTarget->getProcessingNode();
		setFont(vg, 14, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, edgeLabelPos.x, edgeLabelPos.y, StringAsCStr(StringFormat("%d samples", procNode->internalLatency+procNode->inputLatency)), NULL);


	}
}
bool gui_graph::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			evt.requestFocus(this);
			return true;
		}
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (!gui->isVisible())
				continue;
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (canMouseHit()) {
			evt.requestFocus(this);
			return true;
		}
	}
	return false;
}
class guinodeinfo_text : public guibase {
	const DAW::processing_track_node_t* const node;
public:
	guinodeinfo_text(const DAW::processing_track_node_t* const _node) : guibase(), node(_node) {

	}
	void render(NVGcontext* vg) override {
		if (!setScissorTransform(vg)) {
			return;
		}
		const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT)/2;
		int fs = (int)(hpt*0.8);
		int posY = fs*1.2;
		setFont(vg, fs, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
		String latency;
		latency = StringFormat("Stage #%d", static_cast<int32_t>(node->stageId));
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(latency), NULL);
		posY += fs * 1.2;
		latency = StringFormat("inputs: %d", static_cast<int32_t>(node->children.size()));
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(latency), NULL);
		posY += fs * 1.2;
		latency = StringFormat("outputs: %d", static_cast<int32_t>(node->parents.size()));
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(latency), NULL);
		posY += fs * 1.2;
		latency = StringFormat("Latency");
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(latency), NULL);
		posY += fs * 1.2;
		latency = StringFormat("Input: %d", node->inputLatency);
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(latency), NULL);
		posY+=fs*1.2;
		latency = StringFormat("Internal: %d", node->internalLatency);
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(latency), NULL);
	}
};
void gui_graph::updateList(bool resetPositions) {
	std::shared_ptr<DAW::processing_graph_t> lastProcessingList;
	{
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		lastProcessingList = vsthost::getInstance()->lastProcessingList;
	}
	ivec2 cs = getSizeContent();
	cs.x = math::max(400, cs.x);
	cs.y = math::max(400, cs.y);
	std::vector<gui_graph_n*> listNodes;
	std::vector<gui_graph_entry*> listEntries;
	if (resetPositions) {
		impl->listNodes.clear();
	}
	if (lastProcessingList) {
		const DAW::processing_graph_t& procGraph = *lastProcessingList.get();
		float fontScale = 14;
		int32_t nodeWidth = 180;
		int32_t nodeHeight = fontScale*2;
		ivec2 nodeSize = ivec2(nodeWidth, nodeWidth);
		int32_t posGridStepX = nodeWidth+fontScale*4;
		ivec2 posGrid = ivec2(fontScale, fontScale);
		for (DAW::processing_track_node_t* node : procGraph.nodesFlatOrdered) {
			gui_graph_n* entry = new gui_graph_n(node);
			entry->rowHeight = fontScale;
			entry->size = nodeSize;
			entry->pos = posGrid;
			const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);

			if (node->trackOptional) {
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meter);
				meter->size = {fontScale, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};

				entry->add(meter);
				auto g = new guinodeinfo_text{node};
				g->size = {cs.x-meter->size.x, cs.y-hpt};
				g->pos = {0, hpt};
				entry->add(g);
			}
			auto it = std::find_if(impl->listNodes.cbegin(), impl->listNodes.cend(), [stageId = node->stageId](gui_graph_n* gn) {
				return gn->getProcessingNode()->stageId == stageId;
			});
			if (it != impl->listNodes.cend()) {
				gui_graph_n* gTarget = *it;
				entry->pos = gTarget->pos;
				entry->size = gTarget->size;
			} else {

				posGrid.x += posGridStepX;
			}
			listEntries.push_back(entry);
			listNodes.push_back(entry);
			if (posGrid.x + posGridStepX > cs.x) {
				posGrid = ivec2(fontScale, posGrid.y+posGridStepX);
			}
		}
	}
	std::shared_ptr<DAW::processing_graph_t> curProcList = impl->procList;
	setList(listEntries);
	impl->listNodes = listNodes;
	impl->procList = lastProcessingList;
	impl->edgeList.clear();
	for (gui_graph_n* graphNode : impl->listNodes) {
		auto procNode = graphNode->getProcessingNode();
		for (DAW::track_node_t* procNodeChild : procNode->children) {
			auto it = std::find_if(impl->listNodes.cbegin(), impl->listNodes.cend(), [stageId = procNodeChild->stageId](gui_graph_n* gn) {
				return gn->getProcessingNode()->stageId == stageId;
			});
			if (it != impl->listNodes.cend()) {
				gui_graph_n* gTarget = *it;
				impl->edgeList.push_back(gui_graph::guictr_graph_impl::edge_t{graphNode, gTarget});
			}
		}
	}
	layout();
}

void gui_graph::setList(std::vector<gui_graph_entry*> _newList) {
	for (gui_graph_entry* g : impl->listGuis) {
		remove(g);
		delete g;
	}
	impl->listGuis = _newList;
	for (gui_graph_entry* g : impl->listGuis) {
		add(g);
	}
	layout();
}
void gui_graph::onTick(AppCtrl* appctrl) {
}
guictr_nodes::guictr_nodes(Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip)
	: guictr_base(),
	  impl(new guictr_nodes_impl),
	project(_project),
	graph(),
	scrollbar(1, 0.0f, *this)
{
	setBackgroundRendered(true);
	setCanMouseHit(true);
	add(&scrollbar);
	add(&graph);
}
void guictr_nodes::render(NVGcontext* vg) {
	if (isBackgroundRendered()){
		renderBackground(vg);
	}
	ivec2 cs = getSizeContent();
	ivec2 cp = getPosContent();
	if (cs.y <= 0 || cs.x <= 0) {
		return;
	}

	nvgIntersectScissor(vg, cp.x, cp.y, cs.x, cs.y);
	nvgTranslate(vg, cp.x, cp.y);





	nvgSave(vg);
	scrollbar.render(vg);
	nvgRestore(vg);

	nvgSave(vg);
	graph.render(vg);
	nvgRestore(vg);

}
void guictr_nodes::refresh() {
	impl->refreshQueued = 1;
}
void guictr_nodes::reset() {
	graph.reset();
}

bool guictr_nodes::handleKeyInput(KeyEvent& event) {
	if (event.type != KeyEventType::K_RELEASE) {
		if (event.type == KeyEventType::K_PRESS) {
			KeyCombo kc = KC_REFRESH;
			kc.keyMod = KB_MOD_CTRL;
			if (isKC(kc, event)) {
				impl->refreshQueued = 2;
				return true;
			}
			if (isKC(KC_REFRESH, event)) {
				impl->refreshQueued = 1;
				return true;
			}
		}
	}
	return false;
}
void guictr_nodes::onTick(AppCtrl* appctrl) {
	if (impl->refreshQueued) {
		graph.updateList(impl->refreshQueued==2);
		impl->refreshQueued = 0;
	}
}
bool guictr_nodes::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			evt.requestFocus(this);
			return true;
		}
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (!gui->isVisible())
				continue;
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (canMouseHit()) {
			evt.requestFocus(this);
			return true;
		}
	}
	return false;
}
void guictr_nodes::scrollOffsetChanged(int dir, float offset) {
	ivec2 cs = getSizeContent() - graph.size;
	int32_t scrOffset = math::max(0.0f, offset*(cs[dir]));

}
void guictr_nodes::layout() {

	int scrollW = gui_scrollbar::defaultW;
	ivec2 cs = getSizeContent();
	scrollbar.pos = ivec2(cs.x-scrollW, 0);
	scrollbar.size = ivec2(scrollW, cs.y);
	cs.x -= scrollW;
	graph.pos = {0, 0};
	graph.size = cs;
	graph.determineSize(graph.size);
	//	double f = scrollbar.toPixels();
//		contentHeight = graph.size.y;
//		contentViewSize = cs.y;
//	scrollbar.scrollTo(f);
//	scrollOffsetChanged(1, scrollbar.scrollOffset);
	for (guibase* gui : guis) {
		gui->layout();
	}
}
