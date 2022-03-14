#include "textfield.h"
#include <sstream>
#include <iostream>
#include <nanovg.h>

#include "theme.h"
#include "str_util.h"
#include "gui.h"
#include "basectrl.h"
#include "guicolors.h"
#include "platform.h"
#include "keyboard.h"
#include "guifonts.h"

#define TEXTFIELD_USE_REGEX_PATTERN 0
#if TEXTFIELD_USE_REGEX_PATTERN
#include <regex>
#endif

void gui_textfield::setEditable(bool editable) {
    mEditable = editable;
}

ivec2 gui_textfield::preferredSize(NVGcontext* ctx) const {
    int iH = (int32_t) ceil(fontSize() * 1.4f);
    ivec2 size(0, iH);

    float uw = 0;
    if (!mUnits.empty()) {
        uw = nvgTextBounds(ctx, 0, 0, mUnits.c_str(), nullptr, nullptr);
    }
    float sw = 0;

    float ts = nvgTextBounds(ctx, 0, 0, mValue.c_str(), nullptr, nullptr);
    size[0]  = size[1] + ts + uw + sw;
    return size;
}

bool gui_textfield::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (contains(mpos)) {
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
    if (mEditable) {
        mMouseDownPos      = local;
        mMouseDownModifier = evt.kbmods;

        //TODO: handle double click detection consistently
        auto tmNow = getTimeMillis();
        if (tmNow - m_tmLastClick <= 250) {
            /* Double-click: select all text */
            mSelectionPos = 0;
            mCursorPos    = (int) mValueTemp.size();
            mMouseDownPos = ivec2(-1, -1);
        }
        m_tmLastClick = tmNow;
    }
}
void gui_textfield::handleDraggedMove(MouseEvent& evt) {
    mMouseDragPos     = evt.relMousepos;
    this->mTextOffset = -123123;
    if (mEditable && mFocused) {
    }
}
void gui_textfield::handleDraggedRelease(MouseEvent& evt) {
    mMouseDownPos = ivec2(-1, -1);
    mMouseDragPos = ivec2(-1, -1);
}
void gui_textfield::onTextChange() {
    if (mCallback && !mCallback(mValueTemp)) {
    }
}
void gui_textfield::onTextEndEdit() {
    if (mCallbackEnd && !mCallbackEnd(mValueTemp)) {
    }
}
void setTfFont(NVGcontext* ctx, const gui_textfield* tf) {
    nvgFontSize(ctx, tf->fontSize());
    UIFont::font_instance instance = tf->theme->getFont(UIFont::FONT_TEXFIELD);
    UIFont::bindFont(ctx, instance);
    switch (tf->alignment()) {
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
}
void gui_textfield::layout() {
    if (this->mFontSize < 0) {
        this->mFontSize = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
    }
}
void gui_textfield::render(NVGcontext* ctx) {

    //TODO: find a better place to do this: font metrics require nano-vg context but should be calculated in drag/move on the fly
    setTfFont(ctx, this);
    updateTextLayout(ctx);
    if (!mCommitted) {
        updateCursor(ctx, metrics.textBounds[2]);
        updateShiftCursorVisible();
    }
    this->renderTextField(ctx);
}
#define X_SPACING (size.y * 0.3f)
void gui_textfield::updateTextLayout(NVGcontext* ctx) {

    nvgTextBounds(ctx, 0, 0, mValueTemp.c_str(), nullptr, metrics.textBounds);
    metrics.lineH = metrics.textBounds[3] - metrics.textBounds[1];

    // find cursor positions
    if (metrics.glyphPositions.size() < mValueTemp.length()) {
        metrics.glyphPositions.resize(mValueTemp.length());
    }
    metrics.numGlyphs = nvgTextGlyphPositions(ctx, 0, 0, mValueTemp.c_str(), nullptr, metrics.glyphPositions.data(), metrics.glyphPositions.size());
    metrics.numGlyphs = math::min<int>(metrics.numGlyphs, metrics.glyphPositions.size());
    
    ivec2 insetPos(pos.x, pos.y + size.y * 0.5f + 1);


    float unitWidth = 0;

    if (!mUnits.empty()) {
        unitWidth = nvgTextBounds(ctx, 0, 0, mUnits.c_str(), nullptr, nullptr) + 2;
    }

    float leftInset = 0.f;
    switch (mAlignment) {
        case Alignment::Left:
            insetPos.x += X_SPACING + leftInset;
            break;
        case Alignment::Right:
            insetPos.x += size.x - unitWidth - X_SPACING;
            break;
        case Alignment::Center:
            insetPos.x += size.x * 0.5f;
            break;
    }

    drawPos  = insetPos;
    clipPos  = { pos.x + X_SPACING + leftInset - 1.0f, pos.y + 1.0f };
    clipSize = { size.x - unitWidth - leftInset - 2 * X_SPACING + 2.0f, size.y - 3.0f };
}
void gui_textfield::renderTextField(NVGcontext* ctx) const {

    if (size.x * size.y < 10)
        return;
    int32_t fl = getStateFlags();
    renderWidgetBorder(ctx, fl);

    // nvgBeginPath(ctx);
    // nvgRect(ctx, pos.x + 1, pos.y + 1 + 1.0f, size.x - 2, size.y - 2);
    // if (mEditable && mFocused)
    //     nvgFillColor(ctx, theme->getColor(mValidFormat ? GuiColor::COL_BG_DRK_FOCUSED : GuiColor::COL_INVALID_INPUT));
    // else
    //     nvgFillColor(ctx, theme->getColor(GuiColor::COL_BG_DRK));

    // nvgFill(ctx);


    // clip visible text area
    if (clipSize.x < 1 || clipSize.y < 1)
        return;
    nvgFontSize(ctx, fontSize());
    NVGcolor mTextColorDisabled = theme->getColor(GuiColor::COL_TEXTBOX_TEXT_DISABLED);
    UIFont::font_instance instance = theme->getFont(UIFont::FONT_TEXFIELD);
    UIFont::bindFont(ctx, instance);
    if (!mUnits.empty()) {
        nvgFillColor(ctx, mTextColorDisabled);
        setTfFont(ctx, this);
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, pos.x + size.x - X_SPACING, drawPos.y, mUnits.c_str(), nullptr);
    }

    NVGcolor mTextColor         = theme->getColor(GuiColor::COL_TEXTBOX_TEXT);
    NVGcolor mTextColorMarked   = theme->getColor(GuiColor::COL_TEXTBOX_TEXT_MARKED);

    NVGcolor mColor = isEnabled() && (!mCommitted || !mValue.empty()) ? mTextColor : mTextColorDisabled;


    nvgSave(ctx);
    nvgIntersectScissor(ctx, clipPos.x, clipPos.y, clipSize.x, clipSize.y);

    if (mCommitted) {
        nvgFillColor(ctx, mColor);
        setTfFont(ctx, this);
        nvgText(ctx, drawPos.x, drawPos.y, mValue.empty() ? mPlaceholder.c_str() : mValue.c_str(), nullptr);
    } else {

        nvgTranslate(ctx, drawPos.x + mTextOffset, drawPos.y);
        //
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
        setTfFont(ctx, this);
        // draw text with offset
        nvgText(ctx, 0, 0, mValueTemp.c_str(), nullptr);
    }
    nvgRestore(ctx);
}

