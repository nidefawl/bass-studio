#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <glm/geometric.hpp>
#include <nanovg.h>
#include <nanovg_min.h>
#include <numeric>
#include <splines/natural_spline.h>
#include <type_traits>
#include <utility>
#include <glm/vec2.hpp>
#include <glm/gtx/norm.hpp>

#include "gui/controls/textfield.h"
#include "guicolors.h"
#include "host/automation/automation.h"
#include "basectrl.h"
#include "color_util.h"
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
#include "host/daw/mainctrl.h"
#include "host/graph/track_graph.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/group/group.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "host/plugin/modules.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "saferef.h"
#include "seq_util.h"
#include "theme.h"
#include "host/track/track_impl.h"
#include "snapshot/track-snapshot.h"
#include "host/track/track.h"
#include "host/track/track.h"
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
const float nodePortRadius = 8.0f;
const auto colEdgeSignal   = NVGcolor{ 0.1f, 0.6f, 0.1f, 1.0f };
const auto GRAPH_NODE_SIZE = vec2(260);
const auto GRAPH_FONT_SIZE = 16;

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
        if (!gui->isVisible())
            continue;
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
    // nvgSave(vg);
    // int32_t i2                      = padding * 2;
    // const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    // int32_t h                       = TRACK_HEIGHT_STEP - i2;
    // setFont(vg, G_FONT_SCALE(h), THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    // for (guibase* gui : guis) {
    //     renderText(vg, vec2(i2, gui->top() + gui->size.y * 0.5f), vec2(gui->pos.x-i2, size.y), gui->label, TRACK_HEIGHT_STEP);
    // }
    // nvgRestore(vg);
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
                    if (ref.stage.buffer == existingRef.stage.buffer
                           && ref.stage.stageRef.stageId == existingRef.stage.stageRef.stageId) {
                        if (matchSrcDstAll == 0)
                            return ref.srcChannelOffset == existingRef.srcChannelOffset;
                        if (matchSrcDstAll == 1)
                            return ref.dstChannelOffset == existingRef.dstChannelOffset;
                        return ref.srcChannelOffset == existingRef.srcChannelOffset
                                && ref.dstChannelOffset == existingRef.dstChannelOffset;
                    }
                    return false;
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
    edge_spline spline;
    SafeRef<guibase> lastSnapPort;
public:
    gui_graph_port(gui_graph_n* _parentGraphNode)
        : guibase(),
          parentGraphNode(_parentGraphNode) {
        setCanMouseHit(true);
        spline.setPerPixelSegments(1.0f/4.0f);
    }

    virtual bool isInput() = 0;
    virtual int32_t getChannelIdx() const = 0;
    virtual bool isMidi() = 0;

    vec2 getCenterPos2f() const {
        return vec2(pos) + vec2(size) * 0.5f;
    }

    gui_graph_n* getNode() {
        return this->parentGraphNode;
    }

    gui_graph_n* getNode() const {
        return this->parentGraphNode;
    }

    vec2 getPortPos() {
        return parent->parent->toParentSpace2f(parent->toParentSpace2f(getCenterPos2f()));
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
    gui_graph_port* findClosestPort(guibase* target, ivec2 mousepos);

    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
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
            auto graph = parent->parent->parent;
            dbgassert(graph);
            // render without editor scaling
            // convert to container space of parent of graph (graph applies scale + translate)
            ivec2 posSS = getPortPos();

            ivec2 destPos  = toControlsObjectSpace(mousepos, graph);


            auto guiPortSnap = dynamic_cast<gui_graph_port*>(safeRefGet(lastSnapPort));
            if (guiPortSnap) {
                ivec2 posPortSnap = guiPortSnap->getPortPos();
                destPos     = posPortSnap;
            }

            // ivec2 graphPos = graph->toScreenSpace(ivec2(0));

            if (isInput()) {
                std::swap(destPos, posSS);
            }
            std::vector<vec2>& pts = spline.calculateSplineVectors(graph->toScreenSpace(destPos), graph->toScreenSpace(posSS));
            if (pts.empty())
                return;
            nvgSave(vg);
            // nvgTranslate(vg, graphPos.x, graphPos.y);
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

class gui_graph_port_audio : public gui_graph_port {
    friend class gui_graph::guictr_graph_impl;
    stage_bufferpoint stageBufferPoint;
    DAW::channel_desc channelDesc;
public:
    gui_graph_port_audio(gui_graph_n* _parentGraphNode, stage_bufferpoint _stageBufferPoint, DAW::channel_desc _channelDesc)
        : gui_graph_port(_parentGraphNode),
          stageBufferPoint(_stageBufferPoint),
          channelDesc(_channelDesc) {
    }

    void addPropertiesTooltip(Table::tbl& table) {
        table.rows.push_back({ {channelDesc.name} });
        determine_string_width strw(parentCtrl, theme);
        auto widthLabel = strw.getStringWidth(channelDesc.name, table.rowHeight, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        table.tableWidth = widthLabel + INSET_TABLE_CELL_PADDING * 3;
    }

    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
        return new guitooltip<gui_graph_port_audio>(this);
    }

    stage_bufferpoint getBufferPoint() const {
        return stageBufferPoint;
    }

    DAW::channel_desc getChannelDesc() const {
        return this->channelDesc;
    }

    bool isInput() override {
        return getBufferPoint() == stage_bufferpoint::INPUT;
    }

    int32_t getChannelIdx() const override {
        return channelDesc.offset;
    }

    bool isMidi() override {
        return false;
    }
};

class gui_graph_port_midi : public gui_graph_port {
    const bool bIsInput;
    const int32_t channelIdx;
    friend class gui_graph::guictr_graph_impl;
public:
    gui_graph_port_midi(gui_graph_n* _parentGraphNode, bool isInput, int32_t _channelIdx)
        : gui_graph_port(_parentGraphNode),
            bIsInput(isInput),
            channelIdx(_channelIdx) {
    }

    bool isInput() override {
        return bIsInput;
    }

    int32_t getChannelIdx() const override {
        return channelIdx;
    }

    bool isMidi() override {
        return true;
    }

    void addPropertiesTooltip(Table::tbl& table) {
        auto str = StringFormat("Midi Channel %d", channelIdx);
        if (channelIdx < 0) {
            str = "Midi Channel All";
        }
        if (bIsInput) {
            str = "Input " + str;
        } else {
            str = "Output " + str;
        }
        table.rows.push_back({ {str} });
        determine_string_width strw(parentCtrl, theme);
        auto widthLabel = strw.getStringWidth(str, table.rowHeight, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        table.tableWidth = widthLabel + INSET_TABLE_CELL_PADDING * 3;
    }

    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
        return new guitooltip<gui_graph_port_midi>(this);
    }
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        auto centerPos = getCenterPos2f();
        nvgBeginPath(vg);
        nvgCircle(vg, centerPos.x, centerPos.y, nodePortRadius);
        nvgFillColor(vg, NVGcolor{0.1f, 0.1f, 0.6f, 1.0f});
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, centerPos.x, centerPos.y, nodePortRadius);
        nvgStrokeColor(vg, theme->getFrameColorOutline());
        nvgStrokeWidth(vg, 2.0);
        nvgStroke(vg);
        auto tl = toScreenSpace(getLeftTop());
        auto br = toScreenSpace(getRightBottom());
        auto widthHeight = br - tl;
        auto fontsize = GRAPH_FONT_SIZE;
        while (fontsize > widthHeight.x && fontsize > 4) {
            fontsize -= 1;
        }
        if (widthHeight.x >= fontsize) {
            auto str = channelIdx == -1 ? "A" : StringFormat("%d", channelIdx);
            centerPos.x += 0.5f;
            renderTextLabel(
                vg, centerPos, widthHeight, str, theme, fontsize, THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
    }
};
template<>
void guitooltip<gui_graph_port_audio>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    ptr->addPropertiesTooltip(table);
}
template<>
void guitooltip<gui_graph_port_midi>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    ptr->addPropertiesTooltip(table);
}

