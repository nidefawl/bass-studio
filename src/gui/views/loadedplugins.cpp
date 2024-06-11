#include <algorithm>
#include <nanovg.h>
#include "color_util.h"
#include "gui/container/container_builder.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/tooltip/tooltip.h"
#include "gui/controls/button.h"
#include "gui/controls/list.h"
#include "gui/controls/textfield.h"
#include "renderresources.h"
#include "platform.h"
#include "host/plugin/base/base-plugin.h"
#include "host/daw/mainctrl.h"
#include "host/host.h"
#include "host/audiohost/audio_host.h"
#include "seq_util.h"

void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();

namespace {
class guigraph2d final : public guictr_base {
        std::vector<vec2> m_data;

    public:
        guigraph2d() : guictr_base() {
            padding = 0;
            margin  = 0;
            setCanMouseHit(true);
        }
        ~guigraph2d() override = default;
        std::vector<vec2>& getData() {
            return m_data;
        }
        void render(NVGcontext* vg) override {
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
            //for (auto c : guis) {
            nvgSave(vg);
            if (m_data.size()) {
                auto cs       = getSizeContent();
                auto fcs      = vec2{ cs.x, cs.y };
                auto vecFirst = m_data[0] * fcs;
                nvgBeginPath(vg);
                nvgMoveTo(vg, vecFirst.x, cs.y - 1 - vecFirst.y);
                for (auto& vec : m_data) {
                    auto vPt = vec * fcs;
                    nvgLineTo(vg, vPt.x, cs.y - 1 - vPt.y);
                }
                nvgStrokeWidth(vg, 1.0f);
                nvgStrokeColor(vg, rgbaToNvg(0xFFFFFFFF));
                nvgStroke(vg);
            }
            //c->render(vg);
            nvgRestore(vg);
            //}
        }
    };
class gui_test final : public guictxtmenu_base {
        SafeRef<effectbase> ref;
        guigraph2d graph;
        bool hadMouseFocus = false;

    public:
        gui_test(SafeRef<effectbase> _ref) : guictxtmenu_base(), ref(_ref) {
            add(&graph);
            setBackgroundRendered(false);
//            setBackgroundRendered(true);
//            setBackgroundRenderedInset(false);
            scrollbarOutside = true;
            maxHeight        = 220;
        }

        ~gui_test() override {
            remove(&graph);
        }

        guigraph2d& getGraph() {
            return graph;
        }

        void updateGraph() {
            auto* _entry = safeRefGet(ref);
            if (_entry) {
                stats_processing_timings_t procStatsCopy = _entry->procStatsAvg;
                auto& vecOut                             = graph.getData();
                vecOut.resize(STATS_PROCESSING_MAX_SAMPLES);
                vec2 scale = { 1.0f / (float) STATS_PROCESSING_MAX_SAMPLES, 1.0f / 21333.33f };
//                std::transform(_entry->procStatsAvg.statsProcSamples,
//                    _entry->procStatsAvg.statsProcSamples+STATS_PROCESSING_MAX_SAMPLES,
//                    std::back_inserter(vecOut), [&posX, scale](int64_t sample){
//                        return vec2{posX++, sample} * scale;
//                    });
                for (size_t i = 0; i < STATS_PROCESSING_MAX_SAMPLES; i++) {
                    size_t idx = i + procStatsCopy.statsWriteOffset;
                    if (idx == 0) {
                        idx = STATS_PROCESSING_MAX_SAMPLES - 1;
                    } else {
                        idx = idx - 1;
                    }
                    int64_t sample = procStatsCopy.statsProcSamples[idx % STATS_PROCESSING_MAX_SAMPLES];
                    float y        = ((float) sample);
                    vecOut[i]      = vec2{ i, y } * scale;
                }
            }
        }

        void layout() override {
            graph.pos  = { 0, 0 };
            graph.size = this->size;
        }


