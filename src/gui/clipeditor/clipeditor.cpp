

#include "gui/linetess/pymachine.h"
#include "appsettings.h"
#include "assert_dbg.h"
#include "color_util.h"
#include "gui/container/container_builder.h"
#include "host/clip/clip.h"
#include "host/daw/clipboard.h"
#include "event.h"
#include "gui/clipeditor/clipeditor.h"
#include "guiglobals.h"
#include "host/daw/mainctrl.h"
#include "logging.h"
#include "note.h"
#include "platform.h"
#include "rand.h"
#include "seq_time.h"
#include "math/seq_math.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu_grid.h"
#include "gui/track/trackcontent.h"
#include "clipeditor.h"
#include "gui/cliprenderer/cliprenderer_cache.h"
#include "host/shape/shape.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/host_pluginmanager.h"
#include "appconfig.h"
#include "str_util.h"
#include "types.h"
#include <cstdint>
#include <nanovg.h>

#include <nanovg_min.h>
#include <utility>
#include <vector>

constexpr int32_t VEL_SELECT_DISTANCE = 16;
constexpr int32_t PIANOROLL_MIN_SCALE = 4;
constexpr int32_t PIANOROLL_MAX_SCALE = 48;

class guictxtmenu_noteeditor final : public guictxtmenu {
    guictr_noteeditor* editor;
    ctxtmenu_color_select* sel = nullptr;
    ctxtmenu_time_select* timeSel1 = nullptr;
    ctxtmenu_time_select* timeSel2 = nullptr;

public:
    explicit guictxtmenu_noteeditor(guictr_noteeditor* _editor) {
        this->size.x  = 260;
        this->maxHeight = 0;
        this->editor  = _editor;
        this->dawCtrl = _editor->dawCtrl;
        auto& cursor = dawCtrl->getCursor();

        bool bHasContentSelected = cursor.getRange();
        if (bHasContentSelected) {
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_MUTE));
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_QUANTIZE));
            addEntry(new ctxtmenu_splitter());
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_CUT));
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_COPY));
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_DUPLICATE));
        }
        addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_PASTE));
        if (bHasContentSelected) {
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_DELETE));
            // addEntry(new ctxtmenu_splitter());
            // sel = new ctxtmenu_color_select("Pick Color", 100);
            // addEntry(sel);
        }
        addEntry(new ctxtmenu_splitter());
        timeSel1     = new ctxtmenu_time_select(_editor->getGrid(), "Adaptive Grid", 0);
        timeSel1->initAdaptive();
        addEntry(timeSel1);
        timeSel2 = new ctxtmenu_time_select(_editor->getGrid(), "Fixed Grid", 0);
        timeSel2->initFixed();
        addEntry(timeSel2);
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        auto& grid = editor->getGrid();
        if (e == this->timeSel1 || e == this->timeSel2) {
            if (_id == 110 + 9) {// OFF
                grid.grid_dens.enabled = false;
            } else if (_id >= 110) {
                grid.grid_dens.enabled   = true;
                grid.grid_dens.fixedBars = int8_t(_id - 110);
                grid.grid_dens.isfixed   = true;
            } else {
                grid.grid_dens.enabled        = true;
                grid.grid_dens.dynamicDensity = int8_t(_id - 100);
                grid.grid_dens.isfixed        = false;
            }
            grid.notifyChange();
        } else if (e == this->sel) {
            if (_id >= sel->id) {
                _id -= sel->id;
                if (_id < COLOR_PALETTE_LEN) {
                    auto clip = editor->getClipView().clip();
                    if (clip) {
                        clip->rgb = colorPalette[_id];
                    }
                }
            }
        } else {
            if (dawCtrl && editor && e->commandtype != GlobalCommandType::CMD_NONE) {
                DAW::UI::CommandContext ctxt = {e->commandtype};
                editor->handleEditorCommand(ctxt);
            }
        }
        dawCtrl->getDaw()->updateVisibleTrackContents();
        closeContextMenu();
        return true;
    }
};

guictr_clipeditor::guictr_clipeditor()
    : guictr_base(),
      noteeditor(*this, view),
      audioeditor(*this, view),
      settingsCtr(*this, view),
      arp(view) {
    padding = 0;
    margin = 0;
    settingsScrollCtr.maxHeight = -1;
    noteeditor.padding = 2;
    audioeditor.padding = 2;
    arp.padding = 2;
    noteeditor.margin = 2;
    audioeditor.margin = 2;
    arp.margin = 2;
    setGuiType(gui_type::CTR_TYPE_CLIPEDITOR);
    setBackgroundRendered(true);
    setBackgroundRenderedInset(false);
    add(&noteeditor);
    add(&audioeditor);
    add(&arp);
    add(&settingsScrollCtr);
    settingsScrollCtr.add(&settingsCtr);
}

guictr_clipeditor::~guictr_clipeditor() {
    settingsScrollCtr.remove(&settingsCtr);
    remove(&settingsScrollCtr);
    remove(&arp);
    remove(&audioeditor);
    remove(&noteeditor);
}

void guictr_clipeditor::storeEditorLayout() {
    const clip_t* clip = view.clip();
    if (clip || view.isAbsoluteTimeMode()) {
        if (!clip || clip->clipType == CLIP_MIDI) {
            noteeditor.storeEditorLayout();
        } else {
            audioeditor.storeEditorLayout();
        }
    }
}

void guictr_clipeditor::updateClipViewReferences() {
    settingsCtr.updateClipViewReferences();
    arp.updateClipViewReferences();
    auto clip = view.clip();
    bool bIsMidi = !clip || clip->clipType == CLIP_MIDI;
    noteeditor.setVisible(bIsMidi);
    audioeditor.setVisible(!bIsMidi);
    noteeditor.updateCopiedClipData();
}

void guictr_clipeditor::selectEditClip(clip_t* clip) {
    bool bChanged = clip != view.clip();
    storeEditorLayout();
    view.setSelected(clip);
    if (bChanged || !clip) {
        updateClipViewReferences();
        noteeditor.selectEditClip(clip);
        audioeditor.selectEditClip(clip);
        if (parent)
            layout();
    }
}

void guictr_clipeditor::setSingleClip(clip_t* clip) {
    bool bChanged = clip != view.clip();
    storeEditorLayout();
    view.setSingleClip(clip);
    if (parent)
        layout();
    if (bChanged || !clip) {
        updateClipViewReferences();
        noteeditor.onClipChanged();
        audioeditor.onClipChanged();
        noteeditor.relayout();
        audioeditor.relayout();
        if (parent)
            layout();
    }
}

void guictr_clipeditor::setEditorSelection(clip_t* clip, const editor_view_selection_t& clipboardView) {
    bool bChanged = clip != view.clip();
    if (bChanged) {
        storeEditorLayout();
    }
    view.setEditorSelection(clip, clipboardView);
    if (parent)
        layout();
    if (bChanged || !clip) {
        updateClipViewReferences();
        noteeditor.onClipChanged();
        audioeditor.onClipChanged();
        noteeditor.relayout();
        audioeditor.relayout();
        if (parent)
            layout();
    }
}

void guictr_clipeditor::resetClipView() {

    bool bChanged = nullptr != view.clip();
    storeEditorLayout();
    view.reset();
    if (parent)
        layout();
    if (bChanged) {
        updateClipViewReferences();
        noteeditor.relayout();
        audioeditor.relayout();
        layout();
    }
}

bool guictr_clipeditor::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    // if (!view.clip()) return false;
    return guictr_base::mouseHitTest(mpos, evt);
}

void guictr_clipeditor::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    auto posContent = vec2(getPosContent());
    auto sizeContent = vec2(getSizeContent());
    nvgTranslate(vg, posContent.x, posContent.y);
    auto clip = view.clip();
    if (clip) {
        if (settingsScrollCtr.isVisible()) {
            nvgSave(vg);
            settingsScrollCtr.render(vg);
            nvgRestore(vg);
        }
        if (arp.isVisible()) {
            nvgSave(vg);
            arp.render(vg);
            nvgRestore(vg);
        }
    }
    nvgSave(vg);
    if (clip && clip->clipType == CLIP_AUDIO) {
        if (audioeditor.isVisible()) {
            audioeditor.render(vg);
        }
    } else {
        if (noteeditor.isVisible()) {
            noteeditor.render(vg);
        }
    }
    nvgRestore(vg);
    for (guibase* gui : guis) {
        if (gui == &audioeditor)
            continue;
        if (gui == &noteeditor)
            continue;
        if (gui == &settingsScrollCtr)
            continue;
        if (gui == &arp)
            continue;
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
    if (!clip) {
        renderText(vg, posContent + sizeContent * 0.5f, sizeContent * 0.5f, "No clip selected", 18, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
}

void guictr_clipeditor::layout() {
    const int32_t padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);

    ivec2 cs      = getSizeContent();
    settingsCtr.pos = settingsScrollCtr.pos  = ivec2(0, 0);
    settingsCtr.size = settingsScrollCtr.size = ivec2(240, cs.y);
    settingsCtr.layout();
    ivec2 sizeSettings = settingsScrollCtr.size;
    if (settingsScrollCtr.isVisible()) {
        settingsScrollCtr.determineSize(sizeSettings);
        settingsScrollCtr.layout();
    }

    guibase* leftContainer = nullptr;
    auto& dawSettings = daw_tls::getDawSettings();
    arp.setVisible(dawSettings.uiShowSettingsArp);
    settingsScrollCtr.setVisible(dawSettings.uiShowSettingsClip);
    if (settingsScrollCtr.isVisible()) {
        leftContainer = &settingsScrollCtr;
    }
    if (arp.isVisible()) {
        leftContainer = &arp;
        arp.size      = ivec2(220, cs.y);
        if (settingsScrollCtr.isVisible()) {
            arp.pos = ivec2(settingsScrollCtr.right() + padding, 0);
        } else {
            arp.pos = ivec2(0, 0);
        }
        arp.layout();
    }
    if (settingsScrollCtr.isVisible() && arp.isVisible()) {
        ivec2 sizeArp{};
        arp.determineSize(sizeArp);
        if (cs.y - sizeSettings.y > sizeArp.y + padding) {
            settingsScrollCtr.size.y = sizeSettings.y;
            arp.size.y = math::min(cs.y - sizeSettings.y - padding, sizeArp.y);
            arp.pos.y = settingsScrollCtr.bottom() + padding;
            arp.pos.x = settingsScrollCtr.left();
            arp.size.x = settingsScrollCtr.size.x;
        }
    }

    ivec2 posEditor = ivec2(0, 0);
    ivec2 sizeEditor = cs;
    if (leftContainer) {
        posEditor = ivec2(leftContainer->right() + padding, 0);
        sizeEditor = ivec2(cs.x - posEditor.x, cs.y);
    } else {
    }
    noteeditor.pos   = posEditor;
    noteeditor.size  = sizeEditor;
    audioeditor.pos  = posEditor;
    audioeditor.size = sizeEditor;

    for (guibase* gui : guis) {
        gui->layout();
    }
}

bool guictr_clipeditor::handleKeyInput(KeyEvent& kevt) {
    if (audioeditor.isVisible()) {
        return audioeditor.handleKeyInput(kevt);
    }
    return noteeditor.handleKeyInput(kevt);
}

bool guictr_clipeditor::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    if (audioeditor.isVisible()) {
        return audioeditor.handleEditorCommand(ctxt);
    }
    return noteeditor.handleEditorCommand(ctxt);
}

void gui_clipcontent::handleRightClick(MouseEvent& evt) {
    guictr_noteeditor* editor = dynamic_cast<guictr_noteeditor*>(this->parent);
    dawCtrl->openContextMenu(new guictxtmenu_noteeditor(editor), evt.mousepos);
}