class guinodeinfo_text final : public guictr_base {
    const DAW::processing_track_node_t* node;
    float posY = 0;
public:
    explicit guinodeinfo_text(const DAW::processing_track_node_t* const _node)
        : guictr_base(),
          node(_node)
    {
        padding = 2;
        margin = 0;
        setBackgroundRendered(true);
        setBackgroundRenderedInset(false);
        // setCanMouseHit(true);
    }
    void setNode(const DAW::processing_track_node_t* _node) {
        node = _node;
    }
    void text(NVGcontext* vg, const String& text) {
        nvgText(vg, INSET_TITLE, posY, StringAsCStr(text), nullptr);
        posY += GRAPH_FONT_SIZE*1.1f;
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
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

class gui_graph_node_bus : public guictr_base {
protected:
    const bool bIsInput = false;
    guibuttontoggle fold;
    bool bIsFolded = false;
public:
    std::vector<gui_graph_port*> guiPorts;
    explicit gui_graph_node_bus(bool _bIsInput, bool _bIsFolded) : guictr_base(), bIsInput(_bIsInput), bIsFolded(_bIsFolded) {
        padding = 0;
        margin = 0;
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        fold.setRadius(nodePortRadius);
        fold.setStateRef(&bIsFolded);
        fold.getIcon = [this] { return bIsFolded ? (this->isInput() ? ICON_ARR_LEFT : ICON_ARR_RIGHT) : ICON_ARR_DOWN; };
        add(&fold);
        setLabel("Bus");
        setBackgroundRendered(true);
        setBackgroundRenderedInset(false);
    }
    ~gui_graph_node_bus() override {
        removeGuis();
    }

    bool isInput() {
        return bIsInput;
    }

    bool isFolded() {
        return bIsFolded;
    }

    void setFolded(bool b) {
        bIsFolded = b;
        updatePortList();
    }

    template<class T>
    void setPorts(std::vector<T> _guiPorts) {
        for (auto port : guiPorts) {
            remove(port);
            delete port;
        }
        guiPorts.clear();
        for (auto port : _guiPorts) {
            guiPorts.push_back(port);
            add(port);
        }
        updatePortList();
    }
    void determineSize(ivec2& prefSize) override {
        if (guiPorts.empty()) {
            prefSize = ivec2(0);
            return;
        }
        auto visiblePorts = std::count_if(guiPorts.begin(), guiPorts.end(), [](auto port) { return port->isVisible(); });
        prefSize.y = nodePortRadius * 3;
        if (visiblePorts > 0) {
            prefSize.y += nodePortRadius * 0.5f + visiblePorts * nodePortRadius * 3;
        }
        prefSize.x = 64;
    }
    void layout() override {
        fold.size = ivec2(nodePortRadius * 3, nodePortRadius * 3);
        fold.pos = ivec2(-fold.size.x/2, 0);
        if (!bIsInput) {
            fold.pos.x = getSizeContent().x - fold.size.x/2;
        }

        auto pos = fold.getLeftBottom();
        pos.y += nodePortRadius * 0.5f;
        int32_t i = 0;
        for (auto port : guiPorts) {
            port->size = ivec2(nodePortRadius * 2, nodePortRadius * 2);
            if (!bIsFolded) {
                port->pos = pos + ivec2(nodePortRadius * 0.5f, i * nodePortRadius * 3.0f + nodePortRadius * 0.5f);
            } else {
                port->pos = fold.pos + ivec2(nodePortRadius * 0.5f, nodePortRadius * 0.5f);
            }
            i++;
        }
        for (auto port : guis) {
            port->layout();
        }
    }
    bool setScissorTransform(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg)) {
            return false;
        }
        ivec2 posInset  = getPosContent();
        nvgTranslate(vg, posInset.x, posInset.y);
        nvgTranslateZ(vg, -4.0f);
        return true;
    }
    bool contains(ivec2 mpos) const override {
        if (mpos.x >= pos.x &&
            mpos.y >= pos.y &&
            mpos.x < pos.x + size.x &&
            mpos.y < pos.y + size.y)
            return true;
        ivec2 localPos = toContainerSpace(mpos);
        for (auto* gui : guis) {
            if (gui->isVisible() && gui->contains(localPos))
                return true;
        }
        return false;
    }

    void render(NVGcontext* vg) override {
        guictr_base::render(vg);
        const auto bgColor = getOuterBackgroundColorFromState(getStateFlags());
        auto cs   = getSizeContent();
        auto sizeText = vec2(cs.x - fold.size.x*0.5, nodePortRadius * 3);
        auto posInset = fold.getLeftTop() - ivec2(sizeText.x - 4, 0);
        if (bIsInput) {
            posInset = fold.getRightTop() - ivec2(4, 0);
        }
        // nvgBeginPath(vg);
        // nvgRect(vg, posInset.x, posInset.y, sizeText.x, sizeText.y);
        // nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
        // nvgFill(vg);
        // nvgStrokeColor(vg, theme->getFrameColorOutline());
        // nvgStrokeWidth(vg, 2.0);
        // nvgStroke(vg);

        renderTextLabel(vg,
                        vec2(posInset) + sizeText * 0.5f,
                        sizeText,
                        label,
                        theme,
                        int(nodePortRadius * 1.8f),
                        theme->getContrastColor(bgColor),
                        NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE); 
    }
    void updatePortList() {
        for (auto port : guiPorts) {
            port->setVisible(!bIsFolded);
        }
    }

