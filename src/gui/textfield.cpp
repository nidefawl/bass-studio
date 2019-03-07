#include "textfield.h"
#include <regex>
#include <sstream>
#include <iostream>
#include <nanovg.h>
#include "theme.h"
#include "platform.h"
#include "keyboard.h"
#include "leak_detect.h"


gui_textfield::gui_textfield()
    : guibase(),
      mEditable(true),
      mCommitted(true),
      mValue(""),
      mDefaultValue(""),
      mAlignment(Alignment::Left),
      mUnits(""),
      mFormat(""),
      mValidFormat(true),
      mValueTemp(""),
      mCursorPos(-1),
      mSelectionPos(-1),
      mMousePos(Vector2i(-1,-1)),
      mMouseDownPos(Vector2i(-1,-1)),
      mMouseDragPos(Vector2i(-1,-1)),
      mMouseDownModifier(0),
      mTextOffset(0),
      mLastClick(0), mVisible(true), mEnabled(true),
      mFocused(false),
	  mFontSize(28.0f),
	  mMouseFocus(false)
{
//    if (mTheme) mFontSize = mTheme->mTextBoxFontSize;
//    mIconExtraScale = 0.8f;// widget override

}

void gui_textfield::setEditable(bool editable) {
    mEditable = editable;
//    setCursor(editable ? Cursor::IBeam : Cursor::Arrow);
}


Vector2i gui_textfield::preferredSize(NVGcontext *ctx) const {
	int iH = (int32_t)ceil(fontSize() * 1.4f);
    Vector2i size(0, iH);

    float uw = 0;
    if (!mUnits.empty()) {
        uw = nvgTextBounds(ctx, 0, 0, mUnits.c_str(), nullptr, nullptr);
    }
    float sw = 0;

    float ts = nvgTextBounds(ctx, 0, 0, mValue.c_str(), nullptr, nullptr);
    size[0] = size[1] + ts + uw + sw;
    return size;
}