template<typename T>
void renderNote(NVGcontext* vg, gui_clipcontent* c, T* note, float yscale, tick_t offset = 0) {

    float ny     = c->toScreenF(note->pitch);
    float nx     = c->grid.tickToScreenD(note->time + offset);
    float nw     = c->grid.tickLenToScreen(note->len);
    float nh     = yscale;
    float insetx = calcInset(1, nw);
    float insety = calcInset(1, nh);
    nvgBatchedRect(vg, nx + insetx, ny - yscale + insety, nw - insetx * 2, nh - insety * 2);
}
void renderNoteName(NVGcontext* vg, const gui_clipcontent* c, const note_t* note, float nx, float ny, float nw, float nh, tick_t absPos, bool bRenderPosLen) {
    const float insetx = calcInset(5, nw);
    const auto color = c->theme->getColor(GuiColor::COL_NOTE_TEXT);
    auto posText = vec2(nx + insetx, ny - nh + nh / 2.0f);
    auto sizeText = vec2(nw - insetx + 2, nh);
    float w = renderTextLabel(vg,
        posText,
        sizeText,
        noteName(note->pitch),
        c->theme, 18, color, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    if (bRenderPosLen) {
        const String strStart = tickAsBeatString(note->start(), false);
        const String strEnd = tickAsBeatString(note->len, true);
        posText.x += w+insetx;
        sizeText.x -= w+insetx;
        w = renderTextLabel(vg, 
            posText,
            sizeText,
            strStart,
            c->theme, 18, color, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        posText.x += w+insetx;
        sizeText.x -= w+insetx;
        renderTextLabel(vg, 
            posText,
            sizeText,
            strEnd,
            c->theme, 18, color, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
}

inline int32_t screenToVel(int y, int h) {
    dbgassert(h > 0);
    return (int32_t) ((h - 1 - y) * 127 / h);
}

note_t* getMinDistNoteVel(clip_notes_t& notes, int32_t tickExact, int32_t tickDist, int32_t velExact, int32_t velDist) {
    int32_t minDist     = 0;
    int32_t minDistV    = 0;
    note_t* minDistNote = nullptr;
    auto checkNoteDist  = [&minDistNote, &minDist, &minDistV, tickExact, tickDist, velExact, velDist](note_t& note) {
        int32_t dist  = note.start() - tickExact;
        int32_t distV = note.velocity - velExact;
        if (dist > -tickDist && dist < tickDist && distV > -velDist && distV < velDist) {
            if (minDistNote == nullptr || minDist > math::abs(dist) || minDistV > math::abs(distV)) {
                minDistNote = &note;
                minDist     = math::abs(dist);
                minDistV    = math::abs(distV);
            }
        }
    };
    notes.visitSelection([&checkNoteDist](note_t* pNote) {
        checkNoteDist(*pNote);
    });
    if (minDistNote != nullptr) {
        return minDistNote;
    }
    notes.visitNotes([&checkNoteDist](note_t& note) {
        checkNoteDist(note);
    });
    return minDistNote;
}

void duplicateClipLoop(DawInstance* daw, clip_view_t& view) {
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }

    if (clip->loopLen > 0) {
        ThreadLock lock                = daw->lockPlayThread();
        clip_t clipBefore              = *clip;
        clip_notes_t& notes            = clip->notes;
        clip_control_data_t& data      = clip->controlData;
        clip_cursor_t& cursor          = view.m_cursor;
        clip_cursor_t cursorBefore     = cursor;// copy
        const clip_notes_t notesBefore = notes; // copy


        int32_t loopStart = clip->loopStart;
        int32_t loopEnd   = loopStart + clip->loopLen;
        int32_t offset    = clip->loopLen;
        {
            {
                clip_notes_t notesCopy = notes;// copy
                std::vector<note_t> newNotes;
                std::vector<note_t> selNotes;
                notesCopy.storeSelection(selNotes);
                notesCopy.clearSelection();
                notesCopy.visitNotes([loopStart, loopEnd, offset, &newNotes](note_t& note) {
                    if (note.time >= loopStart && note.time < loopEnd) {
                        note_t noteCpy = note;
                        noteCpy.time += offset;
                        newNotes.push_back(noteCpy);
                    }
                    if (note.time >= loopEnd) {
                        note.time += offset;
                    }
                });
                notesCopy.addAll(newNotes);
                notesCopy.restoreSelection(selNotes);
                notesCopy.updateBounds();

                notes = notesCopy;
            }
            {
                clip_control_data_t dataCopy = data;// copy
                dataCopy.copyRangeFrom(clip, loopEnd, loopStart, clip->loopLen);
                dataCopy.eraseDuplicates();
                dataCopy.updateBounds();
                data = dataCopy;
            }

            clip->loopLen *= 2;

            String desc = "Duplicate clip loop";
            daw->pushHist(new action_modify_clip(desc, view, clipBefore, cursorBefore));
            clip->setDirty();
            view.updateNotePitches(false);
        }
    }
}

void gui_clipcontent_base::renderBackground(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    ivec2 bgPos{0, 0};
    ivec2 bgSize{this->size};
    auto bgRepeat = grid.incr_bg * 2.0;
    auto bgOffset = fmod(double(grid.offset), bgRepeat);
    int steps_bg  = math::ceildS32((bgSize.x + bgRepeat) / grid.incr_bg);
    nvgBeginPath(vg);
    nvgRect(vg, bgPos.x + -2, bgPos.y, bgSize.x + 2, bgSize.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);

    auto bgImage = theme->getBackgroundImage(GuiBackgroundImage::BG_NOTEEDITOR_1);
    if (bgImage) {
        bgImage->render(this, vg);
    }

    double x = -bgOffset;
    for (int i = 0; i < steps_bg; i += 2) {
        nvgBeginPath(vg);
        nvgRect(vg, float(bgPos.x + x), bgPos.y, float(grid.incr_bg), bgSize.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgFill(vg);
        x += grid.incr_bg * 2.0;
        if (x > bgSize.x)
            break;
    }
}

gui_clipcontent_control_data::gui_clipcontent_control_data(scaled_grid& _grid, clip_view_t& _view, layout_pianoroll_t& _layout)
    : gui_clipcontent(_grid, _view, _layout, true),
    shapeEdit(_grid, _view)
{
    setGuiType(gui_type::CTR_TYPE_CLIPEDITOR_CONTROLDATA);
    shapeEdit.setEditorCurve(&tmpShape);
    shapeEdit.callback = [this](const DAW::Shape::shape_t& shape, bool bIsDragMove) {
        const auto clip = view.clip();
        if (clip) {
            action_modify_clip_control_data* undoAction = nullptr;
            if (!bIsDragMove) {
                undoAction = new action_modify_clip_control_data("Modify clip control data", view, controlDataBegin, view.m_cursor);
            }
            tmpShape = shape;
            if (this->cc == 0) {
                clip->controlData.pitchBend.shape = shape;
                clip->controlData.pitchBend.updateBounds();
            } else {
                if (!clip->controlData.ccChannels.count(this->cc)) {
                    clip->controlData.createCCChannel(this->cc);
                }
                clip->controlData.ccChannels[this->cc].shape = shape;
                clip->controlData.ccChannels[this->cc].updateBounds();
            }
            if (undoAction) {
                dawCtrl->getDaw()->pushHist(undoAction);
            }
        }
    };
}

gui_clipcontent_control_data::~gui_clipcontent_control_data() = default;

void gui_clipcontent_control_data::showEditClip() {
    shapeEdit.resetSelection();
    setSelectedData(this->cc);
}
void gui_clipcontent_control_data::setSelectedData(int32_t cc) {
    this->cc = cc;
    const auto clip = view.clip();
    if (clip) {
        if (cc == 0) {
            tmpShape = clip->controlData.pitchBend.shape;
        } else {
            if (!clip->controlData.ccChannels.count(cc)) {
                clip->controlData.createCCChannel(cc);
            }
            tmpShape = clip->controlData.ccChannels[cc].shape;
        }
    } else {
        tmpShape = DAW::Shape::GetShapeSaw(DAW::Shape::ShapeFlags::SHAPE_UNCLAMPPED | DAW::Shape::ShapeFlags::SHAPE_SHAPED);
        tmpShape.pts[1].pos.x *= grid.getTickLength();
    }
}

void gui_clipcontent_control_data::layout() {
    shapeEdit.layoutEditor(size);
    gui_clipcontent_base::layout();
}

bool gui_clipcontent_control_data::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (gui_clipcontent::mouseHitTest(mpos, evt)) {
        return true;
    }
    if (this->contains(mpos)) {
        // if (shapeEdit.mouseHitCurveEditor(tmpShape, localMouse)) 
        // {
            evt.requestFocus(this);
            return true;
        // }
    }
    return false;
}

void gui_clipcontent_control_data::handleDraggedBegin(MouseEvent& evt) {
    MouseEvent kevt = evt;
    kevt.relMousepos.x -= grid.tickToScreenD(getTickOffset());
    if (view.clip()) {
        controlDataBegin = view.clip()->controlData;
    }
    if (shapeEdit.onBeginDragCurveEditor(kevt)) {
        bIsDraggingShape = true;
        return;
    }
    gui_clipcontent::handleDraggedBegin(evt);
}

void gui_clipcontent_control_data::handleDraggedMove(MouseEvent& evt) {
    if (bIsDraggingShape) {
        evt.relMousepos.x -= grid.tickToScreenD(getTickOffset());
        shapeEdit.onMoveDragCurveEditor(evt);
        return;
    }
    dragTo = evt.relMousepos;
    if (dragMode == drag_frame && size.y > 2) {
        *evt.dragDistance     = ivec2(0);
        auto xStart     = math::min(dragBegin.x, dragTo.x);
        auto xEnd       = math::max(dragBegin.x, dragTo.x);
        auto yStart     = math::min(dragBegin.y, dragTo.y) / float(size.y);
        auto yEnd       = math::max(dragBegin.y, dragTo.y) / float(size.y);
        tick_t tickStart = grid.screenToTickSnap(xStart, SNAP_OFF) - getTickOffset();
        tick_t tickEnd   = grid.screenToTickSnap(xEnd, SNAP_OFF) - getTickOffset();
        shapeEdit.setSelectRect(vec4{tickStart, 1.0-yEnd, tickEnd, 1.0-yStart});
        // tick_t tickOver  = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        auto& cursor = view.m_cursor;
        cursor.start = tickStart;
        cursor.end   = tickEnd;
    }
    setGlobalSelectionFromClipSelection();
}

void gui_clipcontent_control_data::handleDraggedRelease(MouseEvent& evt) {
    if (bIsDraggingShape) {
        evt.relMousepos.x -= grid.tickToScreenD(getTickOffset());
        shapeEdit.onReleaseDragCurveEditor(evt);
        bIsDraggingShape = false;
        return;
    }
    gui_clipcontent::handleDraggedRelease(evt);
}

void gui_clipcontent_control_data::handleRightClick(MouseEvent& evt) {
    MouseEvent kevt = evt;
    kevt.relMousepos.x -= grid.tickToScreenD(getTickOffset());
    if (view.clip()) {
        controlDataBegin = view.clip()->controlData;
    }
    if (shapeEdit.onRightClickCurveEditor(kevt)) {
        return;
    }
    // gui_clipcontent::handleRightClick(evt);
}

void gui_clipcontent_control_data::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    renderBackground(vg);
    renderGridLines(vg, theme, grid.gridList, size);
    const auto clip = view.clip();
    if (!clip) {
        if (dragMode == drag_frame) {
            renderFrame(vg, dragBegin, dragTo);
        }
        return;
    }
    shapeEdit.layoutEditor(size);
    ivec2 localMouse = toControlsObjectSpace(parentCtrl->m_mousePos, this);
    // auto scaledPos = shapeEdit.toNormalizedSpace(localMouse);
    // // auto higlightHit = tmpShape.getMouseHit(scaledPos, shapeEdit.editorScale);
    // // float clipTickMin = grid.screenToTickD(0.0);
    // // float clipTickMax = grid.screenToTickD(size.x);
    const auto shapePos    = vec2(grid.tickToScreenD(getTickOffset()), 0);
    localMouse.x -= shapePos.x;
    // const auto shapeScale  = vec2(grid.tickLenToScreen(1.0), size.y);
    shapeEdit.renderEditor(vg, shapePos, theme, localMouse, false, &shapeEdit.getSelectedNodeIndices());
    // DAW::Shape::DrawShapeOneShot(tmpShape, 
    //                             vg, 
    //                             theme,
    //                             GuiColor::COL_SHAPE_CURVE,
    //                             GuiColor::COL_SHAPE_CURVE_HIGHLIGHT,
    //                             shapePos,
    //                             shapeScale,
    //                             clipTickMin,
    //                             clipTickMax,
    //                             higlightHit);
    // this->shapeEdit.setEditorCurve(&clip->controlData.pitchBend.shape);
    // this->shapeEdit.renderEditor(vg, {}, theme, relMousepos, false);
    auto x = float(grid.tickToScreenD(view.m_cursor.start));
    if (view.m_cursor.start == view.m_cursor.end) {
        if (x >= -2 && x < size.x + 2) {
            x += 0.5;
            NVGcolor cursorColor = getCursorColor();
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, 1);
            nvgLineTo(vg, x, size.y - 1);
            nvgStrokeColor(vg, cursorColor);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    } else {
        float x2 = (float) grid.tickToScreenD(view.m_cursor.end);
        if (x2 < x) {
            std::swap(x, x2);
        }
        if (x2 > -4.0f && x < size.x + 4.0f) {
            float xBegin = CLAMP_I(x, -4.0f, size.x + 3.0f);
            float xEnd   = CLAMP_I(x2, -3.0f, size.x + 4.0f);
            float width  = xEnd - xBegin;
            nvgBeginPath(vg);
            nvgRect(vg, xBegin, -2.0f, width, size.y + 2.0f);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_SELECTION_BACKGROUND));
            nvgFill(vg);
        }
    }
    if (dragMode == drag_frame) {
        renderFrame(vg, dragBegin, dragTo);
    }
}

