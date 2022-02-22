#include "glheaders.h"
#include "about.h"
#include "math/vec.h"
#include "str_util.h"
#include "button.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "buildinfo.h"
#include <nanovg.h>
#ifdef _WIN32
#include <Windows.h>
#endif

constexpr int ID_BTN_CLOSE    = 1;
constexpr int BTN_FONT_SIZE   = 16;

guidialog_about::guidialog_about() : guidialog_base(ivec2{440, 560}) {
    setBackgroundRendered(true);
    add(&btnClose);
    btnClose.id = ID_BTN_CLOSE;
    btnClose.setText("Close");
    setLabel("About");
    strings.emplace_back(String("Version: "), String(BuildInfo::BUILD_BINARY_VERSION));
    strings.emplace_back(String("Platform: "), String(BuildInfo::BUILD_BINARY_NAME));
#if defined(_WIN32) && defined(WINVER)
    strings.emplace_back(String("WINVER: "), StringFormat("0x%04X", WINVER));
#endif
    strings.emplace_back(String("GL_RENDERER: "), String((char*)glGetString(GL_RENDERER)));
    strings.emplace_back(String("GL_VERSION: "), String((char*)glGetString(GL_VERSION)));
    strings.emplace_back(String("GL_VENDOR: "), String((char*)glGetString(GL_VENDOR)));

    strings.emplace_back(String("BUILD_TIMESTAMP: "), String(BuildInfo::BUILD_TIMESTAMP));
    strings.emplace_back(String("COMPILER_ID: "), BuildInfo::COMPILER_ID);
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
    float y = 0;
    renderTextLabel(vg,
        vec2(x, y), 
        vec2(size.x, titleHeight), 
        label,
        theme,
        titleHeight, 
        theme->getContrastColor(GuiColor::COL_BG_DRKER), NVG_ALIGN_TOP | NVG_ALIGN_LEFT);

    y += titleHeight;
    
    auto yPosLines = y;
    for (AboutLine& t : strings) {
        renderTextLabel(vg,
            vec2(x, y), 
            vec2(size.x*0.5f, rowHeight), 
            std::get<0>(t),
            theme,
            rowHeight, 
            theme->getContrastColor(GuiColor::COL_BG_DRKER), NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
        nvgText(vg, x, y, StringAsCStr(std::get<0>(t)), nullptr);
        y += rowHeight;
    }
    auto width  = getSizeContent().x;
    auto xRight = x + width;
    nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
    y = yPosLines;
    for (const AboutLine& t : strings) {
        renderTextLabel(vg,
            vec2(xRight, y), 
            vec2(size.x*0.5f, rowHeight), 
            std::get<1>(t),
            theme,
            rowHeight, 
            theme->getContrastColor(GuiColor::COL_BG_DRKER), NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        y += rowHeight;
    }

    const auto rowHeightDet = rowHeight*0.75f;
    float bounds[4]{0};
    xRight = x + width / 6;

    for (const DetailedAbout& t : strings2) {
        renderTextLabel(vg,
            vec2(x, y), 
            vec2(size.x*0.5f, rowHeightDet), 
            std::get<0>(t),
            theme,
            rowHeightDet, 
            theme->getContrastColor(GuiColor::COL_BG_DRKER), NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);

        y += rowHeightDet;
        nvgTextBoxBounds(vg, xRight, y, width - xRight, std::get<1>(t), nullptr, bounds);
        nvgTextBox(vg, xRight, y, width - xRight, std::get<1>(t), nullptr);
        y = bounds[3];
        y += rowHeightDet;
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
    btnClose.setFontSize(rowHeight);
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
}