        void render(NVGcontext* vg) override {
            guictr_base::render(vg);
        }

        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            if (contains(mpos)) {
                if (evt.type == MouseHitType::MOUSE_LEFT || evt.type == MouseHitType::MOUSE_RIGHT)
                    hadMouseFocus = true;
                evt.requestFocus(this);
                return true;
            }
            return false;
        }
        bool isTransient() const override {
            return !hadMouseFocus;
        }

        bool clickedElement(ctxtmenu_entry* e, int _id) {
            closeContextMenu();
            return true;
        }

        void onTick(AppCtrl* appctrl) override {
            updateGraph();
        }
    };
}// namespace

class gui_pluginsloaded_list_entry final : public gui_list_entry {
    SafeRef<effectbase> ref;
    String tmp;

public:
    SafeRef<effectbase>& getRef() {
        return ref;
    }
    explicit gui_pluginsloaded_list_entry(SafeRef<effectbase> _ref) : gui_list_entry(), ref(_ref) {
        auto* _entry = safeRefGet(ref);
        dbgassert(_entry);
        icon = _entry->isSynth ? ICON_SYNTH : ICON_EFFECT;
        tmp  = getText() + "...";
    }
    String getText() override {
        auto* _entry = safeRefGet(ref);
        if (!_entry)
            return "<invalid reference>";
        return _entry->getName();
    }
    bool isSynth() {
        auto* _entry = safeRefGet(ref);
        if (!_entry)
            return false;
        return _entry->isSynth;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
    }

    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        BaseCtrl* ctrl  = parentCtrl;
        float spacing   = INSET_TITLE;
        float x         = spacing;
        float rowHeight = size.y;
        bool focused = ctrl->isCtrOrChildFocused(this);
        if (focused || selected) {
            auto color = theme->getColor(focused ? GuiColor::COL_BG_DRKER : GuiColor::COL_BG_DRK);
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
        nvgTranslate(vg, pos.x, pos.y);
        if (icon > -1) {
            RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
            drawIcon(vg, size, &image);
            x += rowHeight + spacing;
        }

        const float percWidth = size.x / 4;
        float xText = size.x - spacing;
        auto* _entry = safeRefGet(ref);
        if (_entry) {
            host_stats_reducted_t stats{};
            auto host = dawCtrl->getDaw()->getHost();
            host->getShortStats(stats);
            float fPercentLoad = stats.timePerBlock_usec <= 0 ? 0 : _entry->procStatsAvg.timeTrackProcessPlugins * 100.0f / stats.timePerBlock_usec;
            String str = StringFormat("%.2f%%", fPercentLoad);
            float x2 = size.x - spacing;
            xText = x2 - renderText(vg, vec2(x2, rowHeight / 2), vec2(size.x*0.8, size.y), str, 0, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
            if (size.x > rowHeight*10) {
                str = StringFormat("%zdµs", _entry->procStatsAvg.timeTrackProcessPlugins);
                x2 = size.x - percWidth - spacing;
                 xText = x2 - renderText(vg, vec2(x2, rowHeight / 2), vec2(size.x*0.3, size.y), str, 0, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
            }
        }
        renderText(vg, vec2(x, rowHeight*0.5f), vec2(xText-x, size.y), getText());
        nvgTranslate(vg, -pos.x, -pos.y);
    }
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
        auto tooltip  = new gui_test(ref);
        auto* graph   = &tooltip->getGraph();
        graph->size   = { 256, 128 };
        graph->pos    = { 0, 0 };
        tooltip->size = graph->size;
        tooltip->layout();
//        tooltip->canTakeInputFocus = true;
        tooltip->maxHeight = graph->size.y;
        return tooltip;
    }
};

class gui_list_plugins final : public guictr_base {
    std::vector<gui_pluginsloaded_list_entry*>& entries;

public:
    gui_list list;

