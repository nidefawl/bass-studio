#include <algorithm>
#include <utility>
#include <vector>
#include "clipeditor.h"

#include "commands.h"
#include "event.h"
#include "gui/clipeditor/clipeditor_python_processor.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "guiconstant.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "seq_time.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/track/trackctr_types.h"
#include "gui/track/trackctr.h"
#include "note.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"


gui_clipsettings::gui_clipsettings(guictr_clipeditor& parentClipEditor, clip_view_t& _view)
    : parentClipEditor(parentClipEditor),
      view(_view),
      clipLoopStart(),
      clipLoopLen(true),
      clipTimeStart(),
      clipTimeLen(true),
      clipTimeStartOffsetTicks(true),
      clipTimeStartOffsetSamples(nullptr),
      clipAudioId(nullptr),
      clipAudioPitch(nullptr),
      clipAudioStretch(nullptr),
#ifndef NDEBUG
      clipAudioFlags(nullptr),
#endif
      grooveSettings(*this, _view),
      quantization()
{
    setCanMouseHit(true);
    btnLoop.drawFn   = drawTextureSymbol;
    btnLoop.drawParm = ICON_LOOP;
    btnLoop.setFlag(FLG_RENDER_BUTTON_WITH_LED, true);
    btnLoop.setStateRef(nullptr);
    clipLoopStart.clearRef();
    clipLoopLen.clearRef();
    clipTimeStart.clearRef();
    clipTimeLen.clearRef();
    clipTimeStartOffsetTicks.clearRef();
    clipTimeStartOffsetSamples.setRef(nullptr);
    clipAudioId.setRef(nullptr);
    btnLoop.setLabel("Loop");
    clipLoopStart.setLabel("Loop Position");
    clipLoopLen.setLabel("Loop Length");
    clipTimeStart.setLabel("Clip Position");
    clipTimeLen.setLabel("Clip Length");
    clipTimeStartOffsetTicks.setLabel("Tick offset");
    clipTimeStartOffsetSamples.setLabel("Sample offset");
    clipAudioId.setLabel("Sample ID");
    clipAudioPitch.setLabel("Pitch");
    clipAudioStretch.setLabel("Stretch");
#ifndef NDEBUG
    clipAudioFlags.setLabel("Flags");
#endif
    btnDuplicateLoop.setText("Duplicate Loop");
    btnReverseClip.setText("Reverse");
    btnSelectMuted.setText("Select all muted");
    clipAudioPitch.setStepSize(0.01f);
    clipAudioStretch.setStepSize(0.01f);
    add(&btnLoop);
    add(&clipLoopStart);
    add(&clipLoopLen);
    add(&clipTimeStart);
    add(&clipTimeLen);
    add(&clipTimeStartOffsetTicks);
    add(&clipTimeStartOffsetSamples);
    add(&clipAudioId);
    add(&clipAudioPitch);
    add(&clipAudioStretch);
#ifndef NDEBUG
    add(&clipAudioFlags);
#endif
    add(&btnReverseClip);
    add(&btnDuplicateLoop);
    add(&btnSelectMuted);
    add(&grooveSettings);
    add(&quantization);
    auto updatePitchStretch = [this](gui_numberinput_field_base*, auto) {
        clip_t* clip = view.clip();
        if (clip && clip->clipType == CLIP_AUDIO) {
            dawCtrl->getDaw()->updateDerivedAudio(clip, clipAudioSettings);
        }
    };
    clipAudioPitch.fnValueEditChanged = updatePitchStretch;
    clipAudioStretch.fnValueEditChanged = updatePitchStretch;
#ifndef NDEBUG
    clipAudioFlags.fnValueEditChanged = updatePitchStretch;
#endif
}

gui_clipsettings::~gui_clipsettings() {
    removeGuis();
    for (auto& g : noteEditorScripts) {
        delete g;
    }
}

void gui_clipsettings::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}

