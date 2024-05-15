

#include "gui/container/container_layout_types.h"
#include "gui/controls/button.h"
#include "gui/linetess/pymachine.h"
#include "appsettings.h"
#include "assert_dbg.h"
#include "color_util.h"
#include "gui/container/container_builder.h"
#include "host/audiobuffer/audioblock.h"
#include "host/clip/clip.h"
#include "host/daw/clipboard.h"
#include "event.h"
#include "gui/clipeditor/clipeditor.h"
#include "guiglobals.h"
#include "host/daw/mainctrl.h"
#include "host/project/project.h"
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
#include "host/audiosample.h"
#include "host/host_pluginmanager.h"
#include "appconfig.h"
#include "seq_util.h"
#include "str_util.h"
#include "types.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <nanovg.h>

#include <nanovg_min.h>
#include <utility>
#include <vector>

constexpr int32_t VEL_SELECT_DISTANCE = 16;
constexpr int32_t PIANOROLL_MIN_SCALE = 1;
constexpr int32_t PIANOROLL_MAX_SCALE = 48;
constexpr int32_t NOTES_LABEL_MIN_HEIGHT = 10;

std::pair<note_t*, note_t*> getMinMaxTime(clip_view_t& view) {
    std::pair<note_t*, note_t*> pairPtr{nullptr, nullptr};
    for (auto& entry : view.m_notesDragged) {
        auto& notes = entry.second.draggedSelection;
        if (notes.empty()) {
            continue;
        }
        auto min = std::min_element(notes.begin(), notes.end(),
                                    [](note_t const& lhs, note_t const& rhs) { return lhs.time < rhs.time; });
        auto max = std::max_element(notes.begin(), notes.end(),
                                    [](note_t const& lhs, note_t const& rhs) { return (lhs.time + lhs.len) < (rhs.time + rhs.len); });

        if (pairPtr.first == nullptr) {
            pairPtr.first = &*min;
        } else if (min != notes.end() && min->time < pairPtr.first->time) {
            pairPtr.first = &*min;
        }
        if (pairPtr.second == nullptr) {
            pairPtr.second = &*max;
        } else if (max != notes.end() && (max->time + max->len) > (pairPtr.second->time + pairPtr.second->len)) {
            pairPtr.second = &*max;
        }
    }

    return pairPtr;
}
std::pair<note_t*, note_t*> getMinMaxSemitones(clip_view_t& view) {
    std::pair<note_t*, note_t*> pairPtr{nullptr, nullptr};
    for (auto& entry : view.m_notesDragged) {
        auto& notes = entry.second.draggedSelection;
        if (notes.empty()) {
            continue;
        }
        auto minmax = std::minmax_element(notes.begin(), notes.end(),
                                        [](note_t const& lhs, note_t const& rhs) { return lhs.pitch < rhs.pitch; });
        if (pairPtr.first == nullptr) {
            pairPtr.first = &*minmax.first;
        } else if (minmax.first != notes.end() && minmax.first->pitch < pairPtr.first->pitch) {
            pairPtr.first = &*minmax.first;
        }
        if (pairPtr.second == nullptr) {
            pairPtr.second = &*minmax.second;
        } else if (minmax.second != notes.end() && minmax.second->pitch > pairPtr.second->pitch) {
            pairPtr.second = &*minmax.second;
        }
    }
    return pairPtr;
}

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
            addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_NOTE_ARP_RESET));
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
    bool bChanged = clip != view.clip() || view.isAbsoluteTimeMode();
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
    bool bWillBeAbsTime = clipboardView.totalClipCount > 1 || clip == nullptr;
    bool bChanged = clip != view.clip() || (bWillBeAbsTime != view.isAbsoluteTimeMode());
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

void guictr_clipeditor::refreshAudioWaveform() {
    audioeditor.content.releaseRendered();
    audioeditor.content.updatePosition();
}

void guictr_clipeditor::buttonClicked(guibase* button) {
    if (&settingsCtr.clipAudioPitch == button || &settingsCtr.clipAudioStretch == button) {
        clip_t* clip = view.clip();
        if (clip && clip->clipType == CLIP_AUDIO) {
            dawCtrl->getDaw()->updateDerivedAudio(clip, settingsCtr.clipAudioSettings);
        }

    } else if (parent) {
        parent->buttonClicked(button);
    }
}