void gui_clipcontent_velocities::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    renderBackground(vg);
    renderGridLines(vg, theme, grid.gridList, size);
    auto clip = view.clip();
    if (!clip) {
        if (dragMode == drag_frame) {
            renderFrame(vg, dragBegin, dragTo);
        }
        return;
    }
    float w = size.x;
    const float h = size.y;
    const clip_notes_t& notes = clip->notes;
    auto tickOffset = tick_t(0);
    if (view.isAbsoluteTimeMode()) {
        tickOffset = clip->time;
    }
    NVGpaint paint{};
    paint.image     = -1;
    paint.customPar = 1;
    nvgShapeAntiAlias(vg, 0);
    const float extendCullCheck = 8.0f;
    if (!notes.isEmpty()) {
        const int32_t nw = 4;
        const float r    = 4;
        for (int i = 0; i < 2; i++) {
            int nRendered = 0;
            for (const note_t& note: notes.m_list) {
                if ((i == 0) != note.isEnabled())
                    continue;
                auto nx = grid.tickToScreenD(note.time + tickOffset);
                if (nx + nw / 2.0f < -extendCullCheck) continue;
                if (nx - nw / 2.0f > w + extendCullCheck) continue;
                auto nh     = velocityToFloat(note.velocity) * h;
                auto insetx = calcInset(1, nw);
                auto insety = calcInset(1, nh);
                nvgBatchedRect(vg, float(nx - nw / 2.0f + insetx), size.y - nh + insety, nw - insetx * 2, nh - insety * 2);
                nRendered++;
            }
            if (nRendered) {
                paint.innerColor = theme->getColor(i == 0 ? GuiColor::COL_NOTE : GuiColor::COL_NOTE_MUTE);
                paint.innerColor.a *= 0.8f;
                paint.renderType = 4;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);

                for (const note_t& note: notes.m_list) {
                    if ((i == 0) != note.isEnabled())
                        continue;
                    auto nx = grid.tickToScreenD(note.time + tickOffset);
                    if (nx + r < -extendCullCheck) continue;
                    if (nx - r > w + extendCullCheck) continue;
                    auto nh = velocityToFloat(note.velocity) * h;
                    nvgBatchedRect(vg, float(nx - r), size.y - nh - r, r * 2, r * 2);
                }
                paint.renderType = 5;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }
    }
    if (!notes.selection.empty()) {
        const int32_t nw = 5;
        const float r    = 5;
        for (int i = 0; i < 2; i++) {
            int nRendered = 0;
            for (const note_t* pnote: notes.selection) {
                if ((i == 0) != pnote->isEnabled())
                    continue;
                auto nx = grid.tickToScreenD(pnote->time + tickOffset);
                if (nx + nw / 2.0f < -extendCullCheck) continue;
                if (nx - nw / 2.0f > w + extendCullCheck) continue;
                auto nh     = velocityToFloat(pnote->velocity) * h;
                auto insetx = calcInset(1, nw);
                auto insety = calcInset(1, nh);
                nvgBatchedRect(vg, float(nx - nw / 2.0f + insetx), size.y - nh + insety, nw - insetx * 2, nh - insety * 2);
                nRendered++;
            }
            if (nRendered) {
                paint.innerColor = theme->getColor(i == 0 ? GuiColor::COL_NOTE_SELECTED : GuiColor::COL_NOTE_MUTE);
                paint.renderType = 4;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
                for (const note_t* pnote: notes.selection) {
                    if ((i == 0) != pnote->isEnabled())
                        continue;
                    auto nx = grid.tickToScreenD(pnote->time + tickOffset);
                    if (nx + r < -extendCullCheck) continue;
                    if (nx - r > w + extendCullCheck) continue;
                    auto nh = velocityToFloat(pnote->velocity) * h;
                    nvgBatchedRect(vg, float(nx - r), size.y - nh - r, r * 2, r * 2);
                }
                paint.renderType = 5;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }
    }
    nvgShapeAntiAlias(vg, USE_NANOVG_AA);
    if (dragMode <= drag_frame) {
        const int32_t nw = 6;
        const float r = 6;
        ivec2 imouse  = toControlsObjectSpace(dawCtrl->m_mousePos, this);
        bool mouseIn  = dawCtrl->getGuiOverRef() == toRef() && contains(imouse + getPosContent());
        if (mouseIn) {
            tick_t mouseTick = !mouseIn ? INVALID_TICK : grid.screenToTickSnap(imouse.x, SNAP_OFF);
            int32_t velClicked = screenToVel(imouse.y, size.y);
            int32_t velDist    = VEL_SELECT_DISTANCE * 127 / size.y;
            auto clip = view.clip();
            note_t* contextNote = !clip ? nullptr : getMinDistNoteVel(clip->notes, mouseTick - tickOffset, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
            if (contextNote) {
                nvgBeginPath(vg);
                auto nx     = grid.tickToScreenD(contextNote->time + tickOffset);
                auto nh     = velocityToFloat(contextNote->velocity) * h;
                auto insetx = calcInset(1, nw);
                auto insety = calcInset(1, nh);
                nvgRect(vg, float(nx - nw / 2.0f + insetx), size.y - nh + insety, nw - insetx * 2, nh - insety * 2);

                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_SELECTED));
                nvgFill(vg);

                nvgBeginPath(vg);
                nvgCircle(vg, float(nx), size.y - nh, r);

                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_SELECTED));
                nvgFill(vg);
            }
        }
    }
    //  hit_result currentDragged = dragged.mode || !mouseIn ? dragged : hitTest(fmouse);
    //  if (currentDragged.mode == dragmode::drag_node) {
    //    int32_t ptIdx = currentDragged.dataPt;
    //    dbgassert(ptIdx >= 0 && ptIdx < (int)data.points.size());
    //    automation_point_t& pt = data.points[ptIdx];
    //    vec2* point = getPathPointSafe(currentDragged.segidx);
    //    mouseTick = pt.time;
    //    fmouse.x = point->x;
    //  }

    auto x = float(grid.tickToScreenD(view.m_cursor.start));
    if (view.m_cursor.start == view.m_cursor.end) {
        if (x >= -2 && x < size.x + 2) {
            x += 0.5;
            NVGcolor cursorColor = getCursorColor();
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, 1);
            nvgLineTo(vg, x, size.y - 1);
            nvgStrokeColor(vg, cursorColor);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    } else {
        float x2 = (float) grid.tickToScreenD(view.m_cursor.end);
        if (x2 < x) {
            std::swap(x, x2);
        }
        if (x2 > -4.0f && x < size.x + 4.0f) {
            float xBegin = CLAMP_I(x, -4.0f, size.x + 3.0f);
            float xEnd   = CLAMP_I(x2, -3.0f, size.x + 4.0f);
            float width  = xEnd - xBegin;
            nvgBeginPath(vg);
            nvgRect(vg, xBegin, -2.0f, width, size.y + 2.0f);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_SELECTION_BACKGROUND));
            nvgFill(vg);
        }
    }
    if (dragMode == drag_frame) {
        renderFrame(vg, dragBegin, dragTo);
    }
}