void duplicateClipLoop(DawInstance* daw, clip_view_t& view);//clipeditor.cpp;
void selectAllMuted(DawInstance* daw, clip_view_t& view) {
    auto numSelected = view.selectAll([](note_t* note) { return !note->isEnabled(); });
    String selStatus = StringFormat("%d notes selected", numSelected);
    daw->getMainControl()->setStatusText(selStatus);
}

void gui_clipsettings::buttonClicked(guibase* button) {
    auto const daw = dawCtrl->getDaw();
    auto clip = view.clip();
    if (!clip) return;
    if (&btnLoop == button) {
        clip->loopEnabled = !clip->loopEnabled;
    }
    if (&btnDuplicateLoop == button) {
        duplicateClipLoop(daw, view);
        parentClipEditor.getNoteEditor().updateCopiedClipData();
        auto tick = clip->loopStart + clip->loopLen;
        if (view.isAbsoluteTimeMode()) {
            tick += clip->getOffsetStart();
            tick = math::min(clip->end(), tick);
        }
        parentClipEditor.getNoteEditor().getGrid().makeTickVisible(tick);
    }
    if (&btnReverseClip == button) {
        auto clipEditor = guiParentType<guictr_clipeditor, gui_type::CTR_TYPE_CLIPEDITOR>(this->parent);
        if (!assert_expr(clipEditor)) {
            return;
        }
        auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_REVERSE};
        clipEditor->handleEditorCommand(temp);
    }
    if (&btnSelectMuted == button) {
        selectAllMuted(daw, view);
    }

    if (&btnLoop == button || &clipTimeStart == button || &clipLoopStart == button || &clipTimeLen == button
        || &clipTimeStartOffsetSamples == button || &clipTimeStartOffsetTicks == button || &clipLoopLen == button) {
        clip_t* clip = view.clip();
        if (clip) {
            clip->setDirty();
            for (track_gui_entry_t* entry: clip->trackEntries) {
                track_t* track = entry->track;
                if (track) {
                    resizeOtherClips(track->getClips(), clip);
                    daw->layoutTrackEditors();
                    daw->updateVisibleTrackContents();
                }
            }
        }
    }

    if (&clipAudioPitch == button || &clipAudioStretch == button) {
        parent->buttonClicked(button);
    }
}

void gui_clipsettings::updateClipViewReferences() {
    clip_t* clip = view.clip();
    bool bHasRef = false;
    if (clip && parentCtrl) {
        auto guiClip = clip->getGuiClip(this->dawCtrl);
        if (guiClip) {
            auto ref = guiClip->toRef();
            bHasRef = true;
            btnLoop.setStateRef(&clip->loopEnabled);
            clipLoopStart.setRef(ref, &clip->loopStart);
            clipLoopLen.setRef(ref, &clip->loopLen);
            clipTimeStart.setRef(ref, &clip->time);
            clipTimeLen.setRef(ref, &clip->getLenRef());
            clipTimeStartOffsetSamples.setRef(nullptr);
            clipTimeStartOffsetTicks.setRef(ref, &clip->offsetStart);
            clipAudioId.setRef(&clip->audio.id);
            clipAudioSettings = clip->audio.settings;
            clipAudioPitch.setRef(&clipAudioSettings.pitch);
            clipAudioStretch.setRef(&clipAudioSettings.stretch);
#ifndef NDEBUG
            clipAudioFlags.setRef(&clipAudioSettings.flags);
#endif
            grooveSettings.setSelectedGroove(clip->selectedGroove);
        }
    }
    if (!bHasRef) {
        btnLoop.setStateRef(nullptr);
        clipLoopStart.clearRef();
        clipLoopLen.clearRef();
        clipTimeStart.clearRef();
        clipTimeLen.clearRef();
        clipTimeStartOffsetSamples.setRef(nullptr);
        clipTimeStartOffsetTicks.clearRef();
        clipAudioId.setRef(nullptr);
        clipAudioPitch.setRef(nullptr);
        clipAudioStretch.setRef(nullptr);
#ifndef NDEBUG
        clipAudioFlags.setRef(nullptr);
#endif
        grooveSettings.setSelectedGroove(-1);
    }
    if (dawCtrl) {
        auto project = dawCtrl->getDaw();
        if (project) {
            auto& settings = project->getQuantizeSettings();
            quantization.setQuantization(settings.quantizeStart, settings.quantizeEnd);
        }
    }
}