bool gui_textfield::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    mMousePos = mpos;
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
    if (mEditable && focused()) {
    }
    if (evt.button == 1 && !mFocused) {
//        if (!mSpinnable || spinArea(p) == SpinArea::None) /* not on scrolling arrows */
//            requestFocus();
    }
    ivec2 local = evt.relMousepos;
    if (mEditable) {
        mMouseDownPos = local;
        mMouseDownModifier = evt.kbmods;

        double time = getTimeMillis()/1000.0;
        if (time - mLastClick < 0.25) {
            /* Double-click: select all text */
            mSelectionPos = 0;
            mCursorPos = (int) mValueTemp.size();
            mMouseDownPos = Vector2i(-1, -1);
        }
        mLastClick = time;
    }
}
void gui_textfield::handleDraggedMove(MouseEvent& evt) {
    mMousePos = evt.relMousepos;
    mMouseDragPos = evt.relMousepos;

    if (mEditable && mFocused) {
    }
}
void gui_textfield::handleDraggedRelease(MouseEvent& evt) {
    mMouseDownPos = Vector2i(-1, -1);
    mMouseDragPos = Vector2i(-1, -1);
}
void setTfFont(NVGcontext* ctx, const gui_textfield* tf) {
	nvgFontSize(ctx, tf->fontSize());
	nvgFontFace(ctx, "sans");
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
void gui_textfield::render(NVGcontext* ctx) {

	//TODO: find a better place to do this: font metrics require nano-vg context but should be calculated in drag/move on the fly
	setTfFont(ctx, this);
	updateTextLayout(ctx);
    if (!mCommitted) {
        updateCursor(ctx, metrics.textBounds[2]);
        updateShiftCursorVisible();
#if 0
#endif
    }
	this->renderTextField(ctx);
}
#define X_SPACING (size.y * 0.3f)
void gui_textfield::updateTextLayout(NVGcontext* ctx) {

	nvgTextBounds(ctx, 0, 0, mValueTemp.c_str(), nullptr, metrics.textBounds);
	metrics.lineH = metrics.textBounds[3] - metrics.textBounds[1];

	// find cursor positions
	metrics.numGlyphs = nvgTextGlyphPositions(ctx, 0, 0, mValueTemp.c_str(), nullptr, metrics.glyphPositions, MAX_CHARS);

	Vector2i insetPos(pos.x, pos.y + size.y * 0.5f + 1);


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

	drawPos = insetPos;
	clipPos = {pos.x + X_SPACING + leftInset - 1.0f, pos.y + 1.0f};
	clipSize = {size.x - unitWidth - leftInset - 2 * X_SPACING + 2.0f, size.y - 3.0f};
}
void gui_textfield::renderTextField(NVGcontext* ctx) const {

	if (size.x * size.y < 10)
		return;
	renderWidgetBorder(ctx);

	nvgBeginPath(ctx);
	nvgRoundedRect(ctx, pos.x + 1, pos.y + 1 + 1.0f, size.x - 2, size.y - 2, 3);

	if (mEditable && mFocused)
		nvgFillColor(ctx, mValidFormat ? g_guiColors[COL_BG_DRK_FOCUSED] : nvgRGBA(200, 90, 90, 255));
	else
		nvgFillColor(ctx, g_guiColors[COL_BG_DRK]);

	nvgFill(ctx);


	// clip visible text area
	if (clipSize.x < 1 || clipSize.y < 1)
		return;
	nvgFontSize(ctx, fontSize());
	nvgFontFace(ctx, "sans");
	if (!mUnits.empty()) {
		nvgFillColor(ctx, GUI_COLORA(255, mEnabled ? 64 : 32));
		nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgText(ctx, pos.x + size.x - X_SPACING, drawPos.y, mUnits.c_str(), nullptr);
	}
	setTfFont(ctx, this);

	NVGcolor mTextColor = GUI_COLORA(5, 160);
	NVGcolor mDisabledTextColor = GUI_COLORA(255, 80);
	NVGcolor mColor = mEnabled && (!mCommitted || !mValue.empty()) ? mTextColor : mDisabledTextColor;


	nvgFillColor(ctx, mColor);

	nvgSave(ctx);
	nvgIntersectScissor(ctx, clipPos.x, clipPos.y, clipSize.x, clipSize.y);

	if (mCommitted) {
		nvgText(ctx, drawPos.x, drawPos.y, mValue.empty() ? mPlaceholder.c_str() : mValue.c_str(), nullptr);
	} else {

		nvgTranslate(ctx, drawPos.x + mTextOffset, drawPos.y);

		// draw text with offset
		nvgText(ctx, 0, 0, mValueTemp.c_str(), nullptr);
//
        if (mCursorPos > -1) {
            float lineh = metrics.lineH;
            float caretx = cursorIndex2Position(mCursorPos, metrics.textBounds[2]);
            if (mSelectionPos > -1) {
                float selx = cursorIndex2Position(mSelectionPos, metrics.textBounds[2]);

                if (caretx > selx)
                    std::swap(caretx, selx);

				// draw selection
				nvgBeginPath(ctx);
				nvgFillColor(ctx, nvgRGBA(255, 255, 255, 80));
				nvgRect(ctx, caretx,  - lineh * 0.5f, selx - caretx, lineh);
				nvgFill(ctx);
            }


            // draw cursor
            nvgBeginPath(ctx);
            nvgMoveTo(ctx, caretx,  - lineh * 0.5f);
            nvgLineTo(ctx, caretx,  + lineh * 0.5f);
            nvgStrokeColor(ctx, nvgRGBA(255, 192, 0, 255));
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
        }
	}
	nvgRestore(ctx);
}

void gui_textfield::beginEdit() {
    mValueTemp = mValue;
    mCommitted = false;
    mCursorPos = 0;
    mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
}
void gui_textfield::endEdit() {
    mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
    if (mValidFormat) {
        if (mValueTemp == "")
            mValue = mDefaultValue;
        else
            mValue = mValueTemp;
    }

//            if (mCallback && !mCallback(mValue))
//                mValue = backup;

    mValidFormat = true;
    mCommitted = true;
    mCursorPos = -1;
    mSelectionPos = -1;
    mTextOffset = 0;
}
bool gui_textfield::focusEvent(MouseHitEvt& evt, bool focused) {
//    Widget::focusEvent(focused);
	my_printf("focusEvent %d %d\n", static_cast<int>(evt.type), focused);
    std::string backup = mValue;
    mFocused = focused;
    if (mEditable) {
        if (focused) {
            beginEdit();
        } else {
        	endEdit();
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
                        mValueTemp.erase(mValueTemp.begin() + mCursorPos - 1);
                        mCursorPos--;
                    }
                }
            } else if (key == KEY_DELETE) {
                if (!deleteSelection()) {
                    if (mCursorPos < (int) mValueTemp.length())
                        mValueTemp.erase(mValueTemp.begin() + mCursorPos);
                }
            } else if (key == KEY_ENTER) {
//                if (!mCommitted)
//                    focusEvent(false);
            } else if (key == KEY_A && isCtrl(modifiers)) {
                mCursorPos = (int) mValueTemp.length();
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
		std::ostringstream convert;
		convert << (char) codepoint;
		if (mCursorPos > -1) {
			deleteSelection();
			mValueTemp.insert(mCursorPos, convert.str());
			mCursorPos++;
			onChange();
		}
		return true;
	}

	return false;
}
void gui_textfield::onChange() {
    mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
    if (mValidFormat && mCallback && !mCallback(mValueTemp)){

    }
}
bool gui_textfield::checkFormat(const std::string &input, const std::string &format) {
    if (format.empty())
        return true;
    try {
        std::regex regex(format);
        return regex_match(input, regex);
    } catch (const std::regex_error &) {
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
        std::cerr << "Warning: cannot validate text field due to lacking regular expression support. please compile with GCC >= 4.9" << std::endl;
        return true;
#else
        throw;
#endif
    }
}