void gui_textfield::beginEdit() {
    mValueTemp = mValue;
    mCommitted = false;
    int begin  = mCursorPos;
    int end    = mSelectionPos;

    if (begin > end)
        std::swap(begin, end);
    if (begin < 0 || end > mValue.length()) {
        mCursorPos    = 0;
        mSelectionPos = -1;
    }
    mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
}
void gui_textfield::endEdit(bool success) {
    if (success) {
        mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
        if (mValidFormat) {
            if (mValueTemp == "")
                mValue = mDefaultValue;
            else
                mValue = mValueTemp;
        }
        onTextEndEdit();
    } else {
        mValueTemp = mValue;
    }

    mValidFormat  = true;
    mCommitted    = true;
    mCursorPos    = -1;
    mSelectionPos = -1;
    mTextOffset   = 0;
}
bool gui_textfield::focusEvent(MouseHitEvt& evt, bool focused) {
    //    Widget::focusEvent(focused);
    std::string backup = mValue;
    mFocused           = focused;
    if (fnFocus) {
        fnFocus(evt, focused);
    }
    if (mEditable) {
        if (focused) {
            beginEdit();
        } else {
            endEdit(!mCommitted);
        }
    }

    return true;
}

bool gui_textfield::keyboardEvent(int key, int /* scancode */, KeyEventType action, int modifiers) {
    if (mEditable && mFocused) {
        if (action == KeyEventType::K_PRESS || action == KeyEventType::K_REPEAT) {
            if (key == KEY_LEFT) {
                if (modifiers == KB_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                if (mCursorPos > 0)
                    mCursorPos--;
            } else if (key == KEY_RIGHT) {
                if (modifiers == KB_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                if (mCursorPos < (int) mValueTemp.length())
                    mCursorPos++;
            } else if (key == KEY_HOME) {
                if (modifiers == KB_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                mCursorPos = 0;
            } else if (key == KEY_END) {
                if (modifiers == KB_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                mCursorPos = (int) mValueTemp.size();
            } else if (key == KEY_BACKSPACE) {
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
            } else if (key == KEY_DELETE) {
                if (!deleteSelection()) {
                    if (filter && filter->isReplaceInput()) {
                    } else {
                        if (mCursorPos < (int) mValueTemp.length())
                            mValueTemp.erase(mValueTemp.begin() + mCursorPos);
                    }
                }
            } else if (key == KEY_ESCAPE) {
                if (!mCommitted)
                    endEdit(false);
            } else if (mReturnCommits && (key == KEY_ENTER || key == KEY_KP_ENTER)) {
                if (!mCommitted)
                    endEdit(true);
                else
                    beginEdit();
            } else if (key == KEY_A && isCtrl(modifiers)) {
                mCursorPos    = (int) mValueTemp.length();
                mSelectionPos = 0;
            } else if (key == KEY_X && isCtrl(modifiers)) {
                copySelection();
                deleteSelection();
            } else if (key == KEY_C && isCtrl(modifiers)) {
                copySelection();
            } else if (key == KEY_V && isCtrl(modifiers)) {
                deleteSelection();
                pasteFromClipboard();
            }
            onChange();
        }

        return true;
    }

    return false;
}

bool gui_textfield::handleCharInput(unsigned int codepoint) {
    if (mEditable && mFocused) {
        if (filter) {
            if (!filter->isAllowedChar(codepoint)) {
                return true;
            }
        }
        std::ostringstream convert;
        convert << (char) codepoint;
        if (mCursorPos > -1) {
            auto cvstr = convert.str();
            if (!cvstr.length()) {
                return true;
            }
            auto cvchar = cvstr[0];
            if (filter && filter->isReplaceInput()) {
                int32_t len       = 1;
                int32_t mincursor = mCursorPos;
                if (mSelectionPos > -1) {
                    mincursor = math::min(mSelectionPos, mCursorPos);
                    len       = math::max(mSelectionPos, mCursorPos) - mincursor;
                }
                for (int i = 0; i < len; i++) {
                    mValueTemp[i + mincursor] = cvchar;
                }
                if (mCursorPos > mValueTemp.length() - 1) {
                    mCursorPos = mValueTemp.length() - 1;
                }

                if (mSelectionPos < 0) {
                    mCursorPos++;
                    if (mCursorPos == mValueTemp.length()) {
                        mCursorPos = 2;
                    }
                }
            } else {
                deleteSelection();
                mValueTemp.insert(mCursorPos, cvstr);
                mCursorPos++;
            }
            onChange();
        }
        return true;
    }

    return false;
}
void gui_textfield::onChange() {
    mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
    if (filter) {
        mValueTemp = filter->parse(mValueTemp);
        if (mCursorPos > mValueTemp.length()) {
            mCursorPos = mValueTemp.length();
        }
    }
    onTextChange();
}
bool gui_textfield::checkFormat(const std::string& input, const std::string& format) {
#if TEXTFIELD_USE_REGEX_PATTERN
    if (format.empty())
        return true;
    try {
        std::regex regex(format);
        return regex_match(input, regex);
    } catch (const std::regex_error&) {
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
        std::cerr << "Warning: cannot validate text field due to lacking regular expression support. please compile with GCC >= 4.9" << std::endl;
        return true;
#else
        throw;
#endif
    }
#else
    return true;
#endif
}

bool gui_textfield::copySelectionString(std::string& output) {
    if (mSelectionPos > -1) {
        int begin = mCursorPos;
        int end   = mSelectionPos;

        if (begin > end)
            std::swap(begin, end);
        if ((int) mValueTemp.length() >= end - begin)
            output = mValueTemp.substr(begin, end).c_str();
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
        if ((int) mValueTemp.length() >= end - begin)
            parentCtrl->setClipboardText(mValueTemp.substr(begin, end).c_str());
        onChange();//=??????
        return true;
    }

    return false;
}

void gui_textfield::pasteFromClipboard() {
    if (mCursorPos >= 0 && mCursorPos <= (int) mValueTemp.size()) {
        String str = parentCtrl->getClipboardText();
        mValueTemp.insert(mCursorPos, str);
        mCursorPos += str.length();
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
    // compute text offset
    int prevCPos = mCursorPos > 0 ? mCursorPos - 1 : 0;
    int nextCPos = mCursorPos < metrics.numGlyphs ? mCursorPos + 1 : metrics.numGlyphs;
    float prevCX = cursorIndex2Position(prevCPos, metrics.textBounds[2]);
    float nextCX = cursorIndex2Position(nextCPos, metrics.textBounds[2]);

    switch (mAlignment) {
        case Alignment::Left:
            nextCX = nextCX + drawPos.x - clipPos.x;
            prevCX = prevCX + drawPos.x - clipPos.x;
            break;
        case Alignment::Center:
            nextCX = nextCX + drawPos.x - clipPos.x;
            prevCX = prevCX + drawPos.x - clipPos.x;
            break;
        case Alignment::Right:
            nextCX = nextCX + drawPos.x - clipPos.x;
            prevCX = prevCX + drawPos.x - clipPos.x;
            break;
    }
    float mTextOffset = 0;
    if (nextCX > clipSize.x)
        mTextOffset -= nextCX - clipSize.x + 1;
    if (prevCX < 0)
        mTextOffset += 0 - prevCX + 1;
    //    if (this->mTextOffset != mTextOffset) {
    //        if (nextCX > clipSize.x)
    //            log_printf("nextCX > clipSize.x %f > %f\n", nextCX, clipSize.x);
    //        if (prevCX < 0)
    //            log_printf("prevCX < 0 %f\n", prevCX);
    //        log_printf("prevCPos %d\n", prevCPos);
    //        log_printf("metrics.textBounds[2] %f\n", metrics.textBounds[2]);
    //        log_printf("pos.x %f\n", (float) pos.x);
    //        log_printf("drawPos.x %f\n", (float) drawPos.x);
    //        log_printf("drawPos.x-pos.x %f\n", (float) (drawPos.x - pos.x));
    //        log_printf("clipPos.x %f\n", (float) clipPos.x);
    //    }
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
    float pos = 0;
    if (index >= metrics.numGlyphs)
        pos = lastx;// last character
    else
        pos = metrics.glyphPositions[index].x;

    return pos;
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
