#pragma once
#include "math/seq_math.h"
#include "math/vec.h"
#include <nanovg.h>
#include <vector>
#include <algorithm>

#include "str_util.h"
#include "event.h"
#include "math.h"
#include "saferef.h"
#include "guicolors.h"

struct NVGcontext;
namespace Table {
struct tbl;
}
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

void UTIL_setFont(NVGcontext* vg, const guitheme_t* const theme, float size, NVGcolor color, int alignment);
float textWidth(NVGcontext* vg, const String& str);
void renderText(NVGcontext* ctx, float x, float y, float maxWidth, const char* string);
void renderCenteredMultilineText(NVGcontext* vg, const guitheme_t* const theme, const String& str, int fontScale,  GuiColor::constant_t c, ivec2 renderPos, ivec2 size);

void renderDashedLineFrame(NVGcontext* vg, float x, float y, float w, float h, float thickness);
void drawAttachedBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin);

void drawIcon(NVGcontext* vg, const ivec2& size, RenderResources::NvgImageTexture* image, int32_t extImg = 2);
void drawPlaySymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawRecordSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawCross(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawStopSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawTri(NVGcontext* vg, float xTop, float yTop, float h, const int dir, const NVGcolor& color, const NVGcolor& strokeColor, float strokeWidth);

guitheme_t* getDefaultTheme();
ivec2 toControlsObjectSpace(ivec2& pos, guibase* gui);

inline float calcInset(float desiredInset, float size) {
	return math::min(desiredInset, math::max(0.f, (size-4.0f)/2.0f));
}

enum TextInputState : signed int {
	DISABLED = 0,
	ENABLED = 1
};

#define FLAG_FOCUSED 1
#define FLAG_SELECTED 2
namespace DebugAlloc {
template<typename T>
class Tracker;
}
class guibase {
private:
	int flags = FLG_ENBL|FLG_VISIBLE|FLG_RENDER_BACKGROUND;
public:
	ivec2 pos{0};
	ivec2 size{0};
	int id = 0;
	int zOrder = 0;
	BaseCtrl* parentCtrl = nullptr;
	DawCtrl* dawCtrl = nullptr;
	guibase* parent = nullptr;
	guitheme_t* theme = nullptr;
	SafeRef<guibase> safeRef;
	String label = "";
public:
	int allocId;
	guibase();
	virtual ~guibase();
protected:
	void setFlagInternal(int flag) {
		this->flags |= flag;
	}
	void clearFlagInternal(int flag) {
		this->flags &= ~flag;
	}
public:
	SafeRef<guibase> makeSafeRef();
	virtual bool isVisible() const {
		if (size.x < 0 || size.y < 0)
			return false;
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
	virtual bool isBackgroundRenderedInset() {
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
		label = _str;
	}
	String getLabel() const {
		return label;
	}

	String getClassName();
	guibase(const guibase&) = default; guibase& operator=(const guibase&) = default;
	guibase(guibase&&) = default; guibase& operator=(guibase&&) = default;

	bool contains(ivec2 mpos) {
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
	int right() {
		return pos.x+size.x;
	}
	int top() {
		return pos.y;
	}
	int bottom() {
		return pos.y+size.y;
	}
	int left() {
		return pos.x;
	}
	void setZOrder(int _zOrder) {
		zOrder = _zOrder;
	}
	virtual void render(NVGcontext* vg) {

	}
	virtual void determineSize(ivec2& prefSize)/* const */ {
	}
	virtual void prerender(NVGcontext* vg) {
	}
	virtual void onIdle() {
	}
	virtual void onTick(AppCtrl* appctrl) {

	}
	virtual guictxtmenu_base* getTooltip(AppCtrl* appctrl) {
		return nullptr;
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
		while (parentGui != NULL) {
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
		return false;
	}
	virtual bool handleKeyInput(KeyEvent& kevt) {
		return false;
	}
	virtual bool handleCharInput(unsigned int codepoint) {
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
		ivec2 posInset = pos;
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
		vpos.x = math::max(posTL.x, pos.x);
		vpos.y = math::max(posTL.y, pos.y);
		vsize.x = math::min(posBR.x, (pos+size).x) - vpos.x;
		vsize.y = math::min(posBR.y, (pos+size).y) - vpos.y;
		if (parent != NULL) {
			parent->scissorClip(vpos, vsize);
		}
		vpos = toContainerSpace(vpos);
	}
	virtual ivec2 toParentSpace(ivec2 in) {
		return this->pos + in;
	}
	virtual ivec2 toContainerSpace(ivec2 in) {
		return in - this->pos;
	}
	void getHierachy(std::vector<guibase*>& stack) {
		guibase* p = this->parent;
		while (p != NULL) {
			stack.push_back(p);
			p = p->parent;
		}
	}
	virtual void onChildLayoutChanged(guibase* g) {
		if (this->parent != NULL) {
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
		if (this->parent != NULL){
			return this->parent->getFocusedContainer();
		}
		return NULL;
	}
	void renderWidgetBorder(NVGcontext* vg, int32_t flags) const;
	void renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const;
	virtual ivec2 toScreenSpace(ivec2 in) const {
		in += this->pos;
		if (this->parent != NULL) {
			in = this->parent->toScreenSpace(in);
		}
		return in;
	}
	virtual bool isStaticContainer() {
		return false;
	}
	virtual int32_t getStateFlags() const;


	BaseCtrl* getControl() const {
		return parentCtrl;
	}
	virtual void setControl(BaseCtrl* parentCtrl);
	virtual void setParent(guibase* parent);
	virtual void addProperties(Table::tbl* table);
public:
	virtual bool isSelected();
	int32_t getAllocId() {
		return allocId;
	}
protected:
	virtual NVGcolor getBackgroundColor(int stateflags) const;
	bool isChildOf(guibase* parentSearch);
	void setFont(NVGcontext* vg, float size, NVGcolor color, int alignment);
	void setTheme(guitheme_t* theme);
};