void gui_clipsettings::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
    nvgTranslate(vg, pos.x, pos.y);
    renderFrameBase(vg);

    String text = "Clip properties";

    clip_t* clip = view.clip();
    if (clip) {
        text = clip->name;
    }
    int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : 0;
    if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
    renderTitleBar(vg, size, text, GuiConstant::CONST_FIXED_TITLE_HEIGHT, 0, flags, true);
    renderFrameOutline(vg);
    ivec2 posInset  = getPosContent();
    nvgTranslate(vg, posInset.x-pos.x, posInset.y-pos.y);
    nvgTranslateZ(vg, -4.0f);
    for (guibase* gui: guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }

    const auto rowHeight = static_cast<float>(theme->get(GuiConstant::CONST_ROW_HEIGHT));
    nvgSave(vg);
    nvgTranslate(vg, 0, 0);
    for (guibase* gui: guis) {
        if (gui == &clipTimeStartOffsetSamples) break;
        renderText(vg, vec2(padding*2, gui->top() + gui->size.y * 0.5f), vec2(gui->left()-padding, size.y), gui->label, rowHeight - 2);
    }
    nvgRestore(vg);
}
using DAW::PythonNoteProcessor::python_note_processor_t;
using DAW::PythonNoteProcessor::python_func_param_t;
class gui_script_note_processor final : public guictr_base {
    guictr_clipeditor& parentClipEditor;
    guibutton btnExecute;
    python_note_processor_t processor;
    std::vector<guibase*> inputFields;
    std::array<int32_t, 8> inputParamsInt32{};
    std::array<float, 8> inputParamsFloat{};
public:
    explicit gui_script_note_processor(guictr_clipeditor& parent, python_note_processor_t _processor)
        : parentClipEditor(parent), processor(std::move(_processor))
    {
        setBackgroundRendered(true);
        setBackgroundRenderedInset(true);
        setFlag(FLG_RENDER_LABEL, true);
        setLabel(processor.descriptiveName);
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        int32_t numInputParamsInt32 = 0;
        int32_t numInputParamsFloat = 0;
        for (auto& parameter: processor.params) {
            gui_numberinput_field_base* inputField = nullptr;
            switch (parameter.type) {
                case python_func_param_t::param_type_float: {
                        inputParamsFloat[numInputParamsFloat] = parameter.defValue;
                        auto inputFloat = new gui_numberinput_float(&inputParamsFloat[numInputParamsFloat]);
                        inputFloat->fnClamp = [valMin = parameter.rangeMin, valMax = parameter.rangeMax](float v) -> float {
                            return std::clamp(v, valMin, valMax);
                        };
                        inputField = inputFloat;
                        numInputParamsFloat++;
                    }
                    break;
                case python_func_param_t::param_type_int: {
                        inputParamsInt32[numInputParamsInt32] = math::roundfS32(parameter.defValue);
                        auto inputInt = new gui_numberinput_i32(&inputParamsInt32[numInputParamsInt32]);
                        inputInt->fnClamp = [valMin = math::roundfS32(parameter.rangeMin), valMax = math::roundfS32(parameter.rangeMax)](int32_t v) -> int32_t {
                            return std::clamp(v, valMin, valMax);
                        };
                        inputField = inputInt;
                        numInputParamsInt32++;
                    }
                    break;
            }
            inputField->setLabel(parameter.name);
            inputFields.push_back(inputField);
        }
        for (auto& inputField: inputFields) {
            inputField->setLabel(inputField->label + ":");
            add(inputField);
        }
        btnExecute.setText(processor.descriptiveName);
        add(&btnExecute);
    }
    ~gui_script_note_processor() override {
        removeGuis();
        for (auto& inputField: inputFields) {
            delete inputField;
        }
    }
    void layout() override {
        guictr_base::layout();
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (guibase* gui: guis) {
            nvgSave(vg);
            gui->render(vg);
            nvgRestore(vg);
        }

        const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        nvgSave(vg);
        nvgTranslate(vg, 0, 0);
        for (guibase* gui: guis) {
            renderText(vg, vec2(padding, gui->top() + gui->size.y * 0.5f), vec2(gui->left() - padding, size.y), gui->label, TRACK_HEIGHT_STEP);
        }
        nvgRestore(vg);
    }