bool gui_textfield::copySelectionString(std::string& output) {
	if (mSelectionPos > -1) {
		int begin = mCursorPos;
		int end = mSelectionPos;

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
		int end = mSelectionPos;

		if (begin > end)
			std::swap(begin, end);
		if ((int)mValueTemp.length() >= end-begin)
		parentCtrl->setClipboardText(mValueTemp.substr(begin, end).c_str());
		onChange(); //=??????
		return true;
	}

    return false;
}

void gui_textfield::pasteFromClipboard() {
	if (mCursorPos >= 0 && mCursorPos <= (int)mValueTemp.size()) {
		String str = std::string(parentCtrl->getClipboardText());
		mValueTemp.insert(mCursorPos, str);
		mCursorPos += str.length();
		onChange();
	}
}

bool gui_textfield::deleteSelection() {
    if (mSelectionPos > -1) {
        int begin = mCursorPos;
        int end = mSelectionPos;

        if (begin > end)
            std::swap(begin, end);
        if (mValueTemp.empty()) return false;
        assert(!mValueTemp.empty());
        if (begin == end - 1)
            mValueTemp.erase(mValueTemp.begin() + begin);
        else
            mValueTemp.erase(mValueTemp.begin() + begin,
                             mValueTemp.begin() + end);

        mCursorPos = begin;
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

    mTextOffset = 0;
	if (nextCX > clipSize.x)
		mTextOffset -= nextCX - clipSize.x + 1;
	if (prevCX < 0)
		mTextOffset += 0 - prevCX + 1;
}
void gui_textfield::updateCursor(NVGcontext *, float lastx) {
	// handle mouse cursor events
	if (mMouseDownPos.x != -1) {
		if (mMouseDownModifier == KB_MOD_SHIFT) {
			if (mSelectionPos == -1)
				mSelectionPos = mCursorPos;
		} else
			mSelectionPos = -1;

		mCursorPos = position2CursorIndex(mMouseDownPos.x, lastx);

		mMouseDownPos = Vector2i(-1, -1);
	} else if (mMouseDragPos.x != -1) {
		if (mSelectionPos == -1)
			mSelectionPos = mCursorPos;

		mCursorPos = position2CursorIndex(mMouseDragPos.x, lastx);
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
    if (index == metrics.numGlyphs)
        pos = lastx; // last character
    else
        pos = metrics.glyphPositions[index].x;

    return pos;
}

int gui_textfield::position2CursorIndex(float posx, float lastx) const {
    int mCursorId = 0;
    float caretx = metrics.glyphPositions[mCursorId].x;
    for (int j = 1; j < metrics.numGlyphs; j++) {
        if (std::abs(caretx - posx) > std::abs(metrics.glyphPositions[j].x - posx)) {
            mCursorId = j;
            caretx = metrics.glyphPositions[mCursorId].x;
        }
    }
    if (std::abs(caretx - posx) > std::abs(lastx - posx))
        mCursorId = metrics.numGlyphs;

    return mCursorId;
}