void guictr_clipeditor::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    auto posContent = vec2(getPosContent());
    auto sizeContent = vec2(getSizeContent());
    nvgTranslate(vg, posContent.x, posContent.y);
    auto clip = view.clip();
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
    if (!clip && !(view.isAbsoluteTimeMode() && view.m_selectionView.totalClipCount > 0)) {
        renderText(vg, posContent + sizeContent * 0.5f, sizeContent * 0.5f, "No clip selected", 18, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
}

void guictr_clipeditor::layout() {
    const int32_t padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
    guibase* leftContainer = nullptr;
    auto& dawSettings = daw_tls::getDawSettings();
    arp.setVisible(dawSettings.uiShowSettingsArp);
    settingsScrollCtr.setVisible(dawSettings.uiShowSettingsClip);

    ivec2 cs      = getSizeContent();
    settingsCtr.pos = settingsScrollCtr.pos  = ivec2(0, 0);
    settingsCtr.size = settingsScrollCtr.size = ivec2(240, cs.y);
    settingsCtr.layout();
    ivec2 sizeSettings = settingsScrollCtr.size;
    if (settingsScrollCtr.isVisible()) {
        settingsScrollCtr.determineSize(sizeSettings);
        settingsScrollCtr.layout();
    }

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
void renderNoteName(NVGcontext* vg, const gui_clipcontent* c, const note_t* note, float nx, float ny, float nw, float nh, tick_t absPos, bool bRenderPosLen, GuiColor::constant_t col) {
    const float insetx = calcInset(5, nw);
    const auto color = c->theme->getColor(col);
    auto posText = vec2(nx + insetx, ny - nh + nh / 2.0f);
    auto sizeText = vec2(nw - insetx + 2, nh);
    auto strNoteName = String(noteName(note->pitch));
    if (note->flags & NoteFlags::ARP_RESET) {
        strNoteName += " R";
    }
    float w = renderTextLabel(vg,
        posText,
        sizeText,
        strNoteName,
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
bool gui_clipcontent_velocities::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (math::abs(yoffset) > 0.0) {
        float scale = isCtrl(evt.kbmods) ? 1.0f : 4.0f;
        auto velOffset = scale * yoffset;

        //TODO: history entry
        ThreadLock lock = dawCtrl->lockPlayThread();
        view.visitClipView([&](clip_t* cl) {
            auto& dragged = view.m_notesDragged[cl];
            dragged.draggedSelection = dragged.draggedSelectionBegin;
            auto it          = dragged.draggedSelection.begin();
            const auto itEnd = dragged.draggedSelection.end();
            while (it != itEnd) {
                note_t& note  = *it;
                note.velocity = math::min(127, math::max(0, math::rounddS32(note.velocity + velOffset)));
                it++;
            }
            mergeDraggedNotes(dragMode, cl);
            view.copySelectedNoteList();
            return true;
        });
    }
    return true;
}
void gui_clipcontent_velocities::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    renderBackground(vg);
    renderGridLines(vg, theme, grid.gridList, size);
    float w = size.x;
    const float h = size.y;
    NVGpaint paint{};
    paint.image     = -1;
    paint.customPar = NVGBatchedShading::NVG_BATCHED_SHADED;
    nvgShapeAntiAlias(vg, 0);
    const float extendCullCheck = 8.0f;
    auto currentClip = view.clip();
    view.visitClipView([&](clip_t* clip) {
        auto& noteView = clip->getNoteViewRender();
        auto& noteViewSel = clip->getNoteViewSelection();
        auto pList = view.isAbsoluteTimeMode() ? &noteView.m_list : &clip->notes.m_list;
        auto pListSelection = view.isAbsoluteTimeMode() ? &noteViewSel.m_list : &view.m_notesDragged[clip].draggedSelection;
        auto tickOffset = tick_t(0);
        if (view.isAbsoluteTimeMode()) {
            tickOffset = clip->time;
        }
        if (currentClip == clip && view.isAbsoluteTimeMode()) {
            tickOffset = currentClip->time;
            notesViewTemp.clear();
            currentClip->getNotesView(0, currentClip->getLen(), notesViewTemp, {.bCutNotes = true, .bCutMutedNotes = false, .bApplyGroove = false});
            pList = &notesViewTemp.m_list;
        }
        if (!pListSelection->empty() || !pList->empty()) {
            const int32_t nw = 4;
            const float r    = 4;
            const GuiColor::constant_t colors[3] = {GuiColor::COL_NOTE, GuiColor::COL_NOTE_MUTE, GuiColor::COL_NOTE_SELECTED};
            auto colNote = rgbToNvg(clip->rgb);
            for (int i = 0; i < 3; i++) {
                paint.innerColor = i == 0 ? colNote : theme->getColor(colors[i]);
                if (i < 2){
                    paint.innerColor.a *= 0.8f;
                }
                auto noteList = i < 2 ? pList : pListSelection;
                int nRendered = 0;
                for (const note_t& note: *noteList) {
                    if (!note.isEnabled() && i == 0) {
                        continue;
                    } else if (note.isEnabled() && i == 1) {
                        continue;
                    }
                    if (i >= 2) tickOffset = 0;
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
                    paint.renderType = 4;
                    nvgFillPaint(vg, paint);
                    nvgBatchedRender(vg);
                    nRendered = 0;
                }
                for (const note_t& note: *noteList) {
                    if (!note.isEnabled() && i == 0) {
                        continue;
                    } else if (note.isEnabled() && i == 1) {
                        continue;
                    }
                    auto nx = grid.tickToScreenD(note.time + tickOffset);
                    if (nx + r < -extendCullCheck) continue;
                    if (nx - r > w + extendCullCheck) continue;
                    auto nh = velocityToFloat(note.velocity) * h;
                    nvgBatchedRect(vg, float(nx - r), size.y - nh - r, r * 2, r * 2);
                    nRendered++;
                }
                if (nRendered) {
                    paint.renderType = 5;
                    nvgFillPaint(vg, paint);
                    nvgBatchedRender(vg);
                }
            }
        }
        return true;
    });
    nvgShapeAntiAlias(vg, USE_NANOVG_AA);
    /* if (dragMode <= drag_frame) {
        const int32_t nw = 6;
        const float r = 6;
        ivec2 imouse  = toControlsObjectSpace(dawCtrl->m_mousePos, this);
        bool mouseIn  = dawCtrl->getGuiOverRef() == toRef() && contains(imouse + getPosContent());
        if (mouseIn) {
            tick_t mouseTick = !mouseIn ? INVALID_TICK : grid.screenToTickSnap(imouse.x, SNAP_OFF);
            int32_t velClicked = screenToVel(imouse.y, size.y);
            int32_t velDist    = VEL_SELECT_DISTANCE * 127 / size.y;
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
    } */
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
void gui_clipcontent_notes::renderNoteLabels(NVGcontext* vg, const std::vector<note_t>& clipNotes, vec2 renderPos, vec2 renderSize, tick_t tickOffset, float scale, bool bRenderPosLen, bool bRenderMuted, GuiColor::constant_t col) {
    if (scale >= NOTES_LABEL_MIN_HEIGHT) {
        for (auto& note : clipNotes) {
            if (!bRenderMuted && !note.isEnabled())
                continue;
            auto nx = grid.tickToScreenD(note.time + tickOffset);
            auto nw = grid.tickLenToScreen(note.len);
            if (nx + nw < renderPos.x - 4)
                continue;
            if (nx > renderPos.x + renderSize.x + 4)
                continue;
            renderNoteName(vg, this, &note, nx, toScreenF(note.pitch), nw, scale, note.time + tickOffset, false, col);
        }
    }
}
void gui_clipcontent_notes::renderClipNoteRects(NVGcontext* vg, const std::vector<note_t>& clipNotes, vec2 renderPos, vec2 renderSize, tick_t tickOffset, float scale, float inset, NVGcolor color, NVGBatchedShading shading, bool renderMuted) {
    if (!clipNotes.empty()) {
        NVGpaint paint{};
        paint.customPar  = shading;
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
    clip_t* const currentClip = view.clip();
    auto x = float(grid.tickToScreenD(view.m_cursor.start));
    if (view.m_cursor.start != view.m_cursor.end) {
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

    view.visitClipView([&](clip_t* cl) {
        if (cl != currentClip) {
            auto& notesView = cl->getNoteViewRender();
            if (!notesView.isEmpty()) {
                renderClipNoteRects(vg, notesView.m_list, {}, cs, cl->time, scale, 1.0f, rgbToNvg(cl->rgb), NVGBatchedShading::NVG_BATCHED_SHADED_BORDER_DARK, false);
                renderNoteLabels(vg, notesView.m_list, {}, cs, cl->time, scale, false, false, GuiColor::COL_NOTE_TEXT);
            }
        }
        return true;
    });

    auto tickOffset = tick_t(0);
    if (currentClip) {
        auto pList = currentClip->notes.m_list;
        if (view.isAbsoluteTimeMode()) {
            tickOffset = currentClip->time;
            currentClip->getNotesView(0, currentClip->getLen(), notesViewTemp, {.bCutNotes = true, .bCutMutedNotes = false, .bApplyGroove = false});
            pList = notesViewTemp.m_list;
        }


        //TODO: culling or caching
        // Render muted notes
        renderClipNoteRects(vg, pList, {}, cs, tickOffset, scale, 1.0f, theme->getColor(GuiColor::COL_NOTE_MUTE), NVGBatchedShading::NVG_BATCHED_DIAGONAL_STRIPES, true);
        // Render enabled notes
        renderClipNoteRects(vg, pList, {}, cs, tickOffset, scale, 1.0f, rgbToNvg(currentClip->rgb), NVGBatchedShading::NVG_BATCHED_SHADED_BORDER_BRIGHT, false);
        renderNoteLabels(vg, pList, {}, cs, tickOffset, scale, false, true, GuiColor::COL_NOTE_TEXT);


        if (view.isAbsoluteTimeMode()) {
            tickOffset -= currentClip->offsetStart;
        }
    }

    auto track = view.track();
    if (currentClip && track && track->audio) {


        /* auto daw = dawCtrl->getDaw();
        ThreadLock lock = daw->lockPlayThread();
        std::vector<note_t> heldRealtimeNotes = daw->getHost()->getRealtimeNotes();//TODO: NOT THREADSAFE
        if (heldRealtimeNotes.size()) {
            int nRendered = 0;
            for (note_t& note: heldRealtimeNotes) {
                if (note.isRealtime()) {
                    continue;
                }
                tick_t pos = note.start() - currentClip->start() + currentClip->offsetStart;
                if (currentClip->isLoopEnabled()) {
                    if (pos > currentClip->loopStart) {
                        pos = currentClip->loopStart + (pos - currentClip->loopStart) % currentClip->loopLen;
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
                paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED;
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
                tick_t pos = note.start() - currentClip->start() + currentClip->offsetStart;
                if (currentClip->isLoopEnabled()) {
                    if (pos > currentClip->loopStart) {
                        pos = currentClip->loopStart + (pos - currentClip->loopStart) % currentClip->loopLen;
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
                paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }

        if (track->audio->arp->isProcessingEnabled()) {
            auto& heldNotesArp = track->audio->getArpHeldNotes();
            if (heldNotesArp.size()) {
                // nvgBeginPath(vg);

                for (auto& note: heldNotesArp) {
                    tick_t pos = note.start() - currentClip->start() + currentClip->offsetStart;
                    if (currentClip->isLoopEnabled()) {
                        if (pos > currentClip->loopStart) {
                            pos = currentClip->loopStart + (pos - currentClip->loopStart) % currentClip->loopLen;
                        }
                    }
                    //TODO: CULL
                    renderNote(vg, this, &note, scale, -note.start() + pos);
                }
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_ARP);
                paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED_BORDER_BRIGHT;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            
        }

        float yoff = 0;
        for (int i = 0; i < 2; i++) {
            std::vector<marker_t> markers = track->audio->getArpMarkers(i);//TODO: NOT THREADSAFE
            if (markers.size()) {
                for (marker_t& m: markers) {
                    tick_t pos = m.time - currentClip->start() + currentClip->offsetStart;
                    if (currentClip->isLoopEnabled()) {
                        if (pos > currentClip->loopStart) {
                            pos = currentClip->loopStart + (pos - currentClip->loopStart) % currentClip->loopLen;
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

    NVGpaint paint{};
    paint.image      = -1;
    paint.innerColor = theme->getColor(GuiColor::COL_NOTE_SELECTED);
    paint.customPar  = NVGBatchedShading::NVG_BATCHED_DIAGONAL_STRIPES_ALPHA;
    if (dragMode >= drag_notes_move) {
        view.visitClipView([&](clip_t* cl) {
            if (!view.m_notesDragged.count(cl))
                return true;
            int32_t n = 0;
            for (note_t& note: view.m_notesDragged[cl].draggedSelection) {
                renderNote(vg, this, &note, scale, tickOffset);
                n++;
            }
            if (n) {
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
                renderNoteLabels(vg, view.m_notesDragged[cl].draggedSelection, {}, cs, tickOffset, scale, true, true, GuiColor::COL_NOTE_TEXT);
            }
            return true;
        });
    } else {
        if (view.isAbsoluteTimeMode()) {
            view.visitClipView([&](clip_t* cl) {
                auto& notes = cl->notes;
                if (notes.selection.empty())
                    return true;
                int32_t n = 0;
                auto& view = cl->getNoteViewSelection();
                for (auto& note : view.m_list) {
                    renderNote(vg, this, &note, scale, 0);
                    n++;
                }
                if (n) {
                    nvgFillPaint(vg, paint);
                    nvgBatchedRender(vg);
                    renderNoteLabels(vg, view.m_list, {}, cs, 0, scale, true, true, GuiColor::COL_NOTE_TEXT);
                }
                return true;
            });
        } else if (currentClip) {
            const auto& notes = currentClip->notes;
            int32_t n = 0;
            for (note_t* pnote: notes.selection) {
                renderNote(vg, this, pnote, scale, tickOffset);
                n++;
            }
            if (n) {
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            if (scale >= NOTES_LABEL_MIN_HEIGHT) {
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
                    renderNoteName(vg, this, &note, nx, toScreenF(note.pitch), nw, scale, absPos, true, GuiColor::COL_NOTE_SELECTED);
                }
            }
        }
    }

    // if (view.m_cursor.start == view.m_cursor.end)
    {
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
    }


    if (dragMode == drag_frame) {
        renderFrame(vg, dragBegin, dragTo);
    }
}

void gui_clipcontent::handleDraggedBegin(MouseEvent& evt) {
    dragMode      = drag_none;
    const ivec2 local      = evt.relMousepos;
    const tick_t tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
    clip_t* contextClip    = view.clip();
    int32_t pitch          = math::floorfS32(toNoteF(local.y));
    int32_t velClicked     = screenToVel(local.y, size.y);
    note_t* contextNote    = nullptr;
    note_t* viewNote = nullptr;
    view.visitClipViewReverse([&](clip_t* cl) {
        notesViewTemp.clear();
        clip_notes_t* clNotes = &cl->notes;
        if (view.isAbsoluteTimeMode()) {
            cl->getInTimeRange(cl->start(), cl->end(), -1, -1, notesViewTemp.m_list, {
                .bCutNotes = true,
                .bCutMutedNotes = cl != contextClip,
                .bApplyGroove = false,
                .bRelative = !view.isAbsoluteTimeMode(),
            });
            clNotes = &notesViewTemp;
        }
        if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
            int32_t velDist = VEL_SELECT_DISTANCE * 127 / size.y;
            viewNote = getMinDistNoteVel(*clNotes, tickExact, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
        } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
            viewNote = clNotes->get(tickExact, pitch);
        }
        if (viewNote) {
            if (view.isAbsoluteTimeMode()) {
                dbgassert(viewNote->id >= 0 && size_t(viewNote->id) < cl->notes.m_list.size());
                auto note = &cl->notes.m_list[viewNote->id];
                if (note) {
                    contextNote = note;
                    contextClip = cl;
                    return false;
                }
            } else {
                contextNote = viewNote;
                contextClip = cl;
                return false;
            }
        }
        viewNote = nullptr;
        return true;
    });

    if (view.isAbsoluteTimeMode() && view.clip() != contextClip) {
        auto clipEditor = guiParentType<guictr_clipeditor, gui_type::CTR_TYPE_CLIPEDITOR>(this->parent);
        if (assert_expr(clipEditor)) {
            clipEditor->selectEditClip(contextClip);
        }
    }

    tick_t tickGridNearest = grid.screenToTickSnap(local.x, SNAP_ON);
    tick_t tickGridLeast   = grid.prev(tickExact);
    bool inSelection = false;
    if (contextClip && (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES || guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY)) {
        if (evt.type == M_EVT_DOUBLECLICK) {
            ThreadLock lock  = dawCtrl->lockPlayThread();
            clip_notes_t& notes = contextClip->notes;
            clip_cursor_t cursorBefore = view.m_cursor;
            view.visitClipView([&](clip_t* cl) {
                cl->notes.clearSelection();
                cl->updateNoteViewSelection();
                return true;
            });
            String desc = "???";
            view.m_notesDragged[contextClip].dragStartNotes = notes;
            if (contextNote) {
                if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
                    view.m_cursor.start = view.m_cursor.end = contextNote->start();
                    notes.remove(*contextNote);
                    contextNote = nullptr;
                    desc        = "Delete Note";
                } else {
                    contextNote->toggleFlag(NoteFlags::ENABLED);
                }
                dragMode = drag_note_clicked;
            } else {
                if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
                    note_t note;
                    note.pitch = pitch;
                    note.time = view.isAbsoluteTimeMode() ? contextClip->getLoopedTick(tickGridLeast - contextClip->time) : tickGridLeast;
                    note.len   = grid.getTickLength();
                    notes.paste(note);
                    contextNote = notes.get(note.time, pitch);
                    if (assert_expr(contextNote)) {
                        notes.selection.insert(contextNote);
                        dawCtrl->setStatusText(StringFormat("%d %d %d", note.pitch, note.time, note.len));
                        desc = "Add Note";
                        dragMode = drag_note_right;
                        dragStartCursor = view.m_cursor;
                    }
                }
            }
            dawCtrl->getDaw()->pushHist(new action_modify_notes(desc, view, cursorBefore));
            view.copySelectedNoteList();
            contextClip->setDirty();
            setSelectionFrameFromView();
            view.updateNotePitches(false);
            inSelection = true;
        } else {
            clip_notes_t& notes = contextClip->notes;
            if (contextNote) {
                beginDragNote = *contextNote;
                inSelection = stl_contains(notes.selection, contextNote);
            }
            if (!inSelection && !isShift(evt.kbmods) && !isCtrl(evt.kbmods)) {
                view.visitClipView([&](clip_t* cl) {
                    if (cl->notes.selection.empty())
                        return true;
                    cl->notes.clearSelection();
                    cl->updateNoteViewSelection();
                    return true;
                });
            }
            if (contextNote) {
                if (isShift(evt.kbmods)) {
                    if (!inSelection) {
                        notes.selection.insert(contextNote);
                        inSelection = true;
                        dragMode = drag_note_clicked;
                    } else {
                        notes.selection.erase(contextNote);
                        inSelection = true;
                        dragMode = drag_note_clicked;
                    }
                } else if (!inSelection) {
                    notes.selection.insert(contextNote);
                    inSelection = true;
                }
                contextClip->updateNoteViewSelection();
            }
            view.copySelectedNoteList();
            if (inSelection && dragMode == drag_none) {
                if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
                    dragMode = drag_velocity;
                } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES && viewNote) {
                    auto distL = local.x - grid.tickToScreenD(viewNote->start());
                    auto distR = grid.tickToScreenD(viewNote->end()) - local.x;
                    distL = math::abs(distL);
                    distR = math::abs(distR);
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
            }
            if (dragMode != drag_none) {
                dragStartCursor = view.m_cursor;
                setSelectionFrame(getMinMaxTime(view));
            }
        }
    }
    if (!inSelection) {
        if (contextClip && contextClip->notes.selection.empty())
            contextClip->notes.removeDuplicates();

        selectionsStart.clear();
        view.visitClipView([&](clip_t* cl) {
            selectionsStart[cl] = cl->notes.selection;
            return true;
        });
        if (isShift(evt.kbmods)) {
            if (math::abs(view.m_cursor.start - tickGridNearest) < math::abs(tickGridNearest - view.m_cursor.end)) {
                view.m_cursor.start = tickGridNearest;
            } else {
                view.m_cursor.end = tickGridNearest;
            }
        } else {
            view.visitClipView([&](clip_t* cl) {
                if (cl->notes.selection.empty())
                    return true;
                cl->notes.clearSelection();
                cl->updateNoteViewSelection();
                return true;
            });
            view.copySelectedNoteList();
            view.m_cursor.end = view.m_cursor.start = grid.screenToTickSnap(tickExact, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
            view.m_cursor.start = view.m_cursor.end = tickGridNearest;
            if (view.isAbsoluteTimeMode()) {
                // find clicked clip on same track
                if (!contextClip || view.m_cursor.start < contextClip->start() || view.m_cursor.start >= contextClip->end()) {
                    clip_t* clipClicked = nullptr;
                    auto& tracks = view.m_selectionView.tracks;
                    for (auto& [trGuiEntry, clips]: tracks) {
                        if (!contextClip || stl_contains(clips, contextClip)) {
                            for (auto& clip: clips) {
                                if (clip->start() <= view.m_cursor.start && clip->end() > view.m_cursor.start) {
                                    clipClicked = clip;
                                    break;
                                }
                            }
                        }
                    }
                    if (!clipClicked) {
                        view.visitClipViewReverse([&](clip_t* cl) {
                            if (cl->start() <= view.m_cursor.start && cl->end() > view.m_cursor.start) {
                                clipClicked = cl;
                                return false;
                            }
                            return true;
                        });
                    }
                    auto clipEditor = guiParentType<guictr_clipeditor, gui_type::CTR_TYPE_CLIPEDITOR>(this->parent);
                    if (assert_expr(clipEditor)) {
                        clipEditor->selectEditClip(clipClicked);
                        contextClip = clipClicked;
                    }
                }
            }
        }

        dragMode = drag_frame;

    } else {
        setSelectionFrameFromView();
    }
    if (dragMode != drag_none) {
        dragBegin = local;
        dragTo    = local;
        dragBeginPitch = math::floorfS32(toNoteFNoFolding(dragBegin.y));
        dragBeginTick = grid.screenToTick(dragBegin.x);
    }
    view.copySelectedNoteList();
    setStatusText();
    setGlobalSelectionFromClipSelection();
}

void gui_clipcontent::setGlobalSelectionFromClipSelection() {
    DAW::Cursor& cursor = dawCtrl->getCursor();
    if (!view.isAbsoluteTimeMode()) {
    clip_t* clip = view.clip();
        if (clip) {
            cursor.cursorPos = view.m_cursor.start + clip->start() - clip->offsetStart;
            cursor.selRange  = view.m_cursor.end - view.m_cursor.start;
        }
    } else {
        cursor.cursorPos = view.m_cursor.start;
        cursor.selRange  = view.m_cursor.end - view.m_cursor.start;
    }
}

void gui_clipcontent::setStatusText() {
    auto clip = view.clip();
    if (!clip) {
        return;
    }
    clip_notes_t& notes = clip->notes;
    String selStatus    = StringFormat("%zu notes selected", notes.selection.size());
    auto pair = getMinMaxSemitones(view);
    if (pair.first && pair.second) {
        selStatus += " - ";
        selStatus += StringFormat("pitch %d to %d", pair.first->pitch, pair.second->pitch);
        selStatus += " - ";
        auto pair2 = getMinMaxTime(view);
        selStatus += StringFormat("time %d to %d", pair2.first->start(), pair2.second->end());
    }
    dawCtrl->setStatusText(selStatus);
}

void gui_clipcontent::handleDraggedMove(MouseEvent& evt) {
    if (dragMode <= drag_note_clicked)
        return;
    const bool bIsShift = isShift(evt.kbmods);
    dragTo = evt.relMousepos;
    clip_t* const contextClip = view.clip();
    auto tickOffset1 = tick_t(0);
    if (view.isAbsoluteTimeMode() && contextClip) {
        tickOffset1 = contextClip->time;
        tickOffset1 -= contextClip->offsetStart;
    }
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
        if (bIsShift) {
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
            setSelectionFrame(getMinMaxTime(view));
            cursor.start = math::min(cursor.start, grid.screenToTickSnap(xStart, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON));
            cursor.end = math::max(cursor.end, grid.screenToTickSnap(xEnd, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON));
        }
        bool bChanged = false;
        if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_NOTES) {
            int32_t pitchLow  = math::floorfS32(toNoteF(yEnd));
            int32_t pitchHigh = math::floorfS32(toNoteF(yStart));
            if (view.isAbsoluteTimeMode()) {
                auto trackFilter = isAlt(evt.kbmods) ? view.track() : nullptr;
                view.visitClipViewTracks([&](const track_t* tr, const std::vector<clip_t*>& clips) {
                    if (trackFilter && trackFilter != tr) {
                        return true;
                    }
                    for (auto cl : clips) {
                        std::set<note_t*>& selection = cl->notes.selection;
                        if (bIsShift) {
                            cl->notes.selection = selectionsStart[cl];
                        } else {
                            cl->notes.selection.clear();
                        }
                        auto rangeBegin = math::max(tickStart, cl->start());
                        auto rangeEnd   = math::min(tickEnd, cl->end());
                        if (rangeBegin < rangeEnd) {
                            notesViewTemp.clear();
                            cl->getInTimeRange(rangeBegin, rangeEnd, -1, -1, notesViewTemp.m_list, {
                                .bCutNotes = true,
                                .bCutMutedNotes = cl != contextClip,
                                .bApplyGroove = false,
                            });
                            // create vector of all note.id in range
                            std::vector<int32_t> ids;
                            for (note_t& inSelRange : notesViewTemp.m_list) {
                                if (inSelRange.pitch >= pitchLow && inSelRange.pitch <= pitchHigh
                                    && !std::binary_search(ids.begin(), ids.end(), inSelRange.id)) {
                                    ids.insert(std::upper_bound(ids.begin(), ids.end(), inSelRange.id), inSelRange.id);
                                }
                            }
                            for (auto id : ids) {
                                auto* pNote = &cl->notes.m_list[id];
                                if (bIsShift) {
                                    if (selection.contains(pNote)) {
                                        selection.erase(pNote);
                                    } else {
                                        selection.insert(pNote);
                                    }
                                    bChanged = true;
                                } else {
                                    auto result = selection.insert(pNote);
                                    if (result.second) {
                                        bChanged = true;
                                    }
                                }
                            }
                        }
                        if (bChanged) {
                            cl->updateNoteViewSelection();
                        }
                    }
                    return true;
                });
            } else if (contextClip) {
                clip_notes_t& notes = contextClip->notes;
                if (bIsShift) {
                    notes.selection = selectionsStart[contextClip];
                } else {
                    notes.selection.clear();
                }
                std::vector<note_t*> inRangeList;
                if (notes.getInRange(tickStart, tickEnd, pitchLow, pitchHigh, inRangeList)) {
                    std::set<note_t*>& selection = notes.selection;
                    for (note_t* pNote : inRangeList) {
                        if (bIsShift) {
                            if (selection.contains(pNote)) {
                                selection.erase(pNote);
                            } else {
                                selection.insert(pNote);
                            }
                            bChanged = true;
                        } else {
                            auto result = selection.insert(pNote);
                            if (result.second) {
                                bChanged = true;
                            }
                        }
                    }
                }
                if (bChanged) {
                    contextClip->updateNoteViewSelection();
                }
            }
        } else if (guiType == gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY) {
            int32_t velLow  = screenToVel(yEnd, size.y);
            int32_t velHigh = screenToVel(yStart, size.y);
            if (view.isAbsoluteTimeMode()) {
                auto trackFilter = isAlt(evt.kbmods) ? view.track() : nullptr;
                view.visitClipViewTracks([&](const track_t* tr, const std::vector<clip_t*>& clips) {
                    if (trackFilter && trackFilter != tr) {
                        return true;
                    }
                    for (auto cl : clips) {
                        std::set<note_t*>& selection = cl->notes.selection;
                        if (bIsShift) {
                            cl->notes.selection = selectionsStart[cl];
                        } else {
                            cl->notes.selection.clear();
                        }
                        auto rangeBegin = tickStart;
                        auto rangeEnd   = tickEnd;
                        if (rangeBegin < rangeEnd && (tickEnd >= cl->start() && tickStart < cl->end())) {
                            notesViewTemp.clear();
                            cl->getInTimeRange(cl->start(), cl->end(), -1, -1, notesViewTemp.m_list, {
                                .bCutNotes = true,
                                .bCutMutedNotes = cl != contextClip,
                                .bApplyGroove = false,
                            });
                            // create vector of all note.id in range
                            std::vector<int32_t> ids;
                            for (note_t& inSelRange : notesViewTemp.m_list) {
                                if (inSelRange.start() >= rangeBegin && inSelRange.start() < rangeEnd
                                && inSelRange.velocity >= velLow && inSelRange.velocity <= velHigh
                                    && !std::binary_search(ids.begin(), ids.end(), inSelRange.id)) {
                                    ids.insert(std::upper_bound(ids.begin(), ids.end(), inSelRange.id), inSelRange.id);
                                }
                            }
                            for (auto id : ids) {
                                auto* pNote = &cl->notes.m_list[id];
                                if (bIsShift) {
                                    if (selection.contains(pNote)) {
                                        selection.erase(pNote);
                                    } else {
                                        selection.insert(pNote);
                                    }
                                    bChanged = true;
                                } else {
                                    auto result = selection.insert(pNote);
                                    if (result.second) {
                                        bChanged = true;
                                    }
                                }
                            }
                        }
                        if (bChanged) {
                            cl->updateNoteViewSelection();
                        }
                    }
                    return true;
                });
            } else if (contextClip) {
                clip_notes_t& notes = contextClip->notes;
                if (bIsShift) {
                    notes.selection = selectionsStart[contextClip];
                } else {
                    notes.selection.clear();
                }
                std::vector<note_t*> inRangeList;
                if (notes.getStartsInRangeV(tickStart, tickEnd, velLow, velHigh, grid.pixelsToTicks(VEL_SELECT_DISTANCE), inRangeList)) {
                    std::set<note_t*>& selection = notes.selection;
                    for (note_t* pNote : inRangeList) {
                        if (bIsShift) {
                            if (selection.contains(pNote)) {
                                selection.erase(pNote);
                            } else {
                                selection.insert(pNote);
                            }
                            bChanged = true;
                        } else {
                            auto result = selection.insert(pNote);
                            if (result.second) {
                                bChanged = true;
                            }
                        }
                    }
                }
                if (bChanged) {
                    contextClip->updateNoteViewSelection();
                }
            }
        }
        if (!bIsShift || bChanged) {
            setSelectionFrameFromView();
        }
        view.copySelectedNoteList();

        setStatusText();
    } else if (dragMode == drag_velocity) {
        *evt.dragDistance = ivec2(0);
        int32_t velOffset     = (dragBegin.y - dragTo.y) * 127 / size.y;
        view.visitClipView([&](clip_t* cl) {
            auto& dragged = view.m_notesDragged[cl];
            dragged.draggedSelection = dragged.draggedSelectionBegin;
            {
                auto it          = dragged.draggedSelection.begin();
                const auto itEnd = dragged.draggedSelection.end();
                while (it != itEnd) {
                    note_t& note  = *it;
                    note.velocity = math::min(127, math::max(0, note.velocity + velOffset));
                    it++;
                }
            }
            return true;
        });
        ThreadLock lock       = dawCtrl->lockPlayThread();
        mergeDraggedNotes(dragMode);
        setSelectionFrameFromView();

    } else if (dragMode >= drag_notes_move) {
        ThreadLock lock = dawCtrl->lockPlayThread();
        int modeMove    = SNAP_ON;
        if (isAlt(evt.kbmods)) {
            modeMove = SNAP_OFF;
        }

        tick_t gridSize       = grid.getTickLength();
        int32_t pitchStart    = dragBeginPitch;
        int32_t pitchEnd      = math::floorfS32(toNoteFNoFolding(dragTo.y));
        tick_t pitchOffset    = pitchEnd - pitchStart;
        tick_t tickStartExact = dragBeginTick;
        tick_t tickEndExact   = grid.screenToTick(dragTo.x);
        tick_t timeOffsetEx   = tickEndExact - tickStartExact;
        tick_t timeOffset = 0;
        const note_t noteDrag = this->beginDragNote;

        if (modeMove == SNAP_ON) {
            tick_t handlePos = dragMode == drag_note_right ? noteDrag.end() : noteDrag.start();
            handlePos += tickOffset1;
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

        view.visitClipView([&](clip_t* cl) {
            auto& dragged = view.m_notesDragged[cl];
            dragged.draggedSelection = dragged.draggedSelectionBegin;
            {
                auto& notes = cl->notes;
                auto it          = dragged.draggedSelection.begin();
                const auto itEnd = dragged.draggedSelection.end();
                while (it != itEnd) {
                    note_t& note  = *it;
                    if (dragMode == drag_note_left) {
                        auto before = getFirstBefore(notes.m_list, note.pitch, note.time);
                        note.time      = math::min(note.end() - 1, note.start() + timeOffset);
                        note.len       = math::max(math::max(1, gridSize/4), note.len - timeOffset);
                        if (before) {
                            if (note.start() < before->end()) {
                                note.cutLeft(before->end());
                            }
                        }
                    } else if (dragMode == drag_note_right) {
                        auto after = getFirstAfter(notes.m_list, note.pitch, note.time);
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
                {
                    std::vector<note_t> notesDraggedCopy = dragged.draggedSelection;
                    std::vector<note_t> notesDraggedNoDuplicates = dragged.draggedSelection;

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
                    dragged.draggedSelection = notesDraggedNoDuplicates;
                }
            }
            return true;
        });
        mergeDraggedNotes(dragMode);
        setSelectionFrameFromView();
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
            auto pitch = toNoteF(local.y);
            auto tickExact  = grid.screenToTickSnap(local.x, SNAP_OFF);
            auto tickRange = grid.pixelsToTicks(DRAG_RANGE+2);
            auto currentClip = view.clip();
            view.visitClipViewReverse([&](clip_t* cl) {
                const note_t* contextNote = nullptr;
                if (view.isAbsoluteTimeMode()) {
                    auto rangeBegin = math::max(tickExact-tickRange, cl->start());
                    auto rangeEnd   = math::min(tickExact+tickRange, cl->end());
                    if (rangeBegin >= rangeEnd) {
                        return true;
                    }
                    notesViewTemp.clear();
                    cl->getInTimeRange(rangeBegin, rangeEnd, -1, -1, notesViewTemp.m_list, {
                        .bCutNotes = true,
                        .bCutMutedNotes = cl != currentClip,
                        .bApplyGroove = false,
                    });
                    contextNote = notesViewTemp.get(tickExact, math::floorfS32(pitch));
                } else {
                    contextNote = cl->notes.get(tickExact, math::floorfS32(pitch));
                }
                if (contextNote) {
                    auto distL = local.x - grid.tickToScreenD(contextNote->start());
                    auto distR = grid.tickToScreenD(contextNote->end()) - local.x;
                    distL = math::abs(distL);
                    distR = math::abs(distR);
                    bool bHit = false;
                    if (distL < DRAG_RANGE) {
                        evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
                        bHit = true;
                    }
                    if (distR < DRAG_RANGE && (distL >= DRAG_RANGE || distR < distL)) {
                        evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
                        bHit = true;
                    }
                    if (bHit) {
                        return false;
                    }
                }
                return true;
            });
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}

bool gui_clipcontent::mergeDraggedNotes(dragmode mergeMode, clip_t* cl) {
    auto& dragged = view.m_notesDragged[cl];
    if (dragged.draggedSelection.empty())
        return false;
    auto& notes = cl->notes;
    notes  = dragged.dragStartNotes;
    notes.selection.clear();
    if (mergeMode != dragmode::drag_notes_copy) {
        dbgassert(!notes.hasDuplicates());
        notes.removeAllKeepDuplicates(dragged.draggedSelectionBegin);
    }
    for (note_t& note: dragged.draggedSelection) {
        notes.paste(note, true);
    }
    notes.selectLastN(dragged.draggedSelection.size());
    notes.updateBounds();
    cl->setDirty();
    return true;
}

int32_t gui_clipcontent::mergeDraggedNotes(dragmode mergeMode) {
    int32_t merged = 0;
    view.visitClipView([&](clip_t* cl) {
        if (mergeDraggedNotes(mergeMode, cl)) {
            merged++;
        }
        return true;
    });
    view.updateNotePitches(false);
    return merged;
}

void gui_clipcontent::setSelectionFrameFromView() {
    note_t minNote{ .pitch = -1 };
    note_t maxNote{ .pitch = -1 };
    if (view.isAbsoluteTimeMode()) {
        view.visitClipView([&](clip_t* cl) {
            if (cl->notes.selection.empty()) {
                return true;
            }
            auto& noteView = cl->getNoteViewSelection();
            if (!noteView.isEmpty()) {
                noteView.updateBounds();
                tick_t offset = !view.isAbsoluteTimeMode() ? cl->time : 0;
                if (minNote.pitch < 0 || noteView.firstNote.start() - offset < minNote.start()) {
                    minNote = noteView.firstNote;
                    maxNote.time -= offset;
                }
                if (maxNote.pitch < 0 || noteView.lastNote.end() - offset > maxNote.end()) {
                    maxNote = noteView.lastNote;
                    maxNote.time -= offset;
                }
            }
            return true;
        });
    }

    if (!view.isAbsoluteTimeMode()) {
        auto clip = view.clip();
        if (clip && !clip->notes.selection.empty()) {
            auto minMax = getMinMaxTime(view);
            if (minMax.first && minMax.second) {
                minNote = *minMax.first;
                maxNote = *minMax.second;
            }
        }
    }

    if (minNote.pitch >= 0 && maxNote.pitch >= 0) {
        auto& cursor = view.m_cursor;
        cursor.start = minNote.start();
        cursor.end   = maxNote.end();
    }
}

void gui_clipcontent::expandSelectionFrame(std::pair<note_t*, note_t*> minMax) {
    if (minMax.first && minMax.second) {
        auto& cursor = view.m_cursor;
        cursor.start = math::min(cursor.start, minMax.first->time);
        cursor.end   = math::max(cursor.end, (minMax.second->time + minMax.second->len));
    }
}
void gui_clipcontent::setSelectionFrame(std::pair<note_t*, note_t*> minMax) {
    if (minMax.first && minMax.second) {
        auto& cursor = view.m_cursor;
        cursor.start = minMax.first->time;
        cursor.end   = minMax.second->time + minMax.second->len;
    }
}
void gui_clipcontent::handleDraggedRelease(MouseEvent& evt) {
    if (dragMode == drag_frame) {
        handleDraggedMove(evt);
        dragMode = drag_none;
        return;
    }
    if (dragMode >= drag_notes_move) {
        ThreadLock lock = dawCtrl->lockPlayThread();
        mergeDraggedNotes(dragMode);
        view.visitClipView([&](clip_t* cl) {
            cl->updateNoteViewSelection();
            return true;
        });
        setSelectionFrameFromView();
        String action;
        if (dragMode == drag_velocity) {
            action = "Modify note velocities";
        } else if (dragMode >= drag_note_left) {
            action = "Modify note lengths";
        } else {
            action = "Move notes";
        }
        dawCtrl->getDaw()->pushHist(new action_modify_notes(action, view, dragStartCursor));
        view.copySelectedNoteList();
        view.updateNotePitches(false);
    }
    dragMode = drag_none;
    setSelectionFrameFromView();
    setGlobalSelectionFromClipSelection();
}

std::shared_ptr<notes_clipboard> ClipboardFromView(clip_view_t& view, const clip_cursor_t& cursor, bool bMerged = false) {
    std::shared_ptr<notes_clipboard> clipboard;
    view.visitClipViewReverse([&](clip_t* cl) {
        auto& notes = cl->notes;
        if (notes.selection.empty()) {
            return true;
        }
        if (!clipboard) {
            clipboard = std::make_shared<notes_clipboard>();
            clipboard->cursorRange = cursor.end - cursor.start;
        }
        tick_t offset = -(cursor.start );
        if (!view.isAbsoluteTimeMode()) {
            clipboard->notes.setTo(notes.selection, offset);
        } else {
            clip_notes_t noteViewSelection;
            cl->getInTimeRange(cl->start(), cl->end(), -1, -1, noteViewSelection.m_list, {
                .bCutNotes = false,
                .bCutMutedNotes = false,
                .bApplyGroove = false,
            });
            for (note_t* pnote: notes.selection) {
                auto idx = pnote - notes.m_list.data();
                for (auto& note : noteViewSelection.m_list) {
                    if (note.id == idx) {
                        auto noteOffset = note;
                        noteOffset.time += offset;
                        clipboard->notes.m_list.push_back(noteOffset);
                    }
                }
            }
        }
        return bMerged;
    });
    if (clipboard) {
        clipboard->notes.removeDuplicates();
        clipboard->notes.updateBounds();
    }
    return clipboard;
};
bool gui_clipcontent::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    auto daw = dawCtrl->getDaw();
    auto command = ctxt.type;
    auto& kevt = ctxt.kevt;
    if (focused() && command == CMD_PASTE && daw->getClipboardType() != ClipBoardType::CLIPBOARD_NOTES) {
        // suppress paste of clips by returning true for "is handled"
        return true;
    }
    clip_t* clip = view.clip();
    if (kevt.type != K_RELEASE) {
        clip_cursor_t& cursor          = view.m_cursor;
        clip_cursor_t cursorBefore     = cursor;// copy
        bool edit                      = false;
        String desc                    = "???";

        if (kevt.type == K_PRESS) {
            if (command == CMD_SELECT_ALL) {
                if (view.isAbsoluteTimeMode()) {
                    view.selectAll([](note_t* note) { return true; });
                } else if (clip) {
                    clip_notes_t& notes = clip->notes;
                    notes.clearSelection();
                    notes.updateBounds();
                    notes.selectIdxRange(0, notes.m_list.size());
                    clip->updateNoteViewSelection();
                }
                view.copySelectedNoteList();
                setSelectionFrameFromView();
                setGlobalSelectionFromClipSelection();
            }
            if (command == CMD_DELETE) {
                view.visitClipView([&](clip_t* cl) {
                    if (!cl->notes.selection.empty()) {
                        cl->notes.deleteSelectedNotes(cl->notes);
                        cl->updateNoteViewSelection();
                        cl->setDirty();
                        edit    = true;
                    }
                    return true;
                });
                if (edit) {
                    desc    = "Delete notes";
                }
            }
            if (command == CMD_MUTE) {
                bool bChanged = false;
                view.visitClipView([&](clip_t* cl) {
                    if (cl->notes.selection.empty()) {
                        return true;
                    }
                    bChanged = true;
                    muteNotesToggle(view.m_notesDragged[cl].draggedSelection);
                    mergeDraggedNotes(dragmode::drag_notes_move, cl);
                    cl->setDirty();
                    return true;
                });
                if (bChanged) {
                    edit    = true;
                    desc    = "Mute notes";
                }
            } else if (command == CMD_NOTE_ARP_RESET) {
                bool bChanged = false;
                view.visitClipView([&](clip_t* cl) {
                    if (cl->notes.selection.empty()) {
                        return true;
                    }
                    auto& dragged = view.m_notesDragged[cl].draggedSelection;
                    if (dragged.empty())
                        return true;
                    bChanged = true;
                    bool bIsEnabled = !(dragged[0].flags & NoteFlags::ARP_RESET);
                    for (note_t& note : view.m_notesDragged[cl].draggedSelection) {
                        note.flags &= ~NoteFlags::ARP_RESET;
                        if (bIsEnabled) {
                            note.flags |= NoteFlags::ARP_RESET;
                        }
                    }
                    mergeDraggedNotes(dragmode::drag_notes_move, cl);
                    cl->setDirty();
                    return true;
                });
                if (bChanged) {
                    edit    = true;
                    desc    = "Toggle ARP reset on notes";
                }
            } else if (command == CMD_CUT) {
                auto clipboard = ClipboardFromView(view, cursor, true);
                view.visitClipViewReverse([&](clip_t* cl) {
                    auto& notes = cl->notes;
                    if (notes.selection.empty()) {
                        return true;
                    }
                    notes.deleteSelectedNotes(notes);
                    cl->setDirty();
                    cl->updateNoteViewSelection();
                    edit    = true;
                    return true;
                });
                if (edit && clipboard) {
                    daw->setNotesClipboard(clipboard);
                    desc = "Cut notes";
                }
            } else if (command == CMD_COPY) {
                auto clipboard = ClipboardFromView(view, cursor, true);
                if (clipboard) {
                    daw->setNotesClipboard(clipboard);
                    desc    = "Copy notes";
                }
            } else if (command == CMD_DUPLICATE) {
                if (clip && !clip->notes.selection.empty()) {
                    auto& notes = clip->notes;
#ifndef NDEBUG
                    for (note_t* selPtr: notes.selection) {
                        dbgassert(notes.has(selPtr));
                    }
#endif
                    auto clipboard = ClipboardFromView(view, cursor);
                    if (clipboard) {
                        tick_t cursorRange = cursor.end - cursor.start;
                        cursor.start += cursorRange;
                        cursor.end += cursorRange;
                        notes.clearSelection();
                        view.copySelectedNoteList();
                        auto& dragged = view.m_notesDragged[clip].draggedSelection;
                        dragged.clear();
                        tick_t offset = view.isAbsoluteTimeMode() ? clip->time : 0;
                        auto cursorPos = view.isAbsoluteTimeMode() ? clip->getLoopedTick(cursor.start - offset) : cursor.start;
                        for (auto& note : clipboard->notes.m_list) {
                            dragged.push_back(note);
                            dragged.back().time += cursorPos;
                        }
                        mergeDraggedNotes(dragmode::drag_notes_move);
#ifndef NDEBUG
                        for (note_t* selPtr: notes.selection) {
                            dbgassert(notes.has(selPtr));
                        }
#endif
                        grid.makeTickVisible(cursor.end);
                        clip->setDirty();
                        edit    = true;
                        desc    = "Duplicate notes";
                    }
                }
            } else if (command == CMD_PASTE) {
                if (clip && daw->getClipboardType() == ClipBoardType::CLIPBOARD_NOTES && !daw->getNotesClipboard()->empty()) {
                    auto& notes = clip->notes;
                    auto& clipboard = daw->getNotesClipboard();
                    notes.clearSelection();
                    view.copySelectedNoteList();
                    auto& dragged = view.m_notesDragged[clip].draggedSelection;
                    dragged.clear();
                    tick_t offset = view.isAbsoluteTimeMode() ? clip->time : 0;
                    auto cursorPos = view.isAbsoluteTimeMode() ? clip->getLoopedTick(cursor.start - offset) : cursor.start;
                    for (auto& note: clipboard->notes.m_list) {
                        dragged.push_back(note);
                        dragged.back().time += cursorPos;
                    }
                    mergeDraggedNotes(dragmode::drag_notes_move);
                    cursor.end = cursor.start + clipboard->cursorRange;
                    grid.makeTickVisible(cursor.end);
                    clip->setDirty();
                    edit    = true;
                    desc    = "Paste notes";
                }
            } else if (command == CMD_APPLY_ARP) {
                if (clip) {
                    auto clipBefore = *clip;
                    auto track = view.track();
                    arp_snapshot snapshot;
                    tracksnapshot_store_opts_t opts;
                    opts.storeAutomation = false;
                    opts.storeClips = false;
                    opts.storeLayouts = false;
                    opts.storePluginPreset = true;
                    track->getStage()->arp->createSnapshot(snapshot, opts);
                    DAW::midiarp arpCopy(track->getStage());
                    arpCopy.loadSnapshot(snapshot);
                    auto begin = clip->start();
                    auto end = clip->end();
                    if (clip->isLoopEnabled()) {
                        end = clip->start() + clip->loopStart + clip->loopLen;
                    }
                    std::vector<note_t> notes;
                    clip->getInTimeRange(begin, end, -1, -1, notes, {
                        .bCutNotes = false,
                        .bCutMutedNotes = true,
                        .bApplyGroove = true,
                    });
                    auto host = dawCtrl->getDaw()->getHost();
                    auto state = playback_state::status_playback;
                    std::vector<midievent_note_t> noteEvents;
                    for (auto& note : notes) {
                        InsertMidiEventSorted(noteEvents, {note, note.start() - begin, note.start(), true, false});
                        InsertMidiEventSorted(noteEvents, {note, note.end() - begin, note.end(), false, false});
                    }
                    std::vector<midievent_note_t> noteEventsProcessed;
                    arpCopy.process(host, state, 0, noteEvents, begin, end + 1, -1, -1, noteEventsProcessed);
                    clip_notes_t tmpClipboard;
                    tmpClipboard.m_list.clear();
                    for (auto& note : noteEventsProcessed) {
                        if (note.isNoteOn) {
                            note_t n;
                            n.pitch = note.pitch;
                            n.velocity = note.velocity;
                            n.time = note.tickOffsetInBlock;
                            n.len = 0;
                            n.channel = note.channel;
                            tmpClipboard.m_list.push_back(n);
                        } else {
                            // find last note (reverse)
                            for (auto it = tmpClipboard.m_list.rbegin(); it != tmpClipboard.m_list.rend(); ++it) {
                                if (it->pitch == note.pitch && it->channel == note.channel) {
                                    it->len = note.tickOffsetInBlock - it->time;
                                    break;
                                }
                            }
                        }
                    }
                        // clip->selectedGroove = -1;
                    clip->notes = tmpClipboard;
                    clip->setDirty();
                    // edit = true;
                    desc = "Apply Arp";
                    dawCtrl->getDaw()->pushHist(new action_modify_clip(desc, view, clipBefore, cursorBefore));
                    clip->setDirty();
                    view.updateNotePitches(false);
                }
            } else if (command == CMD_APPLY_GROOVE) {
                auto applyGroove = [&](DawInstance* daw, clip_t* clip, clip_notes_t& notes) {
                    auto& grooves = daw->getGrooveLibrary().getGrooves();
                    auto groove = size_t(clip->selectedGroove);
                    if (groove >= 0 && groove < grooves.size()) {
                        auto minMax = getMinMaxTime(clip->notes.m_list);
                        if (minMax.first && minMax.second) {
                            tick_t start = minMax.first->start();
                            tick_t end = minMax.second->end();
                            if (end - start > 0) {
                                clip->getNotesView(start, end, notes, {
                                    .bCutNotes = false,
                                    .bCutMutedNotes = true,
                                    .bApplyGroove = true,
                                });
                                cutSelfIntersecting(notes.m_list);
                                notes.updateBounds();
                                return true;
                            }
                        }
                    }
                    return false;
                };
                auto daw = dawCtrl->getDaw();
                int32_t numEdited = 0;
                view.visitClipView([&](clip_t* clip) {
                    auto& notes = clip->notes;  
                    clip_notes_t tmpClipboard;
                    view.copySelectedNoteList();
                    notes.clearSelection();
                    auto clipBefore = *clip;
                    if (applyGroove(daw, clip, tmpClipboard)) {
                        clip->selectedGroove = -1;
                        clip->notes = tmpClipboard;
                        clip->setDirty();
                        desc = "Apply groove";
                        daw->pushHist(new action_modify_clip(desc, view, clipBefore, cursorBefore));
                        numEdited++;
                    }
                    return true;
                });
                if (numEdited > 0) {
                    // edit = true;
                    clip->setDirty();
                }
            } else if (command == CMD_QUANTIZE) {
                auto& settings = dawCtrl->getDaw()->getQuantizeSettings();
                if (settings.quantizeStart > 0 || settings.quantizeEnd > 0) {
                    bool bUpdateViewSelection = false;
                    view.visitClipView([&](clip_t* cl) {
                        // auto& notes = cl->notes;
                        /* Quantize notes to grid 
                        * 1. cut notes from clip
                        * 2. quantize notes in isolation
                        * 3. paste notes back to clip, cutting intersections
                        */
                        if (cl->notes.selection.empty()) {
                            return true;
                        }

                        auto& dragged = view.m_notesDragged[cl];
                        dragged.draggedSelection = dragged.draggedSelectionBegin;
                        auto getClipOffset = [](const clip_t* cl, tick_t ticksQ) {
                            auto offset = cl->time % ticksQ;
                            if (cl->loopEnabled) {
                                if (cl->offsetStart < cl->loopStart) {
                                    offset += (cl->loopStart - cl->offsetStart) % ticksQ;
                                } else {
                                    offset += ((cl->loopStart - cl->offsetStart) % cl->loopLen) % ticksQ;
                                }
                            } else {
                                offset -= cl->offsetStart % ticksQ;
                            }
                            return offset % ticksQ;
                        };
                        if (settings.quantizeStart > 0) {
                            auto ticksQ = settings.quantizeStart;
                            tick_t offset = 0;
                            if (view.isAbsoluteTimeMode()) {
                                offset = getClipOffset(cl, ticksQ);
                            }
                            for (auto& note : dragged.draggedSelection) {
                                auto newTime = math::roundfS32((note.start() + offset) / static_cast<float>(ticksQ)) * ticksQ;
                                note.time    = newTime - offset;
                            }
                        }
                        if (settings.quantizeEnd > 0) {
                            auto ticksQ = settings.quantizeEnd;
                            tick_t offset = 0;
                            if (view.isAbsoluteTimeMode()) {
                                offset = getClipOffset(cl, ticksQ);
                            }
                            for (auto& note : dragged.draggedSelection) {
                                auto newEnd = math::roundfS32((note.end() + offset) / static_cast<float>(ticksQ)) * ticksQ;
                                auto newLen = (newEnd - offset) - note.time;
                                if (newLen > 0) {
                                    note.len = newLen;
                                }                            
                            }
                        }
                        bool bRemovedNotes = cutSelfIntersecting(dragged.draggedSelection);
                        if (bRemovedNotes) {
                            log_lf(Log::L_DEBUG, "removed some intersecting notes\n");
                        }
                        mergeDraggedNotes(dragmode::drag_notes_move, cl);
                        if (view.isAbsoluteTimeMode()) {
                            auto pair = getMinMaxTime(cl->notes.selection);
                            if (pair.second)
                                grid.makeTickVisible(pair.second->end() + getTickOffset());
                            expandSelectionFrame(pair);
                        } else {
                            bUpdateViewSelection = true;
                        }
                        cl->setDirty();
                        cl->updateNoteViewSelection();
                        edit = true;
                        return true;
                    });
                    if (edit) {
                        desc = "Quanitize notes";
                    }
                    if (bUpdateViewSelection) {
                        setSelectionFrameFromView();
                    }
                }
            } else if (command == CMD_APPLY_PYTHON_SCRIPT) {
                if (clip) {
                    auto& notes = clip->notes;
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
                    auto& dragged = view.m_notesDragged[clip];
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
                        dragged.draggedSelection.push_back(note);
                    }
                    mergeDraggedNotes(dragmode::drag_notes_move);
#ifndef NDEBUG
                    for (note_t* selPtr: notes.selection) {
                        dbgassert(notes.has(selPtr));
                    }
#endif
                    auto pair = getMinMaxTime(notes.selection);
                    if (pair.second)
                        grid.makeTickVisible(pair.second->end() + getTickOffset());
                    expandSelectionFrame(pair);
                    clip->setDirty();
                    edit = true;
                    desc = "Apply python note processor";
                }
            }
        }
        if (command == GlobalCommandType::CMD_MOVE_CURSOR) {
            view.visitClipView([&](clip_t* cl) {
                auto& dragged = view.m_notesDragged[cl];
                auto& notes = cl->notes;
                auto dir = ivec2(ctxt.argInt0, ctxt.argInt1);
                if (dir.y && !notes.selection.empty()) {
                    if ((kevt.mods & KB_MOD_SHIFT)) {
                        dir *= 12;
                    }
                    changePitch(dragged.draggedSelection, dir.y,
                                layoutRoll.bFoldNotes, layoutRoll.bFoldNotes ? view.notePitches : std::vector<int32_t>{});
                    int32_t merged = mergeDraggedNotes(dragmode::drag_notes_move, cl);
                    if (merged > 0) {
                        notes.updateBounds();
                        cl->updateNoteViewSelection();
                        auto pair = getMinMaxSemitones(view);
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
                    }
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
                            offsetEndTime(dragged.draggedSelection, timeOffset, minLen);
                        } else {
                            offsetStartTime(dragged.draggedSelection, timeOffset);
                        }
                        int32_t merged = mergeDraggedNotes(dragmode::drag_notes_move, cl);
                        if (merged > 0) {
                            notes.updateBounds();
                            cl->updateNoteViewSelection();
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
                        }
                    } else {
                        grid.makeTickVisible(cursor.start);
                    }
                }
                return true;
            });
            if (edit) {
                view.updateNotePitches(false);
                setSelectionFrameFromView();
            }
            desc    = "Move notes";
        }
        if (edit) {
            // notes.updateBounds();
            setGlobalSelectionFromClipSelection();
            dawCtrl->getDaw()->pushHist(new action_modify_notes(desc, view, cursorBefore));
            view.updateNotePitches(false);
        }
    }
    static constexpr auto commands = {CMD_SELECT_ALL, CMD_DELETE, CMD_MUTE, CMD_CUT, CMD_COPY, CMD_DUPLICATE, CMD_PASTE, CMD_APPLY_ARP, CMD_APPLY_GROOVE, CMD_QUANTIZE, CMD_APPLY_PYTHON_SCRIPT};
    return std::find(commands.begin(), commands.end(), command) != commands.end();
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
                        paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED_BORDER_BRIGHT;
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
                            paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED_BORDER_BRIGHT;
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
    if (view.isAbsoluteTimeMode()) {
        return grid.tickToScreenD(getTickOffsetOffset() + (clip->offsetStart < clip->loopStart ? clip->loopStart - clip->offsetStart : 0));
    }
    return (float) grid.tickToScreenD(getTickOffset() + clip->loopStart);
}

