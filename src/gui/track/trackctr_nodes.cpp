#include <cmath>
#include <functional>
#include <glm/geometric.hpp>
#include <nanovg.h>
#include <splines/natural_spline.h>
#include <type_traits>
#include <utility>
#include <glm/vec2.hpp>
#include <glm/gtx/norm.hpp>

#include "automation.h"
#include "basectrl.h"
#include "exceptions.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/gui.h"
#include "gui/meter/guimeter.h"
#include "gui/tooltip/tooltip.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/plugin/pluginctr.h"
#include "host/audio_config.h"
#include "host/daw_channel.h"
#include "host/mainctrl.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/group.h"
#include "host/plugin/internal_plugin.h"
#include "host/vst_host.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_util.h"
#include "theme.h"
#include "track_impl.h"
#include "track_snapshot.h"
#include "track.h"
#include "track.h"
#include "trackcontent.h"
#include "trackcontrols.h"
#include "trackctr_nodes.h"
#include "trackctr.h"
#include "types.h"

using DAW::stage_bufferpoint;

template<typename T>
float lengthSquared(T a) { return glm::length2(a); }

using floating_t = float;

const float fLineWidth     = 4.0f;
const float nodePortRadius = 6.0f;
const auto colEdgeSignal   = NVGcolor{ 0.1f, 0.6f, 0.1f, 1.0f };
const auto GRAPH_NODE_SIZE = vec2(260);
const auto GRAPH_FONT_SIZE = 16;
const auto GRAPH_NODE_METER_WIDTH = 8;


class edge_spline {
public:
    static constexpr bool _c_renderCtrls = false;

private:
    static constexpr float _c_portStraightLen = 8.0f;
    static constexpr float _c_scX             = 1.3f;
    static constexpr float _c_scY             = 1.0f;
    static constexpr float _c_minKnotDist     = 8.0f;
    static constexpr float _c_maxKnotDist     = 16.0f;
    static constexpr float _c_alpha           = 0.5f;
    static constexpr bool _c_inc              = false;
    static constexpr bool _c_endIsKnot        = true;
    float perPixelSegments = 1.0f / 64.0f;
    std::vector<vec2> ctrlPts;
    std::vector<vec2> vec;
    static void calculateCtrlPts(std::vector<vec2>& ctrlPts, vec2 portInputAbsPos, vec2 portOutputAbsPos) {
        const auto portInputPos  = portInputAbsPos - vec2(_c_portStraightLen, 0.0f);
        const auto portOutputPos = portOutputAbsPos + vec2(_c_portStraightLen, 0.0f);
        ctrlPts.resize(2 + 3 + 2);
        auto outIt          = ctrlPts.begin();
        *outIt++            = portInputAbsPos;
        *outIt++            = portInputPos;
        const auto delta    = portOutputPos - portInputPos;
        const auto middlePt = portInputPos + delta * 0.5f;
        if (delta.x > 0.0f) {
            const auto portOffset = vec2(1.0f, delta.y > 0 ? -1.0f : 1.0f) * math::clamp(delta.x, _c_minKnotDist, _c_maxKnotDist) * vec2(_c_scX, _c_scY);
            *outIt++              = portInputPos - portOffset;
            *outIt++              = middlePt;
            *outIt++              = portOutputPos + portOffset;
        } else {
            const auto deltaNorm    = glm::normalize(delta);
            const auto ctrlPtInput  = glm::normalize(-vec2(1.0f, 0.0f) + deltaNorm * 2.5f);
            const auto ctrlPtOutput = glm::normalize(+vec2(1.0f, 0.0f) - deltaNorm * 2.5f);
            const auto ctrlScale    = glm::length(delta) * 0.15f;
            *outIt++                = portInputPos + ctrlPtInput * ctrlScale;
            *outIt++                = middlePt;
            *outIt++                = portOutputPos + ctrlPtOutput * ctrlScale;
        }
        *outIt++ = portOutputPos;
        *outIt++ = portOutputAbsPos;
    }
    static void calculateVectors(std::vector<vec2>& ctrlPts, std::vector<vec2>& vec, float perPixelSegments) {
        using SplineType = NaturalSpline<vec2, floating_t>;
        const SplineType spline(ctrlPts, _c_inc, _c_alpha, _c_endIsKnot ? SplineType::Natural : SplineType::NotAKnot);
        const auto numSegments = math::floorfS32(8.0f + glm::length(ctrlPts.back() - ctrlPts.front()) * perPixelSegments);
        const auto maxT        = spline.getMaxT();
        vec.resize(numSegments + (_c_inc ? 0 : 2));
        auto outIt = vec.begin();
        if (!_c_inc) {
            *outIt++ = ctrlPts.front();
        }
        for (int iPt = 0; iPt < numSegments; ++iPt) {
            float x = (iPt / (float) (numSegments - 1));
            // float t = x * x * (3.0f - 2.0f * x);
            float t = math::calcTanhLike(x, 1.2f);
            *outIt++ = spline.getPosition(t * maxT);
        }
        if (!_c_inc) {
            *outIt++ = ctrlPts.back();
        }
    }

public:
    std::vector<vec2>& calculateSplineVectors(vec2 portInputAbsPos, vec2 portOutputAbsPos) {
        if (ctrlPts.size() >= 2 && !vec.empty()) {
            if (ctrlPts.front() == portInputAbsPos && ctrlPts.back() == portOutputAbsPos) {
                return this->vec;
            }
        }
        vec.clear();
        ctrlPts.clear();
        calculateCtrlPts(ctrlPts, portInputAbsPos, portOutputAbsPos);
        if (ctrlPts.size() > 2) {
            calculateVectors(ctrlPts, vec, perPixelSegments);
        }
        return vec;
    }
    std::vector<vec2>& getCtrlPts() {
        return this->ctrlPts;
    }
    std::vector<vec2>& getVecs() {
        return this->vec;
    }
    void setPerPixelSegments(float f) {
        this->perPixelSegments = f;
    }
};

void guictr_nodes_editor::scrollTo(guibase* g) {
}
void gui_graph_entry::handleDraggedMove(MouseEvent& evt) {
    parentCtrl->objectDragMove(this, evt);
}

void gui_graph_entry::handleDraggedRelease(MouseEvent& evt) {
    parentCtrl->objectDragRelease(this, evt);
}

