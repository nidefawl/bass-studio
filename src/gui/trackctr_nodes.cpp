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

void guictr_nodes_editor::scrollTo(guibase* g) {
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
			DawInstance::get()->setSelectedTrack(node->trackOptional);
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
		switch (node->type) {
		case DAW::track_node_type_t::TRACK:
			return StringFormat("%s", StringAsCStr(node->trackOptional->name));
		case DAW::track_node_type_t::AUDIOSTAGE:
			if (node->stageId == TRACKID_DEFAULT_I32)
				return StringFormat("Output Stage %d", static_cast<int32_t>(node->stageId));
			return StringFormat("Input Stage %d", static_cast<int32_t>(node->stageId));
		case DAW::track_node_type_t::EFFECT:
			return StringFormat("%s", StringAsCStr(node->effectOptional->getName()));
		default:
			break;
		}
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

class guictr_nodes_editor::guictr_nodes_editor_impl
{
	friend class guictr_nodes_editor;
	int refreshQueued = 2;
public:
	guictr_nodes_editor_impl() {

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
	setCanMouseHit(true);
	setBackgroundRendered(true);
}
gui_graph::~gui_graph() {
	destroyGuis();
	delete impl;
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
ivec2 toControlsObjectSpace(ivec2& pos, guibase* gui);
void gui_graph::render(NVGcontext* vg) {
	if (isBackgroundRendered()) {
		renderBackground(vg);
	}
	if (!setScissorTransform(vg)) {
		return;
	}
	ivec2 posIn = ivec2(parentCtrl->m_mousePos);
	ivec2 mouseLocal = toControlsObjectSpace(posIn, this);
	nvgSave(vg);
	nvgTranslate(vg, offset.x, offset.y);
	nvgScale(vg, scale, scale);
	nvgBeginPath(vg);
	nvgCircleFast(vg, mouseLocal.x, mouseLocal.y, 5.0f);
	nvgFillColor(vg, nvgRGBAf(1, 0, 1, 1));
	nvgFill(vg);
	const int stepLineSegments = 32;
	const int stepStart = 1;
	const vec4 colEdgeSignal = {0.1f, 0.6f, 0.1f, 1.0f};
	for (gui_graph::guictr_graph_impl::edge_t& edge : impl->edgeList) {

		gui_graph_n* graphNode = edge.a;
		gui_graph_n* gTarget = edge.b;
		const DAW::processing_track_node_t* node = gTarget->getProcessingNode();
		auto portInputPos = graphNode->getPortInputPos();
		auto portOutputPos = gTarget->getPortOutputPos();
		const float fLineWidth = 4.0f;


		if (node->trackOptional && node->trackOptional->audio) {
			float maxRms = node->trackOptional->audio->meter.getMaxRMS();
			if (maxRms > dsp_util::GAIN_DBFLOOR) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, portInputPos.x, portInputPos.y);
				const vec2 dInOut = vec2(portOutputPos) - vec2(portInputPos);
				for (int i = stepStart; i < stepLineSegments-stepStart; i++) {
					float t = i/(float)(stepLineSegments-1);
					float f = t * t * (3.0 - 2.0 * t);
					vec2 pos = vec2(portInputPos) + vec2(dInOut.x*t, dInOut.y*f);
					nvgLineTo(vg, pos.x, pos.y);
				}
				nvgLineTo(vg, portOutputPos.x, portOutputPos.y);
				nvgStrokeColor(vg, vec4ToNvg(colEdgeSignal));
				nvgStrokeWidth(vg, fLineWidth+2.0f);
				nvgStroke(vg);
			}
		}


		nvgBeginPath(vg);
		nvgMoveTo(vg, portInputPos.x, portInputPos.y);
		vec2 dInOut = vec2(portOutputPos)-vec2(portInputPos);
		vec4 colEdge = {0.05f, 0.05f, 0.05f, 1.0f};

		for (int i = stepStart; i < stepLineSegments-stepStart; i++) {
			float t = i/(float)(stepLineSegments-1);
			float f = t * t * (3.0 - 2.0 * t);
			vec2 pos = vec2(portInputPos) + vec2(dInOut.x*t, dInOut.y*f);
			nvgLineTo(vg, pos.x, pos.y);
		}
		nvgLineTo(vg, portOutputPos.x, portOutputPos.y);
		nvgStrokeColor(vg, vec4ToNvg(colEdge));
		nvgStrokeWidth(vg, fLineWidth);
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
	nvgRestore(vg);
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
		String str;
		str = StringFormat("Stage #%d", static_cast<int32_t>(node->stageId));
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
		posY += fs * 1.2;
		str = StringFormat("inputs: %d", static_cast<int32_t>(node->children.size()));
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
		posY += fs * 1.2;
		str = StringFormat("outputs: %d", static_cast<int32_t>(node->parents.size()));
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
		posY += fs * 1.2;
		str = StringFormat("Latency");
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
		posY += fs * 1.2;
		str = StringFormat("Input: %d", node->inputLatency);
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
		posY+=fs*1.2;
		str = StringFormat("Internal: %d", node->internalLatency);
		nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
		posY+=fs*1.2;



		if (node->trackOptional && node->trackOptional->audio) {
			float maxRmsOut = node->trackOptional->audio->meter.getMaxRMS();
			float maxRmsIn = node->trackOptional->audio->meterInput.getMaxRMS();
			str = StringFormat("Input max rms: %f", maxRmsIn);
			if (maxRmsIn > dsp_util::GAIN_DBFLOOR) {
				setFont(vg, fs, G_GREEN, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			}
			nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
			posY+=fs*1.2;
			str = StringFormat("Output max rms: %f", maxRmsOut);
			if (maxRmsOut > dsp_util::GAIN_DBFLOOR) {
				setFont(vg, fs, G_GREEN, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			} else {
				setFont(vg, fs, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			}
			nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
			posY+=fs*1.2;
			setFont(vg, fs, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
//			int64_t timeProcessRaw = 0;
//			int64_t timeProcess = 0;
//			int64_t timeUpdateParameters = 0;
//			int64_t timeGetNotesInRange = 0;
//			int64_t timeMixInputs = 0;
//			int64_t timeSendNotes = 0;
//			int64_t statsProcSamples[STATS_PROCESSING_MAX_SAMPLES];
//			int32_t statsProcStep = 0;
//			int64_t statsWriteOffset=0;
//			int64_t statsBlocksProcessed=0;
			auto numBlocks = node->trackOptional->audio->procStats.numBlocksProcessed;
			str = StringFormat("Blocks processed: %d", numBlocks);
			nvgText(vg, INSET_TITLE, posY, StringAsCStr(str), NULL);
			posY+=fs*1.2;
		}
	}
};
void gui_graph::updateList(bool resetPositions) {
	std::shared_ptr<DAW::processing_graph_t> lastProcessingList;
	if (!isTrackGraph) {  /* project graph */
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		lastProcessingList = vsthost::getInstance()->lastProcessingList;
	} else {
		auto track = DawInstance::get()->getSelectedTrack();
		if (track && track->audio) {
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			lastProcessingList = vsthost::getInstance()->lastProcessingGraphs[track->audio->stageId];
		}
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
		float fontScale = 14*theme->getFloat(GuiConstant::CONST_NODES_SCALE);
		int32_t nodeWidth = 180*theme->getFloat(GuiConstant::CONST_NODES_SCALE);
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

			auto g = new guinodeinfo_text{node};
			if (node->trackOptional) {
				int32_t meterWidth = fontScale;
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meter);
				meter->size = {meterWidth, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};
				entry->add(meter);
				gui_trackmeter<16000,2>* guimeterInput = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meterInput);
				guimeterInput->size = {meterWidth, entry->getSizeContent().y-hpt};
				guimeterInput->pos = {0, hpt};
				entry->add(guimeterInput);


				g->size = {cs.x-meterWidth*2, cs.y-hpt};
				g->pos = {guimeterInput->size.x, hpt};
			}
			if (node->effectOptional) {
				int32_t meterWidth = fontScale;
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(&node->effectOptional->meter);
				meter->size = {meterWidth, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};
				entry->add(meter);
//				gui_trackmeter<16000,2>* guimeterInput = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meterInput);
//				guimeterInput->size = {meterWidth, entry->getSizeContent().y-hpt};
//				guimeterInput->pos = {0, hpt};
//				entry->add(guimeterInput);

				g->size = {cs.x-meterWidth, cs.y-hpt};
				g->pos = {0, hpt};
			}
			if (node->stage) {
				int32_t meterWidth = fontScale;
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(node->stageId == TRACKID_DEFAULT_I32 ? &node->stage->meter : &node->stage->meterInput);
				meter->size = {meterWidth, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};
				entry->add(meter);
//				gui_trackmeter<16000,2>* guimeterInput = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meterInput);
//				guimeterInput->size = {meterWidth, entry->getSizeContent().y-hpt};
//				guimeterInput->pos = {0, hpt};
//				entry->add(guimeterInput);

				g->size = {cs.x-meterWidth, cs.y-hpt};
				g->pos = {0, hpt};
			}
			entry->add(g);

			/* copy over positions from old layout */
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
guictr_nodes_editor::guictr_nodes_editor(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip)
	: guictr_base(),
	  impl(new guictr_nodes_editor_impl),
	project(_project),
	graph(),
	scrollbar(1, 0.0f, *this)
{
	setBackgroundRendered(true);
	setCanMouseHit(true);
	add(&scrollbar);
	add(&graph);
	graph.setBackgroundRendered(false);
	graph.padding = 0;
}
guictr_nodes_editor::~guictr_nodes_editor() {
	remove(&graph);
	remove(&scrollbar);
	delete impl;
}
void guictr_nodes_editor::render(NVGcontext* vg) {
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
void guictr_nodes_editor::refresh() {
	impl->refreshQueued = 1;
}
void guictr_nodes_editor::reset() {
	graph.reset();
}

bool guictr_nodes_editor::handleKeyInput(KeyEvent& event) {
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
void guictr_nodes_editor::onTick(AppCtrl* appctrl) {
	if (impl->refreshQueued) {
		graph.updateList(impl->refreshQueued==2);
		impl->refreshQueued = 0;
	}
}
bool guictr_nodes_editor::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
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
void gui_graph::handleDraggedBegin(MouseEvent& evt) {
	prevOffset = offset;
}
void gui_graph::handleDraggedMove(MouseEvent& evt) {
	offset = prevOffset + vec2(evt.mousepos-evt.dragStart);

}
void gui_graph::handleDraggedRelease(MouseEvent& evt) {
	prevOffset = offset;
}
bool gui_graph::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
	if (yoffset) {
		float newScale = scale;
		newScale = newScale * (1.0f+(yoffset)/10.0f);
		if (newScale < 1/128.0f)
			newScale = 1/128.0f;
		if (newScale > 128.0f)
			newScale = 128.0f;

		ivec2 mpos = evt.mousepos;
		ivec2 relpos = toControlsObjectSpace(mpos, parent) - getPosContent();

		vec2 mousePosCtrSpace = toContainerSpace2f(relpos);
		vec2 offsetDelta = vec2(mousePosCtrSpace)*(newScale-scale);
		scale = newScale;
		/* alternatively offsetDelta can be calculated this way */
//		ivec2 mousePosCtrSpaceAfter = toContainerSpace2f(relpos);
//		vec2 offsetDelta = (mousePosCtrSpaceAfter-mousePosCtrSpace)*newScale;
		offset -= offsetDelta;

	}
	return true;
}
void guictr_nodes_editor::scrollOffsetChanged(int dir, float offset) {
//	ivec2 cs = getSizeContent() - graph.size;
//	int32_t scrOffset = math::max(0.0f, offset*(cs[dir]));

}
void guictr_nodes_editor::layout() {

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

guictr_nodes_splitview::guictr_nodes_splitview(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip)
	: project(_project), projectView(_cursor, _project, _dragdropclip), trackView(_cursor, _project, _dragdropclip)
{
	trackView.graph.isTrackGraph = true;
	setCanMouseHit(true);
	add(&projectView);
	add(&trackView);
	 padding = 0;
	 margin = 0;
	 setBackgroundRendered(false);
}
guictr_nodes_splitview::~guictr_nodes_splitview() {
	removeGuis();
}

void guictr_nodes_splitview::onChildLayoutChanged(guibase* g) {
	layout();
}

void guictr_nodes_splitview::reset() {
	projectView.reset();
	trackView.reset();
}
void guictr_nodes_splitview::refresh() {
	projectView.refresh();
	trackView.refresh();
}
void guictr_nodes_splitview::buttonClicked(guibase* _button) {
//	if (parent) parent->buttonClicked(_button);
	if (_button->parent == &projectView.graph) {
		trackView.refresh();
	}
	if (_button->parent == &trackView.graph) {

	}
}

void guictr_nodes_splitview::layout() {

	int scrollW = gui_scrollbar::defaultW;
	ivec2 cs = getSizeContent();
	projectView.pos = ivec2(0);
	trackView.pos = ivec2(0, cs.y/2);
	projectView.size = ivec2(cs.x, cs.y/2);
	trackView.size = ivec2(cs.x, cs.y/2);
//	scrollbar.pos = ivec2(cs.x-scrollW, 0);
//	scrollbar.size = ivec2(scrollW, cs.y);
//	cs.x -= scrollW;
//	graph.pos = {0, 0};
//	graph.size = cs;
//	graph.determineSize(graph.size);
	//	double f = scrollbar.toPixels();
//		contentHeight = graph.size.y;
//		contentViewSize = cs.y;
//	scrollbar.scrollTo(f);
//	scrollOffsetChanged(1, scrollbar.scrollOffset);
	for (guibase* gui : guis) {
		gui->layout();
	}
}

ivec2 gui_graph::toScreenSpace(ivec2 in) const {
	in = ivec2(vec2(getPosContent() + in) * scale + offset);
	if (this->parent != NULL) {
		in = this->parent->toScreenSpace(in);
	}
	return in;
}

ivec2 gui_graph::toContainerSpace(ivec2 in) {
	return ivec2((vec2(in - getPosContent()) - offset) * (1.0f / scale));
}
vec2 gui_graph::toContainerSpace2f(vec2 in) {
	return vec2((in - vec2(getPosContent()) - offset) * (1.0f / scale));
}

ivec2 gui_graph::toParentSpace(ivec2 localCoord) {
	return ivec2(vec2(getPosContent() + localCoord) * scale + offset);
}
