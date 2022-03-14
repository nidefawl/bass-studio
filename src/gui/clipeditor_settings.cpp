#include <algorithm>
#include "clipeditor.h"

#include "math/seq_math.h"
#include "seq_time.h"
#include "gui.h"
#include "guicolors.h"
#include "track.h"
#include "track_impl.h"
#include "trackctr_types.h"
#include "trackctr.h"
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
      clipTimeStartOffsetTicks(nullptr),
      clipTimeStartOffsedSamples(nullptr),
      clipAudioId(nullptr) {
    padding          = 2;
    margin           = 0;
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
    btnDuplicateLoop.setLabel("Duplicate Loop");
    btnSelectMuted.setLabel("Select all muted");
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
    String selStatus = StringFormat("%d notes selected", notes.selection.size());
    daw->getMainControl()->setStatusText(selStatus);
}
void gui_clipsettings::buttonClicked(guibase* button) {
    if (&btnLoop == button) {
        clip_t* clip = view.clip();
        if (clip != NULL) {
            clip->loopEnabled = !clip->loopEnabled;
        }
    }
    if (&btnDuplicateLoop == button) {
        duplicateClipLoop(dawCtrl->getDaw(), view);
    }
    if (&btnSelectMuted == button) {
        selectAllMuted(dawCtrl->getDaw(), view);
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

                    DawInstance::get()->layoutTrackEditors();
                    DawInstance::get()->updateVisibleTrackContents();
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
        clipTimeStartOffsedSamples.setRef(&clip->offsetSamples);
        clipTimeStartOffsetTicks.setRef(&clip->offsetStart);
        clipAudioId.setRef(&clip->audio.id);
//        if (clip->noLayout) {
//            grid.showRange(clip->offsetStart, clip->offsetStart + clip->len);
//            zoomPianoRollToClipsNoteRange();
//        } else {
//            clip_editor_layout_t& layout = clip->editorLayout;
//            grid.setLayout(layout.layoutGrid);
//            setLayout(layout.layoutPianoRoll);
//        }
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
}


void gui_clipsettings::render(NVGcontext* vg) {
    if (!setScissorTransformContainer(vg)) {
        return;
    }
    renderFrameBase(vg);

    String text = "Clip properties";

    clip_t* clip = view.clip();
    if (clip) {
        text = clip->name;
    }
    int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
    renderTitleBar(vg, getSizeContent(), text, GuiConstant::CONST_FIXED_TITLE_HEIGHT, 0, flags, true);
    renderFrameOutline(vg);
    for (guibase* gui: guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }

    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    nvgSave(vg);
    nvgTranslate(vg, 0, 0);
    int32_t inset = 4;
    int32_t i2    = inset * 2;
    int32_t h     = TRACK_HEIGHT_STEP - i2;
    setFont(vg, G_FONT_SCALE(h), THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    for (guibase* gui: guis) {

        //gui->render(vg);
        nvgText(vg, i2, gui->top() + G_FONT_MIDDLE_OFFSET(gui->size.y), StringAsCStr(gui->label), nullptr);
    }
    nvgRestore(vg);
}

void gui_clipsettings::layout() {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    int32_t inset                   = 4;
    int32_t w                       = getSizeContent().x;
    int32_t btnW                    = math::max(math::min(w, 120), w / 3);
    int32_t labelWidth              = w - btnW;

    int32_t btnH = TRACK_HEIGHT_STEP;
    int32_t btnX = labelWidth + inset;
    btnW -= inset * 2;
    btnLoop.size                    = ivec2(btnW, btnH);
    btnLoop.pos                     = ivec2(btnX, inset + theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT));
    clipLoopStart.size              = ivec2(btnW, btnH);
    clipLoopStart.pos               = ivec2(btnLoop.left(), btnLoop.bottom() + inset);
    clipLoopLen.size                = ivec2(btnW, btnH);
    clipLoopLen.pos                 = ivec2(clipLoopStart.left(), clipLoopStart.bottom() + inset);
    clipTimeStart.size              = ivec2(btnW, btnH);
    clipTimeStart.pos               = ivec2(clipLoopLen.left(), clipLoopLen.bottom() + inset);
    clipTimeLen.size                = ivec2(btnW, btnH);
    clipTimeLen.pos                 = ivec2(clipTimeStart.left(), clipTimeStart.bottom() + inset);
    clipTimeStartOffsetTicks.size   = ivec2(btnW, btnH);
    clipTimeStartOffsetTicks.pos    = ivec2(clipTimeStart.left(), clipTimeLen.bottom() + inset);
    clipTimeStartOffsedSamples.size = ivec2(btnW, btnH);
    clipTimeStartOffsedSamples.pos  = ivec2(clipTimeStartOffsetTicks.left(), clipTimeStartOffsetTicks.bottom() + inset);
    clipAudioId.size                = ivec2(btnW, btnH);
    clipAudioId.pos                 = ivec2(clipTimeStartOffsedSamples.left(), clipTimeStartOffsedSamples.bottom() + inset);
    btnDuplicateLoop.pos            = ivec2(inset, clipAudioId.bottom() + inset);
    btnDuplicateLoop.size           = ivec2(w - inset * 2, btnH);
    btnSelectMuted.pos              = ivec2(inset, btnDuplicateLoop.bottom() + inset);
    btnSelectMuted.size             = ivec2(w - inset * 2, btnH);
    for (guibase* gui: guis) {
        gui->layout();
    }
}
