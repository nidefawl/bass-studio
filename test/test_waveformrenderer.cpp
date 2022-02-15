#include <nanovg.h>
#include <ctime>
#include <algorithm>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>

#include "appconfig.h"
#include "window.h"
#include "platform.h"
#include "fileio.h"

#include "keyboard.h"
#include "commands.h"

#include "basectrl.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "logging.h"
#include "tls.h"

#include "gui/gui.h"
#include "gui/guicontainer.h"

#include "audiocache.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"

#include "TestBase.hpp"
#include "assert_dbg.h"

#define NUM_RENDERERS 3u


int startApplication(const std::vector<String>& args);

struct waveform_test_entry {
    audiofile_t* sample{};
    gui_waveform_texture_ref ref;
    uint64_t duration{};
};
struct waveform_test {
    std::vector<std::vector<waveform_test_entry>> vecs;
    uint64_t durations[NUM_RENDERERS]{};
    waveformrender* rendererAdv{};
    waveformrender* rendererPolyline{};
    waveformrender* rendererPar{};
    std::vector<waveformrender*> renderers;
    void init() {
        daw_tls::tlsinstance& tls = daw_tls::getTls();
        int sampleRate            = 44100;
        tls.audioCache            = new audiocache(sampleRate);
        rendererAdv               = new waveformrender(pathrenderer_type_e::ADV);
        rendererPolyline          = new waveformrender(pathrenderer_type_e::POLYLINE2D);
        rendererPar               = new waveformrender(pathrenderer_type_e::PAR);
        renderers.push_back(rendererAdv);
        renderers.push_back(rendererPolyline);
        renderers.push_back(rendererPar);
        std::vector<waveform_test_entry> vec;
        std::vector<FileFound> files;
        findFilesWithExt("./cpp-test-data/", "wav", false, files);
        log_printf("findFilesWithExt %d\n", files.size());
        for (auto i = 0u; i < files.size() && vec.size() < 8; i++) {
            size_t filesize = GetFileSizeSafe(files[i].path);
            if (filesize > 1024 * 1024 * 32) {
                continue;
            }
            log_printf("loadFile %s\n", StringAsCStr(files[i].path));
            auto sample = tls.audioCache->loadFile(files[i].path);
            if (!sample) {
                log_printf("Failed loading sample %s\n", StringAsCStr(files[i].path));
            } else {
                vec.push_back(waveform_test_entry{ sample, gui_waveform_texture_ref{}, 0u });
            }
        }
        if (vec.empty()) {
            throw appexception("Failed loading test samples");
        }
        for (auto i = 0u; i < NUM_RENDERERS; i++) {
            vecs.push_back(vec);
        }
    }
    static void renderUpdate(NVGcontext* nanovgCtxt, waveformrender* renderer, waveform_test_entry* e, uint32_t renderStep) {
        auto& ref   = e->ref;
        auto sample = e->sample;
        if (!ref.queued && (!ref.rendered || ref.waveform.audioId != sample->id)) {
            if (ref.rendered) {
                renderer->release(&ref);
                ref.rendered = false;
            }
            auto& w             = ref.waveform;
            w.audioId           = sample->id;
            w.sampleBegin       = 0;
            w.sampleEnd         = sample->sample->nSamples * 30 / math::max<int64_t>(1, (renderStep % 300) + 1);
            w.sampleBeginOffset = 0;
            w.scaleX            = 1.0f;
            w.quality           = 1;
            w.size              = ivec2(512, 64);
            auto nSamples       = w.sampleEnd - w.sampleBeginOffset;
            double samplesPerPx = nSamples / w.size.x;
            double pxPerSample  = 1.0 / samplesPerPx;
            if (nSamples * pxPerSample > FBO_WIDTH) {
                samplesPerPx = (nSamples / FBO_WIDTH);
            }
            w.samplesPerPx = samplesPerPx;
            w.linewidth    = 3.5f;//+min(0.75, max(0.0, grid.zoom*32.0));
            w.method       = SampleMethod::sample_straight;
            w.clipped      = false;
            renderer->queueUpdate(sample, &ref);
            renderer->renderUpdates(nanovgCtxt, 0);
        }
    }
};
namespace MiniApp {
    class guictr_TestNanoVGRenderCache : public guictr_base {
    public:
        guictr_TestNanoVGRenderCache() : guictr_base() {
        }