    void determineSize(ivec2& prefSize) override {
        // return current width and multiple current height by number of visible guis
        prefSize.x = size.x;
        prefSize.y = size.y * guis.size() + (padding * math::max<int32_t>(0, guis.size() - 1));
    }
    void buttonClicked(guibase* _button) override {
        if (_button == &btnExecute) {
            std::vector<float> scriptInputParams;
            int32_t numInputParamsInt32 = 0;
            int32_t numInputParamsFloat = 0;
            for (auto& parameter: processor.params) {
                switch (parameter.type) {
                    case python_func_param_t::param_type_float:
                        scriptInputParams.push_back(inputParamsFloat[numInputParamsFloat++]);
                        break;
                    case python_func_param_t::param_type_int:
                        scriptInputParams.push_back(inputParamsInt32[numInputParamsInt32++]);
                        break;
                }
            }
            auto kEvt = KeyEvent{
                .type = KeyboardState::K_PRESS,
                .keyCode = KeyboardKey::DAW_KB_INVALID,
                .scancode = 0,
                .mods = KeyboardMods::KB_MODS_NONE,
                .keyname = nullptr,
                .cmd = nullptr,
            };
            DAW::UI::CommandContext ctxt {
                .type = GlobalCommandType::CMD_APPLY_PYTHON_SCRIPT,
                .kevt = kEvt,
                .argInt0 = 0,
                .argInt1 = 0,
                .argStr0 = processor.processorName,
                .argFloats = scriptInputParams,
            };
            parentClipEditor.handleEditorCommand(ctxt);
            return;
        }
        guictr_base::buttonClicked(_button);
    }
};

void gui_clipsettings::layout() {
    for (auto gui : noteEditorScripts) {
        remove(gui);
        delete gui;
    }
    noteEditorScripts.clear();
    auto list = DAW::PythonNoteProcessor::GetNoteProcessors();
    for (auto& processor : list) {
        auto gui = new gui_script_note_processor(parentClipEditor, processor);
        add(gui);
        noteEditorScripts.push_back(gui);
    }
    padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
    const auto rowHeight = static_cast<float>(theme->get(GuiConstant::CONST_ROW_HEIGHT));
    int32_t w                       = getSizeContent().x;
    int32_t btnW                    = (w-padding)/2;
    int32_t btnH = math::roundfS32(rowHeight);
    int32_t btnX = btnW + padding/2;
    btnLoop.size                    = ivec2(btnW, btnH);
    btnLoop.pos                     = ivec2(btnX, padding + theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT));
    clipLoopStart.size              = ivec2(btnW, btnH);
    clipLoopStart.pos               = ivec2(btnLoop.left(), btnLoop.bottom() + padding);
    clipLoopLen.size                = ivec2(btnW, btnH);
    clipLoopLen.pos                 = ivec2(clipLoopStart.left(), clipLoopStart.bottom() + padding);
    clipTimeStart.size              = ivec2(btnW, btnH);
    clipTimeStart.pos               = ivec2(clipLoopLen.left(), clipLoopLen.bottom() + padding);
    clipTimeLen.size                = ivec2(btnW, btnH);
    clipTimeLen.pos                 = ivec2(clipTimeStart.left(), clipTimeStart.bottom() + padding);
    clipTimeStartOffsetTicks.size   = ivec2(btnW, btnH);
    clipTimeStartOffsetTicks.pos    = ivec2(clipTimeStart.left(), clipTimeLen.bottom() + padding);
    clipTimeStartOffsetSamples.size = ivec2(btnW, btnH);
    clipTimeStartOffsetSamples.pos  = ivec2(0, clipTimeStartOffsetTicks.bottom() + padding);
    clipAudioId.size                = ivec2(btnW, btnH);
    clipAudioId.pos                 = ivec2(clipTimeStartOffsetSamples.right() + padding, clipTimeStartOffsetTicks.bottom() + padding);
    clipAudioPitch.size             = ivec2(btnW, btnH);
    clipAudioPitch.pos              = ivec2(0, clipAudioId.bottom() + padding);
    clipAudioStretch.size           = ivec2(btnW, btnH);
    clipAudioStretch.pos            = ivec2(clipAudioPitch.right() + padding, clipAudioPitch.top());
