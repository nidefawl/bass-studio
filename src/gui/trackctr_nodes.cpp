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
#include "host/plugin/base_plugin.h"
#include "host/plugin/internal_plugin.h"

const float fLineWidth = 4.0f;
const int stepLineSegments = 32;
const int stepStart = 1;
const vec4 colEdgeSignal = {0.1f, 0.6f, 0.1f, 1.0f};
const float nodePortRadius = 6.0f;

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

bool gui_graph_entry::setScissorTransformContainer(NVGcontext* vg) {
	ivec2 posInset = getPosContent();
	ivec2 sizeInset = getSizeContent();
	if (sizeInset.y <= 0 || sizeInset.x <= 0) {
		return false;
	}
	int32_t scissorExpand = 16;
//	nvgIntersectScissor(vg,
//		math::max<int32_t>(0, pos.x-scissorExpand),
//		math::max<int32_t>(0, pos.y-scissorExpand),
//		size.x+scissorExpand*2,
//		size.y+scissorExpand*2
//	);
	nvgTranslate(vg, posInset.x, posInset.y);
	return true;
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
namespace DAW {
bool getChannelRef(DAW::processing_track_node_t* nodeInput, DAW::channel_ref_t& ref) {
	switch (nodeInput->type) {
	case track_node_type_t::TRACK:
		dbgassert(nodeInput->trackOptional);
		if (nodeInput->trackOptional) {
			dbgassert(nodeInput->trackOptional->audio);
			ref = DAW::ChannelStage(nodeInput->trackOptional->audio, stagebuffer_point::OUTPUT_POST);
			return true;
		}
		break;
	case track_node_type_t::AUDIOSTAGE:
		dbgassert(nodeInput->stage);
		if (nodeInput->stage) {
			ref = DAW::ChannelStage(nodeInput->stage, stagebuffer_point::OUTPUT_POST);
			return true;
		}
		break;
	case track_node_type_t::EFFECT:
		dbgassert(nodeInput->effectOptional);
		if (nodeInput->effectOptional) {
			ref = DAW::ChannelAudioEffect(nodeInput->effectOptional, stagebuffer_point::OUTPUT_POST);
			return true;
		}
		break;
	}
	dbgassert(0);
	return false;
}
bool channelRefEquals(const DAW::channel_ref_t& existingRef, const DAW::channel_ref_t& ref) {
    if (existingRef.type == channel_input_type::INPUT_DEFAULT)
        return true;
	if (existingRef.type == ref.type) {
		switch (ref.type) {
		case channel_input_type::INPUT_DEFAULT:
			return true;
		case channel_input_type::INPUT_EMPTY:
			return true;
		case channel_input_type::INPUT_EXTERNAL_AUDIO:
			return ref.externalInputType == existingRef.externalInputType && ref.inputChannelOffset == existingRef.inputChannelOffset;
		case channel_input_type::INPUT_AUDIOSTAGE:
			return ref.stage.buffer == existingRef.stage.buffer && ref.stage.stageRef.stageId == existingRef.stage.stageRef.stageId;
		case channel_input_type::INPUT_AUDIOSTAGE_EFFECT:
			return ref.projectGlobalId == existingRef.projectGlobalId;
		}
	}
	return false;
}
bool removeDuplicateRoutings(std::vector<DAW::channel_ref_t>& list, const DAW::channel_ref_t& ref) {
	auto it = std::remove_if(list.begin(), list.end(), [ref](DAW::channel_ref_t& existingRef){
		return channelRefEquals(existingRef, ref);
	});
	bool removed = it != list.end();
	list.erase(it, list.end());
	return removed;
}
bool hasDuplicateRoutings(std::vector<DAW::channel_ref_t>& list, const DAW::channel_ref_t& ref) {
	bool b = std::any_of(list.begin(), list.end(), [ref](DAW::channel_ref_t& existingRef){
		return channelRefEquals(ref, existingRef);
	});
	return b;
}
bool disconnectNodes(DAW::processing_track_node_t* nodeDest, DAW::processing_track_node_t* nodeSrc) {
    DAW::channel_ref_t ref;
    if (getChannelRef(nodeSrc, ref)) {
        switch (nodeDest->type) {
		case track_node_type_t::TRACK:
            dbgassert(nodeDest->trackOptional);
            if (nodeDest->trackOptional) {
                nodeDest->trackOptional->audio->inputChannel = DAW::ChannelNone();
                return true;
			}
			break;
		case track_node_type_t::AUDIOSTAGE:
            dbgassert(nodeDest->stage);
            if (nodeDest->stage) {
                bool b = hasDuplicateRoutings(nodeDest->stage->postEffectRouting, ref);
                removeDuplicateRoutings(nodeDest->stage->postEffectRouting, ref);
                return true;
			}
			break;
		case track_node_type_t::EFFECT:
            dbgassert(nodeDest->effectOptional);
            if (nodeDest->effectOptional) {
                bool b = hasDuplicateRoutings(nodeDest->effectOptional->inputChannels, ref);
                removeDuplicateRoutings(nodeDest->effectOptional->inputChannels, ref);
                return true;
			}

			break;
		}
	}
	dbgassert(0);
	return false;
}
bool connectNodes(DAW::processing_track_node_t* nodeInput, DAW::processing_track_node_t* nodeOutput) {
	DAW::channel_ref_t ref;
    if (getChannelRef(nodeOutput, ref)) {
        switch (nodeInput->type) {
		case track_node_type_t::TRACK:
            dbgassert(nodeInput->trackOptional);
            if (nodeInput->trackOptional) {
                nodeInput->trackOptional->audio->inputChannel = ref;
                return true;
			}
			break;
		case track_node_type_t::AUDIOSTAGE:
            dbgassert(nodeInput->stage);
            if (nodeInput->stage) {
                bool b = hasDuplicateRoutings(nodeInput->stage->postEffectRouting, ref);
                removeDuplicateRoutings(nodeInput->stage->postEffectRouting, ref);
                nodeInput->stage->postEffectRouting.push_back(ref);
                nodeInput->stage->routingState = audiostagerouting_state_t::CUSTOM;
                return true;
			}
			break;
		case track_node_type_t::EFFECT:
            dbgassert(nodeInput->effectOptional);
            if (nodeInput->effectOptional) {
                bool b = hasDuplicateRoutings(nodeInput->effectOptional->inputChannels, ref);
                removeDuplicateRoutings(nodeInput->effectOptional->inputChannels, ref);
                nodeInput->effectOptional->inputChannels.push_back(ref);
                nodeInput->effectOptional->getTrackLink()->routingState = audiostagerouting_state_t::CUSTOM;
                return true;
			}

			break;
		}
	}
	dbgassert(0);
	return false;
}
}
class gui_graph_n;
class gui_graph_port : public guibase {
	gui_graph_n* parentGraphNode;
	stagebuffer_point stageBufferPoint;
public:
	gui_graph_port(gui_graph_n* _parentGraphNode, stagebuffer_point _stageBufferPoint)
	  : guibase(),
		parentGraphNode(_parentGraphNode),
		stageBufferPoint(_stageBufferPoint)
	{
		setCanMouseHit(true);
	}
	vec2 getCenterPos2f() const {
		return vec2(pos) + vec2(size) * 0.5f;
	}
	bool wasDragReleaseOnGuiCtrNodes = false;
	void handleDraggedBegin(MouseEvent& evt) override {
	}
	void handleDraggedMove(MouseEvent& evt) override {
		parentCtrl->objectDragMove(this, evt);
	}

	void handleDraggedRelease(MouseEvent& evt) override {
		parentCtrl->objectDragRelease(this, evt);
//		if (wasDragReleaseOnGuiCtrNodes) {
//			auto screenPos = evt.mousepos + evt.dragOffset;
//			ivec2 localPos = toControlsObjectSpace(screenPos, parent);
//			pos = localPos;
//			project_t* project = project_controller_t::get()->getProject();
//			std::map<int32_t, graph_node_layout_t>& graphLayouts = project->graphLayouts;
//			if (graphLayouts.count(id)) {
//				graphLayouts[id].pos = pos;
//			}
//			layout();
//		}
//		wasDragReleaseOnGuiCtrNodes = false;
	}
	virtual String getText() {
		switch (stageBufferPoint) {
			case stagebuffer_point::INPUT:
				return "INPUT";
			case stagebuffer_point::OUTPUT:
				return "OUTPUT";
			case stagebuffer_point::OUTPUT_POST:
				return "OUTPUT_POST";
		}
		dbgassert(0);
		return "INVALID";
	}

	void render(NVGcontext* vg) override {
		auto centerPos = getCenterPos2f();
		nvgBeginPath(vg);
		nvgCircle(vg, centerPos.x, centerPos.y, nodePortRadius);
		nvgFillColor(vg, theme->getFrameColorBase());
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgCircle(vg, centerPos.x, centerPos.y, nodePortRadius);
		nvgStrokeColor(vg, theme->getFrameColorOutline());
		nvgStrokeWidth(vg, 2.0);
		nvgStroke(vg);
	}
	void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override {
		if (parent->parent) {
			dbgassert(parent->parent->parent);
			// render without editor scaling
			// convert to container space of parent of graph (graph applies scale + translate)
			ivec2 posSS = parent->toParentSpace(getCenterPos2f());
            posSS = parent->parent->toParentSpace(posSS);
			ivec2 mouseposSS = toControlsObjectSpace(mousepos, parent->parent->parent);
            ivec2 nodeEditorPosSS = parent->parent->parent->toScreenSpace(ivec2(0));
			nvgSave(vg);
			nvgTranslate(vg, nodeEditorPosSS.x, nodeEditorPosSS.y);
			nvgBeginPath(vg);
			nvgMoveTo(vg, posSS.x, posSS.y);
			const vec2 dInOut = vec2(mouseposSS) - vec2(posSS);
			for (int i = stepStart; i < stepLineSegments-stepStart; i++) {
				float t = i/(float)(stepLineSegments-1);
				float f = t * t * (3.0 - 2.0 * t);
				vec2 pos = vec2(posSS) + vec2(dInOut.x*t, dInOut.y*f);
				nvgLineTo(vg, pos.x, pos.y);
			}
			nvgLineTo(vg, mouseposSS.x, mouseposSS.y);
			nvgStrokeColor(vg, vec4ToNvg(colEdgeSignal));
			nvgStrokeWidth(vg, fLineWidth+2.0f);
			nvgStroke(vg);
			nvgRestore(vg);
		}
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
		if (canMouseHit() && contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos);
	void dragReleaseOn(guibase* target, ivec2 mousepos);
};
class gui_graph_n : public gui_graph_entry {
	friend class gui_graph;
	DAW::processing_track_node_t* const node;
	std::vector<gui_graph_port*> guiPorts;
	std::shared_ptr<PluginViewContainers> viewCtr;
	/* holds guictrs of internal vstplugins with custom gui (non-steinberg api) */
	std::vector<guictr_base*> viewCtrs;
private:
	void setPorts() {
		for (auto guiPort : guiPorts) {
			remove(guiPort);
			delete guiPort;
		}
		guiPorts.clear();
		guiPorts.push_back(new gui_graph_port{this, stagebuffer_point::INPUT});
		guiPorts.push_back(new gui_graph_port{this, stagebuffer_point::OUTPUT_POST});
		for (auto guiPort : guiPorts) {
			add(guiPort);
		}
	}
public:
	gui_graph_n(DAW::processing_track_node_t* const _node) : gui_graph_entry(), node(_node) {
		padding = 0;
		setPorts();
	}
	~gui_graph_n() {
        for (auto viewCtr : viewCtrs) {
            remove(viewCtr);
        }
        for (auto guiPort : guiPorts) {
            remove(guiPort);
            delete guiPort;
        }
		destroyGuis();
		if (viewCtr) {
			viewCtr->setFree();
		}
	}
	const DAW::processing_track_node_t* getProcessingNode() const {
		return node;
	}
	DAW::processing_track_node_t* getProcessingNodePointer() {
		return node;
	}
	vec2 getPortOutputPos() const {
		return toParentSpace2f(guiPorts[1]->getCenterPos2f());
	}
	vec2 getPortInputPos() const {
		return toParentSpace2f(guiPorts[0]->getCenterPos2f());
	}

	void layout() override {
		ivec2 pos = vec2(0);
		ivec2 size = getSizeContent();
		guiPorts[0]->size = {nodePortRadius*2, nodePortRadius*2};
		guiPorts[1]->size = guiPorts[0]->size;
		const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
		guiPorts[0]->pos = pos + ivec2(0, hpt/2) - ivec2(nodePortRadius);
		guiPorts[1]->pos = pos + ivec2(size.x, hpt/2) - ivec2(nodePortRadius);
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
			project_t* project = project_controller_t::get()->getProject();
			std::map<int32_t, graph_node_layout_t>& graphLayouts = project->graphLayouts;
			if (graphLayouts.count(id)) {
				graphLayouts[id].pos = pos;
			}
			layout();
		}
		wasDragReleaseOnGuiCtrNodes = false;
	}
	virtual void dragMoveOn(guibase* target, ivec2 mousepos) {
		wasDragReleaseOnGuiCtrNodes = false;
	};
	virtual void dragReleaseOn(guibase* target, ivec2 mousepos) {
		ivec2 localPos = toControlsObjectSpace(mousepos, parent->parent);
		log_printf("dragReleaseOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
		if (parent->contains(localPos)) {
			wasDragReleaseOnGuiCtrNodes = true;
		}
	};
	virtual String getText() {
		switch (node->type) {
		case DAW::track_node_type_t::TRACK:
			return StringFormat("%s", StringAsCStr(node->trackOptional->name));
		case DAW::track_node_type_t::AUDIOSTAGE:
			if (node->stageId == node->stage->stageId.outputStageId)
				return StringFormat("Output Stage %d", static_cast<int32_t>(node->stageId));
			if (node->stageId == node->stage->stageId.outputPostStageId)
				return StringFormat("Output Post Stage %d", static_cast<int32_t>(node->stageId));
			if (node->stageId == node->stage->stageId.inputStageId)
				return StringFormat("Input Stage %d", static_cast<int32_t>(node->stageId));
			return StringFormat("Stage %d", static_cast<int32_t>(node->stageId));
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
		gui_graph_entry::render(vg);
		//		const float nodePortRadius = 6.0f;
		//		nvgBeginPath(vg);
		//		nvgCircle(vg, ports[0].pos.x, ports[0].pos.y, nodePortRadius);
		//		nvgCircle(vg, ports[1].pos.x, ports[1].pos.y, nodePortRadius);
		//		nvgFillColor(vg, theme->getFrameColorBase());
		//		nvgFill(vg);
		//		nvgBeginPath(vg);
		//		nvgCircle(vg, ports[0].pos.x, ports[0].pos.y, nodePortRadius);
		//		nvgCircle(vg, ports[1].pos.x, ports[1].pos.y, nodePortRadius);
		//		nvgStrokeColor(vg, theme->getFrameColorOutline());
		//		nvgStrokeWidth(vg, 2.0);
		//		nvgStroke(vg);
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
void gui_graph_port::dragReleaseOn(guibase* target, ivec2 mousepos) {
	log_printf("dragReleaseOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
	auto *ptr = dynamic_cast<gui_graph_port*>(target);
	if (ptr != nullptr) {
		auto node = ptr->parentGraphNode->getProcessingNode();
		/* only allow input to output connections */
		if (isStageBufferPointInput(stageBufferPoint) == isStageBufferPointInput(ptr->stageBufferPoint)) {
			return;
		}
		/* allow no connection to self */
		if (ptr == this) {
			return;
		}
		auto ptrNodeInput =
				isStageBufferPointInput(stageBufferPoint) ?
						parentGraphNode->getProcessingNodePointer() : ptr->parentGraphNode->getProcessingNodePointer();
		auto ptrNodeOutput =
				isStageBufferPointInput(stageBufferPoint) ? ptr->parentGraphNode->getProcessingNodePointer()
                                                                       : parentGraphNode->getProcessingNodePointer();
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		DAW::connectNodes(ptrNodeInput, ptrNodeOutput);
        DawInstance::get()->onPluginsChanged();
		vsthost::getInstance()->onTrackLayoutChange();
	}
	//		if (target->parent->parent == target->parent->parent) {
	//			wasDragReleaseOnGuiCtrNodes = true;
	//		}
}

void gui_graph_port::dragMoveOn(guibase* target, ivec2 mousepos) {
	//		wasDragReleaseOnGuiCtrNodes = false;
	log_printf("dragMoveOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
}

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
		gui_graph_n* grphNodeDest;
		gui_graph_n* grphNodeSrc;
	};
	std::shared_ptr<DAW::processing_graph_t> procList;
	std::vector<gui_graph_entry*> listGuis;
	std::vector<gui_graph_n*> listNodes;
	int updateTick = 2220;
	std::vector<edge_t> edgeList;
	guictr_graph_impl() {

	}
	enum hit_result_type {
		HIT_NONE, HIT_EDGE
	};
	struct hit_result {
		hit_result_type hitType = HIT_NONE;
		edge_t* edge = nullptr;
		float distanceEdgeMouse = 0.0f;
	};
	hit_result hitTest(vec2 mouseLocal);
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
	float minEdgeDist = 0.0f;
	gui_graph::guictr_graph_impl::edge_t* minEdge = nullptr;
	for (gui_graph::guictr_graph_impl::edge_t& edge : impl->edgeList) {

		gui_graph_n* graphNodeOutput = edge.grphNodeDest;
		gui_graph_n* graphNodeInput = edge.grphNodeSrc;
		const DAW::processing_track_node_t* nodeInput = graphNodeInput->getProcessingNode();
		const DAW::processing_track_node_t* nodeOutput = graphNodeOutput->getProcessingNode();
		auto portInputPos = graphNodeOutput->getPortInputPos();
		auto portOutputPos = graphNodeInput->getPortOutputPos();
		using meterType = rmsmeterimpl<16000>;
		meterType* ptrMeter = nullptr;
		if (nodeInput->effectOptional) {
			ptrMeter = &nodeInput->effectOptional->meter;
		}
		if (nodeInput->trackOptional && nodeInput->trackOptional->audio) {
			ptrMeter = &nodeInput->trackOptional->audio->meter;
		}
		if (nodeInput->stage) {
			ptrMeter = &nodeInput->stage->meterInput;
		}

		if (ptrMeter) {
			float maxRms = ptrMeter->getMaxRMS();
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


		guictr_graph_impl::hit_result hitResult = impl->hitTest(mouseLocal);
		float f1;
		NVGcolor colEdge = theme->getColor(GuiColor::COL_NODES_EDGE);
		if (hitResult.hitType == guictr_graph_impl::hit_result_type::HIT_EDGE && (&edge == hitResult.edge)) {
			colEdge = NVGcolor{0.45f, 0.05f, 0.45f, 1.0f};
		}

		vec2 dInOut = vec2(portOutputPos)-vec2(portInputPos);
		vec2 lastPos = portInputPos;
		nvgBeginPath(vg);
		nvgMoveTo(vg, portInputPos.x, portInputPos.y);
		for (int i = stepStart; i < stepLineSegments-stepStart; i++) {
			float t = i/(float)(stepLineSegments-1);
			float f = t * t * (3.0 - 2.0 * t);
			vec2 pos = vec2(portInputPos) + vec2(dInOut.x*t, dInOut.y*f);
			nvgLineTo(vg, pos.x, pos.y);
			lastPos = pos;
		}
		nvgLineTo(vg, portOutputPos.x, portOutputPos.y);
		nvgStrokeColor(vg, colEdge);
		nvgStrokeWidth(vg, fLineWidth);
		nvgStroke(vg);


	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
	for (gui_graph::guictr_graph_impl::edge_t& edge : impl->edgeList) {

		gui_graph_n* graphNode = edge.grphNodeDest;
		gui_graph_n* gTarget = edge.grphNodeSrc;
		auto portInputPos = graphNode->getPortInputPos();
		auto portOutputPos = gTarget->getPortOutputPos();
		auto edgeLabelPos = ivec2(portInputPos+vec2(portOutputPos-portInputPos)*0.5f);
		auto procNode = gTarget->getProcessingNode();
		setFont(vg, 14, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, edgeLabelPos.x, edgeLabelPos.y, StringAsCStr(StringFormat("%d samples", procNode->internalLatency+procNode->inputLatency)), NULL);


	}
	nvgRestore(vg);
}
gui_graph::guictr_graph_impl::hit_result gui_graph::guictr_graph_impl::hitTest(vec2 mouseLocal) {
	std::vector<hit_result> hit;
	float minEdgeDist = 0.0f;
	edge_t* minEdge = nullptr;
	for (edge_t& edge : edgeList) {
		gui_graph_n* graphNodeOutput = edge.grphNodeDest;
		gui_graph_n* graphNodeInput = edge.grphNodeSrc;
		const DAW::processing_track_node_t* nodeInput = graphNodeInput->getProcessingNode();
		const DAW::processing_track_node_t* nodeOutput = graphNodeOutput->getProcessingNode();
		auto portInputPos = graphNodeOutput->getPortInputPos();
		auto portOutputPos = graphNodeInput->getPortOutputPos();

		vec2 dInOut = vec2(portOutputPos)-vec2(portInputPos);
		vec4 colEdge = {0.05f, 0.05f, 0.05f, 1.0f};
		vec2 pos = portInputPos;
		float edgeMouseDist = 0.0f;
		for (int i = stepStart; i < stepLineSegments-stepStart; i++) {
			float t = i/(float)(stepLineSegments-1);
			float f = t * t * (3.0 - 2.0 * t);
			vec2 nextPos = vec2(portInputPos) + vec2(dInOut.x*t, dInOut.y*f);
			float fDist = math::distancePointLine(mouseLocal, pos, nextPos);
			if (i == stepStart || fDist < edgeMouseDist) {
				edgeMouseDist = fDist;
			}
			pos = nextPos;
		}
		if (minEdge == nullptr || edgeMouseDist < minEdgeDist) {
			minEdgeDist = edgeMouseDist;
			minEdge = &edge;
		}
		if (edgeMouseDist < 10) {
			hit_result hitSeg{hit_result_type::HIT_EDGE, &edge, edgeMouseDist };
			hit.push_back(std::move(hitSeg));
		}
	}
	std::sort(hit.begin(), hit.end(), [](hit_result const & a, hit_result const & b) {
		return a.distanceEdgeMouse<b.distanceEdgeMouse;
	});
	if (hit.size()) {

		hit_result& h = hit[0];
		if (h.distanceEdgeMouse < 10) {
			return h;
		}
	}
	return {hit_result_type::HIT_NONE, nullptr, 0.0f};
}
bool gui_graph::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (!gui->isVisible())
				continue;
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			evt.requestFocus(this);
			return true;
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
	{
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        lastProcessingList = nullptr;
		if (!isTrackGraph) {  /* project graph */
			//		lastProcessingList = vsthost::getInstance()->lastProcessingList;
			std::shared_ptr<DAW::processing_graph_t> processingGraph;
			auto project = project_controller_t::get()->getProject();
			dbgassert(project);
			auto tracksFlatAll = project->trackList.getAllTracksFlatVec(); //TODO: get rid of copy
			if (!DAW::buildProcessingGraph(vsthost::getInstance(), project, tracksFlatAll, processingGraph)) {
				log_printf("Failed building track graph\n", 0);
			} else {
				lastProcessingList = processingGraph;
            }
        } else {
            lastProcessingList = nullptr;
	//		lastProcessingList = vsthost::getInstance()->lastProcessingGraphs[track->audio->stageId];
			auto track = DawInstance::get()->getSelectedTrack();
			if (track && track->audio) {
		        std::shared_ptr<DAW::effect_processing_graph_t> effProcessingGraph;
		        if (!DAW::buildEffectProcessingGraph(vsthost::getInstance(), nullptr, track->audio, effProcessingGraph)) {
		            log_printf("Failed building effect graph\n", 0);
		        } else {
					lastProcessingList = effProcessingGraph;
				}
            }
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
		project_t* project = project_controller_t::get()->getProject();
		std::map<int32_t, graph_node_layout_t>& graphLayouts = project->graphLayouts;
		std::vector<DAW::processing_track_node_t*> allNodes = procGraph.nodes;
		for (DAW::processing_track_node_t* node : allNodes) {
			gui_graph_n* entry = new gui_graph_n(node);
			entry->id = static_cast<int32_t>(node->stageId);
			entry->rowHeight = fontScale;
			const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);

			if (!graphLayouts.count(entry->id)) {
                graphLayouts[entry->id] = graph_node_layout_t{posGrid, nodeSize};
                posGrid.x += posGridStepX;
                if (posGrid.x + posGridStepX > cs.x) {
                    posGrid = ivec2(fontScale, posGrid.y + posGridStepX);
                }
			}
			graph_node_layout_t* const nodeLayout = &graphLayouts[entry->id];
            entry->pos = nodeLayout->pos;
			entry->size = nodeLayout->size;
			auto guiText = new guinodeinfo_text{node};
			guiText->size = {entry->size.x, entry->size.y-hpt};
			guiText->pos = {0, hpt};
			if (node->trackOptional) {
				int32_t meterWidth = fontScale;
				// meter is updated on audio thread. Copy is not atomic, but should not pose a UB risk since object lifetime is ensured
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meter);
				meter->size = {meterWidth, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};
				entry->add(meter);
				// meter is updated on audio thread. Copy is not atomic, but should not pose a UB risk since object lifetime is ensured
				gui_trackmeter<16000,2>* guimeterInput = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meterInput);
				guimeterInput->size = {meterWidth, entry->getSizeContent().y-hpt};
				guimeterInput->pos = {0, hpt};
				entry->add(guimeterInput);


				guiText->size = {entry->size.x-meterWidth*2, entry->size.y-hpt};
				guiText->pos = {guimeterInput->size.x, hpt};
			}
			if (node->effectOptional) {
				int32_t meterWidth = fontScale;
				// meter is updated on audio thread. Copy is not atomic, but should not pose a UB risk since object lifetime is ensured
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(&node->effectOptional->meter);
				meter->size = {meterWidth, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};
				entry->add(meter);
				// meter is updated on audio thread. Copy is not atomic, but should not pose a UB risk since object lifetime is ensured
				gui_trackmeter<16000,2>* guimeterInput = new gui_trackmeter<16000,2>(&node->effectOptional->meterIn);
				guimeterInput->size = {meterWidth, entry->getSizeContent().y-hpt};
				guimeterInput->pos = {0, hpt};
				entry->add(guimeterInput);


				guiText->size = {entry->size.x-meterWidth*2, entry->size.y-hpt};
				guiText->pos = {guimeterInput->size.x, hpt};
			}
			if (node->stage) {
				int32_t meterWidth = fontScale;
				// meter is updated on audio thread. Copy is not atomic, but should not pose a UB risk since object lifetime is ensured
				gui_trackmeter<16000,2>* meter = new gui_trackmeter<16000,2>(node->stageId == TRACKID_DEFAULT_I32 ? &node->stage->meter : &node->stage->meterInput);
				meter->size = {meterWidth, entry->getSizeContent().y-hpt};
				meter->pos = {entry->getSizeContent().x-meter->size.x, hpt};
				entry->add(meter);
//				gui_trackmeter<16000,2>* guimeterInput = new gui_trackmeter<16000,2>(&node->trackOptional->audio->meterInput);
//				guimeterInput->size = {meterWidth, entry->getSizeContent().y-hpt};
//				guimeterInput->pos = {0, hpt};
//				entry->add(guimeterInput);

				guiText->size = {entry->size.x-meterWidth, entry->size.y-hpt};
				guiText->pos = {0, hpt};
			}
			if (node->type == DAW::track_node_type_t::EFFECT && node->effectOptional) {
				auto intEffect = dynamic_cast<internalplugin*>(node->effectOptional);
                if (intEffect) {
                    entry->viewCtr = intEffect->createInternalView();
                    if (entry->viewCtr) {
                        entry->viewCtr->addTo(entry->viewCtrs);
                        entry->viewCtr->onGuiOpen(nullptr);
                    }
				}
            }
            for (auto* ctr : entry->viewCtrs) {
                entry->add(ctr);
            }
			int32_t insetCtrls = INSET_TITLE;
			auto layoutPos = guiText->pos;
			for (auto* ctr : entry->viewCtrs) {
				ctr->pos = layoutPos + ivec2(insetCtrls, insetCtrls);
                ivec2 prefSizeCtr = guiText->size - ivec2(insetCtrls*2);
				ctr->determineSize(prefSizeCtr);
				ctr->size = prefSizeCtr;
				ctr->layout();
				layoutPos.x = ctr->right() + INSET_TITLE;
			}
			if (entry->viewCtrs.empty()) {
				entry->add(guiText);
			} else {
				delete guiText;
			}

			/* copy over positions from old layout */
			//auto it = std::find_if(impl->listNodes.cbegin(), impl->listNodes.cend(), [stageId = node->stageId](gui_graph_n* gn) {
			//	return gn->getProcessingNode()->stageId == stageId;
			//});
			//if (it != impl->listNodes.cend()) {
			//	gui_graph_n* gTarget = *it;
			//	entry->pos = gTarget->pos;
			//	entry->size = gTarget->size;
			//} else {

			//}
			listEntries.push_back(entry);
			listNodes.push_back(entry);
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
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (!gui->isVisible())
				continue;
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
            evt.requestFocus(this);
            return true;
        }
		if (canMouseHit()) {
			evt.requestFocus(this);
			return true;
		}
	}
	return false;
}
void gui_graph::handleDraggedBegin(MouseEvent& evt) {
	guictr_graph_impl::hit_result hitResult = impl->hitTest(evt.relMousepos);
	if (hitResult.hitType == guictr_graph_impl::hit_result_type::HIT_EDGE && (hitResult.edge)) {
		if (isCtrl(evt.kbmods)) {
	        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			DAW::disconnectNodes(hitResult.edge->grphNodeDest->getProcessingNodePointer(), hitResult.edge->grphNodeSrc->getProcessingNodePointer());
            DawInstance::get()->onPluginsChanged();
			vsthost::getInstance()->onTrackLayoutChange();
			return;
		}
	}
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

ivec2 gui_graph::toContainerSpace(ivec2 in) const {
	return ivec2((vec2(in - getPosContent()) - offset) * (1.0f / scale));
}

ivec2 gui_graph::toParentSpace(ivec2 localCoord) const {
	return ivec2(vec2(getPosContent() + localCoord) * scale + offset);
}

vec2 gui_graph::toContainerSpace2f(vec2 in) const {
	return vec2((in - vec2(getPosContent()) - offset) * (1.0f / scale));
}

vec2 gui_graph::toParentSpace2f(vec2 localCoord) const {
	return (vec2(getPosContent()) + localCoord) * scale + offset;
}
