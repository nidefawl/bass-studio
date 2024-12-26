#pragma once
#include "gui/container/container_builder.h"
#include "container.h"
#include "gui/gui.h"
#include "theme.h"
#include <memory>
#include <vector>
#include <map>
#include <functional>

class DawInstance;
struct GuiCtrLayoutEntry;
class GuiCtrLayoutEntryHandle;
class guictr_layout;
class guictr_layout_base;

enum LayoutCtrType { GUICTR_LAYOUT, GUICTR_BASE };

using SPLayoutEntry = std::shared_ptr<GuiCtrLayoutEntry>;
struct GuiCtrLayoutEntry {
    const gui_type type;
    const LayoutCtrType frameType;
    ivec2 pos{0};
    ivec2 size{0};
    std::shared_ptr<guictr_base> ctr;
    std::shared_ptr<guictr_layout> selfLayoutCtr;
    GuiCtrLayoutEntryHandle* ctrHandle;
    String label;
    int32_t indexInParent = 0;
private:
    guictr_layout_base* parent = nullptr;
    int32_t entryTag = -1;
public:
    GuiCtrLayoutEntry(String label, const std::shared_ptr<guictr_base>& _ctr);
    ~GuiCtrLayoutEntry();
    GuiCtrLayoutEntry(const GuiCtrLayoutEntry& other) = delete;
    GuiCtrLayoutEntry& operator=(const GuiCtrLayoutEntry& other) = delete;
    GuiCtrLayoutEntry(GuiCtrLayoutEntry&& other) = delete;
    GuiCtrLayoutEntry& operator=(GuiCtrLayoutEntry&& other) = delete;
    guictr_layout_base* getParentContainer() { return parent; }
    const guictr_layout_base* getParentContainer() const { return parent; }
    void setParentContainer(guictr_layout_base* _parent) { parent = _parent; }
    
    guictr_base* getGui();
    std::shared_ptr<guictr_base> getSharedGui() const { return ctr; }
    guibase* getHandle();
    gui_type getType() const { return type; }
    LayoutCtrType getFrameType() const { return frameType; }
    String getLabel() const { return label; }
    bool getContainerRef(SPLayoutEntry& out, bool remove);
    void removeEntryFromParent();
    std::shared_ptr<guictr_layout>& getAsLayoutCtr() { return selfLayoutCtr; }
    int32_t getEntryTag() const { return entryTag; }
    void setEntryTag(int32_t tag);
    void assertState() const;
    void updateLabel();
    bool isVisible();
};
template<typename T, typename Y>
T* guictr_cast(Y& entry) {
    if (!assert_expr(!!entry))
        return nullptr;
    return static_cast<T*>(entry->getGui());
}

class DropAreaUILayout {
    guictr_layout_base* const parent;

public:
    ivec2 pos{0, 0};
    ivec2 size{0, 0};
    int32_t priority            = 0;
    dock_pos dockPos            = dock_pos::NONE;
    int32_t tabPosition         = -1;
    int32_t childContainerIndex = -1;
    String label;
    bool bAlwaysShow = false;
    guitheme_t theme;
    void init() {
        pos = {};
        size = {};
        priority = 0;
        dockPos = dock_pos::NONE;
        tabPosition = -1;
        childContainerIndex = -1;
        label = "";
        bAlwaysShow = false;
    }
    explicit DropAreaUILayout(guictr_layout_base* _parent) : parent(_parent) {}
    void render(NVGcontext* vg);
    bool contains(ivec2 mpos) const { return mpos.x >= pos.x && mpos.y >= pos.y && mpos.x < pos.x + size.x && mpos.y < pos.y + size.y; }
    guictr_layout_base* getLayoutCtr() { return parent; }
    dock_pos getDockPos() const { return dockPos; }
    void setAlwaysShow(bool b) { bAlwaysShow = b; }
    bool isAlwaysShow() const { return bAlwaysShow; }
};

class guictr_layout_base {
public:
    virtual ~guictr_layout_base() = default;
    virtual void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<DropAreaUILayout>>& handles)                = 0;
    virtual bool placeContainer(SPLayoutEntry ctr, DropAreaUILayout* area)                   = 0;
    virtual bool getContainerRef(GuiCtrLayoutEntry* ctr, SPLayoutEntry& out, bool remove) = 0;
    virtual SPLayoutEntry replaceContainerWith(guictr_base* ctr, SPLayoutEntry& newEntry) = 0;
    virtual container_layout getLayout() const = 0;
    virtual void postContentChanged() = 0;
    virtual bool activateEntry(GuiCtrLayoutEntry* entry) = 0;
    virtual bool isEntryVisible(GuiCtrLayoutEntry* entry) = 0;
};

struct ContainerInstanceContext {
    DawInstance* const daw = nullptr;
    DawCtrl* const dawCtrl = nullptr;
    std::map<int32_t, SPLayoutEntry> entriesPreconstructed{};
    std::map<gui_type, int32_t> stats{};
    std::map<gui_type, std::vector<SPLayoutEntry>> entriesConstructed{};
};

using ContainerBuilder = std::function<std::shared_ptr<guictr_base>(create_ctr_t& ctxt)>;
using ContainerFactory = std::map<gui_type, ContainerBuilder>;
using ContainerRegistry = std::vector<std::pair<gui_type, String>>;
ContainerFactory& getContainerFactory();
ContainerRegistry& getContainerRegistry();
SPLayoutEntry createGuiCtrLayoutEntry(const std::shared_ptr<guictr_layout>& ctr);
SPLayoutEntry createGuiCtrLayoutEntry(const std::shared_ptr<guictr_base>& ctr);
bool getContainerLabel(gui_type type, String& out);
bool makeContainer(ContainerInstanceContext& ctxt, gui_type type, std::shared_ptr<guictr_base>& out);