        void render(NVGcontext* vg) override {
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
            for (auto c : guis) {
                nvgSave(vg);
                c->render(vg);
                nvgRestore(vg);
            }
        }
        void prerender(NVGcontext* vg) override {
        }
    };
    class ViewContainers_TestNanoVGRenderCache {
    public:
        guictr_TestNanoVGRenderCache ctrMain;
        explicit ViewContainers_TestNanoVGRenderCache(/*const*/ AppCtrl* const ctrl) : ctrMain() {
        }
#if USE_GUI_MENU
        guictr_base* getMenuCtr() {
            return nullptr;
        }
#endif
        void layout(int32_t winW, int32_t winH) {
            int winX     = 0;
            int winY     = 0;
            ctrMain.pos  = { winX, winY };
            ctrMain.size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) {
            v.push_back(&ctrMain);
        }
    };
    template<typename T>
    class MiniAppCtrl : public AppCtrl {
        T* view = nullptr;
        waveform_test& waveformTest;
        uint64_t tmLastRelease = 0;
        uint32_t renderStep      = 0;
        hires_timer_t timer;
        hires_timer_t timerAll;

    public:
        explicit MiniAppCtrl(waveform_test& _waveformTest)
            : waveformTest(_waveformTest) {
        }
        void focusReceived() override {
        }
        void focusLost() override {
        }

        void destroy() override {
            if (!isOK) {
                return;
            }
            isOK = false;
            delete view;

            for (auto* renderer : waveformTest.renderers) {
                renderer->destroy();
            }
            daw_tls::tlsinstance& tls = daw_tls::getTls();
            delete tls.audioCache;
            tls.audioCache = nullptr;
        }

        void menuCommand(const menucmd_t&& command) override {
            switch (command.command) {
                case CMD_EXIT:
                    mainWindow->requestClose();
                    break;
            }
        }
        void startApp() override {
            for (auto* renderer : waveformTest.renderers) {
                renderer->init();
            }
        }
        void initApp(const std::vector<String>& args) override {
            daw_tls::tlsinstance _tls;
            _tls.tlsInitialized = true;
            _tls.config         = new app_config_t{};
            daw_tls::setTls(_tls);
            waveformTest.init();
        }