void gui_clipcontent_notes::renderClipNoteRects(NVGcontext* vg, const std::vector<note_t>& clipNotes, vec2 renderPos, vec2 renderSize, tick_t tickOffset, float scale, float inset, NVGcolor color, bool renderMuted) {
    if (!clipNotes.empty()) {
        NVGpaint paint{};
        paint.customPar  = 1;
        paint.image      = -1;
        paint.innerColor = color;
        int nRendered = 0;
        for (auto& note: clipNotes) {
            if (renderMuted != !note.isEnabled())
                continue;
            float nx = grid.tickToScreenD(note.time + tickOffset);
            float nw = grid.tickLenToScreen(note.len);
            if (nx + nw < renderPos.x - 4)
                continue;
            if (nx > renderPos.x+renderSize.x + 4)
                continue;
            float ny     = toScreenF(note.pitch);
            float nh     = scale;
            float insetx = math::clamp((nw-1.0f)*0.5f, 0.0f, 1.0f);
            float insety = math::clamp((nh-1.0f)*0.5f, 0.0f, 1.0f);
            if (nw < 1.0f) {
                float center = nx + nw * 0.5f;
                nx = center - 0.5f;
                nw = 1.0f;
            }
            if (nh < 1.0f) {
                float center = ny + nh * 0.5f;
                ny = center - 0.5f;
                nh = 1.0f;
            }
            nvgBatchedRect(vg, nx + insetx, ny - scale + insety, nw - insetx * 2, nh - insety * 2);
            nRendered++;
        }
        if (nRendered) {
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
    }
}
void gui_clipcontent_notes::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    if (size.x < 5 || size.y < 5) {
        return;
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    renderBackground(vg);
    const auto cs = getSizeContent();
    const float w = cs.x;
    const float h = cs.y;
    const bool fold           = layoutRoll.bFoldNotes;
    const float offset        = layoutRoll.offset();
    const float scale         = layoutRoll.scale();
    int32_t firstKey    = math::max((int32_t) floorf(offset / scale), 0);
    //render one extra key on top and bottom to fix antialiasing on edge of container
    if (firstKey > 0) {
        firstKey--;
    }
    float yOff = offset - firstKey * scale - scale;
    if (fold) {
        std::vector<int32_t> pitches;
        this->view.getNotePitches(pitches);


        nvgSave(vg);
        nvgTranslate(vg, 0, yOff);
        float y = 0;
        int numRowsSharp = 0;
        int len = CtrSize(pitches);
        for (int i = firstKey; i < len; i++) {
            int32_t pitch = pitches[i];
            if (isSharp(pitch)) {
                if (numRowsSharp == 0) {
                    nvgBeginPath(vg);
                }
                nvgRect(vg, 0, h - y, w, scale);
                numRowsSharp++;
            }
            y += scale;
            if (y >= cs.y + scale * 2) {
                break;
            }
        }
        if (numRowsSharp) {
            nvgFillColor(vg, theme->getColor(GuiColor::COL_CLIPEDITOR_SHARP));
            nvgSetShapeExtents(vg, 0, 0, cs.x, cs.y);
            nvgFill(vg);
        }

        renderGridLines(vg, theme, grid.gridList, cs);

        //    nvgBeginPath(vg);
        //    nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
        //    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
        //    if (firstKey == 0 && octave == 0) {
        //      nvgMoveTo(vg, 0, h - (y-scale));
        //      nvgLineTo(vg, w, h - (y-scale));
        //    }
        //    nvgStroke(vg);

        nvgBeginPath(vg);
        y = 0;
        for (int i = firstKey; i < len; i++) {
            nvgMoveTo(vg, 0, h - y);
            nvgLineTo(vg, w, h - y);
            y += scale;
            if (y >= cs.y + scale * 2) {
                break;
            }
        }
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
        nvgStroke(vg);
        nvgRestore(vg);
        //    if (yoct >= size.y+scale*2) {
        //      break;
        //    }
    } else {

        auto firstOctave = math::floordS32(firstKey / 12.0f);
        firstKey = firstKey % 12;

        //    nvgSave(vg);
        //    nvgTranslate(vg, 0, yOff);
        float yoct = 0;
        for (int32_t octave = firstOctave; octave < MAX_OCTAVES; octave++) {
            float y = yoct;
            nvgBeginPath(vg);
            for (int i = firstKey; i < 12; i++) {
                if (isSharp(i)) {
                    nvgRect(vg, 0, h - y + yOff, w, scale);
                }
                y += scale;
                if (y >= cs.y + scale * 2) {
                    break;
                }
            }
            nvgFillColor(vg, theme->getColor(GuiColor::COL_CLIPEDITOR_SHARP));
            nvgSetShapeExtents(vg, 0, 0, cs.x, cs.y);
            nvgFill(vg);

            y = yoct;
            nvgBeginPath(vg);
            if (firstKey == 0 && octave == 0) {
                nvgMoveTo(vg, 0, h - (y - scale) + yOff);
                nvgLineTo(vg, w, h - (y - scale) + yOff);
            }
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
            nvgStroke(vg);

            nvgBeginPath(vg);
            for (int i = firstKey; i < 12; i++) {
                nvgMoveTo(vg, 0, h - y + yOff);
                nvgLineTo(vg, w, h - y + yOff);
                y += scale;
                if (y >= cs.y + scale * 2) {
                    break;
                }
            }
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
            nvgStroke(vg);
            yoct = y;
            if (yoct >= cs.y + scale * 2) {
                break;
            }
            firstKey = 0;
        }


        renderGridLines(vg, theme, grid.gridList, cs);

    }
    auto& selView = view.m_selectionView;
    clip_t* currentClip = view.clip();
    // this deref is not safe
    for (auto& [trackEntry, vecClips] : selView.tracks) {
        for (clip_t* clip : vecClips) {
            if (!view.clipRef().isTrackValid(trackEntry.track)
                || !trackEntry.track->getClips().hasClip(clip)) {
                continue;
            }
            auto tickOffset = clip->time;
            if (!view.isAbsoluteTimeMode() && currentClip) {
                tickOffset += currentClip->offsetStart;
                if (clip != currentClip) {
                    tickOffset += clip->time - currentClip->time;
                }
            }
            // auto col = track == view.track() ? clip->rgb : track->rgb;
            auto col = clip->rgb;
            auto& notesView = clip->getNoteViewRender();
            if (notesView.isEmpty()) {
                continue;
            }
            renderClipNoteRects(vg, notesView.m_list, {}, cs, tickOffset, scale, 1.0f, rgbToNvg(col), false);
        }
    }
    auto tickOffset = tick_t(0);
    if (currentClip) {
        auto& clipNotes = currentClip->notes;
        if (view.isAbsoluteTimeMode()) {
            tickOffset = currentClip->time;
        }
        renderClipNoteRects(vg, clipNotes.m_list, {}, cs, tickOffset, scale, 1.0f, theme->getColor(GuiColor::COL_NOTE_MUTE), true);
        renderClipNoteRects(vg, clipNotes.m_list, {}, cs, tickOffset, scale, 1.0f, theme->getColor(GuiColor::COL_NOTE), false);
    }
    
    auto x = float(grid.tickToScreenD(view.m_cursor.start));
    if (view.m_cursor.start == view.m_cursor.end) {
        if (x >= -2 && x < cs.x + 2) {
            x += 0.5;
            NVGcolor cursorColor = getCursorColor();
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, 1);
            nvgLineTo(vg, x, cs.y - 1);
            nvgStrokeColor(vg, cursorColor);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    } else {
        float x2 = (float) grid.tickToScreenD(view.m_cursor.end);
        if (x2 < x) {
            std::swap(x, x2);
        }
        if (x2 > -4.0f && x < cs.x + 4.0f) {
            float xBegin = CLAMP_I(x, -4.0f, cs.x + 3.0f);
            float xEnd   = CLAMP_I(x2, -3.0f, cs.x + 4.0f);
            float width  = xEnd - xBegin;
            nvgBeginPath(vg);
            nvgRect(vg, xBegin, -2.0f, width, cs.y + 2.0f);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_SELECTION_BACKGROUND));
            nvgFill(vg);
        }
    }

    clip_t* clip = view.clip();
    auto track = view.track();
    if (clip && track && track->audio) {


        /* auto daw = dawCtrl->getDaw();
        ThreadLock lock = daw->lockPlayThread();
        std::vector<note_t> heldRealtimeNotes = daw->getHost()->getRealtimeNotes();//TODO: NOT THREADSAFE
        if (heldRealtimeNotes.size()) {
            int nRendered = 0;
            for (note_t& note: heldRealtimeNotes) {
                if (note.isRealtime()) {
                    continue;
                }
                tick_t pos = note.start() - clip->start() + clip->offsetStart;
                if (clip->isLoopEnabled()) {
                    if (pos > clip->loopStart) {
                        pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                    }
                }
                //TODO: CULL
                renderNote(vg, this, &note, scale, -note.start() + pos);
                nRendered++;
            }
            if (nRendered) {
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_REALTIME);
                paint.customPar  = 2;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        } */
        ThreadLock lock                = track->audio->midiMutex.lockThread();
        std::vector<note_t>& heldNotes = track->audio->m_heldNotes;
        if (heldNotes.size()) {
            int nRendered = 0;
            for (note_t& note: heldNotes) {
                if (note.isRealtime()) {
                    continue;
                }
                tick_t pos = note.start() - clip->start() + clip->offsetStart;
                if (clip->isLoopEnabled()) {
                    if (pos > clip->loopStart) {
                        pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                    }
                }
                //TODO: CULL
                renderNote(vg, this, &note, scale, -note.start() + pos);
                nRendered++;
            }
            if (nRendered) {
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_PLAYING);
                paint.customPar  = 1;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }

        if (track->audio->arp->isProcessingEnabled()) {
            auto& heldNotesArp = track->audio->getArpHeldNotes();//TODO: NOT THREADSAFE
            if (heldNotesArp.size()) {
                // nvgBeginPath(vg);

                for (auto& note: heldNotesArp) {
                    tick_t pos = note.start() - clip->start() + clip->offsetStart;
                    if (clip->isLoopEnabled()) {
                        if (pos > clip->loopStart) {
                            pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                        }
                    }
                    //TODO: CULL
                    renderNote(vg, this, &note, scale, -note.start() + pos);
                }
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_ARP);
                paint.customPar  = 1234;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            
        }

        float yoff = 0;
        for (int i = 0; i < 2; i++) {
            std::vector<marker_t> markers = track->audio->getArpMarkers(i);//TODO: NOT THREADSAFE
            if (markers.size()) {
                for (marker_t& m: markers) {
                    tick_t pos = m.time - clip->start() + clip->offsetStart;
                    if (clip->isLoopEnabled()) {
                        if (pos > clip->loopStart) {
                            pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                        }
                    }
                    auto nx = grid.tickToScreenD(pos);
                    if (nx < -4)
                        continue;
                    if (nx > w + 4)
                        continue;
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, nx, m.yOffset * 24 + 0 + yoff);
                    nvgLineTo(vg, nx, m.yOffset * 24 + h + yoff);
                    nvgStrokeColor(vg, rgbToNvg(m.color));
                    nvgStrokeWidth(vg, 2.0f);
                    nvgStroke(vg);
                    if (m.desc[0]) {
                        String cstr = m.desc;
                        setFont(vg, G_FONT_SCALE(24), THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                        float bounds[4];
                        float textX = nx + INSET_TRACK_CONTENT;
                        float textY = m.yOffset * 24 + 24 / 2.0f + INSET_TRACK_CONTENT + yoff;
                        nvgTextBounds(vg, textX, textY, cstr.c_str(), nullptr, bounds);
                        nvgBeginPath(vg);
                        nvgRect(vg, bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]);
                        nvgFillColor(vg, rgbaToNvg(i == 0 ? 0xFF121212 : 0xFF444412));
                        nvgFill(vg);
                        nvgFillColor(vg, THEMECOL_TEXT);
                        nvgText(vg, textX, textY, cstr.c_str(), nullptr);
                    }
                    yoff += 3;
                }
            }
        }
    }

    if (currentClip) {
        const auto& notes = currentClip->notes;
        int n2 = 0;
        if (dragMode >= drag_notes_move) {
            for (note_t& note: view.draggedSelection) {
                renderNote(vg, this, &note, scale, tickOffset);
                n2++;
            }
        } else {
            for (note_t* pnote: notes.selection) {
                renderNote(vg, this, pnote, scale, tickOffset);
                n2++;
            }
        }
        if (n2) {
            NVGpaint paint{};
            paint.image      = -1;
            paint.innerColor = theme->getColor(GuiColor::COL_NOTE_SELECTED);
            paint.customPar  = 1234;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
        if (scale >= 10) {
            for (auto& note: notes.m_list) {
                auto nx = grid.tickToScreenD(note.time + tickOffset);
                auto nw = grid.tickLenToScreen(note.len);
                if (nx + nw < -4)
                    continue;
                if (nx > w + 4)
                    continue;
                tick_t absPos = note.start();
                if (currentClip) {
                    absPos = note.start() + currentClip->start() - currentClip->offsetStart;
                }
                renderNoteName(vg, this, &note, nx, toScreenF(note.pitch), nw, scale, absPos, false);
            }
            for (note_t* pNote: notes.selection) {
                auto& note = *pNote;
                auto nx = grid.tickToScreenD(note.time + tickOffset);
                auto nw = grid.tickLenToScreen(note.len);
                if (nx + nw < -4)
                    continue;
                if (nx > w + 4)
                    continue;
                tick_t absPos = note.start();
                if (currentClip) {
                    absPos = note.start() + currentClip->start() - currentClip->offsetStart;
                }
                renderNoteName(vg, this, &note, nx, toScreenF(note.pitch), nw, scale, absPos, true);
            }
        }
    }

    if (dragMode == drag_frame) {
        renderFrame(vg, dragBegin, dragTo);
    }
}

void gui_clipcontent::handleDraggedBegin(MouseEvent& evt) {
    dragMode     = drag_none;
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }
    clip_notes_t& notes    = clip->notes;
    ivec2 local            = evt.relMousepos;
    int32_t pitch          = math::floorfS32(toNoteF(local.y));
    int32_t velClicked     = screenToVel(local.y, size.y);
    auto tickOffset = tick_t(0);
    if (view.isAbsoluteTimeMode()) {
        tickOffset = clip->time;
    }
    const tick_t tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
    note_t* contextNote    = nullptr;
    if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
        int32_t velDist = VEL_SELECT_DISTANCE * 127 / size.y;
        contextNote     = getMinDistNoteVel(notes, tickExact - tickOffset, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
    } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
        contextNote = notes.get(tickExact - tickOffset, pitch);
    }
    tick_t tickGridNearest = grid.screenToTickSnap(local.x, SNAP_ON);
    tick_t tickGridLeast   = grid.prev(tickExact);
    bool inSelection = false;
    if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES || guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
        if (evt.type == M_EVT_DOUBLECLICK) {
            ThreadLock lock            = dawCtrl->lockPlayThread();
            clip_cursor_t cursorBefore = view.m_cursor;
            notes.clearSelection();
            clip_notes_t notesBefore = notes;
            if (contextNote) {
                contextNote = notes.get(tickExact - tickOffset, pitch);
            }
            String desc = "???";
            if (contextNote) {
                if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
                    view.m_cursor.start = view.m_cursor.end = contextNote->start();
                    notes.remove(*contextNote);
                    contextNote = nullptr;
                    desc        = "Delete Note";
                } else {
                    contextNote->toggleFlag(NoteFlags::ENABLED);
                }
            } else {
                if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
                    note_t note;
                    note.pitch = pitch;
                    note.time  = tickGridLeast - tickOffset;
                    note.len   = grid.getTickLength();
                    notes.paste(note);
                    contextNote = notes.get(tickGridLeast - tickOffset, pitch);
                    if (assert_expr(contextNote)) {
                        notes.selection.insert(contextNote);
                        view.copySelectedNoteList();
                        dawCtrl->setStatusText(StringFormat("%d %d %d", note.pitch, note.time, note.len));
                        desc = "Add Note";
                        setSelectionFrame(getMinMaxTime(view.draggedSelection));
                    }
                }
            }
            dawCtrl->getDaw()->pushHist(new action_modify_notes(desc, view, notesBefore, cursorBefore));
            clip->setDirty();
            view.updateNotePitches(false);
            inSelection = true;
        } else {

            if (contextNote) {
                inSelection = stl_contains(notes.selection, contextNote);
                if (!inSelection) {
                    notes.clearSelection();
                    if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
                        int32_t velDist = VEL_SELECT_DISTANCE * 127 / size.y;
                        contextNote     = getMinDistNoteVel(notes, tickExact - tickOffset, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
                    } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
                        contextNote = notes.get(tickExact - tickOffset, pitch);
                    }
                    if (contextNote) {
                        beginDragNote = *contextNote;
                        notes.selection.insert(contextNote);
                        view.copySelectedNoteList();
                        inSelection = true;
                    }
                }
                if (contextNote) {
                    beginDragNote = *contextNote;
                }
            }
            if (inSelection) {
                if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
                    dragMode = drag_velocity;
                } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
                    auto distL = local.x - grid.tickToScreenD(contextNote->start() + tickOffset);
                    auto distR = grid.tickToScreenD(contextNote->end() + tickOffset) - local.x;
                    if (distL < DRAG_RANGE) {
                        dragMode = drag_note_left;
                    }
                    if (distR < DRAG_RANGE && (distL >= DRAG_RANGE || distR < distL)) {
                        dragMode = drag_note_right;
                    }
                    if (dragMode == drag_none) {
                        if (isCtrl(evt.kbmods)) {
                            parentCtrl->cursorIcon = CURSOR_DUPLICATE;
                            dragMode               = drag_notes_copy;
                        } else {
                            dragMode = drag_notes_move;
                        }
                    }
                }
                notes.removeDuplicates();
                view.copySelectedNoteList();

                notes.selection.clear();
                setSelectionFrame(getMinMaxTime(view.draggedSelection));
                dragStartCursor = view.m_cursor;
            }
        }
    }
    if (!inSelection) {
        if (notes.selection.empty())
            notes.removeDuplicates();
        if (isShift(evt.kbmods)) {
            selectionStart = notes.selection;
            if (math::abs(view.m_cursor.start - tickGridNearest) < math::abs(view.m_cursor.end - tickGridNearest)) {
                view.m_cursor.start = tickGridNearest;
            } else {
                view.m_cursor.end = tickGridNearest;
            }
        } else {
            selectionStart.clear();
            notes.clearSelection();
            view.m_cursor.start = view.m_cursor.end = tickGridNearest;
            view.copySelectedNoteList();
        }

        dragMode = drag_frame;
    }
    if (dragMode != drag_none) {
        dragBegin = local;
        dragTo    = local;
    }
    setStatusText();
    setGlobalSelectionFromClipSelection();
}
void gui_clipcontent::setGlobalSelectionFromClipSelection() {
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }
    DAW::Cursor& cursor = dawCtrl->getCursor();
    if (!view.isAbsoluteTimeMode()) {
        cursor.cursorPos = view.m_cursor.start + clip->start() - clip->offsetStart;
        cursor.selRange  = view.m_cursor.end - view.m_cursor.start;
    } else {
        cursor.cursorPos = view.m_cursor.start;
        cursor.selRange  = view.m_cursor.end - view.m_cursor.start;
    }
}
void gui_clipcontent::setStatusText() {
    clip_notes_t& notes = view.clip()->notes;
    String selStatus    = StringFormat("%zu notes selected", notes.selection.size());
    if (!view.draggedSelection.empty()) {
        auto pair = getMinMaxSemitones(view.draggedSelection);
        if (pair.first && pair.second) {
            selStatus += " - ";
            selStatus += StringFormat("pitch %d to %d", pair.first->pitch, pair.second->pitch);
            selStatus += " - ";
            auto pair2 = getMinMaxTime(view.draggedSelection);
            selStatus += StringFormat("time %d to %d", pair2.first->start(), pair2.second->end());
        }
    }
    dawCtrl->setStatusText(selStatus);
}