    void buttonClicked(guibase* button) override {
        if (button == &fold) {
            bIsFolded = !bIsFolded;
            updatePortList();
        }
        if (parent) parent->buttonClicked(this);
    }
};

void renderBusPortMeter(NVGcontext *vg, guitheme_t *theme, const vec2 &pos, const vec2 &size, DAW::rmsmeter *meter) {
    const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
    const auto NCHANNELS = meter->getNumChannels();
    vec2 mtrPos  = pos;
    vec2 mtrSize = {size.x - CONST_PADDING_TRACK_CONTROLS, size.y};
    const auto scaledZero = dsp_util::scaledRange(0, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);

    const auto hZero = (1.0f - scaledZero) * mtrSize.x;
    const auto xZero = mtrPos.x + mtrSize.x - hZero;
    auto y           = mtrPos.y;
    auto channelH    = (mtrSize.y - (NCHANNELS - 1) * CONST_PADDING_TRACK_CONTROLS) / (float) NCHANNELS;

    float mixedlevels[3]    = { 0, 0, 0 };
    for (channelnum_t i = 0; i < NCHANNELS; i++) {
        auto chLvl      = meter->getChannelLvls(i);
        float fMax      = chLvl.fMax;
        float fRms      = chLvl.fLvl;
        float fPeak     = chLvl.fPeak;
        mixedlevels[0]  = math::max(mixedlevels[0], fMax);
        mixedlevels[1]  = fRms;
        mixedlevels[2]  = math::max(mixedlevels[2], fPeak);
        float levels[3] = { fMax, fRms, fPeak };
        //    float levels[3] = {fMax, fRms, fPeak};
        if (mtrSize.x > 4 && channelH >= 1.0f) {
            nvgBeginPath(vg);
            nvgRect(vg, mtrPos.x, y, mtrSize.x, channelH);
            nvgFillColor(vg, theme->getFrameColorOutline());
            nvgFill(vg);
            NVGcolor colGainLvl[6] = {
                G_GREEN_DRK,
                G_YELLOW_DRK,
                G_GREEN,
                G_YELLOW,
                G_GREEN_DRKER,
                G_YELLOW_DRKER,
            };
            for (int j = 0; j < 3; j++) {
                auto fLvl = levels[j];
                if (fLvl < math::F_MIN) {
                    continue;
                }
                auto scale = dsp_util::scaledRange(dsp_util::dBFS(fLvl), dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
                auto hVal = (1.0f - scale) * mtrSize.x;
                auto x = mtrPos.x;
                if (j == 2) {
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, x + hVal, y);
                    nvgLineTo(vg, x + hVal, y + channelH);
                    //int32_t col = fLvl >= 1.0f ? 1 : 0;
                    int32_t col = x < xZero ? 1 : 0;
                    nvgStrokeColor(vg, colGainLvl[j * 2 + col]);
                    nvgStrokeWidth(vg, 1.5f);
                    nvgStroke(vg);
                    continue;
                }
                if (hVal > 0.5) {
                    auto hOvershoot = math::max(0.0f, hVal - hZero);
                    nvgBeginPath(vg);
                    nvgRect(vg, x, y, math::min(hVal, hZero), channelH);
                    nvgFillColor(vg, colGainLvl[j * 2 + 0]);
                    nvgFillCustomPar(vg, -3);
                    nvgFill(vg);
                    if (hOvershoot > 0) {
                        nvgBeginPath(vg);
                        nvgRect(vg, hZero, y, hOvershoot, channelH);
                        nvgFillCustomPar(vg, -3);
                        nvgFillColor(vg, colGainLvl[j * 2 + 1]);
                        nvgFill(vg);
                    }
                }
            }
        }


        y += channelH;
        y += CONST_PADDING_TRACK_CONTROLS;
    }
}

class gui_graph_node_bus_audio : public gui_graph_node_bus {
    DAW::rmsmeter* meter = nullptr;
public:
    gui_graph_node_bus_audio(DAW::processing_track_node_t* node, bool _bIsInput, bool _bIsFolded)
        : gui_graph_node_bus(_bIsInput, _bIsFolded) {
        setMeterFromNode(node);
    }
    void setMeterFromNode(DAW::processing_track_node_t* node) {
        if (node->trackOptional) {
            meter = bIsInput ? &node->trackOptional->audio->meterInput : &node->trackOptional->audio->meter;
        }
        if (node->effectOptional) {
            meter = bIsInput ? &node->effectOptional->meterIn : &node->effectOptional->meter;
        }
        if (node->stage) {
            if (node->stageId == node->stage->stageId.inputStageId) {
                meter = &node->stage->meterInput;
            } else {
                meter = &node->stage->meter;
            }
        }
    }
    void render(NVGcontext* vg) override {
        gui_graph_node_bus::render(vg);
        if (meter) {
            auto meterPos = vec2{};
            auto meterSize = vec2{};
            if (!bIsFolded && guiPorts.size() > 0) {
                // auto posFirstPort = guiPorts.front()->getLeftTop();
                // auto posLastPort  = guiPorts.back()->getRightBottom();

                // meterSize = vec2(size.x - nodePortRadius * 2.0f, posLastPort.y - posFirstPort.y);
                // if (bIsInput) {
                //     meterPos = vec2(guiPorts.front()->getRightTop());
                //     meterPos.x += nodePortRadius * 0.5f;
                // } else {
                //     meterPos = vec2(guiPorts.front()->getLeftTop()) - vec2(meterSize.x, 0);
                //     meterPos.x -= nodePortRadius * 0.5f;
                // }
                // meterPos -= vec2(0, nodePortRadius * 0.5f);
                // meterSize.y += nodePortRadius * 1.0f;
                // nvgBeginPath(vg);
                // int inset = 2;
                // nvgRect(vg, meterPos.x + inset, meterPos.y + inset, meterSize.x - inset * 2, meterSize.y - inset * 2);
                // auto c = theme->getColor(GuiColor::COL_TEXT);
                // c.a = 0.5f;
                // nvgFillColor(vg, c);
                // nvgFill(vg);
                // nvgStrokeColor(vg, theme->getFrameColorOutline());
                // nvgStrokeWidth(vg, 2.0);
                // nvgStroke(vg);
                // renderBusPortMeter(vg, theme, meterPos, meterSize, meter);

                meterSize = vec2(size.x - nodePortRadius * 2.0f, nodePortRadius * 2.5f);
                for (auto port : guiPorts) {
                    meterPos = vec2(port->getCenterPos2f()) - vec2(0, meterSize.y * 0.5f);
                    if (bIsInput) {
                        meterPos.x += nodePortRadius * 1.5f;
                    } else {
                        meterPos.x -= meterSize.x + nodePortRadius * 1.5f;
                    }
                    auto desc = static_cast<gui_graph_port_audio*>(port)->getChannelDesc();
                    if (desc.offset + desc.count <= meter->getNumChannels()) {
                        auto subMeter = meter->getSubChannelMeter(desc.offset, desc.count);
                        renderBusPortMeter(vg, theme, meterPos, meterSize, &subMeter);
                    }
                }
            }
        }
    }
};

