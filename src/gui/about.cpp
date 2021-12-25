#include "glheaders.h"
#include "about.h"
#include "math/vec.h"
#include "str_util.h"
#include "button.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "buildinfo.h"
#ifdef _WIN32
#include <windows.h>
#endif

constexpr int ID_BTN_CLOSE    = 1;
constexpr int TITLE_FONT_SIZE = 26;
constexpr int TEXT_FONT_SIZE  = 18;
constexpr int BTN_FONT_SIZE   = 16;

guidialog_about::guidialog_about() : guidialog_base(ivec2{440, 560}) {
    setBackgroundRendered(true);
    add(&btnClose);
    btnClose.id = ID_BTN_CLOSE;
    btnClose.setText("Close");
    btnClose.setFontSize(BTN_FONT_SIZE);
    setLabel("About");
}

void guidialog_about::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }

    using AboutLine = std::tuple<String, String>;
    using std::make_tuple;
    std::vector<AboutLine> strings;
    String str;
    strings.emplace_back(String("Version: "), String(BuildInfo::BUILD_BINARY_VERSION));
    strings.emplace_back(String("Platform: "), String(BuildInfo::BUILD_BINARY_NAME));
    strings.emplace_back(String("Compiled: "), String(BuildInfo::BUILD_TIMESTAMP));
    strings.emplace_back(String("Compiler ID: "), String(BuildInfo::COMPILER_ID));
#if defined(_WIN32) && defined(WINVER)
    strings.emplace_back(String("WINVER: "), StringFormat("0x%04X", WINVER));
#endif
    strings.emplace_back(String("GL_RENDERER: "), StringFormat("%s", glGetString(GL_RENDERER)));
    strings.emplace_back(String("GL_VERSION: "), StringFormat("%s", glGetString(GL_VERSION)));
    strings.emplace_back(String("GL_VENDOR: "), StringFormat("%s", glGetString(GL_VENDOR)));
    int x = 0;
    int y = 0;
    float lineh;
    setFont(vg, TITLE_FONT_SIZE, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
    nvgTextMetrics(vg, NULL, NULL, &lineh);
    y += lineh;
    nvgText(vg, x, 0, StringAsCStr(label), NULL);
    setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
    nvgTextMetrics(vg, NULL, NULL, &lineh);
    y += lineh;
    int yPosLines = y;
    for (AboutLine& t : strings) {
        nvgText(vg, x, y, StringAsCStr(std::get<0>(t)), NULL);
        y += lineh;
    }
    int width  = getSizeContent().x;
    int xRight = x + width;
    nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
    y = yPosLines;
    for (AboutLine& t : strings) {
        const char* c = StringAsCStr(std::get<1>(t));
        nvgText(vg, xRight, y, c, NULL);
        y += lineh;
    }
    float bounds[4]{0};
    xRight = x + width / 6;

    nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);

    nvgText(vg, x, y, "Compiler Path:", NULL);
    y += lineh;
    nvgTextBoxBounds(vg, xRight, y, width - xRight, BuildInfo::COMPILER_PATH, nullptr, bounds);
    nvgTextBox(vg, xRight, y, width - xRight, BuildInfo::COMPILER_PATH, nullptr);
    y = bounds[3];

    nvgText(vg, x, y, "Flags:", NULL);
    y += lineh;
    nvgTextBoxBounds(vg, xRight, y, width - xRight, BuildInfo::COMPILE_OPTIONS, nullptr, bounds);
    nvgTextBox(vg, xRight, y, width - xRight, BuildInfo::COMPILE_OPTIONS, nullptr);
    y = bounds[3];

    nvgText(vg, x, y, "Defs:", NULL);
    y += lineh;
    nvgTextBoxBounds(vg, xRight, y, width - xRight, BuildInfo::COMPILE_DEFS, nullptr, bounds);
    nvgTextBox(vg, xRight, y, width - xRight, BuildInfo::COMPILE_DEFS, nullptr);
    y = bounds[3];
    for (auto c : guis) {
        nvgSave(vg);
        c->render(vg);
        nvgRestore(vg);
    }
}
void guidialog_about::layout() {
    ivec2 cs      = getSizeContent();
    int32_t size  = 32;
    btnClose.size = ivec2(size * 4, size);
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
