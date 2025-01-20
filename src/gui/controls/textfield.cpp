#include "textfield.hpp"
#include "math/seq_math.hpp"
#include "seq_util.hpp"
#include "theme.hpp"
#include "str_util.hpp"
#include "gui/gui.hpp"
#include "basectrl.hpp"
#include "guicolors.hpp"
#include "platform.hpp"
#include "keyboard.hpp"
#include "guifonts.hpp"

#include <nanovg.h>
#include <utfconv/utf.hpp>


bool gui_textfield::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) return false;
    if (evt.type == MouseHitType::MOUSE_DRAGDROP_FILE) return false;
    if (canMouseHit() && contains(mpos)) {
        evt.requestFocus(this);
        return true;
    }
    return false;
}

bool gui_textfield::handleKeyInput(KeyEvent& kevt) {
    return keyboardEvent(kevt.keyCode, 0, kevt.type, kevt.mods);
}

void gui_textfield::handleRightClick(MouseEvent& evt) {
}

void gui_textfield::handleDraggedBegin(MouseEvent& evt) {
    ivec2 local = evt.relMousepos;
    if (editable()) {
        if (mCommitted && mInputActivates) {
            beginEdit();
        }
        mMouseDownPos      = local;
        mMouseDownModifier = evt.kbmods;

        //TODO: handle double click detection consistently
        auto tmNow = getTimeMillis();
        if (tmNow - m_tmLastClick <= 250) {
            /* Double-click: select all text */
            mSelectionPos = 0;
            mCursorPos    = (int) mValueTemp.size();
            mMouseDownPos = ivec2(-1, -1);
        } else {
            updateCursor(nullptr, metrics.textBounds[2]);
        }
        m_tmLastClick = tmNow;
    }
}

void gui_textfield::handleDraggedMove(MouseEvent& evt) {
    mMouseDragPos     = evt.relMousepos;
    this->mTextOffset = -123123;
    if (editable() && mFocused) {
    }
}

void gui_textfield::handleDraggedRelease(MouseEvent& evt) {
    mMouseDownPos = ivec2(-1, -1);
    mMouseDragPos = ivec2(-1, -1);
}

void gui_textfield::onTextChange() {
    auto tempU8 = utf::as_str8(mValueTemp);
    if (mCallback && !mCallback(tempU8)) {
    }
}

void gui_textfield::onTextEndEdit() {
    auto tempU8 = utf::as_str8(mValueTemp);
    if (mCallbackEnd && !mCallbackEnd(tempU8)) {
        parentCtrl->closePopup();
    }
}

void gui_textfield::render(NVGcontext* vg) {
    auto fs = fontSize();
    auto fs2 = fs;
    if (fs2 <= 0.0f) {
        fs2 = math::clamp(math::min(size.y, size.x), 4, 48) * FONT_AUTOSCALE;
    }
    auto fontScale = math::roundfS32(fs2 * theme->getFloat(GuiConstant::CONST_FONT_SCALE));

    theme->bindFont(vg, UIFont::FONT_TEXTFIELD);
    nvgFontSize(vg, fontScale);

    switch (alignment()) {
        case gui_textfield::Alignment::Left:
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            break;
        case gui_textfield::Alignment::Right:
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            break;
        case gui_textfield::Alignment::Center:
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            break;
    }

    //TODO: find a better place to do this: font metrics require nano-vg context but should be calculated in drag/move on the fly
    updateTextLayout(vg);

    if (!mCommitted) {
        updateCursor(vg, metrics.textBounds[2]);
        updateShiftCursorVisible();
    }
    this->renderTextField(vg);
}