float guictr_cliphandles::clipLoopEndScrX() const {
    auto clip = view.clip();
    if (!assert_expr(clip)) {
        return 0;
    }
    if (view.isAbsoluteTimeMode()) {
        return grid.tickToScreenD(getTickOffsetOffset() + (clip->offsetStart < clip->loopStart ? clip->loopStart - clip->offsetStart : 0) + clip->loopLen);
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
    auto& selClipView = parentEditor.getClipView();
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

void gui_clipgroove_settings::buttonClicked(guibase* button) {
    if (parent) {
        if (button == &btnApply) {
            auto clipEditor = guiParentType<guictr_clipeditor, gui_type::CTR_TYPE_CLIPEDITOR>(this->parent);
            if (!assert_expr(clipEditor)) {
                return;
            }
            auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_APPLY_GROOVE};
            clipEditor->handleEditorCommand(temp);
            return;
        }
        auto daw = dawCtrl->getDaw();
        auto lock = daw->lockPlayThread();
        auto& data = daw->getGrooves();
        // find by name and update
        auto grooveIdx = std::distance(data.begin(), std::find_if(data.begin(), data.end(), [name = grooveData.presetName](const auto& groove) { return groove.presetName == name; }));
        if (grooveIdx >= 0 && grooveIdx < int32_t(data.size())) {
            data[grooveIdx] = this->grooveData;
        }
        if (view.isAbsoluteTimeMode()) {
            for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
                for (clip_t* viewClip : vecClips) {
                    if (trackEntry.track->getClips().hasClip(viewClip)) {
                        viewClip->setDirty();
                        daw->updateClipViews(viewClip);
                    }
                }
            }
            // TODO: history
        } else {
            auto clip = view.clip();
            if (clip) {
                clip->setDirty();
                daw->updateClipViews(clip);
            }
        }
        daw->updateVisibleTrackContents();
    }
}