bool gui_graph_entry::setScissorTransformContainer(NVGcontext* vg) {
    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return false;
    }
    nvgTranslate(vg, pos.x, pos.y);
    return true;
}
void gui_graph_entry::render(NVGcontext* vg) {
    if (!setScissorTransformContainer(vg)) {
        return;
    }
    renderFrameBase(vg);
    String text = getText();
    int flags   = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : 0;
    if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
    renderTitleBar(vg, size, text, GuiConstant::CONST_FIXED_TITLE_HEIGHT, nodePortRadius*0.6f, flags, true);
    renderFrameOutline(vg);
    ivec2 posInset  = getPosContent();
    nvgTranslate(vg, posInset.x-pos.x, posInset.y-pos.y);
    nvgTranslateZ(vg, -4.0f);
    for (guibase* gui : guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
    nvgSave(vg);
    int32_t i2                      = padding * 2;
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    int32_t h                       = TRACK_HEIGHT_STEP - i2;
    setFont(vg, G_FONT_SCALE(h), THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    for (guibase* gui : guis) {
        renderText(vg, vec2(i2, gui->top() + gui->size.y * 0.5f), vec2(gui->pos.x-i2, size.y), gui->label, TRACK_HEIGHT_STEP);
    }
    nvgRestore(vg);
}

void gui_graph_entry::handleDraggedBegin(MouseEvent& evt) {
    if (parent) parent->buttonClicked(this);
}
namespace DAW {
    bool channelRefEquals(const DAW::channel_ref_t& existingRef, const DAW::channel_ref_t& ref, int matchSrcDstAll) {
        if (existingRef.type == ref.type) {
            switch (ref.type) {
                case stage_type::INPUT_EXTERNAL_AUDIO:
                    return ref.externalInputType == existingRef.externalInputType
                            && ref.externalInputIdx == existingRef.externalInputIdx
                            && ref.srcChannelOffset == existingRef.srcChannelOffset;
                case stage_type::INPUT_AUDIOSTAGE:
                    return ref.stage.buffer == existingRef.stage.buffer
                           && ref.stage.stageRef.stageId == existingRef.stage.stageRef.stageId;
                case stage_type::INPUT_AUDIOSTAGE_EFFECT:
                    if (ref.projectGlobalId == existingRef.projectGlobalId) {
                        if (matchSrcDstAll == 0)
                            return ref.srcChannelOffset == existingRef.srcChannelOffset;
                        if (matchSrcDstAll == 1)
                            return ref.dstChannelOffset == existingRef.dstChannelOffset;
                        return ref.srcChannelOffset == existingRef.srcChannelOffset
                                && ref.dstChannelOffset == existingRef.dstChannelOffset;
                    }
                    return false;
                case stage_type::INPUT_DEFAULT:
                case stage_type::INPUT_EMPTY:
                    return true;
            }
        }
        return false;
    }
    bool removeRouting(std::vector<DAW::channel_ref_t>& list, const DAW::channel_ref_t& ref, bool removeDefaultRouting) {
        auto it = std::remove_if(list.begin(), list.end(), [ref](DAW::channel_ref_t& existingRef) {
            return channelRefEquals(existingRef, ref, 2);
        });
        if (it != list.end()) {
            list.erase(it, list.end());
            return true;
        }
        if (removeDefaultRouting) {
            it = std::remove_if(list.begin(), list.end(), [ref](DAW::channel_ref_t& existingRef) {
                return existingRef.getType() == stage_type::INPUT_DEFAULT;
            });
            if (it != list.end()) {
                list.erase(it, list.end());
                return true;
            }
        }
        return false;
    }
}// namespace DAW
class gui_graph_n;
class gui_graph_port : public guibase {
    friend class gui_graph::guictr_graph_impl;
    gui_graph_n* parentGraphNode;
    stage_bufferpoint stageBufferPoint;
    DAW::channel_desc channelDesc; 
    edge_spline spline;
public:
    gui_graph_port(gui_graph_n* _parentGraphNode, stage_bufferpoint _stageBufferPoint, DAW::channel_desc _channelDesc)
        : guibase(),
          parentGraphNode(_parentGraphNode),
          stageBufferPoint(_stageBufferPoint),
          channelDesc(_channelDesc) {
        setCanMouseHit(true);
        spline.setPerPixelSegments(1.0f/4.0f);
    }
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
        return new guitooltip<gui_graph_port>(this);
    }
    void addPropertiesTooltip(Table::tbl& table) {
        table.rows.push_back({ {channelDesc.name} });
        determine_string_width strw(parentCtrl, theme);
        auto widthLabel = strw.getStringWidth(channelDesc.name, table.rowHeight, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        table.tableWidth = widthLabel + INSET_TABLE_CELL_PADDING * 3;
    }

    stage_bufferpoint getBufferPoint() const {
        return stageBufferPoint;
    }

    vec2 getCenterPos2f() const {
        return vec2(pos) + vec2(size) * 0.5f;
    }

    gui_graph_n* getNode() {
        return this->parentGraphNode;
    }

    gui_graph_n* getNode() const {
        return this->parentGraphNode;
    }

    DAW::channel_desc getChannelDesc() const {
        return this->channelDesc;
    }

    vec2 getPortPos() {
        return parent->toParentSpace(getCenterPos2f());
    }

    void handleDraggedBegin(MouseEvent& evt) override {
    }
    void handleDraggedMove(MouseEvent& evt) override {
        parentCtrl->objectDragMove(this, evt);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        parentCtrl->objectDragRelease(this, evt);
    }
    
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (canMouseHit() && contains(mpos)) {
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override;
    void dragReleaseOn(guibase* target, ivec2 mousepos) override;

    virtual String getText() {
        switch (stageBufferPoint) {
            case stage_bufferpoint::INPUT:
                return "INPUT";
            case stage_bufferpoint::OUTPUT:
                return "OUTPUT";
            case stage_bufferpoint::OUTPUT_POST:
                return "OUTPUT_POST";
        }
        dbgassert(0);
        return "INVALID";
    }

    void render(NVGcontext* vg) override {
        auto centerPos = getCenterPos2f();
        nvgBeginPath(vg);
        nvgCircle(vg, centerPos.x, centerPos.y, nodePortRadius);
        nvgFillColor(vg, theme->getFrameColorBright());
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
            posSS       = parent->parent->toParentSpace(posSS);

            ivec2 mouseposSS  = toControlsObjectSpace(mousepos, parent->parent->parent);
            ivec2 editorPosSS = parent->parent->parent->toScreenSpace(ivec2(0));

            if (getBufferPoint() == stage_bufferpoint::INPUT) {
                std::swap(mouseposSS, posSS);
            }
            std::vector<vec2>& pts = spline.calculateSplineVectors(mouseposSS, posSS);
            if (pts.empty())
                return;
            nvgSave(vg);
            nvgTranslate(vg, editorPosSS.x, editorPosSS.y);
            nvgBeginPath(vg);
            auto it = pts.cbegin();
            nvgMoveTo(vg, (*it).x, (*it).y);
            for (; it != pts.cend(); ++it) {
                const auto& pt = *it;
                nvgLineTo(vg, pt.x, pt.y);
            }
            nvgStrokeColor(vg, colEdgeSignal);
            nvgStrokeWidth(vg, fLineWidth + 2.0f);
            nvgStroke(vg);
            nvgRestore(vg);
        }
    }
    
};

template<>
void guitooltip<gui_graph_port>::setContent() {
    ptr->addPropertiesTooltip(table);
}

class gui_graph_n : public gui_graph_entry {
    friend class gui_graph;
    friend class gui_graph_port;
    gui_graph::guictr_graph_impl* const graphImpl;
    DAW::processing_track_node_t* const node;
    std::vector<gui_graph_port*> guiPorts;
    std::vector<gui_graph_port*> portsInput;
    std::vector<gui_graph_port*> portsOutput;
    std::shared_ptr<PluginViewContainers> viewCtr;
    /* holds guictrs of internal vstplugins with custom gui (non-steinberg api) */
    std::vector<guictr_base*> viewCtrs;
    bool wasDragReleaseOnGuiCtrNodes = false;

private:
    void setPorts() {
        dbgassert(guiPorts.empty());
        if (node->type == DAW::track_node_type_t::EFFECT) {
            for (auto& desc : node->effectOptional->inputChannelsDesc) {
                portsInput.push_back(new gui_graph_port{ this, stage_bufferpoint::INPUT, desc});
            }
            for (auto& desc : node->effectOptional->outputChannelsDesc) {
                portsOutput.push_back(new gui_graph_port{ this, stage_bufferpoint::OUTPUT_POST, desc});
            }
        } else {
            portsInput.push_back(new gui_graph_port{ this, stage_bufferpoint::INPUT, DAW::channel_desc{0, 2, "Stereo Input"}});
            portsOutput.push_back(new gui_graph_port{ this, stage_bufferpoint::OUTPUT_POST, DAW::channel_desc{0, 2, "Stereo Output"} });
        }
        addAll(guiPorts, portsInput);
        addAll(guiPorts, portsOutput);
        for (auto guiPort : guiPorts) {
            add(guiPort);
        }
    }

public:
    gui_graph_n(gui_graph::guictr_graph_impl* _graphImpl, DAW::processing_track_node_t* _node) 
    : gui_graph_entry(),
      graphImpl(_graphImpl),
      node(_node)
    {
        // padding = 0;
        setPorts();
        (void)graphImpl;
    }

    ~gui_graph_n() override {
        for (auto ctr : viewCtrs) {
            remove(ctr);
        }
        for (auto port : guiPorts) {
            remove(port);
            delete port;
        }
        destroyGuis();
        if (viewCtr) {
            viewCtr->onGuiClose();
            viewCtr->setFree();
        }
    }

    const DAW::processing_track_node_t* getProcessingNode() const {
        return node;
    }

    DAW::processing_track_node_t* getProcessingNodePointer() {
        return node;
    }

    void layout() override {
        const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        vec2 pos  = -vec2(paddingTL(padding));
        vec2 size = vec2(this->size);
        int inputIndex = 0;
        int outputIndex   = 0;
        for (auto port : guiPorts) {
            port->size = { nodePortRadius * 2, nodePortRadius * 2 };
            
            float topOffsetInputs = portsInput.size() > 1 ? (hpt + nodePortRadius * 1.5f) : (hpt * 0.5f - nodePortRadius);
            float topOffsetOutputs = portsOutput.size() > 1 ? (hpt + nodePortRadius * 1.5f) : (hpt * 0.5f - nodePortRadius);
            switch (port->getBufferPoint()) {
                case stage_bufferpoint::INPUT:
                    port->pos = pos + vec2(0, topOffsetInputs) + vec2(-nodePortRadius, inputIndex * nodePortRadius * 3);
                    ++inputIndex;
                    break;
                default:
                    port->pos = pos + vec2(size.x, topOffsetOutputs) + vec2(-nodePortRadius, outputIndex * nodePortRadius * 3);
                    ++outputIndex;
                    break;
            }
        }

        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        gui_graph_entry::handleDraggedBegin(evt);
        if (node->trackOptional)
            dawCtrl->getDaw()->setSelectedTrack(node->trackOptional);
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        parentCtrl->objectDragRelease(this, evt);
        if (wasDragReleaseOnGuiCtrNodes) {
            auto screenPos     = evt.mousepos + evt.dragOffset;
            ivec2 localPos     = toControlsObjectSpace(screenPos, parent);
            pos                = localPos;
            std::map<int32_t, graph_node_layout_t>& graphLayouts = dawCtrl->getDaw()->getProject()->graphLayouts;
            if (graphLayouts.count(id)) {
                graphLayouts[id].pos = pos;
            }
            layout();
        }
        wasDragReleaseOnGuiCtrNodes = false;
    }

    void dragMoveOn(guibase* target, ivec2 mousepos) override {
        wasDragReleaseOnGuiCtrNodes = false;
    }

    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
        ivec2 localPos = toControlsObjectSpace(mousepos, parent->parent);
        log_lf(Log::L_DEBUG, "dragReleaseOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
        if (parent->contains(localPos)) {
            wasDragReleaseOnGuiCtrNodes = true;
        }
    }

    String getText() override {
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
        return StringFormat("Stage id %d", static_cast<int32_t>(node->stageId));
    }

    void render(NVGcontext* vg) override {
        if (parentCtrl && parentCtrl->guiDragged == this) {
            nvgGlobalAlpha(vg, 0.5f);
        }
        gui_graph_entry::render(vg);
        if (parentCtrl && parentCtrl->guiDragged == this) {
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

namespace NodeGraph {
    struct edge_t {
        gui_graph_port* portDst;
        gui_graph_port* portSrc;
        edge_spline spline;
        edge_t(gui_graph_port* _portDst, gui_graph_port* _portSrc) : portDst(_portDst), portSrc(_portSrc) {
        }
    };
    bool getChannelRef(const gui_graph_port* port, const bool isSrc, DAW::channel_ref_t& ref) {
        auto procNode = port->getNode()->getProcessingNode();
        switch (procNode->type) {
            case DAW::track_node_type_t::TRACK:
                dbgassert(procNode->trackOptional);
                if (procNode->trackOptional) {
                    dbgassert(procNode->trackOptional->audio);
                    ref = DAW::ChannelStage(procNode->trackOptional->audio, stage_bufferpoint::OUTPUT_POST);
                    return true;
                }
                break;
            case DAW::track_node_type_t::AUDIOSTAGE:
                dbgassert(procNode->stage);
                if (procNode->stage) {
                    if (procNode->stage->stageId.inputStageId == procNode->stageId) {
                        ref = DAW::ChannelStage(procNode->stage, stage_bufferpoint::INPUT);
                        return true;
                    }
                    if (procNode->stage->stageId.outputStageId == procNode->stageId) {
                        ref = DAW::ChannelStage(procNode->stage, stage_bufferpoint::OUTPUT);
                        return true;
                    }
                    if (procNode->stage->stageId.outputPostStageId == procNode->stageId) {
                        ref = DAW::ChannelStage(procNode->stage, stage_bufferpoint::OUTPUT_POST);
                        return true;
                    }
                }
                break;
            case DAW::track_node_type_t::EFFECT:
                dbgassert(procNode->effectOptional);
                if (procNode->effectOptional) {
                    DAW::channel_desc srcDesc{};
                    DAW::channel_desc dstDesc{};
                    if (isSrc) {
                        srcDesc = port->getChannelDesc();
                    } else {
                        dstDesc = port->getChannelDesc();
                    }
                    ref = DAW::ChannelAudioEffect(procNode->effectOptional, stage_bufferpoint::OUTPUT_POST, srcDesc, dstDesc);
                    return true;
                }
                break;
        }
        dbgassert(0);
        return false;
    }

    gui_graph_port* getOutputPort(const std::vector<gui_graph_port*>& portsOutput, const DAW::channel_ref_t& channelRef) {
        DAW::channel_ref_t refTmp;
        auto it = std::find_if(portsOutput.begin(), portsOutput.end(), [&refTmp, &channelRef](gui_graph_port* gn) {
            if (NodeGraph::getChannelRef(gn, true, refTmp)) {
                return DAW::channelRefEquals(channelRef, refTmp, 0);
            }
            return false;
        });
        return it != portsOutput.end() ? *it : nullptr;
    }
    
class action_modify_track_routing : public action_base {
    audio_stage_ref_t ref;
    track_io_configuration_snapshot_t snapshotBefore;
    track_io_configuration_snapshot_t snapshotAfter;
public:
    action_modify_track_routing(audio_stage_ref_t _ref, track_io_configuration_snapshot_t _snapshot)
        : ref(_ref), snapshotBefore(_snapshot) {
        desc = "Modify track routing";
    }
    void undo(DawInstance* daw) override {
        auto track = daw->getTracks().resolveTrack(ref);
        if (!track) {
            setError("Failed undoing routing change");
        } else {
            track->audio->createIOSnapshot(snapshotAfter);
            track->audio->loadIOConfiguration(snapshotBefore);
            daw->onPluginsChanged();
            daw->getHost()->onTrackLayoutChange();
        }
    }
    void redo(DawInstance* daw) override {
        auto track = daw->getTracks().resolveTrack(ref);
        if (!track) {
            setError("Failed undoing routing change");
        } else {
            track->audio->createIOSnapshot(snapshotBefore);
            track->audio->loadIOConfiguration(snapshotAfter);
            daw->onPluginsChanged();
            daw->getHost()->onTrackLayoutChange();
        }
    }
};
class action_modify_stage_routing : public action_base {
    audio_stage_ref_t ref;
    track_effect_routing_snapshot_t snapshotBefore;
    track_effect_routing_snapshot_t snapshotAfter;
public:
    action_modify_stage_routing(audio_stage_ref_t _ref, track_effect_routing_snapshot_t _snapshot)
        : ref(_ref), snapshotBefore(_snapshot) {
        desc = "Modify effect routing";
    }
    void undo(DawInstance* daw) override {
        auto stage = daw->getHost()->getAudioStage(ref);
        if (!stage) {
            setError("Failed undoing routing change");
        } else {
            snapshotAfter = {};
            stage->createRoutingSnapshot(snapshotAfter);
            stage->loadRoutingSnapshot(snapshotBefore);
            daw->onPluginsChanged();
            daw->getHost()->onTrackLayoutChange();
        }
    }
    void redo(DawInstance* daw) override {
        auto stage = daw->getHost()->getAudioStage(ref);
        if (!stage) {
            setError("Failed undoing routing change");
        } else {
            snapshotBefore = {};
            stage->createRoutingSnapshot(snapshotBefore);
            stage->loadRoutingSnapshot(snapshotAfter);
            daw->onPluginsChanged();
            daw->getHost()->onTrackLayoutChange();
        }
    }
};
    bool connectPorts(DawInstance* daw, gui_graph_port* portDst, gui_graph_port* portSrc) {
        /* allow no connection to self */
        if (portDst == portSrc) { // TODO: check at lower level
            return false;
        }
        /* only allow input to output connections */
        auto isInputDst = DAW::isStageBufferPointInput(portDst->getBufferPoint());
        auto isInputSrc = DAW::isStageBufferPointInput(portSrc->getBufferPoint());
        if (isInputDst == isInputSrc) {
            return false;
        }
        if (isInputSrc) {
            std::swap(portDst, portSrc);
            std::swap(isInputDst, isInputSrc);
        }
        auto nodeSrc = portSrc->getNode()->getProcessingNodePointer();
        auto nodeDest = portDst->getNode()->getProcessingNodePointer();
        if (nodeSrc->type == DAW::track_node_type_t::TRACK || nodeDest->type == DAW::track_node_type_t::TRACK) {
            DAW::channel_ref_t refDst;
            if (getChannelRef(portDst, false, refDst)) {
                track_io_configuration_snapshot_t snapshot;
                nodeSrc->trackOptional->audio->createIOSnapshot(snapshot);
                auto action = new action_modify_track_routing(nodeSrc->trackOptional->audio->toRef(), snapshot);
                nodeSrc->trackOptional->audio->outputChannel = refDst;
                daw->pushHist(action);
                return true;
            }
            return false;
        }

        DAW::channel_ref_t refSrc;
        if (getChannelRef(portSrc, true, refSrc)) {
            audio_stage_ref_t stageRef;
            track_effect_routing_snapshot_t snapshot;
            //TODO: make sure this is correct for all cases and not just ::EFFECT
            switch (nodeDest->type) {
                case DAW::track_node_type_t::AUDIOSTAGE:
                    dbgassert(nodeDest->stage);
                    nodeDest->stage->createRoutingSnapshot(snapshot);
                    stageRef = nodeDest->stage->toRef();
                    removeRouting(nodeDest->stage->postEffectRouting, refSrc, false);
                    nodeDest->stage->postEffectRouting.push_back(refSrc);
                    nodeDest->stage->routingState = audiostagerouting_state_t::CUSTOM;
                    break;
                case DAW::track_node_type_t::EFFECT:
                    dbgassert(nodeDest->effectOptional);
                    nodeDest->effectOptional->getTrackLink()->createRoutingSnapshot(snapshot);
                    stageRef = nodeDest->effectOptional->getTrackLink()->toRef();
                    refSrc.dstChannelOffset = portDst->getChannelDesc().offset;
                    removeRouting(nodeDest->effectOptional->inputChannels, refSrc, false);
                    nodeDest->effectOptional->inputChannels.push_back(refSrc);
                    nodeDest->effectOptional->getTrackLink()->routingState = audiostagerouting_state_t::CUSTOM;
                    break;
                default:
                    unreachable();
            }
            auto action = new action_modify_stage_routing(stageRef, snapshot);
            daw->pushHist(action);
            return true;
        }
        dbgassert(0);
        return false;
    }
    bool disconnectEdge(DawInstance* daw, edge_t* edge) {
        auto nodeSrc = edge->portSrc->getNode()->getProcessingNodePointer();
        auto nodeDest = edge->portDst->getNode()->getProcessingNodePointer();
        if (nodeSrc->type == DAW::track_node_type_t::TRACK || nodeDest->type == DAW::track_node_type_t::TRACK) {
            auto stage = nodeSrc->trackOptional->audio;
            track_io_configuration_snapshot_t snapshot;
            stage->createIOSnapshot(snapshot);
            auto action = new action_modify_track_routing(stage->toRef(), snapshot);
            stage->outputChannel = DAW::ChannelNone();
            daw->pushHist(action);
            return true;
        }
        DAW::channel_ref_t refSrc;
        if (getChannelRef(edge->portSrc, true, refSrc)) {
            audio_stage_ref_t stageRef;
            track_effect_routing_snapshot_t snapshot;
            switch (nodeDest->type) {
                case DAW::track_node_type_t::AUDIOSTAGE:
                    dbgassert(nodeDest->stage);
                    nodeDest->stage->createRoutingSnapshot(snapshot);
                    stageRef = nodeDest->stage->toRef();
                    removeRouting(nodeDest->stage->postEffectRouting, refSrc, true);
                    nodeDest->stage->routingState = audiostagerouting_state_t::CUSTOM;
                    break;
                case DAW::track_node_type_t::EFFECT:
                    dbgassert(nodeDest->effectOptional);
                    nodeDest->effectOptional->getTrackLink()->createRoutingSnapshot(snapshot);
                    stageRef = nodeDest->effectOptional->getTrackLink()->toRef();
                    refSrc.dstChannelOffset = edge->portDst->getChannelDesc().offset;
                    removeRouting(nodeDest->effectOptional->inputChannels, refSrc, true);
                    nodeDest->effectOptional->getTrackLink()->routingState = audiostagerouting_state_t::CUSTOM;
                    break;
                default:
                    unreachable();
            }
            auto action = new action_modify_stage_routing(stageRef, snapshot);
            daw->pushHist(action);
            return true;
        }
        return false;
    }
}

void gui_graph_port::dragMoveOn(guibase* target, ivec2 mousepos) {
    log_lf(Log::L_DEBUG, "dragMoveOn %s on %s\n", StringAsCStr(this->getClassName()), StringAsCStr(target->getClassName()));
}

class guictr_nodes_editor::guictr_nodes_editor_impl {
    friend class guictr_nodes_editor;
    int refreshQueued = 2;

public:
    guictr_nodes_editor_impl() = default;
};
class gui_graph::guictr_graph_impl {
public:
    std::shared_ptr<DAW::processing_graph_t> procList;
    std::vector<gui_graph_entry*> listGuis;
    std::vector<gui_graph_n*> listNodes;
    std::vector<NodeGraph::edge_t> edgeList;
    guictr_graph_impl() = default;
    enum hit_result_type {
        HIT_NONE,
        HIT_EDGE
    };
    struct hit_result {
        hit_result_type hitType = HIT_NONE;
        NodeGraph::edge_t* edge = nullptr;
        float distanceEdgeMouse = 0.0f;
    };
    hit_result hitTest(vec2 mouseLocal);
    void updateEdgeList(std::shared_ptr<DAW::processing_graph_t>&& _graph, std::vector<gui_graph_n*>&& _listNodes) {
        listNodes = _listNodes;
        procList  = _graph;
        edgeList.clear();
        std::vector<DAW::track_source_t> allSources;
        for (gui_graph_n* const graphNode : listNodes) {
            const auto* const procNode = graphNode->getProcessingNode();
            allSources.clear();
            allSources.insert(allSources.end(), procNode->pushs.cbegin(), procNode->pushs.cend()); // copy
            allSources.insert(allSources.end(), procNode->pulls.cbegin(), procNode->pulls.cend()); // copy
            if (graphNode->portsInput.empty() && !allSources.empty()) {
                log_lf(Log::L_WARN, "Node has pushs/pulls but no input ports %d\n", static_cast<int32_t>(procNode->stageId));
                continue;
            }
            for (const DAW::track_source_t& channelSrc : allSources) {
                auto it = std::find_if(graphNode->portsInput.begin(), graphNode->portsInput.end(), [dstChannelOffset=channelSrc.channel.dstChannelOffset](gui_graph_port* gn) {
                    return gn->getChannelDesc().offset == dstChannelOffset;
                });
                gui_graph_port* portInput = it != graphNode->portsInput.end() ? *it : nullptr;
                gui_graph_port* portOutput = nullptr;
                for (auto& listNode : listNodes) {
                    portOutput = NodeGraph::getOutputPort(listNode->portsOutput, channelSrc.channel);
                    if (portOutput) {
                        break;
                    }
                }
                if (portInput && portOutput) {
                    edgeList.push_back(NodeGraph::edge_t{ portInput, portOutput });
                } else {
                    log_lf(Log::L_WARN, "Did not find UI graph entry for stage %d\n", static_cast<int32_t>(procNode->stageId));
                }
            }
        }
    }

};

void gui_graph_port::dragReleaseOn(guibase* target, ivec2 mousepos) {
    auto* ptr = dynamic_cast<gui_graph_port*>(target);
    if (ptr != nullptr) {
        auto const daw = dawCtrl->getDaw();
        ThreadLock lock = daw->lockPlayThread();
        NodeGraph::connectPorts(daw, this, ptr);
        daw->onPluginsChanged();
        daw->getHost()->onTrackLayoutChange();
    }
}

// class gui_path : public guibase {
//     struct segment {
//         vec2 begin;
//         vec2 end;
//     };
//     std::vector<segment> segments;

// public:
//     gui_path() = default;
//     void render(NVGcontext* vg) override {
//         nvgSave(vg);
//         nvgRestore(vg);
//     }
//     void setFrom() {
//     }
// };
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
    impl->procList   = nullptr;
}

void gui_graph::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    const auto posIn = ivec2(parentCtrl->m_mousePos);
    const auto mouseLocal = toControlsObjectSpace(posIn, this);
    nvgSave(vg);
    nvgTranslate(vg, offset.x, offset.y);
    nvgScale(vg, scale, scale);
    nvgBeginPath(vg);
    nvgCircleFast(vg, mouseLocal.x, mouseLocal.y, 5.0f);
    nvgFillColor(vg, nvgRGBAf(1, 0, 1, 1));
    nvgFill(vg);

    const auto colAutomated = theme->getColor(GuiColor::COL_AUTOMATED);
    for (auto& edge : impl->edgeList) {

        const DAW::processing_track_node_t* nodeInput  = edge.portSrc->getNode()->getProcessingNode();


        DAW::rmsmeter* ptrMeter = nullptr;

        if (nodeInput->effectOptional) {
            ptrMeter = &nodeInput->effectOptional->meter;
        }
        if (nodeInput->trackOptional && nodeInput->trackOptional->audio) {
            ptrMeter = &nodeInput->trackOptional->audio->meter;
        }
        if (nodeInput->stage) {
            ptrMeter = &nodeInput->stage->meterInput;
        }
        if (ptrMeter && ptrMeter->getNumChannels() == 0) {
            ptrMeter = nullptr;
        }


        const guictr_graph_impl::hit_result hitResult = impl->hitTest(mouseLocal);
        NVGcolor colEdge = theme->getColor(GuiColor::COL_NODES_EDGE);
        if (hitResult.hitType == guictr_graph_impl::hit_result_type::HIT_EDGE && (&edge == hitResult.edge)) {
            colEdge = NVGcolor{ 0.45f, 0.05f, 0.45f, 1.0f };
        }

        int numPaths = 1;
        if (ptrMeter && ptrMeter->getMaxRMS() > dsp_util::GAIN_DBFLOOR) {
            numPaths++;
        }

        std::vector<vec2>& pts = edge.spline.calculateSplineVectors(edge.portDst->getPortPos(), edge.portSrc->getPortPos());
        if (pts.empty())
            return;
        for (int iPath = 0; iPath < numPaths; ++iPath) {
            NVGcolor pathColor = iPath == 0 ? colEdge : colEdgeSignal;
            float fPathLineWidth = iPath == 0 ? fLineWidth : (fLineWidth + 2.0f);
            auto it = pts.cbegin();
            nvgBeginPath(vg);
            nvgMoveTo(vg, (*it).x, (*it).y);
            for (; it != pts.cend(); ++it) {
                const auto& pt = *it;
                nvgLineTo(vg, pt.x, pt.y);
            }
            nvgStrokeColor(vg, pathColor);
            nvgStrokeWidth(vg, fPathLineWidth);
            nvgStroke(vg);

            /* for (it = pts.cbegin(); it != pts.cend(); ++it) {
                const auto& pt = *it;
                nvgBeginPath(vg);
                nvgCircle(vg, pt.x, pt.y, 1.5f);
                nvgFillColor(vg, colAutomated);
                nvgFill(vg);
            } */
        }
        if (edge_spline::_c_renderCtrls) {
            auto& ctrlPts = edge.spline.getCtrlPts();
            for (auto pt : ctrlPts) {
                nvgBeginPath(vg);
                nvgCircle(vg, pt.x, pt.y, 6.0f);
                nvgFillColor(vg, colAutomated);
                nvgFill(vg);
            }
        }
    }
    for (auto c : guis) {
        nvgSave(vg);
        c->render(vg);
        nvgRestore(vg);
    }
    for (auto& edge : impl->edgeList) {
        auto portInputPos      = edge.portDst->getPortPos();
        auto portOutputPos     = edge.portSrc->getPortPos();
        auto edgeLabelPos      = ivec2(portInputPos + vec2(portOutputPos - portInputPos) * 0.5f);
        auto procNode          = edge.portSrc->getNode()->getProcessingNode();
        setFont(vg, 14, THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        auto text = StringFormat("%zd samples", procNode->internalLatency + procNode->inputLatency);
        nvgText(vg, edgeLabelPos.x, edgeLabelPos.y, StringAsCStr(text), nullptr);
    }
    nvgRestore(vg);
}
gui_graph::guictr_graph_impl::hit_result gui_graph::guictr_graph_impl::hitTest(vec2 mouseLocal) {
    std::vector<hit_result> hit;
    float minEdgeDist = 0.0f;
    NodeGraph::edge_t* minEdge   = nullptr;
    for (auto& edge : edgeList) {
        std::vector<vec2>& pts = edge.spline.calculateSplineVectors(edge.portDst->getPortPos(), edge.portSrc->getPortPos());
        if (pts.size() < 2)
            continue;
        float edgeMouseDist = math::distancePointLine(mouseLocal, pts[0], pts[1]);
        auto it = pts.cbegin();
        for (++it; it != pts.cend(); ++it) {
            float fDist  = math::distancePointLine(mouseLocal, *(it - 1), *it);
            if (edgeMouseDist < 0 || fDist < edgeMouseDist) {
                edgeMouseDist = fDist;
            }
        }
        if (minEdge == nullptr || edgeMouseDist < minEdgeDist) {
            minEdgeDist = edgeMouseDist;
            minEdge     = &edge;
        }
        if (edgeMouseDist < 10) {
            hit.push_back({hit_result_type::HIT_EDGE, &edge, edgeMouseDist});
        }
    }
    std::sort(hit.begin(), hit.end(), [](hit_result const& a, hit_result const& b) {
        return a.distanceEdgeMouse < b.distanceEdgeMouse;
    });
    if (!hit.empty()) {
        hit_result& h = hit[0];
        if (h.distanceEdgeMouse < 10) {
            return h;
        }
    }
    return { hit_result_type::HIT_NONE, nullptr, 0.0f };
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
    float posY = 0;
public:
    explicit guinodeinfo_text(const DAW::processing_track_node_t* const _node)
        : guibase(),
          node(_node)
    {
    }
    void text(NVGcontext* vg, const String& text) {
        nvgText(vg, INSET_TITLE, posY, StringAsCStr(text), nullptr);
        posY += GRAPH_FONT_SIZE*1.1f;
    }
    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }
        this->posY = GRAPH_FONT_SIZE * 1.2f;
        setFont(vg, GRAPH_FONT_SIZE, THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        text(vg, StringFormat("Stage #%d", static_cast<int32_t>(node->stageId)));
        if (node->effectOptional) {
            text(vg, StringFormat("Global Id #%d", static_cast<int32_t>(node->effectOptional->projectGlobalId)));
        }
        text(vg, StringFormat("inputs: %d", static_cast<int32_t>(node->children.size())));
        text(vg, StringFormat("outputs: %d", static_cast<int32_t>(node->parents.size())));
        text(vg, StringFormat("Latency"));
        text(vg, StringFormat("Input: %zd", node->inputLatency));
        text(vg, StringFormat("Internal: %zd", node->internalLatency));
        if (node->trackOptional && node->trackOptional->audio) {
            float maxRmsOut = node->trackOptional->audio->meter.getMaxRMS();
            float maxRmsIn  = node->trackOptional->audio->meterInput.getMaxRMS();
            if (maxRmsIn > dsp_util::GAIN_DBFLOOR)
                nvgFillColor(vg, G_GREEN);
            text(vg, StringFormat("Input max rms: %f", maxRmsIn));
            if (maxRmsOut > dsp_util::GAIN_DBFLOOR)
                nvgFillColor(vg, G_GREEN);
            else
                nvgFillColor(vg, THEMECOL_TEXT);
            text(vg, StringFormat("Output max rms: %f", maxRmsOut));
            nvgFillColor(vg, THEMECOL_TEXT);
            auto numBlocks = node->trackOptional->audio->procStats.numBlocksProcessed;
            text(vg, StringFormat("Blocks processed: %zd", numBlocks));
        }
    }
};

void gui_graph::updateList(bool resetPositions) {
    auto const daw = dawCtrl->getDaw();
    std::shared_ptr<DAW::processing_graph_t> lastProcessingList;
    {
        module_group* groupSelected = nullptr;
        auto const host = daw->getHost();
        plugin_selection& sel = daw->getMainControl()->getPluginSel();
        if (sel.getSelectionCount() > 0) {
            std::vector<effectbase *> out;
            if (sel.pluginCtr->getSelected(out)) {
                for (auto& plugin : out) {
                    auto p = plugin->getTrackLink()->owner;
                    if (p && p->getModuleType() == PLUGIN_TYPE_GROUP) {
                        groupSelected = static_cast<module_group*>(p);
                        break;
                    }
                }
                if (out.size() && out[0]->getModuleType() == PLUGIN_TYPE_GROUP) {
                    groupSelected = static_cast<module_group*>(out[0]);
                }
            }
        }
        if (graphType == GraphType::Top) {
            if (groupSelected) {
                // get track from group
                auto track = groupSelected->getTrack();
                if (track && track->audio) {
                    std::shared_ptr<DAW::effect_processing_graph_t> effProcessingGraph;
                    if (!DAW::buildEffectProcessingGraph(host, nullptr, track->audio, effProcessingGraph)) {
                        log_lf(Log::L_ERROR, "Failed building effect graph\n");
                    } else {
                        lastProcessingList = std::move(effProcessingGraph);
                    }
                }
            } else { /* project graph */
                std::shared_ptr<DAW::processing_graph_t> processingGraph;
                auto const project = daw->getProject();
                auto tracksFlatAll = project->trackList.getAllTracksFlatVec();//TODO: get rid of copy
                if (!DAW::buildProcessingGraph(host, project, tracksFlatAll, processingGraph)) {
                    log_lf(Log::L_ERROR, "Failed building track graph\n");
                } else {
                    lastProcessingList = std::move(processingGraph);
                }
            }
        } else { /* bottom graph */
            if (groupSelected) {
                lastProcessingList = groupSelected->getLastProcessingGraph();
            } else {
                auto track = daw->getSelectedTrack();
                if (track && track->audio) {
                    std::shared_ptr<DAW::effect_processing_graph_t> effProcessingGraph;
                    if (!DAW::buildEffectProcessingGraph(host, nullptr, track->audio, effProcessingGraph)) {
                        log_lf(Log::L_ERROR, "Failed building effect graph\n");
                    } else {
                        lastProcessingList = std::move(effProcessingGraph);
                    }
                }
            }
        }
    }
    const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);

    ivec2 cs = getSizeContent();
    cs.x = math::max(400, cs.x);
    cs.y = math::max(400, cs.y);
    std::vector<gui_graph_n*> listNodes;
    std::vector<gui_graph_entry*> listEntries;
    if (lastProcessingList) {
        auto& graphLayouts    = daw->getProject()->graphLayouts;
        const auto& procGraph = *lastProcessingList;
        const auto& allNodes  = procGraph.nodesFlatOrdered;

        const auto scale      = theme->getFloat(GuiConstant::CONST_NODES_SCALE);
        const vec2 nodeSize   = GRAPH_NODE_SIZE * math::max<float>(1.0f, scale / 10.0f);
        const auto meterWidth = GRAPH_NODE_METER_WIDTH * scale;
        const vec2 gridStep   = nodeSize * vec2(1.5f, 1.2f);
        const auto inset = 8*scale;
        vec2 posGrid(inset+gridStep.x,inset);
        for (DAW::processing_track_node_t* node : allNodes) {
            auto* entry = new gui_graph_n(impl, node);
            entry->id   = static_cast<int32_t>(node->stageId);

            if (!graphLayouts.count(entry->id) || resetPositions) {
                auto nodePos = posGrid;
                if (node->parents.empty()) {
                    nodePos.y += 3 * scale;
                }
                if (node->children.empty()) {
                    nodePos.y -= 3 * scale;
                }
                bool skipStep = false;
                // if (procGraph.nodesFlatOrdered.back() == node) {
                //     nodePos = {cs.x - gridStep.x - inset, inset};
                //     skipStep = true;
                // }
                // if (procGraph.nodesFlatOrdered.front() == node) {
                //     nodePos = {inset, inset};
                //     skipStep = true;
                // }
                graphLayouts[entry->id] = graph_node_layout_t{ nodePos, nodeSize };
                if (!skipStep) {
                    if (posGrid.x + gridStep.x > cs.x) {
                        posGrid = ivec2(inset + gridStep.x, posGrid.y + gridStep.y);
                    } else {
                        posGrid.x += gridStep.x;
                    }
                }
            }
            auto const nodeLayout = &graphLayouts[entry->id];

            entry->pos    = nodeLayout->pos;
            entry->size   = nodeLayout->size;
            auto guiText  = new guinodeinfo_text{ node };
            guiText->size = { entry->size.x, entry->size.y - hpt };
            guiText->pos  = { 0, hpt };
            gui_trackmeter* meterIn = nullptr;
            gui_trackmeter* meterOut = nullptr;
            if (node->trackOptional) {
                meterOut = new gui_trackmeter(&node->trackOptional->audio->meter);
                meterIn = new gui_trackmeter(&node->trackOptional->audio->meterInput);
            }
            if (node->effectOptional) {
                meterOut = new gui_trackmeter(&node->effectOptional->meter);
                meterIn = new gui_trackmeter(&node->effectOptional->meterIn);
            }
            if (node->stage) {
                if (node->stageId == node->stage->stageId.inputStageId) {
                    meterIn = new gui_trackmeter(&node->stage->meterInput);
                } else {
                    meterOut = new gui_trackmeter(&node->stage->meter);
                }
            }
            if (meterOut) {
                meterOut->size = { meterWidth, entry->getSizeContent().y - hpt };
                meterOut->pos  = { entry->getSizeContent().x - meterOut->size.x, hpt };
                entry->add(meterOut);
                guiText->size.x -= meterOut->size.x;
            }
            if (meterIn) {
                meterIn->size = { meterWidth, entry->getSizeContent().y - hpt };
                meterIn->pos  = { 0, hpt };
                entry->add(meterIn);
                guiText->size.x -= meterIn->size.x;
                guiText->pos.x += meterIn->size.x;
            }
            if (node->type == DAW::track_node_type_t::EFFECT && node->effectOptional) {
                auto intEffect = dynamic_cast<internalplugin*>(node->effectOptional);
                if (intEffect) {
                    entry->viewCtr = intEffect->createInternalView();
                    if (entry->viewCtr) {
                        entry->viewCtr->addTo(entry->viewCtrs);
                    }
                }
            }
            for (auto* ctr : entry->viewCtrs) {
                entry->add(ctr);
            }
            if (entry->viewCtr)
                entry->viewCtr->onGuiOpen();
            int32_t insetCtrls = INSET_TITLE;
            auto layoutPos     = guiText->pos;
            for (auto* ctr : entry->viewCtrs) {
                ctr->pos          = layoutPos + ivec2(insetCtrls, insetCtrls);
                ivec2 prefSizeCtr = guiText->size - ivec2(insetCtrls * 2);
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

            listEntries.push_back(entry);
            listNodes.push_back(entry);
        }
    }
    setList(listEntries);
    impl->updateEdgeList(std::move(lastProcessingList), std::move(listNodes));
    layout();
}

void gui_graph::setList(std::vector<gui_graph_entry*> _newList) {
    for (gui_graph_entry* g : impl->listGuis) {
        remove(g);
        delete g;
    }
    impl->listGuis = std::move(_newList);
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
      scrollbar(1, 0.0f, *this) {
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
    if (isBackgroundRendered()) {
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

void guictr_nodes_editor::resetPositions() {
    impl->refreshQueued = 2;
}

void guictr_nodes_editor::resetRouting() {
    auto const daw = dawCtrl->getDaw();
    {
        module_group* groupSelected = nullptr;
        plugin_selection& sel = daw->getMainControl()->getPluginSel();
        if (sel.getSelectionCount() > 0) {
            std::vector<effectbase *> out;
            if (sel.pluginCtr->getSelected(out)) {
                for (auto& plugin : out) {
                    auto p = plugin->getTrackLink()->owner;
                    if (p && p->getModuleType() == PLUGIN_TYPE_GROUP) {
                        groupSelected = static_cast<module_group*>(p);
                        break;
                    }
                }
                if (out.size() && out[0]->getModuleType() == PLUGIN_TYPE_GROUP) {
                    groupSelected = static_cast<module_group*>(out[0]);
                }
            }
        }
        if (graph.graphType == GraphType::Top) {
            if (groupSelected) {
                // get track from group
                auto track = groupSelected->getTrack();
                if (track && track->audio) {
                    track->audio->configureDefaultRoutings();
                }
            } else { /* project graph */
            }
        } else { /* bottom graph */
            if (groupSelected) {
                auto audio = groupSelected->getTrackLink();
                if (audio) {
                    audio->configureDefaultRoutings();
                }
            } else {
                auto track = daw->getSelectedTrack();
                if (track && track->audio) {
                    track->audio->configureDefaultRoutings();
                }
            }
        }
    }
}

void guictr_nodes_editor::reset() {
    graph.reset();
}

bool guictr_nodes_editor::handleKeyInput(KeyEvent& event) {
    if (event.type != KeyEventType::K_RELEASE) {
        if (event.type == KeyEventType::K_PRESS) {
            KeyCombo kc = KC_REFRESH;
            kc.keyMod   = KB_MOD_CTRL;
            if (isKC(kc, event)) {
                resetPositions();
                return true;
            }
            if (isKC(KC_REFRESH, event)) {
                refresh();
                return true;
            }
        }
    }
    return false;
}

void guictr_nodes_editor::onTick(AppCtrl* appctrl) {
    if (impl->refreshQueued) {
        graph.updateList(impl->refreshQueued == 2);
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
            auto const daw = dawCtrl->getDaw();
            ThreadLock lock = daw->lockPlayThread();
            NodeGraph::disconnectEdge(daw, hitResult.edge);
            daw->onPluginsChanged();
            daw->getHost()->onTrackLayoutChange();
            return;
        }
    }
    prevOffset = offset;
}

void gui_graph::handleDraggedMove(MouseEvent& evt) {
    offset = prevOffset + vec2(evt.mousepos - evt.dragStart);
}

void gui_graph::handleDraggedRelease(MouseEvent& evt) {
    prevOffset = offset;
}

void gui_graph::handleRightClick(MouseEvent& evt) {
    parent->handleRightClick(evt);
}

bool gui_graph::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (yoffset) {
        float newScale = scale;
        newScale       = newScale * (1.0f + (yoffset) / 10.0f);
        if (newScale < 1 / 128.0f)
            newScale = 1 / 128.0f;
        if (newScale > 128.0f)
            newScale = 128.0f;

        ivec2 mpos   = evt.mousepos;
        ivec2 relpos = toControlsObjectSpace(mpos, parent) - getPosContent();

        vec2 mousePosCtrSpace = toContainerSpace2f(relpos);
        vec2 offsetDelta      = vec2(mousePosCtrSpace) * (newScale - scale);
        scale                 = newScale;
        /* alternatively offsetDelta can be calculated this way */
        //ivec2 mousePosCtrSpaceAfter = toContainerSpace2f(relpos);
        //vec2 offsetDelta = (mousePosCtrSpaceAfter-mousePosCtrSpace)*newScale;
        offset -= offsetDelta;
    }
    return true;
}

void guictr_nodes_editor::scrollOffsetChanged(int dir, float offset) {
    //ivec2 cs = getSizeContent() - graph.size;
    //int32_t scrOffset = math::max(0.0f, offset*(cs[dir]));
}

void guictr_nodes_editor::layout() {

    int scrollW    = gui_scrollbar::defaultW;
    ivec2 cs       = getSizeContent();
    scrollbar.pos  = ivec2(cs.x - scrollW, 0);
    scrollbar.size = ivec2(scrollW, cs.y);

    graph.pos  = { 0, 0 };
    graph.size = cs;
    graph.determineSize(graph.size);
    //double f = scrollbar.toPixels();
    //contentHeight = graph.size.y;
    //contentViewSize = cs.y;
    //scrollbar.scrollTo(f);
    //scrollOffsetChanged(1, scrollbar.scrollOffset);
    for (guibase* gui : guis) {
        gui->layout();
    }
}

guictr_nodes_splitview::guictr_nodes_splitview(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip)
    : project(_project),
      graphTop(_cursor, _project, _dragdropclip),
      graphBottom(_cursor, _project, _dragdropclip),
      splitter(0, 0.5) {
    graphBottom.graph.graphType = GraphType::Bottom;
    splitter.setMinMax(0.1f, 0.9f);
    splitter.setCallback(this);
    setCanMouseHit(true);
    add(&splitter);
    add(&graphTop);
    add(&graphBottom);
    padding = 0;
    margin  = 0;
    setBackgroundRendered(false);
}

guictr_nodes_splitview::~guictr_nodes_splitview() {
    removeGuis();
}

void guictr_nodes_splitview::onChildLayoutChanged(guibase* g) {
    layout();
}

void guictr_nodes_splitview::reset() {
    graphTop.reset();
    graphBottom.reset();
}
void guictr_nodes_splitview::refresh() {
    graphTop.refresh();
    graphBottom.refresh();
}
void guictr_nodes_splitview::buttonClicked(guibase* _button) {
    //if (parent) parent->buttonClicked(_button);
    if (_button->parent == &graphTop.graph) {
        graphBottom.refresh();
    }
    if (_button->parent == &graphBottom.graph) {
    }
}

void guictr_nodes_splitview::handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) {
    layout();
}

ivec2 guictr_nodes_splitview::getContainerSize() {
    return size;
}
void guictr_nodes_splitview::onPluginSelected() {
    refresh();
}
void guictr_nodes_splitview::layout() {

    ivec2 cs         = getSizeContent();
    auto topHeight = splitter.leftOrTop(cs.y);
    graphTop.pos  = ivec2(0);
    graphBottom.pos    = ivec2(0, topHeight);
    graphTop.size = ivec2(cs.x, topHeight);
    graphBottom.size   = ivec2(cs.x, cs.y - topHeight);
    
    splitter.pos  = ivec2(0, topHeight - Splitter::SPLITTER_LAYOUT_THICKNESS/2);
    splitter.size = ivec2(cs.x, Splitter::SPLITTER_LAYOUT_THICKNESS);
    
    for (guibase* gui : guis) {
        gui->layout();
    }
}

ivec2 gui_graph::toScreenSpace(ivec2 in) const {
    in = ivec2(vec2(getPosContent() + in) * scale + offset);
    if (this->parent != nullptr) {
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

class guictxtmenu_nodes : public guictxtmenu {
    guictr_nodes_editor* const m_nodesEditor;
    ctxtmenu_entry* cmdRefresh;
    ctxtmenu_entry* cmdResetPositions;
    ctxtmenu_entry* cmdResetRouting;
public:
    guictxtmenu_nodes(DawCtrl* _dawCtrl, guictr_nodes_editor* _nodesEditor)
        : guictxtmenu(),
          m_nodesEditor(_nodesEditor) {
        this->dawCtrl = _dawCtrl;
        this->size.x = 120;
        addEntry(cmdRefresh = new ctxtmenu_entry("Refresh", 1));
        addEntry(cmdResetPositions = new ctxtmenu_entry("Reset Positions", 2));
        addEntry(cmdResetRouting = new ctxtmenu_entry("Reset Routing", 3));
    }
    ~guictxtmenu_nodes() override = default;
    void clicked(int _id) override {
        auto const daw = dawCtrl->getDaw();
        ThreadLock lock = daw->lockPlayThread();
        if (_id == cmdRefresh->id) {
            m_nodesEditor->refresh();
        }
        if (_id == cmdResetPositions->id) {
            m_nodesEditor->resetPositions();
        }
        if (_id == cmdResetRouting->id) {
            m_nodesEditor->resetRouting();
            m_nodesEditor->refresh();
        }
        closeContextMenu();
    }
};

void guictr_nodes_editor::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_nodes(dawCtrl, this), evt.mousepos);
}
