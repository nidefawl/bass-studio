#include <cstdint>
#include <functional>
#include <nanovg_min.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtx/color_space.hpp>
#include "assert_dbg.h"

#include "color_util.h"
#include "error.h"
#include "gui/controls/colorpick.h"
#include "logging.h"
#include "math/seq_math.h"
#include "debugctr.h"
#include "platform.h"
#include "rand.h"
#include "str_util.h"
#include "gui/controls/knob.h"
#include "guiglobals.h"
#include "gui/gui.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/track/trackctr.h"
#include "theme.h"
#include "gui/controls/button.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/clip/clip.h"
#include "host/daw/mainctrl.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "host/audiohost/audio_host.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"
#include "host/daw/edithistory.h"
#include "gui/plugin/plugin.h"
#include "gui/clipeditor/clipeditor.h"
#include "appconfig.h"
#include "util/testing_environment.h"

#ifdef _WIN32
#define DISPLAY_WIN_MSG_STATS 1
#define DISPLAY_HWND_DRAWS 1
#include "platform/win/debug_msg_count.h"
#else
#define DISPLAY_WIN_MSG_STATS 0
#define DISPLAY_HWND_DRAWS 0
#endif

namespace DAW {
    extern bool gClapUseSampleAccurateModulation;
}

enum ID_BTN : int32_t {
    ID_BTN_RESET_HIST = 0,
    ID_OPTION_SCALE_GLOBAL,
    ID_KNOB_SET_THREAD_COUNT,
    ID_BTN_INJECT_SEGFAULT_AUDIO_THREAD,
    ID_BTN_INJECT_BAD_MALLOC_AUDIO_THREAD,
    ID_BTN_INJECT_SEGFAULT_MAIN_THREAD,
    ID_BTN_INJECT_BAD_MALLOC_MAIN_THREAD,
    ID_BTN_TOGGLE_AUDIOGRAPHCACHE,
    ID_BTN_TOGGLE_PLAYBACKPROCESSING,
    ID_BTN_TOGGLE_EFFECTPROCESSING,
    ID_BTN_TOGGLE_SAMPLECONVERSION,
    ID_BTN_TOGGLE_THREADING,
    ID_BTN_TOGGLE_CLIP_RENDER_CACHE,
    ID_BTN_UPDATE_VISIBLE_TRACK_CONTENTS,
    ID_BTN_TOGGLE_WAVEFORM_UPDATES,
    ID_BTN_TOGGLE_CLIPRENDERER_DEBUGLAYER,
    ID_BTN_RESET_AUDIOCACHE,
    ID_BTN_UNLOAD_UNREFERENCED_AUDIOCACHE,
    ID_BTN_RESET_RESAMPLERS,
    ID_BTN_TOGGLE_CLAP_SAMPLEACCURATE_MODULATION,
    ID_BTN_TOGGLE_64_BIT_SUMMING,
    ID_BTN_ADJUST_THEME_HUE,
    ID_BTN_ADJUST_THEME_SAT,
    ID_BTN_ADJUST_THEME_BR,
};
struct gui_ctr_debug::ctr_debug_impl_t {
    std::vector<guibase*> debugGuis;
    std::vector<DAW::Host::thread_stats_process_timings_t> lastProcessingList;
    sampleformat_t sampleformat;
};
gui_ctr_debug::~gui_ctr_debug() {
    removeGuis();
    for (auto* g : impl->debugGuis) {
        delete g;
    }
    delete impl;
}
gui_ctr_debug::gui_ctr_debug(create_ctr_t ctxt, DebugCtrType debugCtrType)
    : guictr_base(),
      impl(new gui_ctr_debug::ctr_debug_impl_t{}),
      dgbCtrType(debugCtrType) {
    dbgassert(ctxt.daw);
    auto guiType = gui_type::CTR_TYPE_DEBUG_0;
    switch (dgbCtrType) {
        case DebugCtrType::TYPE_0:
            guiType = gui_type::CTR_TYPE_DEBUG_0;
            break;
        case DebugCtrType::DEBUG_APPCTRL:
            guiType = gui_type::CTR_TYPE_DEBUG_1;
            break;
        case DebugCtrType::TYPE_2:
            guiType = gui_type::CTR_TYPE_DEBUG_2;
            break;
    }
    setGuiType(guiType);
    auto const host = ctxt.daw->getHost();
#ifdef _WIN32
    msgCounterEnabled = true;
#endif
    setBackgroundRendered(true);
    setCanMouseHit(true);
    std::vector<guibase*>& debugGuis = impl->debugGuis;
    if (dgbCtrType != DebugCtrType::TYPE_2) {
        auto knob        = new guiknob(guiknob::knobtype::KNOB_UNLABELED);
        knob->id         = ID_OPTION_SCALE_GLOBAL;
        knob->fnSetValue = [this](float f, int flags) {
            parentCtrl->getTheme()->set(GuiConstant::CONST_TRACK_HEIGHT_STEP, math::floorfS32(4.0f+60.0f*f));
            parentCtrl->relayout();
            // float guiScale      = math::max(0.05f, f * 2.0f);
            // parentCtrl->m_scale = guiScale;
            // parentCtrl->relayout();
        };
        knob->fnGetValue = [this]() {
            return math::max(0.05f, math::min(1.0f, (parentCtrl->getTheme()->get(GuiConstant::CONST_TRACK_HEIGHT_STEP)-4.0f)/60.0f));
        };
        debugGuis.push_back(knob);
    }
    if (dgbCtrType == DebugCtrType::TYPE_2) {
        auto knob        = new guiknob(guiknob::knobtype::KNOB_UNLABELED);
        knob->id         = ID_KNOB_SET_THREAD_COUNT;
        knob->fnSetValue = [this, knob, host](float f, int flags) {
            uint32_t thrdCntMax = host->getMaxThreadCount();
            uint32_t thrdCnt    = math::clamp<uint32_t>(math::roundfU32(f * thrdCntMax), 1U, thrdCntMax);
            ThreadLock lock     = dawCtrl->lockPlayThread();
            host->setThreadCount(thrdCnt);
            String strThrdCnt = StringFormat("Number of Threads: %d", host->getThreadCount());
            knob->setLabel(strThrdCnt);
        };
        String strThrdCnt = StringFormat("Number of Threads: %d", host->getThreadCount());
        knob->setLabel(strThrdCnt);
        knob->fnGetValue = [host]() {
            return host->getThreadCount() / (float) host->getMaxThreadCount();
        };
        debugGuis.push_back(knob);
    }
    if (dgbCtrType == DebugCtrType::TYPE_0) {
        {

            auto btn = new guibutton;
            btn->id  = ID_BTN_RESET_HIST;
            btn->setText("Reset history");
            debugGuis.push_back(btn);
        }
        {

            auto btn2 = new guibutton;
            btn2->id  = ID_BTN_INJECT_SEGFAULT_AUDIO_THREAD;
            btn2->setText("Segfault on Audiothread");
            debugGuis.push_back(btn2);
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_INJECT_BAD_MALLOC_AUDIO_THREAD;
            btn3->setText("BadAlloc on Audiothread");
            debugGuis.push_back(btn3);
        }
        {

            auto btn2 = new guibutton;
            btn2->id  = ID_BTN_INJECT_SEGFAULT_MAIN_THREAD;
            btn2->setText("Segfault on Mainthread");
            debugGuis.push_back(btn2);
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_INJECT_BAD_MALLOC_MAIN_THREAD;
            btn3->setText("BadAlloc on Mainthread");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_EFFECTPROCESSING;
            btn3->setText("Bypass Eff. Proc. (OFF)");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_SAMPLECONVERSION;
            btn3->setText("Bypass Sample conversion (OFF)");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_PLAYBACKPROCESSING;
            btn3->setText("Bypass Playback Proc. (OFF)");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_AUDIOGRAPHCACHE;
            btn3->setText("Use Audio Graph Cache (OFF)");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_CLIP_RENDER_CACHE;
            btn3->setText("Disable clip render cache");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_WAVEFORM_UPDATES;
            btn3->setText("Disable audio waveform updates");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_CLIPRENDERER_DEBUGLAYER;
            btn3->setText("Enable clip renderer debug layer");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_THREADING;
            btn3->setText("Multithreaded processing (ON)");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_UPDATE_VISIBLE_TRACK_CONTENTS;
            btn3->setText("Update trackcontents");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_RESET_AUDIOCACHE;
            btn3->setText("Reset audiocache");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_UNLOAD_UNREFERENCED_AUDIOCACHE;
            btn3->setText("Unload unreferenced samples");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_RESET_RESAMPLERS;
            btn3->setText("Reset resamplers");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_CLAP_SAMPLEACCURATE_MODULATION;
            btn3->setText("Clap sampleaccurate modulation (OFF)");
            btn3->setText(String(DAW::gClapUseSampleAccurateModulation ? "Clap: Sample accurate modulation (ON)" : "Clap: Sample accurate modulation (OFF)"));
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_TOGGLE_64_BIT_SUMMING;
            btn3->setText(String(ctxt.daw->getHost()->bSummingIn64Bit ? "64 bit summing (ON)" : "64 bit summing (OFF)"));
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_ADJUST_THEME_HUE;
            btn3->setText("Adjust Theme Hue");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_ADJUST_THEME_SAT;
            btn3->setText("Adjust Theme Saturation");
            debugGuis.push_back(btn3);
        }
        {
            auto btn3 = new guibutton;
            btn3->id  = ID_BTN_ADJUST_THEME_BR;
            btn3->setText("Adjust Theme Brightness");
            debugGuis.push_back(btn3);
        }
    }
    for (auto g : debugGuis) {
        add(g);
    }
}
void gui_ctr_debug::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }

    if (dgbCtrType == DebugCtrType::TYPE_2 && impl->sampleformat.sampleRate > 0) {
        auto mikrosPerBlock = (impl->sampleformat.blockSize * 1000000UL) / impl->sampleformat.sampleRate;
        int inset           = 30;
        auto cs             = getSizeContent();
        vec2 graphSize      = vec2(cs.x - 20, cs.y) - inset * 2.0f;
        vec2 graphPos       = vec2(inset, inset);
        auto& list          = this->impl->lastProcessingList;
        nvgSave(vg);
        nvgTranslate(vg, graphPos.x, graphPos.y);


        nvgBeginPath(vg);
        float legendX         = 70;
        float legendY         = 20;
        vec2 graphOnlySize    = graphSize - vec2(legendX, legendY);
        auto graphLegendColor = (int32_t) 0x33ff33;
        nvgMoveTo(vg, legendX, graphOnlySize.y / 2);
        nvgLineTo(vg, legendX, graphOnlySize.y);
        //nvgMoveTo(vg, legendX, graphOnlySize.y);
        nvgLineTo(vg, legendX + graphOnlySize.x, graphOnlySize.y);
        nvgStrokeColor(vg, rgbToNvg(graphLegendColor));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        ivec2 graphLegendSteps(4);
        vec2 graphStepLen = graphOnlySize / (vec2(graphLegendSteps) - 1.0f);
        int markW         = 5;
        nvgBeginPath(vg);
        for (int i = 0; i < graphLegendSteps.x; i++) {
            float xPos = legendX + graphStepLen.x * (i);
            nvgMoveTo(vg, xPos, graphOnlySize.y);
            nvgLineTo(vg, xPos, graphOnlySize.y + markW);
        }
        //for (int i = 0; i < graphLegendSteps.y; i++) {
        //float yPos = graphStepLen.y*(i+0.5f);
        //nvgMoveTo(vg, legendX-markW, yPos);
        //nvgLineTo(vg, legendX, yPos);
        //}
        nvgStrokeColor(vg, rgbToNvg(graphLegendColor));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        for (int i = 0; i < graphLegendSteps.x; i++) {
            float fPos = i / (graphLegendSteps.x - 1.0f);
            float xPos = legendX + graphStepLen.x * (i);
            setFont(vg, 14, THEMECOL_TEXT, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);
            String strThrdCnt = StringFormat("%dµs", static_cast<int32_t>(math::roundfS32(fPos * mikrosPerBlock)));
            if (i == graphLegendSteps.x - 1) {
                strThrdCnt = StringFormat("%dµs deadline", static_cast<int32_t>(mikrosPerBlock));
            }
            nvgText(vg, xPos, graphOnlySize.y + markW + 5, StringAsCStr(strThrdCnt), NULL);
        }
        if (!list.empty()) {
            int32_t maxThread    = -1;
            float yStep          = 16.0f;
            int64_t minTimeStart = list[0].timeStart;
            for (auto entry : list) {
                minTimeStart = math::min(minTimeStart, entry.timeStart);
            }
            nvgSave(vg);
            nvgTranslate(vg, legendX, 0);
            for (int pass = 0; pass < 2; pass++) {
                nvgBeginPath(vg);
                for (auto entry : list) {
                    auto posX1         = graphOnlySize.x * (entry.timeStart - minTimeStart) / (float) mikrosPerBlock;
                    auto posX2         = graphOnlySize.x * (entry.timeEnd - minTimeStart) / (float) mikrosPerBlock;
                    float posY         = graphOnlySize.y - 1 - (entry.threadIdx + 1) * yStep;
                    float hGraph       = yStep * 0.8f;
                    if (pass == 0) {

                        nvgMoveTo(vg, posX1, posY + yStep / 2.0f);
                        nvgLineTo(vg, posX2, posY + yStep / 2.0f);
                    } else {
                        nvgMoveTo(vg, posX1, posY + yStep / 2.0f - yStep / 4.0f);
                        nvgLineTo(vg, posX1, posY + yStep / 2.0f + hGraph / 2.0f);
                        nvgMoveTo(vg, posX2, posY + yStep / 2.0f - hGraph / 2.0f);
                        nvgLineTo(vg, posX2, posY + yStep / 2.0f + hGraph / 2.0f);
                    }
                    maxThread = math::max(maxThread, static_cast<int32_t>(entry.threadIdx));
                }
                if (pass == 0) {
                    auto graphColor = colorOnlyPalette[(pass * 4 + 2) % colorOnlyPaletteLen];
                    nvgStrokeColor(vg, rgbToNvg(graphColor));
                    nvgStrokeWidth(vg, 2.0f);
                    nvgStroke(vg);
                } else {

                    nvgStrokeWidth(vg, 1.0f);
                    nvgStroke(vg);
                }
            }
            nvgRestore(vg);
            setFont(vg, 14, THEMECOL_TEXT, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
            float lineh;
            nvgTextMetrics(vg, NULL, NULL, &lineh);
            for (int i = 0; i <= maxThread; i++) {
                float posX  = 0;
                float posY  = graphOnlySize.y - 1 - (i + 1) * yStep + yStep / 2.0f;
                String proj = StringFormat("Thread #%d", i);
                nvgText(vg, posX, posY, StringAsCStr(proj), NULL);
            }
        }

        nvgRestore(vg);
        auto knobTestThreadCnt = getByID(ID_KNOB_SET_THREAD_COUNT);
        if (knobTestThreadCnt) {
            setFont(vg, 14, THEMECOL_TEXT, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
            String strThrdCnt = StringFormat("Number of Threads: %d", dawCtrl->getDaw()->getHost()->getThreadCount());
            ivec2 lblPos{ knobTestThreadCnt->right(), knobTestThreadCnt->bottom() - knobTestThreadCnt->size.y / 2 };
            nvgText(vg, lblPos.x, lblPos.y, StringAsCStr(strThrdCnt), NULL);
        }
    }

    if (dgbCtrType == DebugCtrType::DEBUG_APPCTRL) {
        auto const ctrl = dawCtrl;
        auto const daw = ctrl->getDaw();

        std::vector<String> strings;
        String str;
        str = StringFormat("%012zX", reinterpret_cast<int64_t>(ctrl->getTheme()));
        strings.push_back(String("ctrl->getTheme: ") + str);
        str = StringFormat("%012zX", parentCtrl ? reinterpret_cast<int64_t>(parentCtrl->getTheme()) : 0);
        strings.push_back(String("parentCtrl->getTheme: ") + str);
        str = StringFormat("%012zX", reinterpret_cast<int64_t>(theme));
        strings.push_back(String("this->theme: ") + str);
        auto guiOver = parentCtrl->getGuiOver();
        auto guiDragged = parentCtrl->getGuiDragged();
        auto guiCaptured = parentCtrl->getGuiCaptured();
        auto guiCtrFocused = parentCtrl->getGuiCtrFocused();
        auto guiFocused = parentCtrl->getGuiFocused();
        str = guiOver ? guiOver->getClassName() : "<null>";
        strings.push_back(String("guiOver: ") + str);
        str = guiDragged ? guiDragged->getClassName() : "<null>";
        strings.push_back(String("guiDragged: ") + str);
        str = guiCaptured ? guiCaptured->getClassName() : "<null>";
        strings.push_back(String("guiCaptured: ") + str);
        str = guiCtrFocused ? guiCtrFocused->getClassName() : "<null>";
        strings.push_back(String("guiCtrFocused: ") + str);
        str = guiFocused ? guiFocused->getClassName() : "<null>";
        strings.push_back(String("guiFocused: ") + str);
        str = "<null>";
        auto& target = ctrl->getDragDropTarget();
        const auto dragdropTargetGui = safeRefGet(target.target);
        strings.push_back(String("DragDropTarget: ") + (dragdropTargetGui ? dragdropTargetGui->getClassName() : "<null>"));
        guibase* p = guiFocused;
        int lvl    = 0;
        while (p) {
            String s = "";
            if (lvl == 0) {
                s = "guiFocused: ";
            }
            for (int i = 0; i < lvl; i++) {
                s += "  ";
            }
            strings.push_back(s + p->getClassName());
            p = p->parent;
            lvl++;
        }

        strings.push_back(String("lastKey: ") + ctrl->lastKeyDebug);
        strings.push_back(StringFormat("undo size: %zu", daw->getHist().getNumUndoSteps()));
        strings.push_back(StringFormat("redo size: %zu", daw->getHist().getNumRedoSteps()));
        auto editor = dawCtrl->getClipEditor();
        if (editor) {
            clip_view_t& clipView = editor->getClipView();
            if (clipView.clip()) {
                strings.push_back(StringFormat("Clip: %s", StringAsCStr(clipView.clip()->name)));
                strings.push_back(StringFormat("Notes: %zu", clipView.clip()->notes.m_list.size()));
                strings.push_back(StringFormat("Selection size: %zu", clipView.clip()->notes.selection.size()));
            }
        }
        auto host = dawCtrl->getDaw()->getHost();
        strings.push_back("sample format");
        strings.push_back(StringFormat(" samplerate: %u", host->m_sampleFormatInternal.sampleRate));
        strings.push_back(StringFormat(" blockSize : %u", host->m_sampleFormatInternal.blockSize));
        strings.push_back(StringFormat(" bit depth : %u", static_cast<int32_t>(host->m_sampleFormatInternal.sampleformat)));

        track_t* track = daw->getTrackId(0);
        if (track && track->audio) {
            strings.push_back(StringFormat("level: %.4f", track->audio->meter.getMaxRMS()));
        }
        const char* clipboardTypeNames[] = {
            "None", "Clip", "Note", "Plugin", "Track"
        };
        strings.push_back(String("ClipboardType: ") + clipboardTypeNames[(int)daw->getClipboardType()]);
        if (guiFocused && guiFocused->getGuiType() == gui_type::CTR_TYPE_PLUGIN) {
            guiplugin* gplugin = dynamic_cast<guiplugin*>(guiFocused);
            if (gplugin) {
                effectbase* effect = gplugin->getModule();
                strings.emplace_back("\n\n");
                effect->getInfo(strings);
            }
        }
        struct win32_msg {
            int id;
            int cnt;
        };
#if DISPLAY_HWND_DRAWS
        std::vector<win32_msg> wnd;
        for (int i = 0; i < msgCounter.getHWNDMapSize(); i++) {
            int cnt = msgCounter.getHWNDCnt(i);
            wnd.push_back({ i, cnt });
        }
        std::sort(wnd.begin(), wnd.end(), [](win32_msg const& a, win32_msg const& b) {
            return a.cnt > b.cnt;
        });
        for (win32_msg& msg : wnd) {
            String s = msgCounter.getHWNDName(msg.id);
            strings.push_back(StringFormat("%s: %d", StringAsCStr(s), msg.cnt));
        }
#endif

#if DISPLAY_WIN_MSG_STATS
        std::vector<win32_msg> msgs;
        for (int i = 0; i < msgCounter.getNumMsg(); i++) {
            int id  = msgCounter.getMsgId(i);
            int cnt = msgCounter.getMsgCnt(i);
            msgs.push_back({ id, cnt });
        }
        std::sort(msgs.begin(), msgs.end(), [](win32_msg const& a, win32_msg const& b) {
            if (a.cnt == b.cnt) {
                return a.id < b.id;
            }
            return a.cnt > b.cnt;
        });
        for (win32_msg& msg : msgs) {
            strings.push_back(StringFormat("WM_ 0x%04X: %d", msg.id, msg.cnt));
        }
#endif
        int x = 5;

        setFont(vg, 14, THEMECOL_TEXT, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
        float lineh;
        nvgTextMetrics(vg, NULL, NULL, &lineh);


        nvgText(vg, x, 0, StringAsCStr(label), NULL);
        String proj = StringFormat("Project: %s", StringAsCStr(ctrl->getProjectPath()));
        nvgText(vg, x, lineh, StringAsCStr(proj), NULL);
        int y = lineh * 3;
        for (String& s : strings) {
            nvgText(vg, x, y, StringAsCStr(s), NULL);
            y += lineh;
        }
        for (String& s : g_debugStrings) {
            nvgText(vg, x, y, StringAsCStr(s), NULL);
            y += lineh;
        }
    }
    for (auto c : guis) {
        nvgSave(vg);
        c->render(vg);
        nvgRestore(vg);
    }
    g_debugStrings.clear();
}
void gui_ctr_debug::layout() {
    ivec2 cs      = getSizeContent();
    int32_t size  = 32;
    auto knobTest = getByID(ID_OPTION_SCALE_GLOBAL);
    if (knobTest) {
        knobTest->size = ivec2(size);
        knobTest->pos  = ivec2(cs.x - knobTest->size.x, cs.y - knobTest->size.y);
    }
    auto knobTestThreadCnt = getByID(ID_KNOB_SET_THREAD_COUNT);
    if (knobTestThreadCnt) {
        knobTestThreadCnt->size = ivec2(size);
        knobTestThreadCnt->pos  = ivec2(0, cs.y - knobTestThreadCnt->size.y);
    }
    auto posY = cs.y;
    auto posX = 0;
    for (auto gui : guis) {
        gui->layout();
        if (gui == knobTest)
            continue;
        if (gui == knobTestThreadCnt)
            continue;
        gui->size = ivec2(math::max(size * 6, cs.x - size * 3), size);
        gui->pos  = ivec2(posX, posY - gui->size.y);
        posY      = gui->top() - INSET_TRACK_CONTENT;
    }
}

int32_t getNumClipAllocations();//clip.cpp
void resetHistAndCheck(DawInstance* daw) {
    daw->setEmptyClipboard();
    daw->getHist().clear(daw);

#ifndef NDEBUG
    int32_t liveClips = 0;
    auto& tracks = daw->getTracks();
    for (auto track : tracks) {
        int nTrackClips = track->getClips().getClips().size();
        liveClips += nTrackClips;
    }
    int32_t allocClips = getNumClipAllocations();
    if (allocClips != liveClips) {
        log_lf(Log::L_WARN, "Clip allocations (%d) does not match used clips (%d)\n", allocClips, liveClips);
    }
#endif
}

void gui_ctr_debug::buttonClicked(guibase* button) {
    auto const daw = dawCtrl->getDaw();
    auto const host = daw->getHost();
    switch (button->id) {
        case ID_BTN_UPDATE_VISIBLE_TRACK_CONTENTS:
            daw->updateVisibleTrackContents();
            break;
        case ID_BTN_RESET_RESAMPLERS: {
            auto lock = daw->getPlayThread()->lockThread();
            auto host = daw->getHost();
            host->resetResamplers();
            break;
        }
        case ID_BTN_UNLOAD_UNREFERENCED_AUDIOCACHE: {
            auto lock = daw->getPlayThread()->lockThread();
            daw->unloadUnreferencedSamples();
            break;
        }
        case ID_BTN_RESET_AUDIOCACHE: {
            auto cache = daw_tls::getTls().audioCache;
            if (cache) {
                auto lock = daw->getPlayThread()->lockThread();
                cache->unloadAll();
            }
            break;
        }
        case ID_BTN_RESET_HIST:
            resetHistAndCheck(daw);
            break;
        case ID_BTN_INJECT_SEGFAULT_AUDIO_THREAD:
            daw->getPlayThread()->call([]() {
              daw_test::debugRaiseSegFault();
            }, false);
            break;
        case ID_BTN_INJECT_BAD_MALLOC_AUDIO_THREAD:
            daw->getPlayThread()->call([]() {
                throw std::bad_alloc();
            }, false);
            break;
        case ID_BTN_INJECT_SEGFAULT_MAIN_THREAD:
            daw_test::debugRaiseSegFault();
            break;
        case ID_BTN_INJECT_BAD_MALLOC_MAIN_THREAD:
            throw std::bad_alloc();
            break;
        case ID_BTN_TOGGLE_PLAYBACKPROCESSING:
            host->bypassPlaybackProcessing = !host->bypassPlaybackProcessing;
            static_cast<guibutton*>(button)->setText(String(host->bypassPlaybackProcessing ? "Bypass Playback Proc. (ON)" : "Bypass Playback Proc. (OFF)"));
            break;
        case ID_BTN_TOGGLE_AUDIOGRAPHCACHE:
            host->cacheAudioGraph = !host->cacheAudioGraph;
            static_cast<guibutton*>(button)->setText(String(host->cacheAudioGraph ? "Use Audio Graph Cache (ON)" : "Use Audio Graph Cache (OFF)"));
            break;
        case ID_BTN_TOGGLE_64_BIT_SUMMING:
            host->bSummingIn64Bit = !host->bSummingIn64Bit;
            static_cast<guibutton*>(button)->setText(String(host->bSummingIn64Bit ? "64 bit summing (ON)" : "64 bit summing (OFF)"));
            break;
        case ID_BTN_TOGGLE_EFFECTPROCESSING:
            host->bypassEffectProcessing = !host->bypassEffectProcessing;
            static_cast<guibutton*>(button)->setText(String(host->bypassEffectProcessing ? "Bypass Effect Processing (ON)" : "Bypass Effect Processing (OFF)"));
            break;
        case ID_BTN_TOGGLE_SAMPLECONVERSION:
            host->bypassSampleConversion = !host->bypassSampleConversion;
            static_cast<guibutton*>(button)->setText(String(host->bypassSampleConversion ? "Bypass Sample conversion (ON)" : "Bypass Sample conversion (OFF)"));

            break;
        case ID_BTN_TOGGLE_CLIP_RENDER_CACHE:
            daw_tls::getTls().runtime->enableCache = !daw_tls::getTls().runtime->enableCache;
            static_cast<guibutton*>(button)->setText(String(daw_tls::getTls().runtime->enableCache ? "Disable clip render cache" : "Enable clip render cache"));

            break;
        case ID_BTN_TOGGLE_WAVEFORM_UPDATES:
            daw_tls::getTls().runtime->disableWaveformUpdates = !daw_tls::getTls().runtime->disableWaveformUpdates;
            static_cast<guibutton*>(button)->setText(String(daw_tls::getTls().runtime->disableWaveformUpdates ? "Enable waveform updates" : "Disable waveform updates"));

            break;
        case ID_BTN_TOGGLE_CLIPRENDERER_DEBUGLAYER:
            daw_tls::getTls().runtime->enableClipRendererDebugLayer = !daw_tls::getTls().runtime->enableClipRendererDebugLayer;
            static_cast<guibutton*>(button)->setText(String(!daw_tls::getTls().runtime->enableClipRendererDebugLayer ? "Enable clip renderer debug layer" : "Disable clip renderer debug layer"));

            break;
        case ID_BTN_TOGGLE_THREADING:
            daw->getPlayThread()->call([host]() {
                host->multithreadedProcessing = 1 - host->multithreadedProcessing;
            }, true);
            static_cast<guibutton*>(button)->setText(String(host->multithreadedProcessing ? "Multithreaded processing (ON)" : "Multithreaded processing (OFF)"));
            break;
        case ID_BTN_TOGGLE_CLAP_SAMPLEACCURATE_MODULATION: {
            auto lock = daw->getPlayThread()->lockThread();
            DAW::gClapUseSampleAccurateModulation = !DAW::gClapUseSampleAccurateModulation;
            static_cast<guibutton*>(button)->setText(String(DAW::gClapUseSampleAccurateModulation ? "Clap: Sample accurate modulation (ON)" : "Clap: Sample accurate modulation (OFF)"));
            break;
        }
        case ID_BTN_ADJUST_THEME_HUE:
        case ID_BTN_ADJUST_THEME_BR:
        case ID_BTN_ADJUST_THEME_SAT: {
            seq_rand rand;
            rand.rng_seed(getTimeMillis());
            auto randHsv = glm::vec3(rand.rng_double() * 360.0, rand.rng_double() * 0.5, 0.5 + rand.rng_double() * 0.5);
            if (button->id != ID_BTN_ADJUST_THEME_HUE) {
                randHsv.g = 0.5;
                randHsv.b = 0.5;
            }
            auto randomColor_vec3 = glm::rgbColor(randHsv);

            auto randomColor_u32 = vec3ToRgbU32(randomColor_vec3);
            gui_color_pick* color = new gui_color_pick();
            color->size = { 480, 240 };
            color->pos = { 0, 0 };
            auto mgr = parentCtrl->getThemeMgr();
            auto themeBaseCopy = mgr->getRef();
            color->fnSetValue = [mgr,themeBaseCopy, buttonId=button->id](uint32_t rgba) {
                mgr->getRef().setColorsFrom(themeBaseCopy);
                mgr->getRef().setThemeBaseColor(rgbaToNvg(rgba), vec3(
                                                                    buttonId == ID_BTN_ADJUST_THEME_HUE,
                                                                    buttonId == ID_BTN_ADJUST_THEME_SAT,
                                                                    buttonId == ID_BTN_ADJUST_THEME_BR
                                                                    ));

            };
            color->setU32(randomColor_u32);
            guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
            ctxtMenu->size = color->size;
            ctxtMenu->add(color);
            ctxtMenu->canTakeInputFocus = true;
            ctxtMenu->maxHeight = color->size.y;
            ctxtMenu->setBackgroundRendered(false);
            parentCtrl->openContextMenu(ctxtMenu, parentCtrl->lastMouseEvent.mousepos);
        }
        break;
    }
}

void gui_ctr_debug::onTick(AppCtrl* ctrl) {
    for (guibase* gui : guis) {
        gui->onTick(ctrl);
    }
    {
        ThreadLock lock = dawCtrl->getDaw()->getPlayThread()->tryLockThread();
        if (lock.isLocked()) {
            auto const host = dawCtrl->getDaw()->getHost();
            host->getBlockThreadStats(impl->lastProcessingList);
            impl->sampleformat = host->m_sampleFormatInternal;
        }
    }
}