class guictxtmenu_add_groove final : public guictxtmenu {
    std::vector<groove_data_t> grooveList;
    DawCtrl* dawCtrl;
    clip_view_t& view;
public:
    guictxtmenu_add_groove(DawCtrl* _dawCtrl, clip_view_t& view) : dawCtrl(_dawCtrl), view(view) {
        grooveList = dawCtrl->getDaw()->getGrooveLibrary().getGrooves();
        for (int i = 0; i < int32_t(grooveList.size()); ++i) {
            addEntry(new ctxtmenu_entry(grooveList[i].grooveName, i));
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        clip_t* clip = view.clip();
        if (clip && _id >= 0 && _id < int32_t(grooveList.size())) {
            auto daw = dawCtrl->getDaw();
            auto lock = daw->lockPlayThread();
            auto& projectGrooves = this->dawCtrl->getDaw()->getGrooves();
            groove_data_t groove{};
            // find unique name
            auto grooveTemplate = grooveList[_id];
            String nameGroove = "Groove #" + std::to_string(projectGrooves.size() + 1);
            int32_t idx = 1;
            while (std::any_of(projectGrooves.begin(), projectGrooves.end(), [&nameGroove](const auto& groove) { return groove.presetName == nameGroove; })) {
                nameGroove = "Groove #" + std::to_string(idx);
                idx++;
            }
            groove = grooveTemplate;
            groove.presetName = nameGroove;
            groove.grooveName = grooveTemplate.grooveName;
            groove.timingData = grooveTemplate.timingData;
            projectGrooves.push_back(groove);
            auto grooveIdx = projectGrooves.size() - 1;
            if (view.isAbsoluteTimeMode()) {
                for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
                    for (clip_t* viewClip : vecClips) {
                        if (trackEntry.track->getClips().hasClip(viewClip)) {
                            viewClip->selectedGroove = grooveIdx;
                            viewClip->setDirty();
                            daw->updateClipViews(viewClip);
                        }
                    }
                }
                // TODO: history
            } else if (clip) {
                auto grooveDataBefore = clip->selectedGroove;
                auto histTask = new action_modify_clip_groove_setting("Edit clip groove", view, grooveDataBefore);
                clip->selectedGroove  = grooveIdx;
                daw->pushHist(histTask);
                clip->setDirty();
                daw->updateClipViews(clip);
            }
            daw->updateVisibleTrackContents();
            closeContextMenu();
        }
        return true;
    }
};
class guictxtmenu_select_groove_preset final : public guictxtmenu {
    DawCtrl* dawCtrl;
    clip_view_t& view;
public:
    guictxtmenu_select_groove_preset(DawCtrl* _dawCtrl, clip_view_t& view) : dawCtrl(_dawCtrl), view(view) {
        auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
        addEntry(new ctxtmenu_entry("Add New Groove", 0));
        addEntry(new ctxtmenu_entry("None", 1));
        for (int i = 0; i < int32_t(projectGrooves.size()); ++i) {
            auto& groove = projectGrooves[i];
            addEntry(new ctxtmenu_entry(groove.presetName, i + 2));
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id > 0) {
            _id -= 2;
            auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
            if (_id < 0 || _id >= int32_t(projectGrooves.size())) {
                _id = -1;
            }
            auto daw              = dawCtrl->getDaw();
            clip_t* clip = view.clip();
            auto lock = dawCtrl->getDaw()->lockPlayThread();
            if (view.isAbsoluteTimeMode()) {
                for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
                    for (clip_t* viewClip : vecClips) {
                        if (trackEntry.track->getClips().hasClip(viewClip)) {
                            viewClip->selectedGroove = _id;
                            viewClip->setDirty();
                            daw->updateClipViews(viewClip);
                        }
                    }
                }
                // TODO: history
            } else if (clip) {
                auto grooveDataBefore = clip->selectedGroove;
                auto histTask = new action_modify_clip_groove_setting("Edit clip groove", view, grooveDataBefore);
                clip->selectedGroove  = _id;
                daw->pushHist(histTask);
                clip->setDirty();
                daw->updateClipViews(clip);
            }
            daw->updateVisibleTrackContents();
            closeContextMenu();
        }
        return true;
    }

    guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override {
        if (e->id == 0) {
            return new guictxtmenu_add_groove(dawCtrl, view);
        }
        return nullptr;
    }
};

