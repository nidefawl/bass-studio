#include <istream>
#include <nanovg.h>
#include <ctime>
#include <algorithm>
#include <streambuf>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>

#include "appconfig.h"
#include "math/seq_math.h"
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
#include "gui/container/container.h"

#include "TestBase.hpp"
#include "assert_dbg.h"

namespace test_fontrenderer {
struct membuf : std::streambuf
{
    template<typename T>
    membuf(T* data, size_t len) {
        auto const begin = reinterpret_cast<char*>(data);
        auto const end = reinterpret_cast<char*>(data + len);
        this->setg(begin, begin, end);
    }
};

class MiniAppCtrl : public AppCtrl {
    std::vector<String> strings;    
    int numFrames = 0;
    float fTime = 0.0f;
    float fTimeStart = 0.0f;
public:

    void startApp() override {
    }

    void initApp(const std::vector<String>& args) override {
        daw_tls::initNewTls();
        const auto filepath = "cpp-test-data/word_dict.txt";
        try {
            std::vector<uint8_t> vec;
            ReadFileVector(filepath, vec);
            if (vec.empty()) {
                log_printf("%s is empty\n", filepath);
                return;
            }
            membuf sbuf(vec.data(), vec.size());
            std::istream in(&sbuf);
            std::string line;
            while (strings.size() < 25 && std::getline(in, line)) {
                strings.push_back(line);
            }
            while (true) {
                String concat;
                if (!std::getline(in, line)) break;
                concat += line;
                concat += " ";
                if (!std::getline(in, line)) break;
                concat += line;
                concat += " ";
                if (!std::getline(in, line)) break;
                concat += line;
                strings.push_back(concat);
            }
        } catch (const std::exception& e) {
            log_printf("While reading %s: %s\n", filepath, e.what());
        }
        fTimeStart = getTimeMillisF();
    }

    bool initAppWindow(window_main* window, NVGcontext* nanovg) override {
        this->mainWindow = window;
        this->window     = window;
        this->vg         = nanovg;
        themes.loadThemes();
        isOK = true;
        return isOK;
    }

    void destroy() override {
        if (!isOK) {
            return;
        }
        isOK = false;
    }

    void relayout(int32_t w, int32_t h) override {
        closeAllAppMenus();
        closeContextMenu();
    }

    void onTick() override {
        if (getTimeMillisF() - fTimeStart > 20000.0f) {
            mainWindow->requestClose();
        }
    }

    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override {
        BaseCtrl::render(nanovgCtxt, x, y, w, h, pixelRatio);
        nvgBeginFrame(vg, w, h, pixelRatio);
        nvgScale(vg, m_scale, m_scale);
        size_t noffset = (getTimeMillis()/200L) % strings.size();
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            vec2 sizeRender = vec2(w, h) * 0.5f;
            vec2 posRender = sizeRender * vec2(quadrant%2, quadrant/2);
            nvgBeginPath(vg);
            nvgRect(vg, posRender.x, posRender.y, sizeRender.x, sizeRender.y);
            nvgFillColor(vg, dbgcolorsArray[quadrant]);
            nvgFill(vg);

            float mInner = 0.8f;
            posRender += sizeRender * (1.0f-mInner) * 0.5f;
            sizeRender *= mInner;
            nvgSave(vg);
            nvgTranslate(vg, posRender.x, posRender.y);
            nvgIntersectScissor(vg, 0, 0, sizeRender.x, sizeRender.y);
            vec2 offsetPos(0);
            vec2 offsetPosMultiline(0);
            
            if (fmodf(getTimeMillisF(),7777.0f)/7777.f > 0.2f) {
                fTime = getTimeMillisF();
            }

            float fProgress = fmodf(fTime,12000.0f)/12000.f;
            fProgress = math::abs(fProgress * 2.f - 1.f) * 1.2f - 0.1f;
            float breakRowWidth = math::clamp(sizeRender.x * fProgress, 0.0f, sizeRender.x);
            numFrames++;
            if (numFrames < 400)
                breakRowWidth = 10000.0f;
            if (numFrames < 300)
                breakRowWidth = 0.0f;
            if (numFrames < 200)
                breakRowWidth = 1.0f;
            if (numFrames < 100)
                breakRowWidth = -1.0f;
            
            for (int i = 0; noffset+i < strings.size() && offsetPos.y+32 < sizeRender.y; ++i) {
                int horizAlign = 1 << (i%3);
                int vertAlign =  1 << (((i/3)%5)+3);
                float fontsize = 8.0f + (i%8) * 6.0f;
                if (horizAlign&NVG_ALIGN_LEFT) {
                    offsetPos.x = 0;
                }
                if (horizAlign&NVG_ALIGN_CENTER) {
                    offsetPos.x = sizeRender.x * 0.5f;
                }
                if (horizAlign&NVG_ALIGN_RIGHT) {
                    offsetPos.x = sizeRender.x;
                }

                if (quadrant == 0) {
                    UTIL_setFont(vg, getTheme(), fontsize, rgbaToNvg(0xffffffff), horizAlign | vertAlign);
                    nvgTextBox(vg, 0, offsetPos.y, breakRowWidth, StringAsCStr(strings[noffset+i]), nullptr);
                }
                if (quadrant == 1) {
                    renderTextLabel(vg, offsetPos, vec2(breakRowWidth, fontsize + 4.0f), strings[noffset+i],
                                    getTheme(), fontsize, rgbaToNvg(0xffffffff), horizAlign | vertAlign);
                }
                if (quadrant == 2) {
                    auto sizeMultiLineBox = vec2(breakRowWidth, fontsize*2.0f + 4.0f);
                    offsetPosMultiline.x = (sizeRender.x-sizeMultiLineBox.x)*0.5f * i/20.0f;
                    renderCenteredMultilineText(vg, getTheme(), strings[noffset+i], 
                            fontsize, GuiColor::COL_LABEL_ACTIVE, offsetPosMultiline, sizeMultiLineBox);
                    offsetPosMultiline.y += sizeMultiLineBox.y;
                }      
                if (quadrant == 3) {
                    auto sizeMultiLineBox = vec2(sizeRender.x*(0.2f+0.8f*i/20.0f), fontsize*2.0f + 4.0f);
                    auto posFixed = vec2(20.0f, i*70.f);
                    if (i > 5) posFixed.x = sizeRender.x*0.33f;
                    renderCenteredMultilineText(vg, getTheme(), strings[i == 0 ? 0 : strings.size() - 1 - i], 
                            23.0f, GuiColor::COL_LABEL_ACTIVE, posFixed, sizeMultiLineBox);
                }      
                offsetPos.y += fontsize;
            }
            nvgRestore(vg); 
        }
        nvgEndFrame(vg);
        
    }
};

class Instance : public AppInstanceService {
  std::shared_ptr<AppCtrl> appctrl;
public:
  std::shared_ptr<AppCtrl> makeApp(const std::vector<String> &args) override {
    appctrl = std::make_shared<MiniAppCtrl>();
    appctrl->initApp(args);
    return appctrl;
  }

  void startApp(std::shared_ptr<AppCtrl> &app) override { app->startApp(); }

  void deleteApp() override { appctrl.reset(); }
};

}// namespace test_fontrenderer

int main(int argc, char* argv[]) {
    test_fontrenderer::Instance instService;
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    int ret = startApplication(vecArgs, instService);
    return ret;
}
