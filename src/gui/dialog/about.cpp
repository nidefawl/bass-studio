#include "glheaders.h"
#include "about.hpp"
#include "gui/dialog/about.hpp"
#include "math/vec.hpp"
#include "str_util.hpp"
#include "gui/controls/button.hpp"
#include "gui/container/container.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "buildinfo.h"
#include "tls.hpp"
#include "appconfig.hpp"
#include <nanovg.h>
#ifdef _WIN32
#include <windows.h>
#endif

constexpr int ID_BTN_CLOSE    = 1;
guidialog_about::guidialog_about() : guidialog_base(ivec2{560, 640}) {
    setBackgroundRendered(true);
    add(&btnClose);
    btnClose.id = ID_BTN_CLOSE;
    btnClose.setText("Close");
    setLabel("About");
    strings.emplace_back(String("Version: "), String(BuildInfo::PRODUCT_NAME_DISPLAY) + " "+ String(BuildInfo::BUILD_BINARY_VERSION));
    auto& systeminfo = daw_tls::getTls().runtime->systeminfo;
    strings2.emplace_back("GIT SHA1", BuildInfo::GIT_SHA1);
    strings2.emplace_back("GL_RENDERER: ", systeminfo.glRenderer.c_str());
    strings2.emplace_back("GL_VERSION: ", systeminfo.glVersion.c_str());
    strings2.emplace_back("BUILD_TIMESTAMP: ", BuildInfo::BUILD_TIMESTAMP);
    strings2.emplace_back("COMPILER_ID: ", BuildInfo::COMPILER_ID);
    strings2.emplace_back("COMPILER_PATH: ", BuildInfo::COMPILER_PATH);
    strings2.emplace_back("COMPILE_OPTIONS: ", BuildInfo::COMPILE_OPTIONS);
    strings2.emplace_back("COMPILE_DEFS: ", BuildInfo::COMPILE_DEFS);
}

void guidialog_about::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }

    const float titleHeight = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
    const float rowHeight = theme->get(GuiConstant::CONST_ROW_HEIGHT);

    float x = 0;
    float y = titleHeight * 0.5f;
    renderText(vg,
        vec2(x, y), 
        vec2(size.x, titleHeight), 
        label,
        titleHeight);

    y += titleHeight;
    
    auto yPosLines = y;
    for (AboutLine& t : strings) {
        renderText(vg,
            vec2(x, y+rowHeight*0.5f), 
            vec2(size.x*0.5f, rowHeight), 
            std::get<0>(t),
            rowHeight);
        y += rowHeight;
    }
    auto width  = getSizeContent().x;
    auto xRight = x + width;
    y = yPosLines;
    for (const AboutLine& t : strings) {
        renderText(vg,
            vec2(xRight, y+rowHeight*0.5f), 
            vec2(size.x*0.5f, rowHeight), 
            std::get<1>(t),
            rowHeight, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        y += rowHeight;
    }

    const auto rowHeightDet = rowHeight*0.75f;
    float bounds[4]{0};
    xRight = x + width / 6.0f;

    for (const DetailedAbout& t : strings2) {
        renderText(vg,
            vec2(x, y+rowHeightDet*0.5f), 
            vec2(size.x*0.5f, rowHeightDet), 
            std::get<0>(t),
            rowHeightDet);

        y += rowHeightDet + 2;
        nvgTextBoxBounds(vg, xRight, y+rowHeightDet*0.5f, width - xRight, std::get<1>(t), nullptr, bounds);
        nvgTextBox(vg, xRight, y+rowHeightDet*0.5f, width - xRight, std::get<1>(t), nullptr);
        y = bounds[3] + 5;
    }

    for (auto c : guis) {
        nvgSave(vg);
        c->render(vg);
        nvgRestore(vg);
    }
}
void guidialog_about::layout() {
    ivec2 cs      = getSizeContent();
    const auto rowHeight = theme->get(GuiConstant::CONST_ROW_HEIGHT);
    btnClose.size = ivec2(rowHeight * 4, rowHeight);
    btnClose.pos  = ivec2(cs.x - btnClose.size.x, cs.y - btnClose.size.y);
    for (auto gui : guis) {
        gui->layout();
    }
}

void guidialog_about::buttonClicked(guibase* button) {
    switch (button->id) {
        case ID_BTN_CLOSE:
            closeContextMenu();
            break;
    }
    guidialog_base::buttonClicked(button);
}