class guictxtmenu_select_groove_pattern final : public guictxtmenu {
    DawCtrl* dawCtrl;
    clip_view_t& view;
public:
    guictxtmenu_select_groove_pattern(DawCtrl* _dawCtrl, clip_view_t& view) : dawCtrl(_dawCtrl), view(view) {
        auto& groovePatterns = dawCtrl->getDaw()->getGrooveLibrary().getGrooves();
        for (int i = 0; i < int32_t(groovePatterns.size()); ++i) {
            auto& groove = groovePatterns[i];
            addEntry(new ctxtmenu_entry(groove.grooveName, i));
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 0) {
            auto& groovePatterns = dawCtrl->getDaw()->getGrooveLibrary().getGrooves();
            if (groovePatterns.empty()) {
                return false;
            }
            if (_id >= int32_t(groovePatterns.size())) {
                _id = 0;
            }
            auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
            auto daw = dawCtrl->getDaw();
            clip_t* clip = view.clip();
            auto lock = dawCtrl->getDaw()->lockPlayThread();
            if (view.isAbsoluteTimeMode()) {
                for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
                    for (clip_t* viewClip : vecClips) {
                        if (trackEntry.track->getClips().hasClip(viewClip)) {
                            if (viewClip->selectedGroove >= 0 && viewClip->selectedGroove < int32_t(projectGrooves.size())) {
                                projectGrooves[viewClip->selectedGroove].grooveName = groovePatterns[_id].grooveName;
                                projectGrooves[viewClip->selectedGroove].timingData = groovePatterns[_id].timingData;
                            }
                            viewClip->setDirty();
                            daw->updateClipViews(viewClip);
                        }
                    }
                }
                // TODO: history
            } else if (clip) {
                if (clip->selectedGroove >= 0 && clip->selectedGroove < int32_t(projectGrooves.size())) {
                    projectGrooves[clip->selectedGroove].grooveName = groovePatterns[_id].grooveName;
                    projectGrooves[clip->selectedGroove].timingData = groovePatterns[_id].timingData;
                }
                clip->setDirty();
                daw->updateClipViews(clip);
            }
            daw->updateVisibleTrackContents();
            closeContextMenu();
        }
        return true;
    }

    guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override {
        if (e->id == 0) {
            return new guictxtmenu_add_groove(dawCtrl, view);
        }
        return nullptr;
    }
};
class guidropdown_groove_pattern final : public guidropdownbase {
    clip_view_t& view;
public:
    explicit guidropdown_groove_pattern(clip_view_t& view)
        : guidropdownbase(), view(view) {
    }
    String getString() override {
        auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
        auto clip = view.clip();
        if (!clip) {
            return "-";
        }
        if (clip->selectedGroove < 0 || clip->selectedGroove >= int32_t(projectGrooves.size())) {
            return "-";
        }
        return projectGrooves[clip->selectedGroove].grooveName;
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        auto popup = new guictxtmenu_select_groove_pattern(dawCtrl, view);
        parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
    int32_t getSelectIndex() override {
        // copy next grooves timing data to clips current groove
        auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
        auto clip = view.clip();
        if (!clip) {
            return -1;
        }
        if (clip->selectedGroove < 0 || clip->selectedGroove >= int32_t(projectGrooves.size())) {
            return -1;
        }
        auto& projectClipGroove = projectGrooves[clip->selectedGroove];
        // check for groove with identical name in groove library
        auto& grooveLibrary = dawCtrl->getDaw()->getGrooveLibrary();
        const groove_data_t* grooveFound = nullptr;
        int32_t idx = 0;
        auto& grooves = grooveLibrary.getGrooves();
        for (auto& g : grooves) {
            if (g.grooveName == projectClipGroove.grooveName) {
                grooveFound = &g;
                break;
            }
            idx++;
        }
        return grooveFound ? idx : -1;
    }
    void setSelectedIndex(int32_t idx) override {
        auto daw = dawCtrl->getDaw();
        auto& projectGrooves = daw->getGrooves();
        auto& groovePatterns = daw->getGrooveLibrary().getGrooves();
        if (groovePatterns.empty())
            return;
        auto clip = view.clip();
        if (!clip) {
            return;
        }
        idx = idx % int32_t(groovePatterns.size());
        while (idx < 0) {
            idx += int32_t(groovePatterns.size());
        }
        auto* newGroove = &groovePatterns[idx];
        auto lock = daw->lockPlayThread();
        if (view.isAbsoluteTimeMode()) {
            int32_t modifyGrooveIdx = -1;
            for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
                for (clip_t* viewClip : vecClips) {
                    if (trackEntry.track->getClips().hasClip(viewClip)) {
                        if (modifyGrooveIdx < 0) {
                            modifyGrooveIdx = viewClip->selectedGroove;
                        }
                        if (modifyGrooveIdx >= 0 && viewClip->selectedGroove == modifyGrooveIdx) {
                            auto& projectClipGroove = projectGrooves[modifyGrooveIdx];
                            projectClipGroove.grooveName = newGroove->grooveName;
                            projectClipGroove.timingData = newGroove->timingData;
                            viewClip->setDirty();
                            daw->updateClipViews(viewClip);
                        }
                    }
                }
            }
            // TODO: history
        } else if (clip) {
            if (!newGroove) {
                auto histTask = new action_modify_clip_groove_setting("Edit clip groove", view, clip->selectedGroove);
                clip->selectedGroove = -1;
                daw->pushHist(histTask);
            } else {
                if (clip->selectedGroove < 0) {
                    return;
                }
                auto& projectClipGroove = projectGrooves[clip->selectedGroove];
                projectClipGroove.grooveName = newGroove->grooveName;
                projectClipGroove.timingData = newGroove->timingData;
                clip->setDirty();
                daw->updateClipViews(clip);
            }
            clip->setDirty();
            daw->updateClipViews(clip);
        }
        daw->updateVisibleTrackContents();
    }
    int32_t getLastIndex() override {
        auto& grooveLibrary = dawCtrl->getDaw()->getGrooveLibrary();
        auto& grooves = grooveLibrary.getGrooves();
        return grooves.size() - 1;
    }
    void select(dropdown_field_selectitem req, int32_t idxOffset) override {
        auto index = getSelectIndex();
        if (index == -1)
            return;

        switch (req) {
            case SELECT_IDX:
                setSelectedIndex(idxOffset);
                break;
            case SELECT_NEXT:
                setSelectedIndex(math::min<int32_t>(getLastIndex() + 1, index + idxOffset));
                break;
            case SELECT_PREVIOUS:
                setSelectedIndex(math::max<int32_t>(-1, index - idxOffset));
                break;
            case SELECT_FIRST:
                setSelectedIndex(0);
                break;
            case SELECT_LAST:
                setSelectedIndex(getLastIndex());
                break;
        }
    }
};