void gui_textfield::updateTextLayout(NVGcontext* ctx) {
    auto tempU8 = utf::as_str8(mValueTemp);
    nvgTextBounds(ctx, 0, 0, tempU8.c_str(), nullptr, metrics.textBounds);
    metrics.lineH = metrics.textBounds[3] - metrics.textBounds[1];

    // find cursor positions
    if (metrics.glyphPositions.size() < mValueTemp.length()) {
        metrics.glyphPositions.resize(mValueTemp.length());
    }
    metrics.numGlyphs = nvgTextGlyphPositions(ctx, 0, 0, tempU8.c_str(), nullptr, metrics.glyphPositions.data(), CtrSize(metrics.glyphPositions));
    metrics.numGlyphs = math::min<int32_t>(metrics.numGlyphs, CtrSize(metrics.glyphPositions));
    
    vec2 insetPos(pos.x, pos.y + size.y * 0.5f);


    float unitWidth = 0;

    if (!mUnits.empty()) {
        unitWidth = nvgTextBounds(ctx, 0, 0, mUnits.c_str(), nullptr, nullptr) + 2;
    }

    switch (mAlignment) {
        case Alignment::Left:
            insetPos.x += TEXT_INSET;
            break;
        case Alignment::Right:
            insetPos.x += size.x - unitWidth - TEXT_INSET;
            break;
        case Alignment::Center:
            insetPos.x += size.x * 0.5f;
            break;
    }

    drawPos  = math::roundvecS32(insetPos);
    clipPos  = { pos.x + TEXT_INSET, pos.y };
    clipSize = { size.x - unitWidth - 2 * TEXT_INSET, size.y };
}

void gui_textfield::renderTextField(NVGcontext* ctx) const {

    if (size.x * size.y < 10)
        return;
    int32_t fl = getStateFlags();
    renderWidgetBorder(ctx, fl);

    // clip visible text area
    if (clipSize.x < 1 || clipSize.y < 1)
        return;
    NVGcolor mTextColorDisabled = theme->getColor(GuiColor::COL_TEXTBOX_TEXT_DISABLED);
    if (!mUnits.empty()) {
        nvgFillColor(ctx, mTextColorDisabled);
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, pos.x + size.x - TEXT_INSET, drawPos.y, mUnits.c_str(), nullptr);
    }
    switch (alignment()) {
        case gui_textfield::Alignment::Left:
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            break;
        case gui_textfield::Alignment::Right:
            nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            break;
        case gui_textfield::Alignment::Center:
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            break;
    }
    NVGcolor mTextColor         = theme->getColor(mColor);
    NVGcolor mTextColorMarked   = theme->getColor(GuiColor::COL_TEXTBOX_TEXT_MARKED);

    NVGcolor mColor = isEnabled() && (!mCommitted || !mValue.empty()) ? mTextColor : mTextColorDisabled;


    nvgSave(ctx);

    if (mCommitted) {
        nvgIntersectScissor(ctx, clipPos.x, clipPos.y, clipSize.x, clipSize.y);
        nvgFillColor(ctx, mColor);
        nvgText(ctx, drawPos.x, drawPos.y, mValue.empty() ? mPlaceholder.c_str() : mValue.c_str(), nullptr);
    } else {

        nvgTranslate(ctx, drawPos.x + mTextOffset, drawPos.y);
        if (mCursorPos > -1) {
            float lineh  = metrics.lineH;
            float caretx = cursorIndex2Position(mCursorPos, metrics.textBounds[2]);
            float carX   = caretx;
            if (mSelectionPos > -1) {
                float selx = cursorIndex2Position(mSelectionPos, metrics.textBounds[2]);

                if (caretx > selx)
                    std::swap(caretx, selx);

                // draw selection
                nvgBeginPath(ctx);
                nvgRect(ctx, caretx, -lineh * 0.5f, selx - caretx, lineh);
                nvgFillColor(ctx, mTextColorMarked);
                nvgFill(ctx);
            }


            // draw cursor
            nvgBeginPath(ctx);
            nvgMoveTo(ctx, carX, -lineh * 0.5f);
            nvgLineTo(ctx, carX, +lineh * 0.5f);
            nvgStrokeColor(ctx, mTextColor);
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
        }

        nvgFillColor(ctx, mColor);
        // draw text with offset
        auto strEditValue = utf::as_str8(mValueTemp);
        nvgText(ctx, 0, 0, strEditValue.c_str(), nullptr);
    }
    nvgRestore(ctx);
}

