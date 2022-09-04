#pragma once
#include <nanovg.h>
#include <vector>
#include <algorithm>
#include "types.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "str_util.h"
#include "event.h"
#include "saferef.h"
#include "guicolors.h"
#include "gui/table/table_fwd.h"

#ifndef NDEBUG
#define TRACK_ALLOCATIONS_GUIBASE
#endif

struct NVGcontext;
namespace RenderResources {
    struct NvgImageTexture;
}
class BaseCtrl;
class AppCtrl;
class DawCtrl;
class guictxtmenu_base;
class guitrack_editor;
class guiplugin;
class guictr_dragged_plugins;
class guictr_base;
class gui_pluginlist_entry;
class gui_track;
struct guitheme_t;
struct dragdrop_midifile;

extern NVGcolor dbgcolorsArray[8];
static constexpr int32_t dbgcolorsArraySize = 8;

void UTIL_setFont(NVGcontext* vg, const guitheme_t* const theme, float size, NVGcolor color, int alignment);
float textWidth(NVGcontext* vg, const String& str);
float renderTextLabel(NVGcontext* vg,
                     const vec2& pos,
                     const vec2& bounds,
                     const String& text,
                     const guitheme_t* theme,
                     const float fontSize,
                     const NVGcolor color,
                     const int32_t alignment);
vec2 getTextLabelBounds(NVGcontext* vg,
                     const vec2& pos,
                     const String& text,
                     const guitheme_t* theme,
                     const float fontSize,
                     const int32_t alignment);
void renderCenteredMultilineText(NVGcontext* vg, const guitheme_t* theme, const String& str, float fontScale, GuiColor::constant_t c, ivec2 renderPos, ivec2 size);
void renderDashedLineFrame(NVGcontext* vg, float x, float y, float w, float h, float thickness);