void gui_clipcontent::handleDraggedMove(MouseEvent& evt) {
    clip_t* clip = view.clip();
    if (!clip)
        return;
    clip_notes_t& notes = clip->notes;
    auto tickOffset = tick_t(0);
    if (view.isAbsoluteTimeMode()) {
        tickOffset = clip->time;
    }
    if (dragMode == drag_none)
        return;
    dragTo = evt.relMousepos;
    if (dragMode == drag_frame && (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES || guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY)) {
        *evt.dragDistance     = ivec2(0);

        auto xStart     = math::min(dragBegin.x, dragTo.x);
        auto xEnd       = math::max(dragBegin.x, dragTo.x);
        auto yStart     = math::min(dragBegin.y, dragTo.y);
        auto yEnd       = math::max(dragBegin.y, dragTo.y);
        tick_t tickStart = grid.screenToTickSnap(xStart, SNAP_OFF);
        tick_t tickEnd   = grid.screenToTickSnap(xEnd, SNAP_OFF);
        tick_t tickOver  = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);

        clip_cursor_t& cursor = view.m_cursor;
        if (isShift(evt.kbmods)) {
            tick_t gridSize = grid.getTickLength();
            if (math::abs(cursor.start - tickOver) < math::abs(cursor.end - tickOver)) {
                if (tickOver < cursor.end - gridSize) {
                    cursor.start = tickOver;
                }
            } else {
                if (tickOver > cursor.start + gridSize) {
                    cursor.end = tickOver;
                }
            }
        } else {
            cursor.start = grid.screenToTickSnap(xStart, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
            cursor.end   = grid.screenToTickSnap(xEnd, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
            setSelectionFrame(getMinMaxTime(view.draggedSelection));
        }
        notes.selection = selectionStart;
        if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {

            int32_t pitchLow  = math::floorfS32(toNoteF(yEnd));
            int32_t pitchHigh = math::floorfS32(toNoteF(yStart));
            std::vector<note_t*> inRangeList;
            //TODO: selection must happen from looped notes in absolute mode
            if (notes.getInRange(tickStart - tickOffset, tickEnd - tickOffset, pitchLow, pitchHigh, inRangeList)) {
                std::set<note_t*>& selection = notes.selection;
                for (note_t* inSelRange: inRangeList) {
                    auto result = selection.insert(inSelRange);
                    if (!result.second) {
                        selection.erase(result.first);
                    }
                }
            }
        } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
            int32_t velLow  = screenToVel(yEnd, size.y);
            int32_t velHigh = screenToVel(yStart, size.y);
            std::vector<note_t*> inRangeList;
            //TODO: selection must happen from looped notes in absolute mode
            if (notes.getStartsInRangeV(tickStart - tickOffset, tickEnd - tickOffset, velLow, velHigh, grid.pixelsToTicks(VEL_SELECT_DISTANCE), inRangeList)) {
                std::set<note_t*>& selection = notes.selection;
                for (note_t* inSelRange: inRangeList) {
                    auto result = selection.insert(inSelRange);
                    if (!result.second) {
                        selection.erase(result.first);
                    }
                }
            }
        }
        if (!isShift(evt.kbmods)) {
            if (!notes.selection.empty()) {
                auto pair = getMinMaxTime(notes.selection);
                expandSelectionFrame(pair);
            }
        }
        view.copySelectedNoteList();

        setStatusText();
    } else if (dragMode == drag_velocity) {
        *evt.dragDistance = ivec2(0);
        //    tick_t tickOver = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        //    clip_cursor_t& cursor = view.m_cursor;
        ThreadLock lock       = dawCtrl->lockPlayThread();
        int32_t velOffset     = (dragBegin.y - dragTo.y) * 127 / size.y;
        view.draggedSelection = view.draggedSelectionBegin;
        {
            auto it          = view.draggedSelection.begin();
            const auto itEnd = view.draggedSelection.end();
            while (it != itEnd) {
                note_t& note  = *it;
                note.velocity = math::min(127, math::max(0, note.velocity + velOffset));
                it++;
            }
        }
        mergeDraggedNotes(dragMode);
        setSelectionFrame(getMinMaxTime(view.draggedSelection));

    } else if (dragMode >= drag_notes_move) {
        ThreadLock lock = dawCtrl->lockPlayThread();
        int modeMove    = SNAP_ON;
        if (isAlt(evt.kbmods)) {
            modeMove = SNAP_OFF;
        }


        tick_t gridSize       = grid.getTickLength();
        int32_t pitchStart    = math::floorfS32(toNoteFNoFolding(dragBegin.y));
        int32_t pitchEnd      = math::floorfS32(toNoteFNoFolding(dragTo.y));
        tick_t pitchOffset    = pitchEnd - pitchStart;
        tick_t tickStartExact = grid.screenToTick(dragBegin.x);
        tick_t tickEndExact   = grid.screenToTick(dragTo.x);
        tick_t timeOffsetEx   = tickEndExact - tickStartExact;

        tick_t timeOffset = 0;
        const note_t noteDrag = this->beginDragNote;
        if (modeMove == SNAP_ON) {
            tick_t handlePos = dragMode == drag_note_right ? noteDrag.end() : noteDrag.start();
            handlePos += tickOffset;
            if (math::abs(timeOffsetEx) > gridSize / 4) {
                tick_t next = grid.next(handlePos + timeOffsetEx) - handlePos;
                tick_t prev = grid.prev(handlePos + timeOffsetEx) - handlePos;
                if (prev < 0 && timeOffsetEx > 0) {
                    prev = next;
                }
                if (next > 0 && timeOffsetEx < 0) {
                    next = prev;
                }
                if (math::abs(next) > math::abs(prev)) {
                    timeOffset = prev;
                } else {
                    timeOffset = next;
                }
            }
        } else {
            timeOffset = timeOffsetEx;
        }
        view.draggedSelection = view.draggedSelectionBegin;
        {
            auto it          = view.draggedSelection.begin();
            const auto itEnd = view.draggedSelection.end();
            while (it != itEnd) {
                note_t& note = *it;
                if (dragMode == drag_note_left) {
                    note_t* before = getFirstBefore(notes.m_list, note.pitch, note.time);
                    note.time      = math::min(note.end() - 1, note.start() + timeOffset);
                    note.len       = math::max(math::max(1, gridSize/4), note.len - timeOffset);
                    if (before) {
                        if (note.start() < before->end()) {
                            note.cutLeft(before->end());
                        }
                    }
                } else if (dragMode == drag_note_right) {
                    note_t* after = getFirstAfter(notes.m_list, note.pitch, note.time);
                    note.len      = math::max(math::max(1, gridSize/4), note.len + timeOffset);
                    if (after) {
                        if (note.end() > after->start()) {
                            note.cutRight(after->start());
                        }
                    }
                } else {
                    note.time += timeOffset;
                    if (layoutRoll.bFoldNotes) {
                        note.pitch = math::floorfS32(view.nextFoldNote(note.pitch, pitchOffset));
                    } else {
                        note.pitch += pitchOffset;
                    }
                }

                it++;
            }
        }
        {
            std::vector<note_t> notesDraggedCopy = view.draggedSelection;
            std::vector<note_t> notesDraggedNoDuplicates = view.draggedSelection;

            auto it    = notesDraggedCopy.begin();
            auto itEnd = notesDraggedCopy.end();
            while (it != itEnd) {
                note_t& noteTest = *it++;
                if (cutIntersectingEliminateDupes(notesDraggedNoDuplicates, noteTest, false) > 0) {
                    notesDraggedCopy = notesDraggedNoDuplicates;
                    it    = notesDraggedCopy.begin();
                    itEnd = notesDraggedCopy.end();
                }
            }
            view.draggedSelection = notesDraggedNoDuplicates;
        }
        mergeDraggedNotes(dragMode);
        setSelectionFrame(getMinMaxTime(view.draggedSelection));
    }
    setGlobalSelectionFromClipSelection();
}
bool gui_clipcontent::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {

        ivec2 local = this->toContainerSpace(mpos);
        for (guibase* gui: guis) {
            if (gui->isVisible() && gui->mouseHitTest(local, evt)) {
                return true;
            }
        }
        if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES && evt.type <= MouseHitType::MOUSE_RIGHT) {
            auto clip  = view.clip();
            if (clip) {
                auto pitch = toNoteF(local.y);
                auto tickExact  = grid.screenToTickSnap(local.x, SNAP_OFF);
                auto tickOffset = tick_t(0);
                if (view.isAbsoluteTimeMode()) {
                    tickOffset = clip->time;
                }
                //TODO: selection must happen from looped notes in absolute mode
                const auto* contextNote = clip->notes.get(tickExact - tickOffset, math::floorfS32(pitch));
                if (contextNote) {
                    auto distL = local.x - grid.tickToScreenD(contextNote->start() + tickOffset);
                    auto distR = grid.tickToScreenD(contextNote->end() + tickOffset) - local.x;
                    if (distL < DRAG_RANGE) {
                        evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
                    }
                    if (distR < DRAG_RANGE && (distL >= DRAG_RANGE || distR < distL)) {
                        evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
                    }
                }
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}

void gui_clipcontent::mergeDraggedNotes(dragmode mergeMode) {
    clip_t* clip        = view.clip();
    clip_notes_t& notes = clip->notes;
    notes               = view.dragStartNotes;
    notes.selection.clear();
    if (mergeMode != dragmode::drag_notes_copy) {
        notes.removeAllKeepDuplicates(view.draggedSelectionBegin);
    }
    for (note_t& note: view.draggedSelection) {
        notes.paste(note, true);
    }
    notes.selectLastN(view.draggedSelection.size());
    notes.updateBounds();
    clip->setDirty();
    view.updateNotePitches(false);
}
void gui_clipcontent::expandSelectionFrame(std::pair<note_t*, note_t*> minMax) {
    if (minMax.first && minMax.second) {
        auto& cursor = view.m_cursor;
        cursor.start = math::min(cursor.start, minMax.first->time + getTickOffset());
        cursor.end   = math::max(cursor.end, (minMax.second->time + minMax.second->len + getTickOffset()));
    }
}
void gui_clipcontent::setSelectionFrame(std::pair<note_t*, note_t*> minMax) {
    if (minMax.first && minMax.second) {
        auto& cursor = view.m_cursor;
        cursor.start = minMax.first->time + getTickOffset();
        cursor.end   = minMax.second->time + minMax.second->len + getTickOffset();
    }
}
void gui_clipcontent::handleDraggedRelease(MouseEvent& evt) {
    if (dragMode == drag_frame) {
        handleDraggedMove(evt);
        dragMode = drag_none;
        return;
    }
    clip_t* clip = view.clip();
    if (clip) {
        if (dragMode >= drag_notes_move) {
            ThreadLock lock = dawCtrl->lockPlayThread();
            clip_notes_t& notes = clip->notes;
            mergeDraggedNotes(dragMode);
            setSelectionFrame(getMinMaxTime(notes.selection));
            String action;
            if (dragMode == drag_velocity) {
                action = "Modify note velocities";
            } else if (dragMode >= drag_note_left) {
                action = "Modify note lengths";
            } else {
                action = "Move notes";
            }
            dawCtrl->getDaw()->pushHist(new action_modify_notes(action, view, view.dragStartNotes, dragStartCursor));
            view.copySelectedNoteList();
            clip->setDirty();
            view.updateNotePitches(false);
        }
    }
    dragMode = drag_none;
    setGlobalSelectionFromClipSelection();
}

bool gui_clipcontent::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    auto daw = dawCtrl->getDaw();
    auto command = ctxt.type;
    auto& kevt = ctxt.kevt;
    if (focused() && command == CMD_PASTE && daw->getClipboardType() != ClipBoardType::CLIPBOARD_NOTES) {
        // suppress paste of clips by returning true for "is handled"
        return true;
    }
    clip_t* clip = view.clip();
    if (!clip) {
        return false;
    }
    clip_notes_t& notes = clip->notes;
    if (kevt.type != K_RELEASE) {
        clip_cursor_t& cursor          = view.m_cursor;
        const clip_notes_t notesBefore = notes; // copy
        clip_cursor_t cursorBefore     = cursor;// copy
        bool handled                   = false;
        bool edit                      = false;
        String desc                    = "???";
        if (kevt.type == K_PRESS) {
            if (command == CMD_SELECT_ALL) {
                notes.clearSelection();
                notes.updateBounds();
                notes.selectIdxRange(0, notes.m_list.size());
                view.copySelectedNoteList();
                setSelectionFrame(getMinMaxTime(notes.selection));
                handled = true;
            }
            if (command == CMD_DELETE && !notes.selection.empty()) {
                notes.deleteSelectedNotes(notes);
                handled = true;
                edit    = true;
                desc    = "Delete notes";
            }
            if (command == CMD_MUTE && !notes.selection.empty()) {
                //        notes.muteToggleSelectedNotes(notes);
                muteNotesToggle(view.draggedSelection);
                mergeDraggedNotes(dragmode::drag_notes_move);
                notes.updateBounds();
                // setSelectionFrame(getMinMaxTime(notes.selection));
                handled = true;
                edit    = true;
                desc    = "Mute notes";
            } else if (command == CMD_CUT && !notes.selection.empty()) {
                auto clipboard = std::make_shared<notes_clipboard>();
                clipboard->cursorRange = cursor.end - cursor.start;
                clipboard->notes.setTo(notes.selection, -(cursor.start - getTickOffset()));
                daw->setNotesClipboard(clipboard);
                notes.deleteSelectedNotes(notes);
                handled = true;
                edit    = true;
                desc    = "Cut notes";
            } else if (command == CMD_COPY && !notes.selection.empty()) {
                auto clipboard = std::make_shared<notes_clipboard>();
                clipboard->cursorRange = cursor.end - cursor.start;
                clipboard->notes.setTo(notes.selection, -(cursor.start - getTickOffset()));
                daw->setNotesClipboard(clipboard);
                handled = true;
                desc    = "Copy notes";// never appears in list
            } else if (command == CMD_DUPLICATE && !notes.selection.empty()) {
                clip_notes_t tmpClipboard;
#ifndef NDEBUG
                for (note_t* selPtr: notes.selection) {
                    dbgassert(notes.has(selPtr));
                }
#endif
                tmpClipboard.setTo(notes.selection, -(cursor.start - getTickOffset()));
                tick_t cursorRange = cursor.end - cursor.start;
                cursor.start += cursorRange;
                cursor.end += cursorRange;
                notes.clearSelection();
                view.copySelectedNoteList();
                view.draggedSelection.clear();
                for (note_t note: tmpClipboard.m_list) {//not using reference here, copy while iterating
                    note.time += cursor.start - getTickOffset();
                    view.draggedSelection.push_back(note);
                }
                mergeDraggedNotes(dragmode::drag_notes_copy);
#ifndef NDEBUG
                for (note_t* selPtr: notes.selection) {
                    dbgassert(notes.has(selPtr));
                }
#endif
                auto pair = getMinMaxTime(notes.selection);
                if (pair.second)
                    grid.makeTickVisible(pair.second->end() + getTickOffset());
                handled = true;
                edit    = true;
                desc    = "Duplicate notes";
            } else if (command == CMD_PASTE && daw->getClipboardType() == ClipBoardType::CLIPBOARD_NOTES && !daw->getNotesClipboard()->empty()) {
                auto& clipboard = daw->getNotesClipboard();
                notes.clearSelection();
                view.copySelectedNoteList();
                view.draggedSelection.clear();
                for (note_t note: clipboard->notes.m_list) {//not using reference here, copy while iterating
                    // note.time += cursor.start;
                    note.time += cursor.start - getTickOffset();
                    view.draggedSelection.push_back(note);
                }
                mergeDraggedNotes(dragmode::drag_notes_move);
                view.m_cursor.end = cursor.start + clipboard->cursorRange;
                auto pair = getMinMaxTime(notes.selection);
                if (pair.second)
                    grid.makeTickVisible(pair.second->end() + getTickOffset());
                handled = true;
                edit    = true;
                desc    = "Paste notes";
            } else if (command == CMD_QUANTIZE && !notes.selection.empty()) {
                auto& settings = dawCtrl->getDaw()->getQuantizeSettings();
                if (settings.quantizeStart > 0 || settings.quantizeEnd > 0) {
                    /* Quantize notes to grid 
                     * 1. cut notes from clip
                     * 2. quantize notes in isolation
                     * 3. paste notes back to clip, cutting intersections
                     */
                    clip_notes_t tmpClipboard;
                    tmpClipboard.setTo(notes.selection, 0);
                    notes.deleteSelectedNotes(notes);
                    notes.clearSelection();
                    view.copySelectedNoteList();
                    view.draggedSelection.clear();
                    if (settings.quantizeStart > 0) {
                        quantizeNoteStartTime(tmpClipboard.m_list, settings.quantizeStart);
                    }
                    if (settings.quantizeEnd > 0) {
                        quantizeNoteEndTime(tmpClipboard.m_list, settings.quantizeEnd);
                    }
                    bool bRemovedNotes = cutSelfIntersecting(tmpClipboard.m_list);
                    if (bRemovedNotes) {
                        log_lf(Log::L_DEBUG, "removed some intersecting notes\n");
                    }
                    for (note_t note: tmpClipboard.m_list) {//not using reference here, copy while iterating
                        view.draggedSelection.push_back(note);
                    }
                    mergeDraggedNotes(dragmode::drag_notes_copy);
#ifndef NDEBUG
                    for (note_t* selPtr: notes.selection) {
                        dbgassert(notes.has(selPtr));
                    }
#endif
                    auto pair = getMinMaxTime(notes.selection);
                    if (pair.second)
                        grid.makeTickVisible(pair.second->end() + getTickOffset());
                    expandSelectionFrame(pair);
                    edit = true;
                    handled = true;
                    desc = "Quanitize notes";
                }
            } else if (command == CMD_APPLY_PYTHON_SCRIPT) {
                if (notes.selection.empty()) {
                    DAW::UI::CommandContext ctxtSelectAll = ctxt;
                    ctxtSelectAll.type = GlobalCommandType::CMD_SELECT_ALL;
                    handleEditorCommand(ctxtSelectAll);
                }
                clip_notes_t tmpClipboard;
                tmpClipboard.setTo(notes.selection, 0);
                notes.deleteSelectedNotes(notes);
                notes.clearSelection();
                view.copySelectedNoteList();
                view.draggedSelection.clear();
                try {
                    DAW::PythonNoteProcessor::python_script_ctxt_t pyCtxt;
                    pyCtxt.notes = tmpClipboard.m_list;
                    seq_rand rnd;
                    rnd.rng_seed(getTimeMillis());
                    pyCtxt.seed = int32_t(rnd.rng_rand());
                    pyCtxt.params = ctxt.argFloats;
                    tmpClipboard.m_list = DAW::PythonNoteProcessor::RunPythonNoteProcessor(ctxt.argStr0, pyCtxt);
                } catch (std::exception& e) {
                    log_lf(Log::L_ERROR, "Python script failed: %s\n", e.what());
                }
                cutSelfIntersecting(tmpClipboard.m_list);
                for (note_t note: tmpClipboard.m_list) {//not using reference here, copy while iterating
                    view.draggedSelection.push_back(note);
                }
                mergeDraggedNotes(dragmode::drag_notes_copy);
#ifndef NDEBUG
                for (note_t* selPtr: notes.selection) {
                    dbgassert(notes.has(selPtr));
                }
#endif
                auto pair = getMinMaxTime(notes.selection);
                if (pair.second)
                    grid.makeTickVisible(pair.second->end() + getTickOffset());
                expandSelectionFrame(pair);
                edit = true;
                handled = true;
                desc = "Apply python note processor";
            }
        }
        if (command == GlobalCommandType::CMD_MOVE_CURSOR) {
            auto dir = ivec2(ctxt.argInt0, ctxt.argInt1);
            if (dir.y && !notes.selection.empty()) {
                if ((kevt.mods & KB_MOD_SHIFT)) {
                    dir *= 12;
                }
                changePitch(view.draggedSelection, dir.y,
                            layoutRoll.bFoldNotes, layoutRoll.bFoldNotes ? view.notePitches : std::vector<int32_t>{});
                mergeDraggedNotes(dragmode::drag_notes_move);
                notes.updateBounds();
                setSelectionFrame(getMinMaxTime(notes.selection));
                auto pair = getMinMaxSemitones(view.draggedSelection);
                if (dir.y < 0) {
                    if (pair.first) {
                        makeNotePitchVisible(pair.first->pitch);
                    }
                } else if (dir.y > 0) {
                    if (pair.second) {
                        makeNotePitchVisible(pair.second->pitch);
                    }
                }
                edit = true;

            } else if (dir.x) {
                tick_t timeOffset = dir.x;
                tick_t minLen     = grid.pixelsToTicks(2);
                if (!isAlt(kevt.mods)) {
                    minLen = grid.getTickLength();
                }
                timeOffset *= minLen;
                cursor.start += timeOffset;
                cursor.end += timeOffset;
                if (!notes.selection.empty()) {
                    if ((kevt.mods & KB_MOD_SHIFT)) {
                        offsetEndTime(view.draggedSelection, timeOffset, minLen);
                    } else {
                        offsetStartTime(view.draggedSelection, timeOffset);
                    }
                    mergeDraggedNotes(dragmode::drag_notes_move);
                    notes.updateBounds();
                    setSelectionFrame(getMinMaxTime(notes.selection));
                    auto pair = getMinMaxTime(notes.selection);
                    if (dir.x < 0) {
                        if (pair.first) {
                            grid.makeTickVisible(pair.first->start() + getTickOffset());
                        }
                    } else if (dir.x > 0) {
                        if (pair.second) {
                            grid.makeTickVisible(pair.second->end() + getTickOffset());
                        }
                    }
                    edit = true;
                } else {
                    grid.makeTickVisible(cursor.start);
                }
            }
            handled = true;
            desc    = "Move notes";
        }
        if (edit) {
            notes.updateBounds();
            dawCtrl->getDaw()->pushHist(new action_modify_notes(desc, view, notesBefore, cursorBefore));
            clip->setDirty();
            view.updateNotePitches(false);
        }
        return handled;
    }
    return false;
}
bool gui_clipcontent_control_data::handleKeyInput(KeyEvent& kevt) {
    clip_t* clip = view.clip();
    if (!clip) {
        return false;
    }
    if (dragMode) {
        return true;
    }
    if (kevt.cmd) {
        auto temp = kevt.cmd->getKeybindContextData(kevt);
        if (handleEditorCommand(temp)) {
            return true;
        }
    }
    if (isArrowKey(kevt.keyCode)) {
        ivec2 dir;
        arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
        DAW::UI::CommandContext ctxt = {GlobalCommandType::CMD_MOVE_CURSOR, kevt, dir.x, dir.y};
        if (handleEditorCommand(ctxt)) {
            return true;
        }
    }
    return false;
}
std::pair<tick_t, tick_t> getMinMaxTimeShape(std::vector<DAW::Shape::shape_pt_t>& shapePt);
bool gui_clipcontent_control_data::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    auto command = ctxt.type;
    auto& kevt = ctxt.kevt;
    clip_t* clip = view.clip();
    if (!clip) {
        return false;
    }
    auto& ctrlData = clip->controlData;
    auto& selection = shapeEdit.getSelectedNodeIndices();
    if (kevt.type != K_RELEASE) {
        clip_cursor_t& cursor      = view.m_cursor;
        clip_t clipBefore          = *clip;
        clip_cursor_t cursorBefore = cursor;// copy
        clip_control_data_t ctrlDataBefore = ctrlData;
        bool handled               = false;
        bool edit                  = false;
        String desc                = "???";
        if (kevt.type == K_PRESS) {
            if (command == CMD_SELECT_ALL) {
                shapeEdit.resetSelection();
                shapeEdit.selectAll();
                if (!tmpShape.pts.empty()) {
                    auto [tmMin, tmMax] = getMinMaxTimeShape(tmpShape.pts);
                    auto& cursor = view.m_cursor;
                    cursor.start = tmMin;
                    cursor.end = tmMax;
                }
                setGlobalSelectionFromClipSelection();
                handled = true;
            }
            if (command == CMD_DELETE && !selection.empty()) {
                shapeEdit.deleteSelectedPoints();
                handled = true;
                edit    = true;
                desc    = "Delete control points";
            }
        }
        if (command == GlobalCommandType::CMD_MOVE_CURSOR) {
            auto dir = ivec2(ctxt.argInt0, ctxt.argInt1);
            if (dir.y && !selection.empty()) {
                if ((kevt.mods & KB_MOD_SHIFT)) {
                    dir *= 12;
                }
                // edit = true;
            } else if (dir.x) {
                tick_t timeOffset = dir.x;
                tick_t minLen     = grid.pixelsToTicks(2);
                if (!isAlt(kevt.mods)) {
                    minLen = grid.getTickLength();
                }
                timeOffset *= minLen;
                cursor.start += timeOffset;
                cursor.end += timeOffset;
                if (!selection.empty()) {
                    edit = true;
                } else {
                    grid.makeTickVisible(cursor.start + getTickOffset());
                }
            }
            handled = true;
            desc    = "Move control points";
        }
        if (edit) {
            clip->controlData.updateBounds();
            clip->setDirty();
            dawCtrl->getDaw()->pushHist(new action_modify_clip_control_data(desc, view, ctrlDataBefore, cursorBefore));
            showEditClip();
        }
        return handled;
    }
    return false;
}
bool gui_clipcontent::handleKeyInput(KeyEvent& kevt) {
    clip_t* clip = view.clip();
    if (!clip) {
        return false;
    }
    if (kevt.type != K_REPEAT && isCtrlKey(kevt.keyCode)) {
        if ((dragMode == drag_notes_move || dragMode == drag_notes_copy)) {
            if ((dragMode == drag_notes_copy) != (kevt.type == K_PRESS)) {
                if (dragMode == drag_notes_move) {
                    dragMode               = drag_notes_copy;
                    parentCtrl->cursorIcon = CURSOR_DUPLICATE;
                } else {
                    dragMode               = drag_notes_move;
                    parentCtrl->cursorIcon = CURSOR_DEFAULT;
                }
                ThreadLock lock = dawCtrl->lockPlayThread();
                mergeDraggedNotes(dragMode);
            }
            return true;
        }
    }
    if (dragMode) {
        return true;
    }
    if (kevt.cmd) {
        auto temp = kevt.cmd->getKeybindContextData(kevt);
        if (handleEditorCommand(temp)) {
            return true;
        }
    }
    if (isArrowKey(kevt.keyCode)) {
        ivec2 dir;
        arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
        DAW::UI::CommandContext ctxt = {GlobalCommandType::CMD_MOVE_CURSOR, kevt, dir.x, dir.y};
        if (handleEditorCommand(ctxt)) {
            return true;
        }
    }
    return false;
}

