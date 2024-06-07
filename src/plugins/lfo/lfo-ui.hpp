#pragma once
#include "lfo-types.hpp"
#include "gui/contextmenu/contextmenu_base.h"

namespace DAW::LFO {

class ctxtmenu_lfo_base : public ctxtmenu_entry {
protected:
    int32_t channel;
    int32_t perRowEntries;

    struct _entry {
        int id;
        int x;
        int y;
        int w;
        String name;
    };
public:
    std::vector<_entry> entries;
public:
    const int pad   = 10;
    const int inset = 5;
public:
    ctxtmenu_lfo_base(int32_t _channel, String _title, int _id)
        : ctxtmenu_entry(std::move(_title), _id),
        channel(_channel)
    {
    }

    void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
        width = size.x;
        this->fontSize = _fontSize;
        const int h    = math::roundfS32(_fontSize);
        layoutE(width, h, perRowEntries);
    }

    void layoutE(int tw, int h, int perRow) {
        int iX      = inset;
        int iY      = h + 2;
        int elW     = (tw - inset * 2) / perRow;
        for (auto& e : entries) {
            this->height = iY + h;
            e.x = iX;
            e.y = iY;
            e.w = elW;
            iX += e.w;
            if (iX >= tw - inset * 2) {
                iX = inset;
                iY += h;
            }
        }
    }

    bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
        return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
    }

    int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
        if (contains(ctxtSize, mouse)) {
            const auto h = this->fontSize;
            for (auto& e : entries) {
                if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                    return this->id + e.id;
                }
            }
        }
        return -1;
    }
};

template<typename ModuleType>
class ctxtmenu_lfo_sync : public ctxtmenu_lfo_base {
    ModuleType* const moduleInstance;
public:
    ctxtmenu_lfo_sync(ModuleType* _module, int32_t _channel, String _title, int _id)
        : ctxtmenu_lfo_base(_channel, _title, _id), moduleInstance(_module)
    {
        entries.push_back({ PluginLFO::NoteRatio::STRAIGHT, 0, 0, 0, "Straight" });
        entries.push_back({ PluginLFO::NoteRatio::TRIPLET, 0, 0, 0, "Triplet" });
        entries.push_back({ PluginLFO::NoteRatio::DOTTED, 0, 0, 0, "Dotted" });
        entries.push_back({ 0, 0, 0, 0, "Off" });
        perRowEntries = 3;
    }


    void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
        auto h = fontSize * 1.1f;

        int32_t sync = moduleInstance->getSyncRatio(channel);
        for (auto& e : entries) {
            if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                nvgBeginPath(vg);
                nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }
            if ((e.id&sync) || (e.id == 0 && sync == 0)) {
                nvgBeginPath(vg);
                nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                nvgFill(vg);
            }
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + h * 0.5f),
                        vec2(width, h),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (auto& e : entries) {
            renderTextLabel(vg,
                            vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                            vec2(width, h),
                            e.name,
                            theme,
                            fontSize * 0.9f,
                            theme->getColor(GuiColor::COL_TEXT),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
};

template<typename ModuleType>
class ctxtmenu_lfo_mode final : public ctxtmenu_lfo_base {
    ModuleType* const moduleInstance;
public:
    ctxtmenu_lfo_mode(ModuleType* _module, int32_t _channel, String _title, int _id)
        : ctxtmenu_lfo_base(_channel, _title, _id), moduleInstance(_module)
    {
        entries.push_back({ 0, 0, 0, 0, "Shape" });
        entries.push_back({ 1, 0, 0, 0, "Random" });
        perRowEntries = 2;
    }

    void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
        auto h = fontSize * 1.1f;

        int32_t isShapeMode = moduleInstance->isShapeMode(channel);
        for (auto& e : entries) {
            if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                nvgBeginPath(vg);
                nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }
            if ((e.id == 0) == isShapeMode) {
                nvgBeginPath(vg);
                nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                nvgFill(vg);
            }
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + h * 0.5f),
                        vec2(width, h),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (auto& e : entries) {
            renderTextLabel(vg,
                            vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                            vec2(width, h),
                            e.name,
                            theme,
                            fontSize * 0.9f,
                            theme->getColor(GuiColor::COL_TEXT),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
};

template<typename ModuleType>
class ctxtmenu_lfo_random_mode final : public ctxtmenu_lfo_base {
    ModuleType* const moduleInstance;
public:
    ctxtmenu_lfo_random_mode(ModuleType* _module, int32_t _channel, String _title, int _id)
        : ctxtmenu_lfo_base(_channel, _title, _id), moduleInstance(_module)
    {
        entries.push_back({ 0, 0, 0, 0, "Smooth" });
        entries.push_back({ 1, 0, 0, 0, "Linear" });
        entries.push_back({ 2, 0, 0, 0, "Exponential" });
        entries.push_back({ 3, 0, 0, 0, "Sample & Hold" });
        perRowEntries = 2;
    }

    void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
        auto h = fontSize * 1.1f;

        int32_t randomMode = moduleInstance->getRandomMode(channel);
        for (auto& e : entries) {
            if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                nvgBeginPath(vg);
                nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }
            if (e.id == randomMode) {
                nvgBeginPath(vg);
                nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                nvgFill(vg);
            }
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + h * 0.5f),
                        vec2(width, h),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (auto& e : entries) {
            renderTextLabel(vg,
                            vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                            vec2(width, h),
                            e.name,
                            theme,
                            fontSize * 0.9f,
                            theme->getColor(GuiColor::COL_TEXT),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
};
} // namespace PluginLFO