void gui_textfield::beginEdit() {
    mValueTemp = utf::as_u32(mValue);
    mCommitted = false;
    int begin  = mCursorPos;
    int end    = mSelectionPos;

    if (begin > end)
        std::swap(begin, end);
    if (begin < 0 || end > static_cast<int32_t>(mValue.length())) {
        mCursorPos    = 0;
        mSelectionPos = -1;
    }
    mValidFormat = (mValueTemp.empty()) || checkFormat(utf::as_str8(mValueTemp), mFormat);
}

void gui_textfield::endEdit(bool success) {
    mCommitted    = true;
    if (success) {
        mValidFormat = (mValueTemp.empty()) || checkFormat(utf::as_str8(mValueTemp), mFormat);
        if (mValidFormat) {
            if (mValueTemp.empty())
                mValue = mDefaultValue;
            else
                mValue = utf::as_str8(mValueTemp);
        }
        onTextEndEdit();
    } else {
        mValueTemp = utf::as_u32(mValue);
    }

    mValidFormat  = true;
    mCursorPos    = -1;
    mSelectionPos = -1;
    mTextOffset   = 0;
}

bool gui_textfield::focusEvent(MouseHitEvt& evt, bool focused) {
    mFocused           = focused;
    if (fnFocus) {
        fnFocus(evt, focused);
    }
    if (editable()) {
        if (focused) {
            if (mCommitted) beginEdit();
        } else {
            if (!mCommitted) endEdit(!mCommitted);
        }
    }

    return true;
}

bool gui_textfield::keyboardEvent(KeyboardKey key, int /* scancode */, KeyboardState action, KeyboardMods modifiers) {
    if (editable() && mFocused) {
        if (action != KeyboardState::K_RELEASE) {
            if (!mCommitted) {
                if (key == KeyboardKey::DAW_KB_LEFT) {
                    if (modifiers == KB_MOD_SHIFT) {
                        if (mSelectionPos == -1)
                            mSelectionPos = mCursorPos;
                    } else {
                        mSelectionPos = -1;
                    }

                    if (mCursorPos > 0)
                        mCursorPos--;
                } else if (key == KeyboardKey::DAW_KB_RIGHT) {
                    if (modifiers == KB_MOD_SHIFT) {
                        if (mSelectionPos == -1)
                            mSelectionPos = mCursorPos;
                    } else {
                        mSelectionPos = -1;
                    }

                    if (mCursorPos < (int) mValueTemp.length())
                        mCursorPos++;
                } else if (key == KeyboardKey::DAW_KB_HOME) {
                    if (modifiers == KB_MOD_SHIFT) {
                        if (mSelectionPos == -1)
                            mSelectionPos = mCursorPos;
                    } else {
                        mSelectionPos = -1;
                    }

                    mCursorPos = 0;
                } else if (key == KeyboardKey::DAW_KB_END) {
                    if (modifiers == KB_MOD_SHIFT) {
                        if (mSelectionPos == -1)
                            mSelectionPos = mCursorPos;
                    } else {
                        mSelectionPos = -1;
                    }

                    mCursorPos = (int) mValueTemp.size();
                } else if (key == KeyboardKey::DAW_KB_BACKSPACE) {
                    if (!deleteSelection()) {
                        if (mCursorPos > 0) {
                            mCursorPos--;
                            if (filter && filter->isReplaceInput()) {
                                mValueTemp[mCursorPos] = '0';
                            } else if (mValueTemp.length()) {
                                mValueTemp.erase(mValueTemp.begin() + mCursorPos);
                            }
                        }
                    }
                } else if (key == KeyboardKey::DAW_KB_DELETE) {
                    if (!deleteSelection()) {
                        if (filter && filter->isReplaceInput()) {
                        } else {
                            if (mCursorPos < (int) mValueTemp.length())
                                mValueTemp.erase(mValueTemp.begin() + mCursorPos);
                        }
                    }
                } else if (key == KeyboardKey::DAW_KB_A && isCtrl(modifiers)) {
                    mCursorPos    = (int) mValueTemp.length();
                    mSelectionPos = 0;
                } else if (key == KeyboardKey::DAW_KB_X && isCtrl(modifiers)) {
                    copySelection();
                    deleteSelection();
                } else if (key == KeyboardKey::DAW_KB_C && isCtrl(modifiers)) {
                    copySelection();
                } else if (key == KeyboardKey::DAW_KB_V && isCtrl(modifiers)) {
                    deleteSelection();
                    pasteFromClipboard();
                } else if (key == KeyboardKey::DAW_KB_ESCAPE) {
                    endEdit(false);
                } else if (mReturnCommits && (key == KeyboardKey::DAW_KB_ENTER || key == KeyboardKey::DAW_KB_KP_ENTER)) {
                    endEdit(true);
                }
                return true;
            } else {
                if (mReturnCommits && (key == KeyboardKey::DAW_KB_ENTER || key == KeyboardKey::DAW_KB_KP_ENTER)) {
                    beginEdit();
                    onChange();
                    return true;
                }
            }
        }
    }

    return false;
}