guictr_clipeditorview::guictr_clipeditorview()
    : guictr_base(),
      cache(new midi_clip_render_cache_t{})
{
}
guictr_clipeditorview::~guictr_clipeditorview() {
    delete cache;
}

void guictr_clipeditorview::resetCache() {
    cache->reset();
}

void guictr_clipeditorview::prerender(NVGcontext* vg) {
    auto clipEditor = getClipEditor();
    if (!clipEditor) {
        return;
    }
    clip_view_t& view  = clipEditor->getClipView();
    clip_t* const cl = view.clip();
    if (!cl) {
        cache->reset();
        return;
    }
    noteview_render_t& notesView = cl->getNoteViewFullClip();

    ivec2 sizeContents  = this->getSizeContent();
    ivec2 clipPosScreen = toScreenSpace(ivec2(0, 0));

    tick_t clipLen = 0;
    if (notesView.firstNote != notesView.lastNote) {
        clipLen = notesView.lastNote.end();
    }
    float numBars  = clipLen / (float) TICKS_BAR;
    float barSize  = sizeContents.x / (float) numBars;

    bool cacheValid = cache->revision == notesView.reqRevision;
    cacheValid &= cache->valid;
    cacheValid &= cache->pos == clipPosScreen;
    cacheValid &= cache->size == sizeContents;
    if (!cacheValid) {

	    nvgReset(vg);
        nvgScale(vg, parentCtrl->m_scale, parentCtrl->m_scale);
        nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);
        nvgCachePath(vg, 1);
        int64_t notesRendered = 0;

        cache->reset();

        NVGcolor rgbNote        = theme->getColor(GuiColor::COL_CLIP_NOTE);
        NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
        NVGcolor rgbNoteMuted   = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);

        int noteRenderMode = theme->get(GuiConstant::CONST_NOTE_RENDER_MODE);
        nvgSave(vg);
        nvgTranslate(vg, clipPosScreen.x, clipPosScreen.y);
        nvgSave(vg);

        if (sizeContents.x > 0 && sizeContents.y > 0) {
            clip_notes_t& notes = notesView;
            if (!notes.isEmpty()) {
                note_t minN      = notesView.minNote;
                note_t maxN      = notesView.maxNote;
                int32_t numNotes = maxN.pitch - minN.pitch + 1;
                float scale      = sizeContents.y / (float) math::max<int32_t>(8, numNotes);
                std::vector<const note_t*> notesClipped;
                std::vector<const note_t*> notesMuted;
                int begin = 0;
                for (const note_t& note : notes.m_list) {
                    tick_t noteTime = note.time;
                    if (noteTime >= clipLen) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (noteTime < 0) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (!note.isEnabled()) {
                        notesMuted.push_back(&note);
                        continue;
                    }
                    if (!begin) {
                        if (noteRenderMode == 0) {
                            nvgBeginPath(vg);
                        }
                        begin++;
                    }
                    float objPosNote = noteTime / (float) TICKS_BAR;
                    float objLenNote = note.len / (float) TICKS_BAR;
                    auto relNotePitch = note.pitch - minN.pitch;
                    if (numNotes < 8) {
                        // offset relNotePitch to center notes
                        relNotePitch += (8 - numNotes) / 2;
                    }
                    float ny     = noteToScreen(relNotePitch, scale, -scale, sizeContents.y);
                    float nx     = math::max(0.0f, objPosNote * barSize);
                    float nw     = math::min(objLenNote * barSize, sizeContents.x - nx);
                    float nh     = math::max(2.0f, scale);
                    float insetStr = math::clamp(math::clamp(nw-1.0f, 0.0f, 4.0f)/4.0f, 0.0f, 1.0f);
                    float nwInset  = math::max(nw - 1.0f*insetStr, 1.0f);
                    float nxInset  = nx + 0.5f*insetStr;
                    if (noteRenderMode == 0) {
                        nvgRect(vg, nxInset, ny, nwInset, nh);
                    } else {
                        nvgBatchedRect(vg, nxInset, ny, nwInset, nh);
                    }

                    notesRendered++;
                }
                if (begin) {
                    if (noteRenderMode == 0) {
                        nvgFillColor(vg, rgbNote);
                        nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                        nvgFill(vg);
                    } else {
                        NVGpaint paint;
                        memset(&paint, 0, sizeof(paint));
                        paint.image      = -1;
                        paint.innerColor = rgbNote;
                        paint.outerColor = rgbNote;
                        paint.customPar  = 1234;
                        nvgFillPaint(vg, paint);
                        nvgBatchedRender(vg);
                    }
                    cache->SaveFill(vg, 0);
                }

                for (int j = 0; j < 2; j++) {
                    auto& list = j == 0 ? notesClipped : notesMuted;
                    if (!list.empty()) {
                        if (noteRenderMode == 0) {
                            nvgBeginPath(vg);
                        }
                        for (const note_t* noteClipped : list) {
                            const note_t& note = *noteClipped;
                            tick_t noteTime    = note.time;

                            float objPosNote = noteTime / (float) TICKS_BAR;
                            float objLenNote = note.len / (float) TICKS_BAR;
                            auto relNotePitch = note.pitch - minN.pitch;
                            if (numNotes < 8) {
                                // offset relNotePitch to center notes
                                relNotePitch += (8 - numNotes) / 2;
                            }
                            float ny     = noteToScreen(relNotePitch, scale, -scale, sizeContents.y);
                            float nx     = math::max(0.0f, objPosNote * barSize);
                            float nw     = math::min(objLenNote * barSize, sizeContents.x - nx);
                            float nh     = math::max(2.0f, scale);
                            float insetStr = math::clamp(math::clamp(nw-1.0f, 0.0f, 4.0f)/4.0f, 0.0f, 1.0f);
                            float nwInset  = math::max(nw - 1.0f*insetStr, 1.0f);
                            float nxInset  = nx + 0.5f*insetStr;
                            if (noteRenderMode == 0) {
                                nvgRect(vg, nxInset, ny, nwInset, nh);
                            } else {
                                nvgBatchedRect(vg, nxInset, ny, nwInset, nh);
                            }
                            notesRendered++;
                        }

                        if (noteRenderMode == 0) {
                            nvgFillColor(vg, j == 0 ? rgbNoteOverlap : rgbNoteMuted);
                            nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                            nvgFill(vg);
                        } else {
                            NVGpaint paint;
                            memset(&paint, 0, sizeof(paint));
                            paint.image      = -1;
                            paint.innerColor = j == 0 ? rgbNoteOverlap : rgbNoteMuted;
                            paint.outerColor = j == 0 ? rgbNoteOverlap : rgbNoteMuted;
                            paint.customPar  = 1234;
                            nvgFillPaint(vg, paint);
                            nvgBatchedRender(vg);
                        }

                        if (j == 0) {
                            cache->SaveFill(vg, 1);
                        } else {
                            cache->SaveFill(vg, 2);
                        }
                    }
                }
            }
        }
        nvgRestore(vg);
        nvgCachePath(vg, 0);

        cache->valid          = true;
        cache->pos            = clipPosScreen;
        cache->size           = sizeContents;
        cache->notesRendered  = notesRendered;
        cache->revision       = notesView.reqRevision;
    }
    
    for (guibase* gui : guis) {
        gui->prerender(vg);
    }
}