void drawIcon(NVGcontext* vg, const ivec2& size, RenderResources::NvgImageTexture* image, int32_t extImg = 2);
void drawIconColored(NVGcontext* vg, const ivec2& size, RenderResources::NvgImageTexture* image, NVGcolor color, int32_t extImg = 2);
void drawPlaySymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawRecordSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawCross(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawStopSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawTri(NVGcontext* vg, float xTop, float yTop, float h, const int dir, const NVGcolor& color, const NVGcolor& strokeColor, float strokeWidth);
void drawImage(NVGcontext* vg, int image, float alpha,
               float sx, float sy, float sw, float sh,// sprite location on texture
               float x, float y, float w, float h);   // position and size of the sprite rectangle on screen

guitheme_t* getDefaultTheme();
ivec2 toControlsObjectSpace(ivec2 pos, guibase* gui);

inline float calcInset(float desiredInset, float size) {
    return math::min(desiredInset, math::max(0.f, (size - 4.0f) / 2.0f));
}
enum guiflag : int32_t {
    FLG_NONE                     = 0x00000000,
    FLG_VISIBLE                  = 0x00000001,
    FLG_RENDER_BACKGROUND        = 0x00000002,
    FLG_RENDER_BACKGROUND_INSET  = 0x00000004,
    FLG_ENBL                     = 0x00000008,
    FLG_HVRD                     = 0x00000010,
    FLG_FOC                      = 0x00000020,
    FLG_ACT                      = 0x00000040,
    FLG_DRG                      = 0x00000080,
    FLG_HAS_COLOR_BG             = 0x00000100,
    FLG_CANFOCUS                 = 0x00000200,
    FLG_RENDER_DRAGGED           = 0x00000400,
    FLG_RENDER_LABEL             = 0x00000800,
    FLG_VERTICAL_LABEL           = 0x00001000,
    FLG_BG_SHADING               = 0x00002000,
    FLG_IMPL_SPEC2               = 0x00004000,
    FLG_NO_LAYOUT                = 0x00008000,
    FLG_RENDER_BUTTON_WITH_LED   = 0x00010000
};
enum guiflag_titlebar : int32_t {
    TITLEBAR_FLG_NONE = 0,
    TITLEBAR_FLG_FOCUSED = 1,
    TITLEBAR_FLG_SELECTED = 2
};
enum gui_type : uint16_t {
    GUI_TYPE_UNKNOWN = 0,
    GUI_TYPE_BUTTON,
    GUI_TYPE_KNOB,
    GUI_TYPE_SCROLLBAR,
    GUI_TYPE_TEXTFIELD,
    CTR_TYPE_UNKNOWN = 100,
    CTR_TYPE_LAYOUT,
    CTR_TYPE_PROPERTIES,
    CTR_TYPE_THEME,
    CTR_TYPE_HISTORY,
    CTR_TYPE_SHADERVIEW,
    CTR_TYPE_SETTINGS,
    CTR_TYPE_EFFECTLIBRARY,
    CTR_TYPE_PLUGINSLOADED,
    CTR_TYPE_DEBUG_0,
    CTR_TYPE_DEBUG_1,
    CTR_TYPE_DEBUG_2,
    CTR_TYPE_PERFORMANCE,
    CTR_TYPE_EXPORT,
    CTR_TYPE_CLIPEDITOR,
    CTR_TYPE_PLUGIN,
    CTR_TYPE_PLUGINS_DRAGGED,
};
namespace DebugAlloc {
    template<typename T>
    class Tracker;
}
class guibase {
private:
    int32_t flags = FLG_ENBL | FLG_VISIBLE | FLG_RENDER_BACKGROUND;
protected:
    gui_type guiType;
public:
    ivec2 pos{ 0 };
    ivec2 size{ 0 };
    int32_t id               = 0;
    int32_t zOrder           = 0;
    BaseCtrl* parentCtrl = nullptr;
    DawCtrl* dawCtrl     = nullptr;
    guibase* parent      = nullptr;
    guitheme_t* theme    = nullptr;
    SafeRef<guibase> safeRef;
    String label;
    String tooltipText;

public:
#ifdef TRACK_ALLOCATIONS_GUIBASE
    int64_t allocId = 0;
#endif
    explicit guibase(gui_type guiType = gui_type::GUI_TYPE_UNKNOWN);
    virtual ~guibase();
    guibase(const guibase& graph) = delete;
    guibase& operator=(const guibase& graph) = delete;
    guibase(guibase&& graph)                 = delete;
    guibase& operator=(guibase&& graph) = delete;
protected:
    void setFlagInternal(int flag) {
        this->flags |= flag;
    }
    void clearFlagInternal(int flag) {
        this->flags &= ~flag;
    }

public:
    SafeRef<guibase> makeSafeRef();
    gui_type getGuiType() const {
        return guiType;
    }
    virtual bool isVisible() const {
        //if (size.x < 0 || size.y < 0)
        //  return false;
        return (flags & FLG_VISIBLE) != 0;
    }
    void setVisible(bool b) {
        if (!b)
            flags &= ~FLG_VISIBLE;
        else
            flags |= FLG_VISIBLE;
    }
    int getFlags() const {
        return flags;
    }
    virtual bool isFlag(int32_t flag) const {
        return (flags & flag) != 0;
    }
    virtual bool setFlag(int32_t flag, bool b) {
        if (!b)
            flags &= ~flag;
        else
            flags |= flag;
        return (flags & flag) != 0;
    }
    virtual bool isBackgroundRendered() const {
        return (flags & FLG_RENDER_BACKGROUND) != 0;
    }
    virtual bool isDragRendered() const {
        return (flags & FLG_RENDER_DRAGGED) != 0;
    }
    void setDragRendered(bool b) {
        if (!b)
            flags &= ~FLG_RENDER_DRAGGED;
        else
            flags |= FLG_RENDER_DRAGGED;
    }
    void setBackgroundRendered(bool b) {
        if (!b)
            flags &= ~FLG_RENDER_BACKGROUND;
        else
            flags |= FLG_RENDER_BACKGROUND;
    }
    virtual bool isBackgroundRenderedInset() const {
        return (flags & FLG_RENDER_BACKGROUND_INSET) != 0;
    }
    void setBackgroundRenderedInset(bool b) {
        if (!b)
            flags &= ~FLG_RENDER_BACKGROUND_INSET;
        else
            flags |= FLG_RENDER_BACKGROUND_INSET;
    }
    virtual bool canMouseHit() const {
        return (flags & FLG_CANFOCUS) != 0;
    }
    void setCanMouseHit(bool b) {
        if (!b)
            flags &= ~FLG_CANFOCUS;
        else
            flags |= FLG_CANFOCUS;
    }
    virtual bool isEnabled() const {
        return (flags & FLG_ENBL) != 0;
    }
    void setEnabled(bool b) {
        if (!b)
            flags &= ~FLG_ENBL;
        else
            flags |= FLG_ENBL;
    }
    virtual bool hovered() const;
    virtual bool pressed() const;
    virtual bool focused() const;
    void setLabel(String _str) {
        label = std::move(_str);
    }
    String getLabel() const {
        if (label.empty()) {
            return getClassName();
        }
        return label;
    }

    String getClassName() const;

    virtual bool contains(ivec2 mpos) const {
        return mpos.x >= pos.x &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + size.x &&
               mpos.y < pos.y + size.y;
    }
    ivec2 getLeftTop() {
        return pos;
    }
    ivec2 getRightTop() {
        return pos + ivec2(size.x, 0);
    }
    ivec2 getRightBottom() {
        return pos + size;
    }
    int right() {
        return pos.x + size.x;
    }
    int top() {
        return pos.y;
    }
    int bottom() {
        return pos.y + size.y;
    }
    int left() {
        return pos.x;
    }
    void setZOrder(int _zOrder) {
        zOrder = _zOrder;
    }
    float renderText(NVGcontext* vg,
                    const vec2& pos,
                    const vec2& bounds,
                    const String& text,
                    const float fontSize    = 0.0f,
                    const int32_t alignment = 0);
    virtual void render(NVGcontext* vg) {
    }
    virtual void determineSize(ivec2& prefSize) /* const */ {
    }
    virtual void prerender(NVGcontext* vg) {
    }
    virtual void onIdle() {
    }
    virtual void onTick(AppCtrl* appctrl) {
    }
    virtual guictxtmenu_base* getTooltip(AppCtrl* appctrl);
    void setTooltipText(String _tooltipText) {
        tooltipText = std::move(_tooltipText);
    }
    const String& getTooltipText() const {
        return tooltipText.empty() ? label : tooltipText;
    }
    virtual void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset);
    virtual void layout() {
    }
    virtual void onRemove() {
    }
    virtual void onAdded() {
        if (parent) {
            setTheme(parent->theme);
        }
    }
    virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
        if (canMouseHit() && contains(mpos)) {
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    guibase* getTopParent() {
        guibase* parentGui = parent;
        while (parentGui) {
            parentGui = parentGui->parent;
        }
        return parentGui;
    }
    virtual void handleRightClick(MouseEvent& evt) {
    }
    virtual void handleDraggedBegin(MouseEvent& evt) {
    }
    virtual void handleDraggedMove(MouseEvent& evt) {
    }
    virtual void handleDraggedRelease(MouseEvent& evt) {
    }
    void handleMouseDownBegin(MouseEvent& evt);
    virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
        if (parent) {
            return parent->handleMouseScroll(evt, xoffset, yoffset);
        }
        return false;
    }
    virtual bool handleKeyInput(KeyEvent& kevt) {
        return false;
    }
    virtual bool handleCharInput(uint32_t codepoint) {
        return false;
    }
    virtual bool trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) {
        return false;
    }
    virtual void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
    }
    virtual void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
    }
    virtual void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
    }
    virtual bool clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
        return false;
    }
    virtual bool clipDropMove(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
        return false;
    }
    virtual bool clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
        return false;
    }
    virtual void clipDropCancel() {
    }
    virtual void pluginDragMove(guiplugin* g, ivec2 mousepos) {
    }
    virtual void pluginDragRelease(guiplugin* g, ivec2 mousepos) {
    }
    virtual void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) {
    }
    virtual void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) {
    }
    virtual void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) {
    }
    virtual void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) {
    }
    virtual void trackEntryDragMove(gui_track* g, ivec2 mousepos) {
    }
    virtual void trackEntryDragRelease(gui_track* g, ivec2 mousepos) {
    }
    virtual void dragBeginOn(guibase* target, ivec2 mousepos) {
    }
    virtual void dragMoveOn(guibase* target, ivec2 mousepos) {
    }
    virtual void dragReleaseOn(guibase* target, ivec2 mousepos) {
    }
    virtual void buttonClicked(guibase* button) {
    }
    virtual void rightClicked(MouseEvent& evt, guibase* button) {
    }
    /*
     * determines if drag operations should focus containers
     * when hovering target containers for short periods
     */
    virtual bool isDragMoveable() {
        return false;
    }
    virtual bool setScissorTransform(NVGcontext* vg) {
        ivec2 posInset  = pos;
        ivec2 sizeInset = size;
        if (sizeInset.y <= 0 || sizeInset.x <= 0) {
            return false;
        }
        nvgIntersectScissor(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
        nvgTranslate(vg, posInset.x, posInset.y);
        return true;
    }
    virtual void scissorClip(ivec2& vpos, ivec2& vsize) {
        ivec2 posTL = toParentSpace(vpos);
        ivec2 posBR = toParentSpace(vpos + vsize);
        vpos.x      = math::max(posTL.x, pos.x);
        vpos.y      = math::max(posTL.y, pos.y);
        vsize.x     = math::min(posBR.x, (pos + size).x) - vpos.x;
        vsize.y     = math::min(posBR.y, (pos + size).y) - vpos.y;
        if (parent) {
            parent->scissorClip(vpos, vsize);
        }
        vpos = toContainerSpace(vpos);
    }
    virtual ivec2 toParentSpace(ivec2 in) const {
        return this->pos + in;
    }
    virtual ivec2 toContainerSpace(ivec2 in) const {
        return in - this->pos;
    }
    virtual vec2 toParentSpace2f(vec2 in) const {
        return (vec2(this->pos)) + in;
    }
    virtual vec2 toContainerSpace2f(vec2 in) const {
        return in - (vec2(this->pos));
    }
    void getHierachy(std::vector<guibase*>& stack) {
        guibase* p = this->parent;
        while (p) {
            stack.push_back(p);
            p = p->parent;
        }
    }
    virtual void onChildLayoutChanged(guibase* g) {
        if (this->parent) {
            this->parent->onChildLayoutChanged(this);
        }
    }
    virtual guibase* getFocusedControl() {
        return this;
    }
    virtual guibase* getDraggedControl() {
        return this;
    }
    virtual bool focusEvent(MouseHitEvt& evt, bool focused) {
        return true;
    }
    virtual guibase* getFocusedContainer() {
        if (this->parent) {
            return this->parent->getFocusedContainer();
        }
        return nullptr;
    }
    void renderWidgetBorder(NVGcontext* vg, int32_t flags) const;
    virtual void renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const;
    virtual ivec2 toScreenSpace(ivec2 in) const {
        in += this->pos;
        if (this->parent) {
            in = this->parent->toScreenSpace(in);
        }
        return in;
    }
    virtual bool isStaticContainer() {
        return false;
    }
    virtual int32_t getStateFlags() const;

    virtual GuiColor::constant_t getBackgroundColor() const {
        return getBackgroundColorFromState(getStateFlags());
    }

    virtual GuiColor::constant_t getLabelColor() const {
        return (getStateFlags() & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
    }

    virtual GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const {
        if (!(stateflags & FLG_ENBL)) {
            return GuiColor::COL_BASE_BG_DISABLED;
        }
        if (stateflags & FLG_DRG) {
            return GuiColor::COL_BASE_BG_PRESSED;
        }
        if (stateflags & FLG_RENDER_BACKGROUND_INSET) {
            return GuiColor::COL_BG_WIDGET;
        }
        return GuiColor::COL_BASE_BG;
    }

    BaseCtrl* getControl() const {
        return parentCtrl;
    }
    virtual void setControl(BaseCtrl* parentCtrl);
    virtual void setParent(guibase* parent);
    virtual void addProperties(Table::tbl* table);

public:
    virtual bool isSelected();

protected:
    bool isChildOf(guibase* parentSearch);
    void setFont(NVGcontext* vg, float size, NVGcolor color, int alignment);
    void setTheme(guitheme_t* theme);
};