bool gui_textfield::canHandleCharInput(uint32_t codepoint) {
    if (editable()) {
        const bool bIsGlobalKey = parentCtrl->isGlobalKeybindCodepoint(codepoint);
        const bool bIsFiltered = filter && filter->isAllowedChar(codepoint);

        if (mFocused) {
            return !bIsFiltered;
        }
        if (mInputActivates) {
            return !bIsGlobalKey && !bIsFiltered;
        }
    }
    return false;
}

bool gui_textfield::handleCharInput(uint32_t codepoint) {
    if (editable()) {
        if (!mFocused) {
            return false;
        }
        const bool bIsGlobalKey = parentCtrl->isGlobalKeybindCodepoint(codepoint);
        const bool bIsFiltered = filter && filter->isAllowedChar(codepoint);
        if (bIsFiltered) {
            return false;
        }
        if (mCommitted) {
            if (bIsGlobalKey) return false;
            if (!mInputActivates) return false;
            beginEdit();
        }
        if (mCursorPos > -1) {
            if (filter && filter->isReplaceInput()) {
                int32_t len       = 1;
                int32_t mincursor = mCursorPos;
                if (mSelectionPos > -1) {
                    mincursor = math::min(mSelectionPos, mCursorPos);
                    len       = math::max(mSelectionPos, mCursorPos) - mincursor;
                }
                for (int i = 0; i < len; i++) {
                    mValueTemp[i + mincursor] = codepoint;
                }

                if (mSelectionPos < 0) {
                    mCursorPos++;
                }
                if (mCursorPos > static_cast<int32_t>(mValueTemp.length())) {
                    mCursorPos = static_cast<int32_t>(mValueTemp.length());
                }
            } else {
                deleteSelection();
                mValueTemp.insert(mValueTemp.begin() + mCursorPos, codepoint);
                mCursorPos += 1;
            }
            onChange();
        }
        return true;
    }

    return false;
}

void gui_textfield::onChange() {
    mValidFormat = mValueTemp.empty() || checkFormat(utf::as_str8(mValueTemp), mFormat);
    if (filter) {
        mValueTemp = utf::as_u32(filter->parse(utf::as_str8(mValueTemp)));
        if (mCursorPos > static_cast<int32_t>(mValueTemp.length())) {
            mCursorPos = static_cast<int32_t>(mValueTemp.length());
        }
    }
    onTextChange();
}

bool gui_textfield::checkFormat(const std::string& input, const std::string& format) {
    return true;
}

bool gui_textfield::copySelectionString(std::string& output) {
    if (mSelectionPos > -1) {
        int begin = mCursorPos;
        int end   = mSelectionPos;

        if (begin > end)
            std::swap(begin, end);
        if ((int) mValueTemp.length() >= end - begin)
            output = utf::as_str8(mValueTemp.substr(begin, end));
        return true;
    }

    return false;
}