float guictr_clipeditorview::getScaleX() {
    float scaleX = 1.0f;
    auto& view = getClipView();
    auto& grid = getGrid();
    auto* clip = view.clip();
    auto contentLenTicks = TICKS_BAR*4;
    if (clip) {
        contentLenTicks = clip->getLen();
    }
    if (view.isAbsoluteTimeMode()) {
        contentLenTicks = view.m_selectionView.viewEnd;
    }
    auto clipEditor = getClipEditor();
    if (!clipEditor) {
        return 1.0f;
    }
    auto csEditor = clipEditor->noteeditor.sizeContentArea;
    auto barBeginEditor = grid.toObjSpace(0.0);
    auto barEndEditor = grid.toObjSpace(csEditor.x);
    auto barLenClip = contentLenTicks / static_cast<double>(TICKS_BAR);
    auto barLenEditor = barEndEditor - barBeginEditor;
    scaleX  = math::max(static_cast<float>(barLenEditor/barLenClip), 0.0f);
    return scaleX;
}

float guictr_clipeditorview::getScreenSpaceScaleX() {
    auto cs = getSizeContent();
    auto clipEditor = getClipEditor();
    if (!clipEditor) {
        return 1.0f;
    }
    auto csEditor = clipEditor->noteeditor.sizeContentArea;
    if (cs.x <= 0)
        return 1.0f;
    return csEditor.x / static_cast<float>(cs.x);
}

