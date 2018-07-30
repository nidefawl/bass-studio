#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
using glm::vec2;
using glm::ivec2;

#include <nanovg.h>
#include <vector>
#include <algorithm>
#include <typeinfo>

#include "str_util.h"
#include "event.h"
#include "math.h"
#include "guicolors.h"
#include "logging.h"
#include "renderresources.h"
#include "basectrl.h"
#include "theme.h"
#include "basectrl.h"
#define GUI_PLUGIN_VIEW 1
#define GUI_PLUGIN 2

struct NVGcontext;
class guitrack_editor;
class guiplugin;
class guictr_base;
class gui_pluginlist_entry;
class gui_track;
struct dragdrop_midifile;

extern int allocCount;
extern std::vector<guibase*> g_guis;
void initColor();
void setFont(NVGcontext* vg, float size, NVGcolor color, int alignment);
void renderText(NVGcontext* ctx, float x, float y, float maxWidth, const char* string);
void renderDashedLineFrame(NVGcontext* vg, float x, float y, float w, float h, float thickness);
void drawAttachedBackground(NVGcontext* vg, ivec2 posInset, ivec2 sizeInset, int margin);

void drawPlaySymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawStopSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2);
void drawTri(NVGcontext* vg, float xTop, float yTop, float h, const int dir, const NVGcolor& color, const NVGcolor& strokeColor, float strokeWidth);
guitheme_t* getDefaultTheme();
ivec2 toControlsObjectSpace(ivec2& pos, guibase* gui);

inline float calcInset(float desiredInset, float size) {
	return min(desiredInset, max(0.f, (size-4.0f)/2.0f));
}
enum TextInputState : signed int {
	DISABLED = 0,
	ENABLED = 1
};
class guibase {
public:
	ivec2 pos{0};
	ivec2 size{0};
	guibase* parent = NULL;
	int zOrder = 0;
	int id;
	guitheme_t* theme = getDefaultTheme();
	int dummy0 = 0;
	bool canTextInput = false;
	String label = "";
	const int guiType;
	guibase(int guiTypeId = 0) : guiType(guiTypeId) {
		id = allocCount;
		allocCount++;
		g_guis.push_back(this);
	}
	guibase(ivec2 _pos, ivec2 _size) : guibase() {
		this->pos = _pos;
		this->size = _size;
	}
	bool hasTextinput() {
		return canTextInput;
	}
	String getClassName() {
		return typeName(*this);
	}
	void setLabel(String _str) {
		label = _str;
	}
	virtual ~guibase() {
		AppCtrl* ctrl = AppCtrl::get();
		if (ctrl)
			ctrl->onGuiRemoved(this); // TODO: don't call this from here
		allocCount--;
		auto it = std::find(g_guis.begin(), g_guis.end(), this);
		g_guis.erase(it);
		if (!theme->isDefault) {
			delete theme;
		}
	}
	void setColor(uint32_t hex) {
		if (theme->isDefault) {
			theme = new guitheme_t(false);
		}
		theme->setBgColor(hex);
	}
	void setActiveColor(uint32_t hex) {
		if (theme->isDefault) {
			theme = new guitheme_t(false);
		}
		theme->setActiveColor(hex);
	}
	guibase(const guibase&) = default; guibase& operator=(const guibase&) = default;
	guibase(guibase&&) = default; guibase& operator=(guibase&&) = default;

	bool contains(ivec2 mpos) {
		return mpos.x >= pos.x &&
			mpos.y >= pos.y &&
			mpos.x < pos.x + size.x &&
			mpos.y < pos.y + size.y;
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
	virtual void determineSize() {
	}
	virtual void prerender(NVGcontext* vg) {
	}
	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos) {

	}
	virtual void layout() {
	}
	virtual void onRemove() {
	}
	virtual void onAdded() {
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
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
		vpos.x = max(posTL.x, pos.x);
		vpos.y = max(posTL.y, pos.y);
		vsize.x = min(posBR.x, (pos+size).x) - vpos.x;
		vsize.y = min(posBR.y, (pos+size).y) - vpos.y;
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
    virtual bool focusEvent(bool focused) {
    	return true;
    }
	virtual guibase* getFocusedContainer() {
		if (this->parent != NULL){
			return this->parent->getFocusedContainer();
		}
		return NULL;
	}
	void renderWidgetBorder(NVGcontext* vg, int32_t flags = FLG_ENBL);
	virtual ivec2 toScreenSpace(ivec2 in) {
		in += this->pos;
		if (this->parent != NULL) {
			in = this->parent->toScreenSpace(in);
		}
		return in;
	}
	virtual bool isStaticContainer() {
		return true;
	}
	virtual int32_t getStateFlags() {
		return FLG_ENBL;
	}
protected:

};