bool gui_textfield::copySelection() {
    if (mSelectionPos > -1) {
        int begin = mCursorPos;
        int end   = mSelectionPos;

        if (begin > end)
            std::swap(begin, end);
        if ((int) mValueTemp.length() >= end - begin) {
            parentCtrl->setClipboardText(utf::as_str8(mValueTemp.substr(begin, end)));
        }
        onChange();//=??????
        return true;
    }

    return false;
}

void gui_textfield::pasteFromClipboard() {
    if (mCursorPos >= 0 && mCursorPos <= (int) mValueTemp.size()) {
        String str = parentCtrl->getClipboardText();
        auto u32str = utf::as_u32(str);
        mValueTemp.insert(mCursorPos, u32str);
        mCursorPos += int32_t(u32str.length());
        onChange();
    }
}

bool gui_textfield::deleteSelection() {
    if (mSelectionPos > -1) {
        int begin = mCursorPos;
        int end   = mSelectionPos;

        if (begin > end)
            std::swap(begin, end);
        if (mValueTemp.empty()) return false;
        dbgassert(!mValueTemp.empty());
        if (begin == end - 1)
            mValueTemp.erase(mValueTemp.begin() + begin);
        else
            mValueTemp.erase(mValueTemp.begin() + begin,
                             mValueTemp.begin() + end);

        mCursorPos    = begin;
        mSelectionPos = -1;
        return true;
    }

    return false;
}

void gui_textfield::updateShiftCursorVisible() {
    int prevCPos = mCursorPos > 0 ? mCursorPos - 1 : 0;
    int nextCPos = mCursorPos < metrics.numGlyphs ? mCursorPos + 1 : metrics.numGlyphs;
    float prevCX = cursorIndex2Position(prevCPos, metrics.textBounds[2]);
    float nextCX = cursorIndex2Position(nextCPos, metrics.textBounds[2]);

    nextCX = nextCX + drawPos.x - clipPos.x;
    prevCX = prevCX + drawPos.x - clipPos.x;
    float mTextOffset = 0;
    if (nextCX > clipSize.x)
        mTextOffset -= nextCX - clipSize.x + 1;
    if (prevCX < 0)
        mTextOffset += 0 - prevCX + 1;
    this->mTextOffset = mTextOffset;
}

void gui_textfield::updateCursor(NVGcontext*, float lastx) {
    // handle mouse cursor events
    if (mMouseDownPos.x != -1) {
        if (mMouseDownModifier == KB_MOD_SHIFT) {
            if (mSelectionPos == -1)
                mSelectionPos = mCursorPos;
        } else
            mSelectionPos = -1;

        mCursorPos = position2CursorIndex(mMouseDownPos.x - (drawPos.x - pos.x), lastx);

        mMouseDownPos = ivec2(-1, -1);
    } else if (mMouseDragPos.x != -1) {
        if (mSelectionPos == -1)
            mSelectionPos = mCursorPos;

        mCursorPos = position2CursorIndex(mMouseDragPos.x - (drawPos.x - pos.x), lastx);
    } else {
        // set cursor to last character
        if (mCursorPos == -2)
            mCursorPos = metrics.numGlyphs;
    }

    if (mCursorPos == mSelectionPos)
        mSelectionPos = -1;
}

float gui_textfield::cursorIndex2Position(int index, float lastx) const {
    if (index >= 0 && index < metrics.numGlyphs)
        return metrics.glyphPositions[index].x;
    return lastx;
}

int gui_textfield::position2CursorIndex(float posx, float lastx) const {
    //posx += drawPos.x;
    int mCursorId = 0;
    float caretx  = 0;
    for (int j = 0; j < metrics.numGlyphs; j++) {
        float x = metrics.glyphPositions[j].x;
        if (math::abs(caretx - posx) > math::abs(x - posx)) {
            caretx = x;
            mCursorId = j;
        }
    }
    if (math::abs(caretx - posx) > math::abs(lastx - posx))
        mCursorId = metrics.numGlyphs;

    return mCursorId;
}

std::string gui_textfield::getEditValue() const {
    if (!mCommitted) {
        return utf::as_str8(mValueTemp);
    }
    return mValue;
}
