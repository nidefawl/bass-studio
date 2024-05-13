#pragma once
#include "nanovg/nanovg_min.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "seq_util.h"
#include "str_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guiconstant.h"
#include "gui/gui.h"
#include "assert_dbg.h"
#include "guibackgroundimage.h"

class BaseCtrl;
struct guitheme_t;

enum autolayout_mode : uint8_t {
    LAYOUT_NONE = 0,
    LAYOUT_HORIZONTAL,
    LAYOUT_VERTICAL
};
enum class dock_pos : int32_t { NONE = 0, CENTER, LEFT, RIGHT, TOP, BOTTOM, STACK };
enum class container_layout : int32_t { SOLE, SPLIT_H, SPLIT_V, TABBED };

enum GuiContainerTag : int32_t {
    TAG_LEFT = 100,
    TAG_CENTER,
    TAG_RIGHT,
    TAG_TAB_TOP,
    TAG_TAB_BOTTOM,
    TAG_TRACKS,
    TAG_NODES,
    TAG_CLIPEDIT,
    TAG_PLUGINS,
};

class guictr_base : public guibase {
protected:
    autolayout_mode layoutMode{LAYOUT_NONE};
public:
    int padding = CONTENT_INSET;
    int margin  = CTR_SPACING;
    ivec4 snapSides{ 0, 0, 0, 0 };
    std::vector<guibase*> guis;
    bool sortChildren = false;
public:
    explicit guictr_base(gui_type guiType = gui_type::CTR_TYPE_UNKNOWN);

    ~guictr_base() override {
        // The derived class has to remove guis before this dtor is called
        // destroyGuis() will not be called here
        dbgassert(guis.empty());
    }

    virtual void destroyGuis() {
        for (guibase* g : guis) {
            g->onRemove();
            g->setParent(nullptr);
            delete g;
        }
        guis.clear();
    }

    virtual void removeGuis() {
        for (guibase* g : guis) {
            g->onRemove();
            g->setParent(nullptr);
        }
        guis.clear();
    }

