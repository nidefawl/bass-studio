#include <algorithm>
#include "clipeditor.h"

#include "commands.h"
#include "event.h"
#include "guiconstant.h"
#include "math/seq_math.h"
#include "seq_time.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "track.h"
#include "track_impl.h"
#include "trackctr_types.h"
#include "gui/track/trackctr.h"
#include "note.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"


gui_clipsettings::gui_clipsettings(scaled_grid&, clip_view& _view)
    : guictr_base(),
      /*grid(_grid),*/
      view(_view),
      clipLoopStart(nullptr),
      clipLoopLen(nullptr, true),
      clipTimeStart(nullptr),
      clipTimeLen(nullptr, true),
      clipTimeStartOffsetTicks(nullptr, true),
      clipTimeStartOffsedSamples(nullptr),
      clipAudioId(nullptr),
      quantization()
    {
    setCanMouseHit(true);
    btnLoop.drawFn   = drawTextureSymbol;
    btnLoop.drawParm = ICON_LOOP;
    btnLoop.setFlag(FLG_RENDER_BUTTON_WITH_LED, true);
    btnLoop.setStateRef(nullptr);
    clipLoopStart.setRef(nullptr);
    clipLoopLen.setRef(nullptr);
    clipTimeStart.setRef(nullptr);
    clipTimeLen.setRef(nullptr);
    clipTimeStartOffsetTicks.setRef(nullptr);
    clipTimeStartOffsedSamples.setRef(nullptr);
    clipAudioId.setRef(nullptr);
    btnLoop.setLabel("Loop");
    clipLoopStart.setLabel("Loop Position");
    clipLoopLen.setLabel("Loop Length");
    clipTimeStart.setLabel("Clip Position");
    clipTimeLen.setLabel("Clip Length");
    clipTimeStartOffsetTicks.setLabel("Tick offset");
    clipTimeStartOffsedSamples.setLabel("Sample offset");
    clipAudioId.setLabel("Sample ID");
    btnDuplicateLoop.setText("Duplicate Loop");
    btnSelectMuted.setText("Select all muted");
    add(&btnLoop);
    add(&clipLoopStart);
    add(&clipLoopLen);
    add(&clipTimeStart);
    add(&clipTimeLen);
    add(&clipTimeStartOffsetTicks);
    add(&clipTimeStartOffsedSamples);
    add(&clipAudioId);
    add(&btnDuplicateLoop);
    add(&btnSelectMuted);
    add(&quantization);
}

gui_clipsettings::~gui_clipsettings() {
    removeGuis();
}

void gui_clipsettings::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}

void duplicateClipLoop(DawInstance* daw, clip_view& view);//clipeditor.cpp;
void selectAllMuted(DawInstance* daw, clip_view& view) {
    clip_notes_t& notes = view.clip()->notes;
    notes.selection.clear();
    notes.visitNotes([&notes](note_t& n) {
        if (!n.isEnabled()) {
            notes.selection.insert(&n);
        }
    });
    String selStatus = StringFormat("%zu notes selected", notes.selection.size());
    daw->getMainControl()->setStatusText(selStatus);
}
void gui_clipsettings::buttonClicked(guibase* button) {
    auto const daw = dawCtrl->getDaw();
    if (&btnLoop == button) {
        clip_t* clip = view.clip();
        if (clip != NULL) {
            clip->loopEnabled = !clip->loopEnabled;
        }
    }
    if (&btnDuplicateLoop == button) {
        duplicateClipLoop(daw, view);
    }
    if (&btnSelectMuted == button) {
        selectAllMuted(daw, view);
    }

    if (&btnLoop == button || &clipTimeStart == button || &clipLoopStart == button || &clipTimeLen == button
        || &clipTimeStartOffsedSamples == button || &clipTimeStartOffsetTicks == button || &clipLoopLen == button) {
        clip_t* clip = view.clip();
        if (clip) {
            clip->setDirty();
            for (track_gui_entry_t* entry: clip->trackEntries) {
                track_t* track = entry->track;
                if (track) {
                    resizeOtherClips(track->getMidi(), clip);
                    daw->layoutTrackEditors();
                    daw->updateVisibleTrackContents();
                }
            }
        }
    }
}