#ifndef NDEBUG
    clipAudioFlags.size             = ivec2(btnW, btnH);
    clipAudioFlags.pos              = ivec2(0, clipAudioPitch.bottom() + padding);
    btnReverseClip.size             = ivec2(btnW, btnH);
    btnReverseClip.pos              = ivec2(clipAudioFlags.right() + padding, clipAudioFlags.top());
#else
    btnReverseClip.size             = ivec2(w, btnH);
    btnReverseClip.pos              = ivec2(0, clipAudioPitch.bottom() + padding);
#endif
    btnDuplicateLoop.pos            = ivec2(0, btnReverseClip.bottom() + padding);
    btnDuplicateLoop.size           = ivec2(w, btnH);
    btnSelectMuted.pos              = ivec2(0, btnDuplicateLoop.bottom() + padding);
    btnSelectMuted.size             = ivec2(w, btnH);

    auto grooveSettingsVisibleChildren = std::count_if(grooveSettings.guis.begin(), grooveSettings.guis.end(), [](guibase* gui) {
        return gui->isVisible();
    });
    grooveSettings.padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
    grooveSettings.pos     = ivec2(grooveSettings.margin, btnSelectMuted.bottom() + grooveSettings.margin * 2);
    grooveSettings.size    = ivec2(w - grooveSettings.margin * 2, (btnH * (grooveSettingsVisibleChildren) + grooveSettings.padding * (grooveSettingsVisibleChildren+2)));

    quantization.padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
    quantization.pos     = ivec2(quantization.margin, grooveSettings.bottom() + quantization.margin * 3);
    quantization.size    = ivec2(w - quantization.margin * 2, (btnH * 3 + quantization.padding * 5));

    guibase* prevGui = &quantization;
    for (auto* gui : noteEditorScripts) {
        gui->padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        gui->pos = ivec2(gui->margin, prevGui->bottom() + gui->margin*3);
        gui->size = ivec2(w - gui->margin*2, btnH);
        ivec2 size{};
        gui->determineSize(size);
        gui->size = size;
        prevGui = gui;
    }
    for (guibase* gui : guis) {
        gui->layout();
    }
}

void gui_quantize_clip::buttonClicked(guibase* button) {
    if (&inputEnds == button) {
        tickEnd = math::max(0, tickEnd);
        auto& settings = dawCtrl->getDaw()->getQuantizeSettings();
        auto p = inputEnds.getSafeIntRef();
        if (p) {
            settings.quantizeEnd = *p;
        }
    } else if (&inputStarts == button) {
        tickStart = math::max(0, tickStart);
        auto& settings = dawCtrl->getDaw()->getQuantizeSettings();
        auto p = inputStarts.getSafeIntRef();
        if (p) {
            settings.quantizeStart = *p;
        }
    }
    if (&btnQuantize == button) {
        auto clipEditor = guiParentType<guictr_clipeditor, gui_type::CTR_TYPE_CLIPEDITOR>(this->parent);
        if (!assert_expr(clipEditor)) {
            return;
        }
        auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_QUANTIZE};
        clipEditor->handleEditorCommand(temp);
    }
}
