#pragma once
#include "event.hpp"
#include "gui/gui.hpp"
#include "keyboard.hpp"
#include "str_util.hpp"
#include "color_util.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/button.hpp"
#include "host/track/track.hpp"
#include "basectrl.hpp"
#include "gui/table/table.hpp"
#include "host/daw/mainctrl.hpp"
#include "math/vec.hpp"
#include "gui/plugin/plugin.hpp"
#include <memory>

class vstplugin;
class effectbase;
struct audio_stage_t;
namespace DAW {

enum class action_plugin_ctr {
    PLUGINS_SELECTALL,
    PLUGINS_DELETE,
    PLUGINS_CUT,
    PLUGINS_COPY,
    PLUGINS_PASTE,
    PLUGINS_DUPLICATE,
    PLUGINS_MOVE_CURSOR,
};
}

class guiplaceholder final : public guibase {
public:
    String message;
    guiplaceholder() : guibase() {
    }
    ~guiplaceholder() override = default;
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        renderCenteredMultilineText(vg, theme, message, 18, getLabelColor(), pos, size);
    }
    void determineSize(ivec2& prefSize) override {
        size.x = math::max(100, size.y * 3 / 5);
    }
};

class guictr_dragged_plugins final : public guictr_base {
    const int HEIGHT_ENTRY = 20;

public:
    std::vector<effectbase*> effects;
    audio_stage_t* trackImpl = nullptr;
    Table::tbl table;
    guictr_dragged_plugins() : guictr_base(gui_type::CTR_TYPE_PLUGINS_DRAGGED) {
        pos = { 0, 0 };
        setDragRendered(true);
    }
    ~guictr_dragged_plugins() override = default;
    void layout() override {
    }
    virtual audio_stage_t* getTrackLink() {
        return trackImpl;
    }
    void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
    void setStrings(std::vector<String> list);
    void handleDraggedRelease(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void dragMoveOn(guibase* target, ivec2 mousepos) override;
    void dragReleaseOn(guibase* target, ivec2 mousepos) override;
};

class guictr_plugins final : public guictr_base {
public:
    guiplaceholder pluginCtrEmpty;
    guictr_dragged_plugins dragged;
    track_t* track          = nullptr;
    audio_stage_t* stage    = nullptr;
    int scrolloffset        = 0;
    bool isDefaultPluginCtr = true;
    int32_t uuid = 0;
    struct guiplugin_entry {
        automatable_param_ref_t pluginRef;
        std::shared_ptr<guiplugin> guiPlugin;
    };
    std::vector<guiplugin_entry> guiPlugins;
    void onRemove() override;
    void onAdded() override;

public:
    explicit guictr_plugins(int32_t uuid) 
    : guictr_base(),
        uuid(uuid) {
        setGuiType(gui_type::CTR_TYPE_PLUGINS);
        setCanMouseHit(true);
        setBackgroundRendered(true);
        dragged.setParent(this);
    }
    ~guictr_plugins() override {
        removeEntry(guis, &pluginCtrEmpty);
        guis.clear();
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        pluginCtrEmpty.setControl(parentCtrl);
        dragged.setControl(parentCtrl);
    }
    void scrollToPluginGui(effectbase* plugin);
    void setScrolloffset(int offset);
    ivec2 toContainerSpace(ivec2 in) const override {
        ivec2 offsetPos = in - getPosContent();
        offsetPos.x += scrolloffset;
        return offsetPos;
    }
    ivec2 toParentSpace(ivec2 in) const override {
        in.x -= scrolloffset;
        ivec2 offsetPos = getPosContent() + in;
        return offsetPos;
    }
    ivec2 toScreenSpace(ivec2 in) const override {
        in += getPosContent();
        in.x -= scrolloffset;
        if (this->parent != NULL) {
            in = this->parent->toScreenSpace(in);
        }
        return in;
    }
    void verticalLineAt(NVGcontext* vg, ivec2 posHL) {
        nvgLineCap(vg, NVGlineCap::NVG_ROUND);
        nvgBeginPath(vg);
        nvgMoveTo(vg, posHL.x, 4);
        nvgLineTo(vg, posHL.x, getSizeContent().y - 4);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_DRAGDROPMOVE_HIGHLIGHT));
        nvgStrokeWidth(vg, 4.0);
        nvgStroke(vg);
        nvgLineCap(vg, NVGlineCap::NVG_BUTT);
    }
    uint32_t getTitlebarColorFromState(int32_t flags) override;
    void render(NVGcontext* vg) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleCommand(DAW::UI::CommandContext& ctxt);
    void layout() override;
    int slotFromCoord(ivec2 _pos);
    int slotFromChild(guibase* child) {
        int slot = 0;
        for (guibase* gui : guis) {
            if (gui == child) {
                return slot;
            }
            slot++;
        }
        return -1;
    }
    int getTotalWidth() {
        guibase* last = guis.empty() ? NULL : guis.back();
        if (!last) {
            return 1;
        }
        return last->pos.x + last->size.x + 50;
    }

    void onTick(AppCtrl* ctrl) override;
    void pluginDragMove(guiplugin* g, ivec2 mousepos) override;
    void pluginDragRelease(guiplugin* g, ivec2 mousepos) override;
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override;
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override;
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override;
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override;
    bool fileDropMove(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override;
    bool fileDropRelease(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override;
    void showTrack(audio_stage_t* track, std::shared_ptr<guictr_plugins>& ctr);
    void relayout();
    void resetTrackIf(audio_stage_t* _track);
    void hideTrack();
    void onSelected(MouseEvent& evt, effectbase* plugin);
    void addGui(effectbase* plugin);
    void onChildLayoutChanged(guibase* g) override;
    void determineSize(ivec2& prefSize) override;
    guibase* getDraggedControl() override;
    void getEffects(std::vector<effectbase*>& out);
    bool isSelected() override;
    void handleRightClick(MouseEvent& evt) override;
    bool getSelected(std::vector<effectbase*>& out);
};
class guictr_pluginview final : public guictr_base {
    SPLayoutEntry pluginCtr;
public:
    int lastscrolloffset = 0;
    guictr_pluginview() : guictr_base() {
        setCanMouseHit(true);
    }
    ~guictr_pluginview() override = default;
    void setPluginCtr(SPLayoutEntry& _pluginCtr) {
        pluginCtr = _pluginCtr;
    }
    guictr_plugins* getPluginCtr();
    vec2 getScale();
    guibase* getFocusedContainer() override;
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    float getMinScale();
    void handleDraggedMove(MouseEvent& evt) override;
    void layout() override;
    void handleDragDropHover(MouseHitEvt& mouseHit) override;
};

class action_remove_modules final : public action_base {
    std::vector<effectbase*> effects;
    std::vector<DAW::removed_modulation_routings> modulationRoutings;
    audio_stage_ref_t ref;
    int32_t dstSlot;
    bool weOwn = true;

protected:
public:
    action_remove_modules(String s, std::vector<effectbase*>&& _effects, std::vector<DAW::removed_modulation_routings>&& _modRoutings, audio_stage_ref_t _ref, int32_t _dst);
    void undo(DawInstance* daw) override;
    void redo(DawInstance* daw) override;
    void releaseResources(DawInstance* daw) override;
};

class action_shift_modules final : public action_base {
    audio_stage_ref_t ref;
    int32_t dst;
    int32_t src;
    int32_t len;

protected:
public:
    action_shift_modules(String s, audio_stage_ref_t _ref, int32_t _dst, int32_t _src, int32_t _len)
        : action_base(), ref(_ref), dst(_dst), src(_src), len(_len) {
        desc = s;
    }
    void undo(DawInstance* daw) override {
        audio_stage_t* stage = daw->getPluginManager()->getAudioStage(ref);
        if (!stage) {
            setError("missing trackimpl");
            return;
        }
        daw->getPluginManager()->movePluginsOnStage(stage, dst, src, len);
        daw->onPluginsChanged();
    }
    void redo(DawInstance* daw) override {
        audio_stage_t* stage = daw->getPluginManager()->getAudioStage(ref);
        if (!stage) {
            setError("missing trackimpl");
            return;
        }
        daw->getPluginManager()->movePluginsOnStage(stage, src, dst, len);
        daw->onPluginsChanged();
    }
};

class action_move_modules final : public action_base {
    audio_stage_ref_t refdst;
    audio_stage_ref_t refsrc;
    int32_t dst;
    int32_t src;
    int32_t len;

protected:
public:
    action_move_modules(String s, audio_stage_ref_t _refdst, audio_stage_ref_t _refsrc, int32_t _dst, int32_t _src, int32_t _len)
        : action_base(), refdst(_refdst), refsrc(_refsrc), dst(_dst), src(_src), len(_len) {
        desc = std::move(s);
    }
    void undo(DawInstance* daw) override;
    void redo(DawInstance* daw) override;
};

class action_insert_effect final : public action_base {
    effectbase* effect;
    audio_stage_ref_t ref;
    int32_t dstSlot;
    bool weOwn = false;

protected:
public:
    action_insert_effect(String s, effectbase* _effect, audio_stage_ref_t _ref, int32_t _dst)
        : action_base(), effect(_effect), ref(_ref), dstSlot(_dst) {
        desc = std::move(s);
    }
    void releaseResources(DawInstance* daw) override;
    void undo(DawInstance* daw) override;
    void redo(DawInstance* daw) override;
};