class gui_graph_n final : public gui_graph_entry {
    friend class gui_graph;
    friend class gui_graph_port;
    gui_graph::guictr_graph_impl* const graphImpl;
    DAW::processing_track_node_t* node;
    std::vector<gui_graph_node_bus*> guiBusses;
    std::vector<gui_graph_port_audio*> portsInput;
    std::vector<gui_graph_port_audio*> portsOutput;
    std::vector<gui_graph_port_midi*> portsMidiInput;
    std::vector<gui_graph_port_midi*> portsMidiOutput;
    gui_graph_node_bus_audio busInput;
    gui_graph_node_bus_audio busOutput;
    gui_graph_node_bus busMidiInput;
    gui_graph_node_bus busMidiOutput;
    std::shared_ptr<PluginViewContainer> viewCtr;
    guinodeinfo_text guiText;
    /* holds guictrs of internal vstplugins with custom gui (non-steinberg api) */
    std::vector<guictr_base*> viewCtrs;
    bool wasDragReleaseOnGuiCtrNodes = false;

private:
    void setPorts() {
        portsInput.clear();
        portsOutput.clear();
        portsMidiInput.clear();
        portsMidiOutput.clear();

        if (node->type == DAW::track_node_type_t::EFFECT) {
            for (auto& desc : node->effectOptional->inputChannelsDesc) {
                portsInput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::INPUT, desc});
            }
            for (auto& desc : node->effectOptional->outputChannelsDesc) {
                portsOutput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::OUTPUT_POST, desc});
            }
        } else if (node->type == DAW::track_node_type_t::TRACK) {
            portsInput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::INPUT, DAW::channel_desc{0, 2, "Stereo Input 0"}});
            portsInput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::INPUT, DAW::channel_desc{2, 2, "Stereo Input 1"}});
            portsOutput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::OUTPUT_POST, DAW::channel_desc{0, 2, "Stereo Output 0"} });
            portsMidiInput.push_back(new gui_graph_port_midi{ this, true, -1 });
            for (int32_t i = 0; i < 16;  ++i) {
                portsMidiInput.push_back(new gui_graph_port_midi{ this, true, i });
            }
            portsMidiOutput.push_back(new gui_graph_port_midi{ this, false, -1 });
            for (int32_t i = 0; i < 16;  ++i) {
                portsMidiOutput.push_back(new gui_graph_port_midi{ this, false, i });
            }
        } else if (node->type == DAW::track_node_type_t::AUDIOSTAGE) {
            if (node->stageId == node->stage->stageId.inputStageId) {
                portsOutput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::OUTPUT, DAW::channel_desc{0, 2, "Stereo Input 0"}});
                portsOutput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::OUTPUT, DAW::channel_desc{2, 2, "Stereo Input 1"}});
            } else {
                portsInput.push_back(new gui_graph_port_audio{ this, stage_bufferpoint::INPUT, DAW::channel_desc{0, 2, "Stereo Output 0"} });
            }
        }
        busMidiInput.setPorts(portsMidiInput);
        busMidiOutput.setPorts(portsMidiOutput);
        busInput.setPorts(portsInput);
        busOutput.setPorts(portsOutput);
        for (auto bus : guiBusses) {
            bus->setVisible(!bus->guiPorts.empty());
        }
    }