class guidropdown_groove_preset final : public guidropdownbase {
    clip_view_t& view;
public:
    explicit guidropdown_groove_preset(clip_view_t& view)
        : guidropdownbase(), view(view) {
    }
    String getString() override {
        auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
        auto clip = view.clip();
        if (!clip) {
            return "Select Groove";
        }
        if (clip->selectedGroove < 0 || clip->selectedGroove >= int32_t(projectGrooves.size())) {
            return "Select Groove";
        }
        return projectGrooves[clip->selectedGroove].presetName;
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        auto popup = new guictxtmenu_select_groove_preset(dawCtrl, view);
        parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
    int32_t getSelectIndex() override {
        // copy next grooves timing data to clips current groove
        auto& projectGrooves = dawCtrl->getDaw()->getGrooves();
        auto clip = view.clip();
        if (!clip) {
            return -1;
        }
        if (clip->selectedGroove < 0 || clip->selectedGroove >= int32_t(projectGrooves.size())) {
            return 0;
        }
        return clip->selectedGroove + 1;
    }
    void setSelectedIndex(int32_t idx) override {
        idx -= 1;
        auto daw = dawCtrl->getDaw();
        auto& projectGrooves = daw->getGrooves();
        auto clip = view.clip();
        if (!clip) {
            return;
        }
        if (idx < 0 || idx >= int32_t(projectGrooves.size())) {
            idx = -1;
        }
        auto* newGroove = idx >= 0 ? &projectGrooves[idx] : nullptr;
        auto newGrooveIdx = newGroove ? idx : -1;
        auto lock = daw->lockPlayThread();
        if (view.isAbsoluteTimeMode()) {
            for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
                for (clip_t* viewClip : vecClips) {
                    if (trackEntry.track->getClips().hasClip(viewClip)) {
                        viewClip->selectedGroove = newGrooveIdx;
                        viewClip->setDirty();
                        daw->updateClipViews(viewClip);
                    }
                }
            }
            // TODO: history
        } else if (clip) {
            auto histTask = new action_modify_clip_groove_setting("Edit clip groove", view, clip->selectedGroove);
            clip->selectedGroove = newGrooveIdx;
            daw->pushHist(histTask);
            clip->setDirty();
            daw->updateClipViews(clip);
        }
        daw->updateVisibleTrackContents();
    }
    int32_t getLastIndex() override {
        auto& grooves = dawCtrl->getDaw()->getGrooves();
        return grooves.size();
    }
};
gui_clipgroove_settings::gui_clipgroove_settings(gui_clipsettings& parent, clip_view_t& _view)
    : guictr_base(),
      parentClipSettings(parent),
      view(_view),
      lenQuantization(true),
      strengthQuantization(nullptr),
      strengthGroove(nullptr),
      strengthVelocity(nullptr),
      randomTiming(nullptr),
      randomVelocity(nullptr),
      dropdownSelectPreset(new guidropdown_groove_preset(view)),
      dropdownSelectGroove(new guidropdown_groove_pattern(view))
{
    lenQuantization.setRef(toRef(), &grooveData.lenQuantization);
    strengthQuantization.setRef(&grooveData.strengthQuantization);
    strengthGroove.setRef(&grooveData.strengthGroove);
    strengthVelocity.setRef(&grooveData.strengthVelocity);
    randomTiming.setRef(&grooveData.randomTiming);
    randomVelocity.setRef(&grooveData.randomVelocity);
    setLabel("Groove");
    setBackgroundRendered(true);
    setBackgroundRenderedInset(true);
    setFlag(FLG_RENDER_LABEL, true);
    dropdownSelectPreset->setLabel("Groove");
    dropdownSelectGroove->setLabel("Pattern");
    lenQuantization.setLabel("Quantization Length");
    strengthQuantization.setLabel("Strength Quantization");
    strengthGroove.setLabel("Strength Groove");
    strengthVelocity.setLabel("Strength Velocity");
    randomTiming.setLabel("Random Timing");
    randomVelocity.setLabel("Random Velocity");
    btnApply.setText("Apply");
    add(dropdownSelectPreset);
    add(dropdownSelectGroove);
    add(&lenQuantization);
    add(&strengthQuantization);
    add(&strengthGroove);
    add(&strengthVelocity);
    add(&randomTiming);
    add(&randomVelocity);
    add(&btnApply);

    (void) parentClipSettings;
}