void guictr_clipeditorview::getFrameBounds(vec2& posFrame, vec2& sizeFrame) {
    auto& grid = getGrid();
    float scaleX = getScaleX();
    float scaleXSS = getScreenSpaceScaleX();
    ivec2 posContents = this->getPosContent();
    ivec2 sizeContents = this->getSizeContent();
    float offset = static_cast<float>(grid.getOffset()) * scaleX/scaleXSS;
    auto rightBottom = vec2(posContents + sizeContents);
    posFrame = vec2(math::min<float>(posContents.x + offset, rightBottom.x), posContents.y);
    sizeFrame = vec2(math::min<float>(sizeContents.x * scaleX, rightBottom.x - posFrame.x), sizeContents.y);
}

void guictr_clipeditorview::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    ivec2 posContents = this->getPosContent();
    ivec2 sizeContents = this->getSizeContent();

    bool visible = dawCtrl->isClipEditorVisible();
    if (visible) {
        int topOffset = CTR_SPACING / 2 + 1;
        drawBackground(vg, theme, posContents + ivec2(0, -topOffset), sizeContents+ivec2(0, topOffset), margin, false);
    }
    drawInsetBackground(vg, theme, posContents, sizeContents);

    auto& view = getClipView();
    clip_t* const cl = view.clip();
    if (cl && cache->valid) {
        NVGcolor color = rgbToNvg(cl->rgb);
        if (!cl->enabled) {
            color = rgbToNvg(0x333333);
        }
        dbgassert(cache->valid);
        if (cache->valid && std::any_of(cache->arr.cbegin(), cache->arr.cend(), [](const auto* ptr) { return !!ptr; })) {

            int64_t notesRendered = 0;

            nvgSave(vg);
            nvgTranslate(vg, posContents.x, posContents.y);
            nvgSave(vg);
            if (cache->isCacheValid(1)) {
                nvgFillFromCache(vg, cache->arr[1]);
            }
            if (cache->isCacheValid(2)) {
                nvgFillFromCache(vg, cache->arr[2]);
            }
            if (cache->isCacheValid(0)) {
                nvgFillFromCache(vg, cache->arr[0]);
            }
            notesRendered += cache->notesRendered;
            nvgRestore(vg);
            if (cl->isLoopEnabled()) {
                if (cache->isCacheValid(3)) {
                    nvgFillFromCache(vg, cache->arr[3]);
                }
            }
            nvgRestore(vg);
            daw_tls::getTls().runtime->renderStats.notesRendered += notesRendered;
        }
        daw_tls::getTls().runtime->renderStats.clipsRendered++;
    }
    auto* clip = view.clip();
    if (clip) {
        vec2 posFrame, sizeFrame;
        getFrameBounds(posFrame, sizeFrame);
        if (sizeFrame.x > 0 && sizeFrame.y > 0) {
            nvgBeginPath(vg);
            nvgRect(vg, posFrame.x, posFrame.y, sizeFrame.x, sizeFrame.y);
            nvgStrokeWidth(vg, 3);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLUGIN_VIEW_FRAME));
            nvgStroke(vg);
        }
    }
}

void guictr_clipeditorview::handleDraggedBegin(MouseEvent& evt) {
    dragMode      = drag_none;
    if (isCtrl(evt.kbmods) || evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
        dawCtrl->toggleViewModeEditArea();
        return;
    }
    if (!dawCtrl->isClipEditorVisible()) {
        dawCtrl->showClipEditor();
        return;
    }
    vec2 posFrame, sizeFrame;
    getFrameBounds(posFrame, sizeFrame);
    // if click is outside frame then set offset to mousepos
    /* float scaleX   = getScaleX();
    float scaleXSS = getScreenSpaceScaleX();
    if (evt.mousepos.x < posFrame.x || evt.mousepos.x > posFrame.x + sizeFrame.x) {
        auto& grid = getGrid();
        auto newOffset = (evt.relMousepos.x - sizeFrame.x * 0.5f) * (scaleXSS / scaleX);
        grid.setOffset(math::roundfS32(math::max(0.0f, newOffset)));
        grid.notifyChange();
        return;
    } */
    parentCtrl->captureMouse(this);
    dragMode      = drag_view;
    dragDirection = -1;
}

void guictr_clipeditorview::handleDraggedMove(MouseEvent& evt) {
    if (dragMode == drag_none) {
        return;
    }

    if (evt.guiDragged == this) {
        auto& grid = getGrid();
        float scaleX   = getScaleX();
        float scaleXSS = getScreenSpaceScaleX();
        bool bChanged  = false;
        if (math::abs(evt.dragDistance->x) > 3) {
            auto newOffset      = grid.offset + evt.dragDistance->x * 1.0 / scaleX;
            evt.dragDistance->x = 0;
            grid.setOffset(math::rounddS32(math::max(0.0, newOffset)));
            grid.notifyChange();
            bChanged |= true;
        }

        if (math::abs(evt.dragDistance->y) > 3) {
            auto disty            = 1.0f + evt.dragDistance->y * -0.01f;
            evt.dragDistance->y   = 0;
            float anchor_dragposx = math::max(0.0f, evt.relMousepos.x * (scaleXSS / scaleX) - grid.getOffset());
            auto dragPosObjSpace  = grid.toObjSpace(anchor_dragposx);
            grid.setZoom(grid.zoom * disty);
            auto newOffset = grid.calcOffset(anchor_dragposx, dragPosObjSpace);
            grid.setOffset(math::rounddS32(newOffset));
            bChanged |= true;
        }
        if (bChanged) {
            grid.notifyChange();
        }
    }
}

void guictr_clipeditorview::handleDraggedRelease(MouseEvent& evt) {
    if (dragMode == drag_none) {
        return;
    }
    dawCtrl->getDaw()->updateVisibleTrackContents();
}

bool guictr_clipeditorview::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        evt.requestFocus(this);
        return true;
    }
    return false;
}

float guictr_cliphandles::clipStartScrX() const {
    return (float) (grid.tickToScreenD(getTickOffsetOffset()));
}

float guictr_cliphandles::clipEndScrX() const {
    auto clip = view.clip();
    if (!assert_expr(clip)) {
        return 0;
    }
    return (float) (grid.tickToScreenD(getTickOffsetOffset() + clip->getLen()));
}

float guictr_cliphandles::clipLoopStartScrX() const {
    auto clip = view.clip();
    if (!assert_expr(clip)) {
        return 0;
    }
    return (float) grid.tickToScreenD(getTickOffset() + clip->loopStart);
}

float guictr_cliphandles::clipLoopEndScrX() const {
    auto clip = view.clip();
    if (!assert_expr(clip)) {
        return 0;
    }
    return (float) grid.tickToScreenD(getTickOffset() + clip->loopStart + clip->loopLen);
}

namespace DAW::UI {
guictr_base* makeGuiClipEditor(create_ctr_t ctxt) {
    return new guictr_clipeditor();
}
}

void piano_scale::setOffset(float f) {
    auto minOffset            = -(layoutRoll.scale() * MAX_OCTAVES * 1);
    auto maxOffset            = layoutRoll.scale() * (MAX_OCTAVES - 1) * 12;
    this->layoutRoll.offset() = math::clamp(f, minOffset, maxOffset);
}

void piano_scale::setScale(float f) {
    this->layoutRoll.scale() = math::clamp<float>(f, PIANOROLL_MIN_SCALE, PIANOROLL_MAX_SCALE);
}

void CCEdit::setSelectRect(vec4 rect) {
    selectedNodeIndices.clear();
    auto& curve = *this->curve;
    int32_t len = int32_t(curve.pts.size());
    for (int32_t i = 0; i < len; ++i) {
        auto& pt = curve.pts[i].pos;
        if (rect.x <= pt.x && pt.x <= rect.z && rect.y <= pt.y && pt.y <= rect.w) {
            selectedNodeIndices.push_back(i);
        }
    }
}

tick_t guictr_cliphandles::getTickOffset() const {
    auto tickOffset = tick_t(0);
    auto selClipView = parentEditor.getClipView();
    auto selClip = selClipView.clip();
    auto thizClip = view.clip();
    if (selClipView.isAbsoluteTimeMode() && thizClip) {
        tickOffset = thizClip->time;
    } 
    if (!selClipView.isAbsoluteTimeMode() && selClip && thizClip && selClip != thizClip) {
        tickOffset = selClip->time - thizClip->time + thizClip->offsetStart;
    }
    return tickOffset;
}

tick_t guictr_cliphandles::getTickOffsetOffset() const {
    auto clip   = view.clip();
    auto offset = getTickOffset();
    if (!assert_expr(clip)) {
        return offset;
    }
    if (!parentEditor.getClipView().isAbsoluteTimeMode()) {
        offset += clip->offsetStart;
    }
    return offset;
}
bool guictr_editor_base::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        size_t numHandleDist = 0;

        std::array<guictr_cliphandles::dist_dragzone_handle, 4> dragZones;
        for (auto& clipHandle : clipsHandles) {
            dbgassert(clipHandle->pos.x == timeline.pos.x);
            if (clipHandle->isVisible() && clipHandle->parent && clipHandle->contains(localMouse)) {
                ivec2 localMouseHandle = clipHandle->toContainerSpace(localMouse);
                auto dragZone = clipHandle->getDragZone(localMouseHandle);
                if (dragZone.handle && dragZone.mode != guictr_cliphandles::dragmode::drag_handle_none) {
                    dragZones[numHandleDist++] = dragZone;
                    if (numHandleDist >= dragZones.size()) {
                        break;
                    }
                }
            }

        }
        if (numHandleDist > 0) {
            std::sort(dragZones.begin(), dragZones.begin() + numHandleDist, [](const auto& a, const auto& b) {
                return a.dist < b.dist;
            });
            auto closesDragZone = dragZones.front();
            bool bIsNotLoopDrag = closesDragZone.mode != guictr_cliphandles::dragmode::drag_handle_loopbar && 
                            closesDragZone.mode != guictr_cliphandles::dragmode::drag_handle_loopleft &&
                            closesDragZone.mode != guictr_cliphandles::dragmode::drag_handle_loopright;
            if (!bIsNotLoopDrag) {
                bIsNotLoopDrag = true;
            }
            if (!closesDragZone.handle->isHandleActive()) {
                dbgassert(closesDragZone.mode != guictr_cliphandles::dragmode::drag_handle_loopbar && 
                            closesDragZone.mode != guictr_cliphandles::dragmode::drag_handle_loopleft &&
                            closesDragZone.mode != guictr_cliphandles::dragmode::drag_handle_loopright);
            }

            switch (closesDragZone.mode) {
                case guictr_cliphandles::dragmode::drag_handle_loopleft:
                case guictr_cliphandles::dragmode::drag_handle_left:
                    evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
                    evt.requestFocus(closesDragZone.handle);
                    closesDragZone.handle->setDragMode(closesDragZone.mode);
                    return true;
                case guictr_cliphandles::dragmode::drag_handle_loopright:
                case guictr_cliphandles::dragmode::drag_handle_right:
                    evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
                    evt.requestFocus(closesDragZone.handle);
                    closesDragZone.handle->setDragMode(closesDragZone.mode);
                    return true;
                case guictr_cliphandles::dragmode::drag_handle_loopbar:
                    evt.requestCursor(CURSOR_RESIZE_H);
                    evt.requestFocus(closesDragZone.handle);
                    closesDragZone.handle->setDragMode(closesDragZone.mode);
                    return true;
                default:
                    break;
            }
        }
        // iterate over guis vector in reverse
        for (auto it = guis.rbegin(); it != guis.rend(); ++it) {
            auto gui = *it;
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_SCROLL) {
            evt.requestFocus(this);
            return true;
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
            evt.requestFocus(this);
            return true;
        }
        if (canMouseHit() && evt.type == MouseHitType::MOUSE_LEFT) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}
bool guictr_noteeditor::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    return guictr_editor_base::mouseHitTest(mpos, evt);
}

tick_t gui_clipcontent_base::getTickOffset() const {
    auto clip = view.clip();
    if (!(clip)) {
        return 0;
    }
    if (view.isAbsoluteTimeMode()) {
        return clip->time;
    }
    return 0;
}
float CCEdit::snapH(float x) {
    clip_t* offsetClip = nullptr;
    if (view.isAbsoluteTimeMode()) {
        offsetClip = view.clip();
    }
    if (offsetClip)
        x += offsetClip->start();
    x = grid.tickSnapExact(math::roundfS32(x), SNAP_ON);
    if (offsetClip)
        x -= offsetClip->start();
    return x;
}

bool gui_clipsettings::isVisible() const {
    auto clip = view.clip();
    if (!clip) return false;
    return guictr_base::isVisible() && daw_tls::getDawSettings().uiShowSettingsClip;
}

void gui_clipsettings::determineSize(ivec2& prefSize) {
    prefSize = ivec2(0, 0);
    guictr_base::determineSize(prefSize);
}