public:
    gui_graph_n(gui_graph::guictr_graph_impl* _graphImpl, DAW::processing_track_node_t* _node)
        : gui_graph_entry(),
          graphImpl(_graphImpl),
          node(_node),
          busInput(_node, true, false),
          busOutput(_node, false, false),
          busMidiInput(true, true),
          busMidiOutput(false, true),
          guiText(_node) {
        add(&guiText);
        setPorts();
        (void)graphImpl;
        busInput.setLabel("Audio");
        busOutput.setLabel("Audio");
        busMidiInput.setLabel("Midi");
        busMidiOutput.setLabel("Midi");
        guiBusses.push_back(&busInput);
        guiBusses.push_back(&busOutput);
        guiBusses.push_back(&busMidiInput);
        guiBusses.push_back(&busMidiOutput);
        for (auto port : guiBusses) {
            add(port);
        }
    }

    ~gui_graph_n() override {
        for (auto bus : guiBusses) {
            bus->removeGuis();
            remove(bus);
        }
        for (auto bus : {&busMidiInput, &busMidiOutput}) {
            for (auto port : bus->guiPorts) {
                delete port;
            }
        }
        for (auto bus : {&busInput, &busOutput}) {
            for (auto port : bus->guiPorts) {
                delete port;
            }
        }
        for (auto ctr : viewCtrs) {
            remove(ctr);
        }
        remove(&guiText);
        destroyGuis();
        if (viewCtr) {
            viewCtr->onGuiClose();
            viewCtr->setFree();
        }
    }

    void setBusFoldState(uint8_t state) {
        busInput.setFolded(!(state & 1));
        busOutput.setFolded(!(state & 2));
        busMidiInput.setFolded(!(state & 4));
        busMidiOutput.setFolded(!(state & 8));
    }

    const DAW::processing_track_node_t* getProcessingNode() const {
        return node;
    }

    DAW::processing_track_node_t* getProcessingNodePointer() {
        return node;
    }

    void setNode(DAW::processing_track_node_t* _node) {
        node = _node;
        busInput.setMeterFromNode(node);
        busOutput.setMeterFromNode(node);
        guiText.setNode(_node);
        setPorts();
    }

    void layout() override {
        const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        auto overlap = math::max(busInput.size.x, busOutput.size.x) - nodePortRadius;
        busInput.pos = ivec2(-padding + overlap, 0) + ivec2(0 - busInput.size.x, hpt);
        busOutput.pos = ivec2(-padding - overlap, 0) + ivec2(size.x, hpt);
        busMidiInput.pos = busInput.getLeftBottom() + ivec2(0, padding);
        busMidiOutput.pos = busOutput.getLeftBottom() + ivec2(0, padding);
        std::map<int32_t, graph_node_layout_t>& graphLayouts = dawCtrl->getDaw()->getProject()->graphLayouts;
        if (graphLayouts.count(id)) {
            uint8_t busState = 0;
            if (!busInput.isFolded()) {
                busState |= 1;
            }
            if (!busOutput.isFolded()) {
                busState |= 2;
            }
            if (!busMidiInput.isFolded()) {
                busState |= 4;
            }
            if (!busMidiOutput.isFolded()) {
                busState |= 8;
            }
            graphLayouts[id].busState = busState;
        }

        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void determineSize(ivec2& prefSize) override {
        prefSize.y = math::max<int32_t>(prefSize.y, theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT));
        for (auto bus : guiBusses) {
            bus->size = { 0, 0};
            bus->determineSize(bus->size);
        }
        auto overlap = busInput.size.x - nodePortRadius;
        const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        const auto ctrPadding = (paddingTL(padding) + paddingBR(padding));
        auto ecs = prefSize - ctrPadding;
        guiText.size = { 140, ecs.y - hpt };
        guiText.pos  = { overlap, hpt };
        prefSize.x = math::max<int32_t>(prefSize.x, overlap + guiText.right() + ctrPadding.x);
        if (viewCtr) {
            ivec2 sizeCtr(0);
            viewCtr->getFixedSize(&sizeCtr.x, &sizeCtr.y);
            sizeCtr.x = (int) ((sizeCtr.x / (float) sizeCtr.y) * (ecs.y - hpt));
            sizeCtr.y = (ecs.y - hpt);
            viewCtr->layout(sizeCtr.x, sizeCtr.y);
            for (auto* viewCtrChild : viewCtrs) {
                viewCtrChild->pos = guiText.getRightTop();
                prefSize.x = math::max(prefSize.x, ctrPadding.x+viewCtrChild->right());
            }
        }
        for (auto bus : guiBusses) {
            bus->layout();
            prefSize.y = math::max<int32_t>(prefSize.y, bus->getRightBottom().y);
        }
        prefSize.y += (paddingTL(padding) + paddingBR(padding)).y;
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        gui_graph_entry::handleDraggedBegin(evt);
        if (node->trackOptional)
            dawCtrl->setSelectedTrack(node->trackOptional);
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
        if (!isRenderableSizeAndContext(vg))
            return;
        if (parentCtrl && parentCtrl->getGuiDraggedRef() == toRef()) {
            nvgGlobalAlpha(vg, 0.5f);
        }
        gui_graph_entry::render(vg);
        if (parentCtrl && parentCtrl->getGuiDraggedRef() == toRef()) {
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
    bool getChannelRef(const gui_graph_port_audio* port, const bool isSrc, DAW::channel_ref_t& ref) {
        auto procNode = port->getNode()->getProcessingNode();
        switch (procNode->type) {
            case DAW::track_node_type_t::TRACK:
                dbgassert(procNode->trackOptional);
                if (procNode->trackOptional) {
                    dbgassert(procNode->trackOptional->audio);
                    DAW::channel_desc dstDesc = port->getChannelDesc();
                    ref = DAW::ChannelStage(procNode->trackOptional->audio, stage_bufferpoint::OUTPUT_POST, 0, dstDesc.offset);
                    return true;
                }
                break;
            case DAW::track_node_type_t::AUDIOSTAGE:
                dbgassert(procNode->stage);
                if (procNode->stage) {
                    if (procNode->stage->stageId.inputStageId == procNode->stageId) {
                        DAW::channel_desc srcDesc = port->getChannelDesc();
                        ref = DAW::ChannelStage(procNode->stage, stage_bufferpoint::INPUT, srcDesc.offset, 0);
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

    gui_graph_port* getOutputPort(const std::vector<gui_graph_port_audio*>& portsOutput, const DAW::channel_ref_t& channelRef) {
        DAW::channel_ref_t refTmp;
        auto it = std::find_if(portsOutput.begin(), portsOutput.end(), [&refTmp, &channelRef](gui_graph_port_audio* gn) {
            if (NodeGraph::getChannelRef(gn, true, refTmp)) {
                return DAW::channelRefEquals(channelRef, refTmp, 0);
            }
            return false;
        });
        return it != portsOutput.end() ? *it : nullptr;
    }
    
class action_modify_track_routing final : public action_base {
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
        }
    }
};
class action_modify_stage_routing final : public action_base {
    audio_stage_ref_t ref;
    track_effect_routing_snapshot_t snapshotBefore;
    track_effect_routing_snapshot_t snapshotAfter;
public:
    action_modify_stage_routing(audio_stage_ref_t _ref, track_effect_routing_snapshot_t _snapshot)
        : ref(_ref), snapshotBefore(_snapshot) {
        desc = "Modify effect routing";
    }
    void undo(DawInstance* daw) override {
        auto stage = daw->getPluginManager()->getAudioStage(ref);
        if (!stage) {
            setError("Failed undoing routing change");
        } else {
            snapshotAfter = {};
            stage->createRoutingSnapshot(snapshotAfter);
            stage->loadRoutingSnapshot(snapshotBefore);
            daw->onPluginsChanged();
        }
    }
    void redo(DawInstance* daw) override {
        auto stage = daw->getPluginManager()->getAudioStage(ref);
        if (!stage) {
            setError("Failed undoing routing change");
        } else {
            snapshotBefore = {};
            stage->createRoutingSnapshot(snapshotBefore);
            stage->loadRoutingSnapshot(snapshotAfter);
            daw->onPluginsChanged();
        }
    }
};
    bool connectAudioPorts(DawInstance* daw, gui_graph_port_audio* portDst, gui_graph_port_audio* portSrc) {
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
            if (nodeSrc->type != nodeDest->type) {
                return false;
            }
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
                    removeRouting(nodeDest->stage->postEffectRouting, refSrc, true);
                    nodeDest->stage->postEffectRouting.push_back(refSrc);
                    nodeDest->stage->routingState = audiostagerouting_state_t::CUSTOM;
                    break;
                case DAW::track_node_type_t::EFFECT:
                    dbgassert(nodeDest->effectOptional);
                    nodeDest->effectOptional->getTrackLink()->createRoutingSnapshot(snapshot);
                    stageRef = nodeDest->effectOptional->getTrackLink()->toRef();
                    refSrc.dstChannelOffset = portDst->getChannelIdx();
                    removeRouting(nodeDest->effectOptional->inputChannels, refSrc, true);
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
    bool disconnectAudioEdge(DawInstance* daw, gui_graph_port_audio* portSrc, gui_graph_port_audio* portDst) {
        auto nodeSrc = portSrc->getNode()->getProcessingNodePointer();
        auto nodeDest = portDst->getNode()->getProcessingNodePointer();
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
        if (getChannelRef(portSrc, true, refSrc)) {
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
                    refSrc.dstChannelOffset = portDst->getChannelIdx();
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
    bool disconnectMidiEdge(DawInstance* daw, gui_graph_port_midi* portSrc, gui_graph_port_midi* portDst) {
        auto nodeSrc = portSrc->getNode()->getProcessingNodePointer();
        auto nodeDest = portDst->getNode()->getProcessingNodePointer();
        if (nodeSrc->type == DAW::track_node_type_t::TRACK || nodeDest->type == DAW::track_node_type_t::TRACK) {
            auto stage = nodeDest->trackOptional->audio;
            track_io_configuration_snapshot_t snapshot;
            stage->createIOSnapshot(snapshot);
            auto action = new action_modify_track_routing(stage->toRef(), snapshot);
            for (auto it = stage->midiInputChannels.begin(); it != stage->midiInputChannels.end(); ++it) {
                if (it->getType() == DAW::midistage_type::INPUT_AUDIOSTAGE && it->stage.stageRef.stageId == nodeSrc->stageId) {
                    if (it->srcChannel == portSrc->getChannelIdx() && it->dstChannel == portDst->getChannelIdx()) {
                        stage->midiInputChannels.erase(it);
                        break;
                    }
                }
            }
            daw->pushHist(action);
            return true;
        }
        return false;
    }
    bool connectMidiPorts(DawInstance* daw, gui_graph_port_midi* portSrc, gui_graph_port_midi* portDst) {
        /* allow no connection to self */
        if (portDst == portSrc) { // TODO: check at lower level
            return false;
        }
        /* only allow input to output connections */
        auto isInputDst = portDst->isInput();
        auto isInputSrc = portSrc->isInput();
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
            if (nodeSrc->type != nodeDest->type) {
                return false;
            }
            if (nodeDest->trackOptional && nodeDest->trackOptional->audio) {
                track_io_configuration_snapshot_t snapshot;
                nodeDest->trackOptional->audio->createIOSnapshot(snapshot);
                auto action = new action_modify_track_routing(nodeDest->trackOptional->audio->toRef(), snapshot);
                auto srcStage = nodeSrc->trackOptional->audio;
                auto dstStage = nodeDest->trackOptional->audio;
                if (dstStage->midiInputChannels.size() == 1 && (dstStage->midiInputChannels.front().getType() == DAW::midistage_type::INPUT_EMPTY || dstStage->midiInputChannels.front().getType() == DAW::midistage_type::INPUT_DEFAULT)) {
                    dstStage->midiInputChannels.clear();
                }
                dstStage->midiInputChannels.push_back(DAW::MidiChannelStage(srcStage, stage_bufferpoint::OUTPUT_POST, portSrc->getChannelIdx(), portDst->getChannelIdx()));
                daw->pushHist(action);
                return true;
            }
        }

        return false;
    }
    bool disconnectEdge(DawInstance* daw, edge_t* edge) {
        if (!edge->portSrc->isMidi() && !edge->portDst->isMidi()) {
            if (NodeGraph::disconnectAudioEdge(daw, static_cast<gui_graph_port_audio*>(edge->portSrc), static_cast<gui_graph_port_audio*>(edge->portDst))) {
                daw->onPluginsChanged();
                return true;
            }
        }
        if (edge->portSrc->isMidi() && edge->portDst->isMidi()) {
            if (NodeGraph::disconnectMidiEdge(daw, static_cast<gui_graph_port_midi*>(edge->portSrc), static_cast<gui_graph_port_midi*>(edge->portDst))) {
                daw->onPluginsChanged();
                return true;
            }
        }
        return false;
    }
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
    void updateEdgeList() {
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
                    return gn->getChannelIdx() == dstChannelOffset;
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
                    edgeList.emplace_back(portInput, portOutput);
                } else {
                    // to be expected when a node has no connections
                    // log_lf(Log::L_WARN, "Did not find UI graph entry for stage %d\n", static_cast<int32_t>(procNode->stageId));
                }
            }
            if (procNode->type == DAW::track_node_type_t::TRACK && procNode->trackOptional) {
                auto& stage = procNode->trackOptional->audio;
                if (stage) {
                    for (auto& midiInputChannel : stage->midiInputChannels) {
                        gui_graph_port* portInput = nullptr;
                        if (midiInputChannel.dstChannel < 0) {
                            portInput = graphNode->portsMidiInput[0];
                        } else if (1 + midiInputChannel.dstChannel < int32_t(graphNode->portsMidiInput.size())) {
                            portInput = graphNode->portsMidiInput[1 + midiInputChannel.dstChannel];
                        }
                        if (!portInput) {
                            continue;
                        }
                        switch (midiInputChannel.getType()) {
                            case DAW::midistage_type::INPUT_DEFAULT:
                            case DAW::midistage_type::INPUT_EMPTY:
                            case DAW::midistage_type::INPUT_EXTERNAL_MIDI:
                                break;
                            case DAW::midistage_type::INPUT_AUDIOSTAGE: {
                                for (auto& listNode : listNodes) {
                                    auto out = listNode->getProcessingNode();
                                    if (out->trackOptional && out->trackOptional->getStage()->stageId.stageId == midiInputChannel.stage.stageRef.stageId) {
                                        gui_graph_port* portOutput = nullptr;
                                        if (midiInputChannel.srcChannel < 0) {
                                            portOutput = listNode->portsMidiOutput[0];
                                        } else if (1 + midiInputChannel.srcChannel < int32_t(listNode->portsMidiOutput.size())) {
                                            portOutput = listNode->portsMidiOutput[1 + midiInputChannel.srcChannel];
                                        }
                                        if (portOutput) {
                                            edgeList.emplace_back(portInput, portOutput);
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }

            }
        }
    }

};

gui_graph_port* gui_graph_port::findClosestPort(guibase* target, ivec2 mousepos) {
    gui_graph_port* portClosest = dynamic_cast<gui_graph_port*>(target);
    if (portClosest == nullptr) {
        auto* ptrGuiGraph = dynamic_cast<gui_graph*>(target);
        while (ptrGuiGraph == nullptr && target->parent != nullptr) {
            target = target->parent;
            ptrGuiGraph = dynamic_cast<gui_graph*>(target);
        }
        // find closest port
        if (ptrGuiGraph != nullptr) {
            auto mousePosParent = vec2(toControlsObjectSpace(mousepos, ptrGuiGraph));
            auto const& listNodes = ptrGuiGraph->getImpl()->listNodes;
            float distanceClosest = 0.0f;
            const float maxDistance = 64.0f;
            for (auto const& node : listNodes) {
                for (auto const& bus : node->guiBusses) {
                    for (auto const& port : bus->guiPorts) {
                        auto distance = math::distvec2(port->getPortPos(), mousePosParent);
                        if (distance < maxDistance && (portClosest == nullptr || distance < distanceClosest)) {
                            portClosest = port;
                            distanceClosest = distance;
                        }
                    }
                }
            }
        }
    }
    if (portClosest) {
        auto nodeSrc = this->getNode()->getProcessingNode();
        auto nodeDest = portClosest->getNode()->getProcessingNode();
        if (nodeSrc->type == DAW::track_node_type_t::TRACK || nodeDest->type == DAW::track_node_type_t::TRACK) {
            if (nodeSrc->type != nodeDest->type) {
                return nullptr;
            }
        }
    }
    return portClosest;
}

void gui_graph_port::dragMoveOn(guibase* target, ivec2 mousepos) {
    auto port = findClosestPort(target, mousepos);
    lastSnapPort = {};
    if (port) {
        lastSnapPort = port->toRef();
    }
}

void gui_graph_port::dragReleaseOn(guibase* target, ivec2 mousepos) {
    gui_graph_port* port = findClosestPort(target, mousepos);
    if (port) {
        auto const daw = dawCtrl->getDaw();
        ThreadLock lock = daw->lockPlayThread();
        if (port->isMidi() && this->isMidi()) {
            NodeGraph::connectMidiPorts(daw, static_cast<gui_graph_port_midi*>(port), static_cast<gui_graph_port_midi*>(this));
            daw->onPluginsChanged();
        } else if (!port->isMidi() && !this->isMidi()) {
            NodeGraph::connectAudioPorts(daw, static_cast<gui_graph_port_audio*>(port), static_cast<gui_graph_port_audio*>(this));
            daw->onPluginsChanged();
        }
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
    impl->edgeList.clear();
    removeGuis();
    impl->procList   = nullptr;
    updateList(false);
}
void gui_graph::reset() {
    impl->edgeList.clear();
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


        gui_graph_port_audio* portAudio = nullptr;
        if (!edge.portSrc->isMidi()) {
            portAudio = static_cast<gui_graph_port_audio*>(edge.portSrc);

        }
        int numPaths = 1;
        if (portAudio) {
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


            if (ptrMeter) {
                auto channelDesc = portAudio->getChannelDesc();
                auto meter = ptrMeter->getSubChannelMeter(channelDesc.offset, channelDesc.count);
                if (meter.getMaxRMS() > dsp_util::GAIN_DBFLOOR) {
                    numPaths++;
                }
            }
        }
        const guictr_graph_impl::hit_result hitResult = impl->hitTest(mouseLocal);
        NVGcolor colEdge = theme->getColor(GuiColor::COL_NODES_EDGE);
        if (hitResult.hitType == guictr_graph_impl::hit_result_type::HIT_EDGE && (&edge == hitResult.edge)) {
            colEdge = NVGcolor{ 0.45f, 0.05f, 0.45f, 1.0f };
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
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
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

void gui_graph::updateList(bool resetPositions) {
    if (!dawCtrl) {
        return;
    }
    auto const daw = dawCtrl->getDaw();
    std::shared_ptr<DAW::processing_graph_t> lastProcessingList;
    {
        module_group* groupSelected = nullptr;
        auto const host = daw->getHost();
        plugin_selection& sel = dawCtrl->getPluginSel();
        if (sel.getSelectionCount() > 0) {
            std::vector<effectbase *> out;
            if (sel.pluginCtr->getSelected(out)) {
                for (auto& plugin : out) {
                    auto p = plugin->getTrackLink()->owner;
                    if (p && p->getModuleType() == MODULE_TYPE_INTERNAL_EFFECT && static_cast<internalplugin*>(p)->getPluginType() == PLUGIN_TYPE_GROUP) {
                        groupSelected = static_cast<module_group*>(p);
                        break;
                    }
                }
                if (out.size() && out[0]->getModuleType() == MODULE_TYPE_INTERNAL_EFFECT && static_cast<internalplugin*>(out[0])->getPluginType() == PLUGIN_TYPE_GROUP) {
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
                if (track)
                    setLabel(track->name + " " +groupSelected->getAutomatableName());
            } else { /* project graph */
                std::shared_ptr<DAW::processing_graph_t> processingGraph;
                auto const project = daw->getProject();
                auto tracksFlatAll = project->trackList.getAllTracksFlatVec();//TODO: get rid of copy
                setLabel("Project");
                if (!DAW::buildProcessingGraph(host, project, tracksFlatAll, processingGraph)) {
                    log_lf(Log::L_ERROR, "Failed building track graph\n");
                } else {
                    lastProcessingList = std::move(processingGraph);
                }
            }
        } else { /* bottom graph */
            if (groupSelected) {
                lastProcessingList = groupSelected->getLastProcessingGraph();
                auto track = groupSelected->getTrack();
                if (track)
                    setLabel(track->name + " " +groupSelected->getAutomatableName());
            } else {
                auto track = dawCtrl->getSelectedTrack();
                if (track && track->audio) {
                    std::shared_ptr<DAW::effect_processing_graph_t> effProcessingGraph;
                    if (!DAW::buildEffectProcessingGraph(host, nullptr, track->audio, effProcessingGraph)) {
                        log_lf(Log::L_ERROR, "Failed building effect graph\n");
                    } else {
                        lastProcessingList = std::move(effProcessingGraph);
                    }
                }
                if (track)
                    setLabel(track->name);
            }
        }
    }

    ivec2 cs = getSizeContent();
    cs.x = math::max(400, cs.x);
    cs.y = math::max(400, cs.y);
    std::vector<gui_graph_n*> tmpListNew;
    std::vector<gui_graph_n*> tmpListPrev = impl->listNodes;
    if (lastProcessingList) {
        auto& graphLayouts    = daw->getProject()->graphLayouts;
        const auto& procGraph = *lastProcessingList;
        const auto& allNodes  = procGraph.nodesFlatOrdered;

        const auto scale      = theme->getFloat(GuiConstant::CONST_NODES_SCALE);
        const vec2 nodeSize   = GRAPH_NODE_SIZE * math::max<float>(1.0f, scale / 10.0f);
        const vec2 gridStep   = nodeSize * vec2(1.5f, 1.2f);
        const auto inset = 8*scale;
        vec2 posGrid(inset + gridStep.x, inset);
        for (DAW::processing_track_node_t* node : allNodes) {
            int32_t stageIdI32 = DAW::GetUnqiueProcessingNodeId(*node);
            if (!assert_expr(stageIdI32 >= 0)) {
                continue;
            }
            gui_graph_n* entry  = nullptr;
            // find matching entry in impl->listNodes and remove it
            for (auto it = tmpListPrev.begin(); it != tmpListPrev.end(); ++it) {
                if ((*it)->id == stageIdI32) {
                    entry = *it;
                    tmpListNew.push_back(entry);
                    it = tmpListPrev.erase(it);
                    break;
                }
            }
            bool bNewEntry = false;
            if (!entry) {
                bNewEntry = true;
                entry = new gui_graph_n(impl, node);
                entry->id = stageIdI32;
                if (node->type == DAW::track_node_type_t::EFFECT && node->effectOptional) {
                    auto intEffect = dynamic_cast<internalplugin*>(node->effectOptional);
                    if (intEffect) {
                        entry->viewCtr = intEffect->openViewCtr(UID_VIEW_CTR_NODES);
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
                add(entry);
                tmpListNew.push_back(entry);
            } else {
                if (!hasGui(entry)) {
                    add(entry);
                }
                entry->setNode(node);
            }
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
                bNewEntry = true;
            }
            if (bNewEntry || resetPositions) {
                auto const nodeLayout = &graphLayouts[entry->id];
                entry->pos  = nodeLayout->pos;
                entry->size = nodeLayout->size;
                entry->setBusFoldState(resetPositions ? 0x3 : nodeLayout->busState);
            }
        }
    }
    for (auto* entry : tmpListPrev) {
        remove(entry);
        delete entry;
    }
    impl->listNodes = std::move(tmpListNew);
    impl->procList  = std::move(lastProcessingList);
    impl->updateEdgeList();
    layout();
}

void gui_graph::layout() {
    auto const daw = dawCtrl->getDaw();
    auto& graphLayouts    = daw->getProject()->graphLayouts;
    for (auto* entry : impl->listNodes) {
        auto const nodeLayout = &graphLayouts[entry->id];
        entry->size = nodeLayout->size;
        entry->determineSize(entry->size);
    }
    for (guibase* gui : guis) {
        gui->layout();
    }
}

void gui_graph::setList(std::vector<gui_graph_entry*> _newList) {
}

void gui_graph::onTick(AppCtrl* appctrl) {
}

guictr_nodes_editor::guictr_nodes_editor(DAW::Cursor& _cursor, project_t& _project, dragdrop_file_clipboard& _dragdropclip)
    : guictr_base(),
      impl(new guictr_nodes_editor_impl),
      project(_project),
      graph(),
      scrollbar(1, 0.0f, *this) {
    padding = 2;
    margin  = 2;
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
    const int htt = theme->get(GuiConstant::CONST_SMALL_LABEL_HEIGHT);
    renderTextLabel(vg,
                    vec2(htt/4, cs.y - htt/4),
                    vec2(cs.x, math::min(htt, cs.y)),
                    graph.getLabel(),
                    theme,
                    htt,
                    theme->getColor(GuiColor::COL_LABEL_AUTOMATION_TRACK),
                    NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM
                    );
}

void guictr_nodes_editor::refresh() {
    graph.updateList(false);
}

void guictr_nodes_editor::resetPositions() {
    impl->refreshQueued = 2;
}

void guictr_nodes_editor::resetRouting() {
    {
        module_group* groupSelected = nullptr;
        plugin_selection& sel = dawCtrl->getPluginSel();
        if (sel.getSelectionCount() > 0) {
            std::vector<effectbase *> out;
            if (sel.pluginCtr->getSelected(out)) {
                for (auto& plugin : out) {
                    auto p = plugin->getTrackLink()->owner;
                    if (p && p->getModuleType() == MODULE_TYPE_INTERNAL_EFFECT && static_cast<internalplugin*>(p)->getPluginType() == PLUGIN_TYPE_GROUP) {
                        groupSelected = static_cast<module_group*>(p);
                        break;
                    }
                }
                if (out.size() && out[0]->getModuleType() == MODULE_TYPE_INTERNAL_EFFECT && static_cast<internalplugin*>(out[0])->getPluginType() == PLUGIN_TYPE_GROUP) {
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
                auto track = dawCtrl->getSelectedTrack();
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
    if (event.type != KeyboardState::K_RELEASE) {
        if (event.type == KeyboardState::K_PRESS) {
            KeyCombo KC_REFRESH = { 0, KeyboardKey::DAW_KB_F5, "" };
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
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
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

guictr_nodes_splitview::guictr_nodes_splitview(DAW::Cursor& _cursor, project_t& _project, dragdrop_file_clipboard& _dragdropclip)
    : project(_project),
      graphTop(_cursor, _project, _dragdropclip),
      graphBottom(_cursor, _project, _dragdropclip),
      splitter(0, 0.5) {
    setGuiType(gui_type::CTR_TYPE_NODES);
    graphBottom.graph.graphType = GraphType::Bottom;
    splitter.setMinMax(0.1f, 0.9f);
    splitter.setCallback(this);
    setCanMouseHit(true);
    add(&graphTop);
    add(&graphBottom);
    add(&splitter);
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

void guictr_nodes_splitview::onRemove() {
    reset();
    guictr_base::onRemove();
}

void guictr_nodes_splitview::onVisibleChanged(bool b) {
    if (b) {
        graphTop.refresh();
        graphBottom.refresh();
    }
}

void guictr_nodes_splitview::onAdded() {
    guictr_base::onAdded();
    graphTop.refresh();
    graphBottom.refresh();
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

class guictxtmenu_nodes final : public guictxtmenu {
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
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
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
        return true;
    }
};

void guictr_nodes_editor::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_nodes(dawCtrl, this), evt.mousepos);
}