    gui_list_plugins(std::vector<gui_pluginsloaded_list_entry*>& _entries) : guictr_base(), entries(_entries) {
        setBackgroundRendered(true);
        list.padding = 0;
        list.setBackgroundRendered(false);
        list.setRowHeight(14);
        add(&list);
    }
    void sort() {

        list.sort([](gui_list_entry* ptrA, gui_list_entry* ptrB) {
            dbgassert(ptrA && ptrB);
            effectbase* ptrEffA = safeRefGet(static_cast<gui_pluginsloaded_list_entry*>(ptrA)->getRef());
            effectbase* ptrEffB = safeRefGet(static_cast<gui_pluginsloaded_list_entry*>(ptrB)->getRef());
            dbgassert(ptrEffB && ptrEffA);
            if (!ptrEffA)
                return true;
            if (!ptrEffB)
                return false;

            return ptrEffA->procStatsAvg.timeTrackProcessPlugins > ptrEffB->procStatsAvg.timeTrackProcessPlugins;
        });
    }
    void layout() override {
        auto cs           = getSizeContent();
        list.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        list.pos            = { 0, 0 };
        list.size           = cs;
        for (auto* g : guis) {
            g->layout();
        }
    }
    void buttonClicked(guibase* button) override {
        if (stl_contains(entries, button)) {
            auto entry = static_cast<gui_pluginsloaded_list_entry*>(button);
            auto* effectbase = safeRefGet(entry->getRef());
            if (effectbase) {
                track_t* tr = effectbase->getTrack();
                if (tr) {
                    dawCtrl->setSelectedTrack(tr);
                    dawCtrl->showPluginView();
                    dawCtrl->revealPlugin(effectbase);
                }
            }
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto* g : guis) {
            if (g->isVisible()) {
                nvgSave(vg);
                g->render(vg);
                nvgRestore(vg);
            }
        }
    }
    ~gui_list_plugins() override {
        removeGuis();
    }
};
class gui_pluginsloaded_list final : public guictr_base {
    std::vector<gui_pluginsloaded_list_entry*> listEntriesLoadedPlugins;
    std::vector<gui_pluginsloaded_list_entry*> listEntriesDef;
    gui_list_plugins listCtr;
    gui_list_plugins listDeferredCtr;
    guibutton btnLoadAll;
    String curquery     = "";
    int64_t tmLastUpdate = 0;

public:
    gui_pluginsloaded_list() : guictr_base(), listCtr(listEntriesLoadedPlugins), listDeferredCtr(listEntriesDef) {
        setGuiType(CTR_TYPE_PLUGINSLOADED);
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
        add(&btnLoadAll);
        add(&listCtr);
        add(&listDeferredCtr);
    }
    ~gui_pluginsloaded_list() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        update();
    }
    void update() {
        auto daw = dawCtrl->getDaw();
        ThreadLock lock = daw->getPlayThread()->tryLockThread();
        auto tmNow = getTimeMillis();
        if (!lock.isLocked()) {
            // force lock and update if 5 seconds have elapsed
            if (tmNow - tmLastUpdate > 5000) {
                lock = daw->lockPlayThread();
            } else {
                return;
            }
        }
        tmLastUpdate = tmNow;

        std::vector<effectbase*> effects;
        std::vector<effectbase*> deferredEffects;

        auto pluginMgr = daw->getPluginManager();
        pluginMgr->getAllInstances(effects);
        pluginMgr->getDeferredEffects(deferredEffects);

        std::vector<gui_list_entry*> _newList;
        std::vector<gui_list_entry*> _newListDef;
        std::vector<gui_pluginsloaded_list_entry*> _newListLoadedPlugins;
        std::vector<gui_pluginsloaded_list_entry*> _newListDefPlugins;
        std::stable_sort(deferredEffects.begin(), deferredEffects.end(), [](const effectbase* ptrA, const effectbase* ptrB) {
            if (ptrA->sName == ptrB->sName)
                return (size_t) ptrA > (size_t) ptrB;
            return ptrA->sName > ptrB->sName;
        });
        for (effectbase* eff : deferredEffects) {
            SafeRef<effectbase> safeRef = eff->makeSafeRef();

            auto it = std::find_if(listEntriesDef.begin(), listEntriesDef.end(),
                                    [&safeRef](gui_pluginsloaded_list_entry* p) {
                                        return p->getRef().refId == safeRef.refId;
                                    });
            gui_pluginsloaded_list_entry* g;
            if (it == listEntriesDef.end()) {
                g = new gui_pluginsloaded_list_entry(safeRef);
            } else {
                g = *it;
            }
            _newListDef.push_back(g);
            _newListDefPlugins.push_back(g);
        }
        for (effectbase* eff : effects) {
            SafeRef<effectbase> safeRef = eff->makeSafeRef();
            auto it = std::find_if(listEntriesLoadedPlugins.begin(), listEntriesLoadedPlugins.end(),
                                    [&safeRef](gui_pluginsloaded_list_entry* p) {
                                        return p->getRef().refId == safeRef.refId;
                                    });
            gui_pluginsloaded_list_entry* g;
            if (it == listEntriesLoadedPlugins.end()) {
                g = new gui_pluginsloaded_list_entry(safeRef);
            } else {
                g = *it;
            }
            _newList.push_back(g);
            _newListLoadedPlugins.push_back(g);
        }
        auto numDeferred = deferredEffects.size();
        btnLoadAll.setLabel("Load all deferred");
        btnLoadAll.setEnabled(numDeferred > 0);
        if (numDeferred) {
            btnLoadAll.setText(StringFormat("Load %zu Plugins", numDeferred));
        } else {
            btnLoadAll.setText(btnLoadAll.getLabel());
        }

        listCtr.list.setList(_newList);
        listDeferredCtr.list.setList(_newListDef);
        listEntriesLoadedPlugins = _newListLoadedPlugins;
        listEntriesDef           = _newListDefPlugins;
        listDeferredCtr.sort();
        listCtr.sort();
        layout();
    }
    void buttonClicked(guibase* button) override {
        if (&btnLoadAll == button) {
            auto daw = dawCtrl->getDaw();
            AppWndProc_enableBlockReentrant();
            ThreadLock lock = daw->getPlayThread()->lockThread();
            auto* host = daw->getHost();
            std::vector<effectbase*> pluginsDeferred;
            std::vector<audio_stage_t*> audioStagesAffected;
            host->getDeferredEffects(pluginsDeferred);
            log_printf("loading %zu plugins\n", pluginsDeferred.size());
            for (auto plugin : pluginsDeferred) {
                if (!plugin->getTrackLink())
                    continue;
                log_printf("activate %s\n", StringAsCStr(plugin->sName));
                effectbase* effectLoaded = nullptr;
                host->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY, &effectLoaded);
                if (effectLoaded) {
                    audioStagesAffected.push_back(effectLoaded->getTrackLink());
                }
            }
            AppWndProc_disableBlockReentrant();
            daw->onPluginsChanged();
        }
    }
    void layout() override {
        ivec2 cs = getSizeContent();
        const int32_t CONST_FIXED_TITLE_HEIGHT = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        int posLists    = 0;
        int heightLists = cs.y - CONST_FIXED_TITLE_HEIGHT;

        const int32_t inset = 2;
        listCtr.pos          = ivec2(0, posLists + 0);
        listDeferredCtr.pos  = ivec2(0, posLists + heightLists / 2 + inset / 2);
        listCtr.size         = ivec2(cs.x, posLists + heightLists / 2 - inset / 2);
        listDeferredCtr.size = ivec2(cs.x, heightLists / 2 - inset / 2);
        btnLoadAll.pos       = { inset, posLists + heightLists + inset / 2 };
        btnLoadAll.size      = { cs.x - inset * 2, CONST_FIXED_TITLE_HEIGHT - inset*2 };
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto* g : guis) {
            nvgSave(vg);
            g->render(vg);
            nvgRestore(vg);
        }
    }
};

namespace DAW::UI {
    guictr_base* makeGuiPluginsLoadedList(create_ctr_t ctxt) {
        return new gui_pluginsloaded_list();
    }
}