        void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override {
            BaseCtrl::render(nanovgCtxt, x, y, w, h, pixelRatio);
            nvgBeginFrame(vg, w, h, pixelRatio);
            nvgScale(vg, m_scale, m_scale);
            //ivec2 offsetPos(0, 0);
            for (auto i = 0u; i < NUM_RENDERERS; i++) {
                std::vector<waveform_test_entry>& vec = waveformTest.vecs[i];

                int yPos = 0;
                for (waveform_test_entry& e : vec) {
                    if (e.ref.rendered) {
                        auto& ref    = e.ref;
                        ivec2 wvSize = ref.waveform.size;

                        ivec2 offsetPos(i * wvSize.x + 10, yPos * wvSize.y + 10);
                        nvgSave(vg);
                        nvgTranslate(nanovgCtxt, offsetPos.x, offsetPos.y);
                        nvgBeginPath(vg);
                        nvgRect(vg, 0, 0, wvSize.x, wvSize.y);
                        nvgFillColor(vg, rgbToNvg(0xFFFFFF));
                        nvgFill(vg);
                        nvgBeginPath(vg);
                        nvgRect(vg, 2, 2, wvSize.x - 4, wvSize.y - 4);
                        nvgFillColor(vg, rgbToNvg(0xFF00FF));
                        nvgFill(vg);
                        waveformTest.renderers[i]->draw(vg, &ref, ref.waveform.size);
                        ivec2 txt(70, 14);
                        nvgBeginPath(vg);
                        nvgRect(vg, 10, wvSize.y - txt.y, txt.x, txt.y);
                        nvgFillColor(vg, rgbToNvg(0x0));
                        nvgFill(vg);
                        UTIL_setFont(vg, &themes.getRef(), txt.y - 2, rgbaToNvg(0xffffffff), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                        String str = StringFormat("%llumicros", e.duration);
                        nvgTextBox(vg, 12, wvSize.y - txt.y / 2, txt.x, StringAsCStr(str), nullptr);

                        nvgRestore(vg);
                        //offsetPos.y += wvSize.y + 10;
                        //if (offsetPos.y + wvSize.y >= h) {
                        //    offsetPos.y = 0;
                        //    offsetPos.x += wvSize.x + 10;
                        //}
                        yPos++;
                    }
                }
                ivec2 wvSize = waveformTest.vecs[0][0].ref.waveform.size;
                ivec2 txt(70, 14);
                ivec2 offsetPos(i * wvSize.x + 10, yPos * wvSize.y + 10);
                nvgSave(vg);
                nvgTranslate(nanovgCtxt, offsetPos.x, offsetPos.y);
                UTIL_setFont(vg, &themes.getRef(), txt.y - 2, rgbaToNvg(0xffffffff), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                String names[3] = { "ADV", "POLYLINE", "PAR" };
                nvgTextBox(vg, 12, wvSize.y - txt.y / 2, txt.x, StringAsCStr(names[i]), nullptr);
                nvgRestore(vg);
            }

            nvgEndFrame(vg);
        }
        void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override {

            for (guictr_base* ctr : containers) {
                ctr->prerender(nanovgCtxt);
            }
            if (getTimeMillis() - tmLastRelease >= 60) {
                tmLastRelease = getTimeMillis();
                for (auto i = 0u; i < NUM_RENDERERS; i++) {
                    std::vector<waveform_test_entry>& vec = waveformTest.vecs[i];
                    for (waveform_test_entry& e : vec) {
                        auto& ref = e.ref;
                        if (ref.rendered) {
                            waveformTest.renderers[i]->release(&ref);
                            ref.rendered = false;
                        }
                    }
                }
                timerAll.reset();
                for (auto i = 0u; i < NUM_RENDERERS; i++) {
                    int64_t lTook                         = 0L;
                    std::vector<waveform_test_entry>& vec = waveformTest.vecs[i];
                    for (waveform_test_entry& e : vec) {
                        timer.reset();
                        waveform_test::renderUpdate(nanovgCtxt, waveformTest.renderers[i], &e, renderStep);
                        lTook      = timer.getTime();
                        e.duration = lTook;
                        waveformTest.durations[i] += lTook;
                    }
                }
                log_printf("took %llu\n", timerAll.getTime());

                renderStep++;
            }
            if (renderStep >= 100) {
                log_printf("request close!\n", 0);
                this->mainWindow->requestClose();
            }
        }
        bool initAppWindow(window_main* window, NVGcontext* nanovg) override {
            this->mainWindow = window;
            this->window     = window;
            this->vg         = nanovg;
            themes.loadThemes();

            view = new T(this);
            view->addTo(this->containers);
            for (guictr_base* ctr : containers) {
                ctr->setControl(this);
            }

            this->updateMenubar();
#if !USE_GUI_MENU
            this->mainWindow->updateMenu();
#endif
            isOK = true;
            return isOK;
        }

        void onTick() override {
            for (guictr_base* ctr : containers) {
                ctr->onTick(this);
            }
            mainWindow->requestRedraw();
        }

        void relayout(int32_t w, int32_t h) override {
            closeAllAppMenus();
            closeContextMenu();
            view->layout(w, h);

            for (guictr_base* ctr : containers) {
                ctr->layout();
            }
        }
        void mouseMoved(ivec2 mousePos, ivec2 deltaPos) override {
            BaseCtrl::mouseMoved(mousePos, deltaPos);
        }

        bool processGlobalKeyevent(KeyEvent& event) override {
            return false;
        }

        bool mouseDownPre() override {
            closeAllContextMenus();
            return true;
        }

        void setStatusText(String s) {
            view->statusbar.setTitle(s);
        }
    };
    static std::shared_ptr<AppCtrl> appctrl;
}// namespace MiniApp


static waveform_test waveformTest;

std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) {
    MiniApp::appctrl = std::make_shared<MiniApp::MiniAppCtrl<MiniApp::ViewContainers_TestNanoVGRenderCache>>(waveformTest);
    MiniApp::appctrl->initApp(args);
    return MiniApp::appctrl;
}

void startApp(std::shared_ptr<AppCtrl>& app) {
    app->startApp();
}


void deleteApp() {
    MiniApp::appctrl.reset();
}

int main(int argc, char* argv[]) {
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    int ret = startApplication(vecArgs);
    for (auto i = 0u; i < NUM_RENDERERS; i++) {
        log_printf("Renderer %u took %llumicros\n", i, waveformTest.durations[i]);
    }
    return ret;
}