void gui_clipgroove_settings::setSelectedGroove(const int32_t& _selectedGroove) {
    auto data = dawCtrl->getDaw()->getGrooves();
    if (_selectedGroove < 0 || _selectedGroove >= int32_t(data.size())) {
        grooveData = {};
    } else {
        grooveData = data[_selectedGroove];
    }
    // hide all controls if groove is not selected
    dropdownSelectGroove->setVisible(_selectedGroove >= 0);
    lenQuantization.setVisible(_selectedGroove >= 0);
    strengthQuantization.setVisible(_selectedGroove >= 0);
    strengthGroove.setVisible(_selectedGroove >= 0);
    strengthVelocity.setVisible(_selectedGroove >= 0);
    randomTiming.setVisible(_selectedGroove >= 0);
    randomVelocity.setVisible(_selectedGroove >= 0);
    btnApply.setVisible(_selectedGroove >= 0);
    onChildLayoutChanged(this);
}

void gui_clipgroove_settings::layout() {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

    int32_t w = getSizeContent().x;
    auto pos  = ivec2(0, 0);
    for (guibase* gui : guis) {
        gui->size = ivec2(w, TRACK_HEIGHT_STEP);
        gui->pos  = pos;
        pos.y += TRACK_HEIGHT_STEP + padding;
    }

    for (guibase* gui : guis) {
        gui->layout();
    }
}

void gui_clipgroove_settings::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    for (guibase* gui : guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
}

gui_clipgroove_settings::~gui_clipgroove_settings() {
    removeGuis();
    delete dropdownSelectGroove;
    delete dropdownSelectPreset;
}

bool gui_pianoroll::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (isCtrl(evt.kbmods)) {
        dragPosObjSpace = toNoteF(evt.relMousepos.y);
        setScale(layoutRoll.scale() + yoffset);
        int32_t rel  = math::min(size.y - 1, math::max(0, size.y - evt.relMousepos.y));
        float offset = (size.y - toScreenF(dragPosObjSpace)) + layoutRoll.offset();
        setOffset(offset - rel);
        return true;
    }
    return guibase::handleMouseScroll(evt, xoffset, yoffset);
}
