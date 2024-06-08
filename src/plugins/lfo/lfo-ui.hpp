#pragma once
#include "lfo-types.hpp"
#include "gui/contextmenu/contextmenu_base.h"

namespace DAW::LFO {

template<typename ModuleType>
class ctxtmenu_lfo_sync : public ctxtmenu_enum_option_select_base<ctxmenu_enum_select_entry> {
    ModuleType* const moduleInstance;
    const int32_t channel;
public:
    ctxtmenu_lfo_sync(ModuleType* _module, int32_t _channel, String _title, int32_t _id)
        : ctxtmenu_enum_option_select_base(_id, _title), moduleInstance(_module), channel(_channel)
    {
        entries.push_back({ int32_t(PluginLFO::NoteRatio::STRAIGHT), "Straight" });
        entries.push_back({ int32_t(PluginLFO::NoteRatio::TRIPLET), "Triplet" });
        entries.push_back({ int32_t(PluginLFO::NoteRatio::DOTTED), "Dotted" });
        entries.push_back({ 0, "Off" });
        perRowEntries = 3;
    }
    bool isEntrySelected(ctxmenu_enum_select_entry& e) const override {
        int32_t sync = moduleInstance->getSyncRatio(channel);
        return (e.id&sync) || (e.id == 0 && sync == 0);
    }
};

template<typename ModuleType>
class ctxtmenu_lfo_mode : public ctxtmenu_enum_option_select_base<ctxmenu_enum_select_entry> {
    ModuleType* const moduleInstance;
    const int32_t channel;
public:
    ctxtmenu_lfo_mode(ModuleType* _module, int32_t _channel, String _title, int32_t _id)
        : ctxtmenu_enum_option_select_base(_id, _title), moduleInstance(_module), channel(_channel)
    {
        entries.push_back({ 0, "Shape" });
        entries.push_back({ 1, "Random" });
        perRowEntries = 2;
    }
    bool isEntrySelected(ctxmenu_enum_select_entry& e) const override {
        int32_t isShapeMode = moduleInstance->isShapeMode(channel);
        return (e.id == 0) == isShapeMode;
    }
};

template<typename ModuleType>
class ctxtmenu_lfo_random_mode final : public ctxtmenu_enum_option_select_base<ctxmenu_enum_select_entry> {
    ModuleType* const moduleInstance;
    const int32_t channel;
public:
    ctxtmenu_lfo_random_mode(ModuleType* _module, int32_t _channel, String _title, int32_t _id)
        : ctxtmenu_enum_option_select_base(_id, _title), moduleInstance(_module), channel(_channel)
    {
        entries.push_back({ 0, "Smooth" });
        entries.push_back({ 1, "Linear" });
        entries.push_back({ 2, "Exponential" });
        entries.push_back({ 3, "Sample & Hold" });
        perRowEntries = 2;
    }
    bool isEntrySelected(ctxmenu_enum_select_entry& e) const override {
        int32_t randomMode = moduleInstance->getRandomMode(channel);
        return e.id == randomMode;
    }
};
} // namespace PluginLFO