void gui_clipsettings::showEditClip() {
    clip_t* clip = view.clip();
    if (clip != NULL) {
        btnLoop.setStateRef(&clip->loopEnabled);
        clipLoopStart.setRef(&clip->loopStart);
        clipLoopLen.setRef(&clip->loopLen);
        clipTimeStart.setRef(&clip->time);
        clipTimeLen.setRef(&clip->getLenRef());
        clipTimeStartOffsetTicks.setRef(&clip->offsetStart);
        clipAudioId.setRef(&clip->audio.id);
    } else {

        btnLoop.setStateRef(nullptr);
        clipLoopStart.setRef(nullptr);
        clipLoopLen.setRef(nullptr);
        clipTimeStart.setRef(nullptr);
        clipTimeLen.setRef(nullptr);
        clipTimeStartOffsedSamples.setRef(nullptr);
        clipTimeStartOffsetTicks.setRef(nullptr);
        clipAudioId.setRef(nullptr);
    }
    auto project = project_controller_t::get();
    if (project) {
        auto& settings = project->getQuantizeSettings();
        quantization.setQuantization(settings.quantizeStart, settings.quantizeEnd);

    }
}


void gui_clipsettings::render(NVGcontext* vg) {
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
    renderTitleBar(vg, size, text, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, 0, flags, true);
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
        if (gui == &clipTimeStartOffsedSamples) break;
        renderText(vg, vec2(padding, gui->top() + gui->size.y * 0.5f), vec2(gui->left()-padding, size.y), gui->label, rowHeight);
    }
    nvgRestore(vg);
}

void gui_clipsettings::layout() {
    padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
    const auto rowHeight = static_cast<float>(theme->get(GuiConstant::CONST_ROW_HEIGHT));
    int32_t w                       = getSizeContent().x;
    int32_t btnW                    = (w-padding)/2;
    int32_t btnH = math::roundfS32(rowHeight);
    int32_t btnX = btnW + padding/2;
    btnLoop.size                    = ivec2(btnW, btnH);
    btnLoop.pos                     = ivec2(btnX, padding + theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT));
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
    clipTimeStartOffsedSamples.size = ivec2(btnW, btnH);
    clipTimeStartOffsedSamples.pos  = ivec2(0, clipTimeStartOffsetTicks.bottom() + padding);
    clipAudioId.size                = ivec2(btnW, btnH);
    clipAudioId.pos                 = ivec2(clipTimeStartOffsedSamples.right() + padding, clipTimeStartOffsetTicks.bottom() + padding);
    btnDuplicateLoop.pos            = ivec2(0, clipAudioId.bottom() + padding);
    btnDuplicateLoop.size           = ivec2(w, btnH);
    btnSelectMuted.pos              = ivec2(0, btnDuplicateLoop.bottom() + padding);
    btnSelectMuted.size             = ivec2(w, btnH);
    quantization.pos                = ivec2(quantization.margin, btnSelectMuted.bottom()+quantization.margin*2);
    quantization.size               = ivec2(w - quantization.margin*2, (btnH*3+quantization.padding*5));
    for (guibase* gui: guis) {
        gui->layout();
    }
}

void gui_quantizationsettings::buttonClicked(guibase* button) {
    if (&inputEnds == button) {
        tickEnd = math::max(0, tickEnd);
        auto& settings = project_controller_t::get()->getQuantizeSettings();
        settings.quantizeEnd = inputEnds.getTime();
    } else if (&inputStarts == button) {
        tickStart = math::max(0, tickStart);
        auto& settings = project_controller_t::get()->getQuantizeSettings();
        settings.quantizeStart = inputStarts.getTime();
    }
    if (&btnQuantize == button) {
        auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_QUANTIZE};
        dawCtrl->getClipEditor()->handleEditorCommand(temp);
    }
}