    void setControl(BaseCtrl* parentCtrl) override;
    void setParent(guibase* parent) override;

public:
    virtual void drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset = true);
    void drawInsetBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset);

    void onRemove() override;
    void onAdded() override;

    void determineSize(ivec2& prefSize) override;

    virtual ivec2 paddingTL(int _padding) const {
        return ivec2(_padding - margin * snapSides.x, _padding - margin * snapSides.y);
    }
    virtual ivec2 paddingBR(int _padding) const {
        return ivec2(_padding - margin * snapSides.z, _padding - margin * snapSides.w);
    }
    virtual ivec2 getPosContent() const {
        return pos + paddingTL(padding);
    }
    virtual ivec2 getSizeContent() const {
        return size - (paddingTL(padding) + paddingBR(padding));
    }
    ivec2 getPadding() {
        return (paddingTL(padding) + paddingBR(padding));
    }
    void setLayoutMode(autolayout_mode mode) {
        layoutMode = mode;
    }
    autolayout_mode getLayoutMode() const {
        return layoutMode;
    }
    virtual void layoutEntries(ivec2 pos, ivec2 cs, ivec2 dir);
    void layout() override;

    GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const override {
        return getInnerBackgroundColorFromState(stateflags);
    }

    virtual GuiColor::constant_t getInnerBackgroundColorFromState(int32_t stateflags) const {
        if (isBackgroundRenderedInset()) {
            return GuiColor::COL_BG_BRT;
        }
        return getOuterBackgroundColorFromState(stateflags);
    }

    virtual GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const {
        if (focused()) {
            return GuiColor::COL_BG_DRK_FOCUSED;
        }
        return GuiColor::COL_BG_DRK;
    }

    virtual uint32_t getTitlebarColorFromState(int32_t flags);
    void renderTitleBar(NVGcontext* vg, const ivec2& sizeContent, String text, const GuiConstant::constant_t& constantHeight, float textOffsetX, int flags, bool isHorizontalTitle);
    void renderFrameBase(NVGcontext* vg);
    void renderFrameOutline(NVGcontext* vg);
    virtual void renderBackground(NVGcontext* vg);
    virtual void renderContainerLabel(NVGcontext* vg);

    void render(NVGcontext* vg) override;
    bool isRenderableSizeAndContext(NVGcontext* vg) override;
    virtual bool setScissorTransformContainer(NVGcontext* vg);
    bool setScissorTransform(NVGcontext* vg) override;

    void setSnapSides(ivec4 _snapSides) {
        this->snapSides = _snapSides;
    }

    void scissorClip(ivec2& vpos, ivec2& vsize) override;

    ivec2 toContainerSpace(ivec2 in) const override {
        return in - getPosContent();
    }

    ivec2 toParentSpace(ivec2 in) const override {
        return getPosContent() + in;
    }
    vec2 toParentSpace2f(vec2 in) const override {
        return (vec2(getPosContent())) + in;
    }
    vec2 toContainerSpace2f(vec2 in) const override {
        return in - (vec2(getPosContent()));
    }

    ivec2 toScreenSpace(ivec2 in) const override {
        in += getPosContent();
        if (this->parent != nullptr) {
            in = this->parent->toScreenSpace(in);
        }
        return in;
    }

    guibase* getByID(int id) {
        auto it = std::find_if(guis.begin(), guis.end(), [id](auto g) {
            return g->id == id;
        });
        if (it == guis.end()) {
            return nullptr;
        }
        return *it;
    }

    virtual void add(guibase* gui) {
        dbgassert(gui && gui != this);
        auto it = std::find(guis.begin(), guis.end(), gui);
        if (it != guis.end()) {
            throw applogicexception(StringFormat("%s - attempt to add gui twice", StringAsCStr(getClassName())));
        }
        guis.push_back(gui);
        if (sortChildren) {
            std::sort(guis.begin(), guis.end(), [](guibase* a, guibase* b) {
                return a->zOrder > b->zOrder;
            });
        }
        gui->setParent(this);
        auto thisCtrl = getControl(); 
        if (thisCtrl)
            gui->setControl(thisCtrl);
        gui->onAdded();
    }

    void moveToBegin(guibase* gui) {
        auto it = std::find(guis.begin(), guis.end(), gui);
        if (it == guis.end()) {
            throw applogicexception(StringFormat("%s - attempt to move gui that is not in list", StringAsCStr(getClassName())));
        }
        guis.erase(it);
        guis.insert(guis.begin(), gui);
    }

    template<typename Container>
    void sortChildrenByList(Container& container) {
        //TODO: very inefficient
        std::sort(guis.begin(), guis.end(), [&container](guibase* a, guibase* b) {
            int indexA = indexOfCtr(container, a);
            int indexB = indexOfCtr(container, b);
            if (indexA < 0)
                return true;
            if (indexB < 0)
                return false;
            return indexA < indexB;
        });
    }

    bool hasGui(guibase* gui) {
        auto it = std::find(guis.begin(), guis.end(), gui);
        return it != guis.end();
    }

    virtual void remove(guibase* gui) {
        auto it = std::find(guis.begin(), guis.end(), gui);
        if (it == guis.end()) {
            if (gui->parent == nullptr)
                return;
            throw applogicexception(StringFormat("%s - attempt to remove non-present element", StringAsCStr(getClassName())));
        }
        gui->onRemove();
        guis.erase(it);
        if (sortChildren) {
            std::sort(guis.begin(), guis.end(), [](guibase* a, guibase* b) {
                return a->zOrder > b->zOrder;
            });
        }
        gui->setParent(nullptr);
    }

    virtual void addUNCHECKED(guibase* gui) {
        auto it = std::find(guis.begin(), guis.end(), gui);
        if (it != guis.end()) {
            return;
        }
        guis.push_back(gui);
        if (sortChildren) {
            std::sort(guis.begin(), guis.end(), [](guibase* a, guibase* b) {
                return a->zOrder > b->zOrder;
            });
        }
        gui->setParent(this);
        auto thisCtrl = getControl(); 
        if (thisCtrl)
            gui->setControl(thisCtrl);
    }

    virtual void removeUNCHECKED(guibase* gui) {
        auto it = std::find(guis.begin(), guis.end(), gui);
        if (it == guis.end()) {
            return;
        }
        guis.erase(it);
        gui->setParent(nullptr);
    }

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;

    void prerender(NVGcontext* vg) override {
        for (guibase* gui : guis) {
            gui->prerender(vg);
        }
    }

    void onTick(AppCtrl* ctrl) override {
        for (guibase* gui : guis) {
            if (gui->isVisible()) {
                gui->onTick(ctrl);
            }
        }
    }

    guibase* getFocusedContainer() override {
        if (this->parent != nullptr) {
            return this->parent->getFocusedContainer();
        }
        return this;
    }

    void addProperties(Table::tbl* table) override;

    void renderDebug(NVGcontext* vg, NVGcolor color) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, color);
        nvgFill(vg);
        ivec2 posInset  = getPosContent();
        ivec2 sizeInset = getSizeContent();
        nvgBeginPath(vg);
        nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
        nvgFillColor(vg, color);
        nvgFill(vg);
    }
    void buttonClicked(guibase* button) override {
        if (parent) {
            parent->buttonClicked(button);
        }
    }
    virtual String storeContainerData() {
        return "";
    }
    virtual void loadContainerData(const String& data) {
    }
};

class guictr_vert_layout : public guictr_base {
public:
    struct layout_entry_t {
        float scale;
        guibase* gui;
    };
protected:
    std::vector<layout_entry_t> layouts;
    const uint8_t dir = 1;
    ivec2 layoutPadding = {1, 1};
public:
    explicit guictr_vert_layout(int32_t direction = 1)
        : guictr_base(), dir(direction) {
        setLayoutMode(autolayout_mode::LAYOUT_NONE);
        padding = margin = 0;
    }
    ~guictr_vert_layout() override {
        destroyGuis();
    }
    void setLayoutPadding(ivec2 _layoutPadding) {
        layoutPadding = _layoutPadding;
    }
    void layout() override;
    void addElement(const layout_entry_t& entry) {
        layouts.push_back(entry);
        add(entry.gui);
    }
    void clear() {
        layouts.clear();
        destroyGuis();
    }

};