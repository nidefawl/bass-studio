#include "textfield.h"
#include <regex>
#include <sstream>
#include <iostream>
#include <nanovg.h>
#include "theme.h"
#include "leak_detect.h"


gui_textfield::gui_textfield()
    : guibase(),
      mEditable(true),
      mSpinnable(false),
      mCommitted(true),
      mValue(""),
      mDefaultValue(""),
      mAlignment(Alignment::Left),
      mUnits(""),
      mFormat(""),
      mUnitsImage(-1),
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
      mFocused(false), mFontSize(28.0f), mMouseFocus(false) {
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
    if (mUnitsImage > 0) {
        int w, h;
        nvgImageSize(ctx, mUnitsImage, &w, &h);
        float uh = size[1] * 0.4f;
        uw = w * uh / h;
    } else if (!mUnits.empty()) {
        uw = nvgTextBounds(ctx, 0, 0, mUnits.c_str(), nullptr, nullptr);
    }
    float sw = 0;
    if (mSpinnable) {
        sw = 14.f;
    }

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

        double time = glfwGetTime();
        if (time - mLastClick < 0.25) {
            /* Double-click: select all text */
            mSelectionPos = 0;
            mCursorPos = (int) mValueTemp.size();
            mMouseDownPos = Vector2i(-1, -1);
        }
        mLastClick = time;
    } else if (mSpinnable && !mFocused) {
        if (spinArea(local) == SpinArea::None) {
            mMouseDownPos = local;
            mMouseDownModifier = evt.kbmods;

            double time = glfwGetTime();
            if (time - mLastClick < 0.25) {
                /* Double-click: reset to default value */
                mValue = mDefaultValue;
                if (mCallback)
                    mCallback(mValue);

                mMouseDownPos = Vector2i(-1, -1);
            }
            mLastClick = time;
        }
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
void gui_textfield::render(NVGcontext* ctx) {
//    Widget::draw(ctx);

//    NVGpaint bg = nvgBoxGradient(ctx,
//        pos.x + 1, pos.y + 1 + 1.0f, size.x - 2, size.y - 2,
//        3, 4, GUI_COLORA(COL_BG_DRKER, 255), GUI_COLORA(32, 255));
//    NVGpaint fg1 = nvgBoxGradient(ctx,
//        pos.x + 1, pos.y + 1 + 1.0f, size.x - 2, size.y - 2,
//        3, 4, GUI_COLORA(150, 255), GUI_COLORA(32, 255));
//    NVGpaint fg2 = nvgBoxGradient(ctx,
//        pos.x + 1, pos.y + 1 + 1.0f, size.x - 2, size.y - 2,
//        3, 4, nvgRGBA(255, 0, 0, 255), nvgRGBA(255, 0, 0, 255));
	if (size.x*size.y < 10)
		return;
    renderWidgetBorder(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, pos.x + 1, pos.y + 1 + 1.0f, size.x - 2,
                   size.y - 2, 3);

    if (mEditable && mFocused)
    	nvgFillColor(ctx, mValidFormat ? g_guiColors[COL_BG_DRK_FOCUSED] : nvgRGBA(200, 90, 90, 255));
    else if (mSpinnable && mMouseDownPos.x != -1)
        nvgFillColor(ctx, g_guiColors[COL_BG_DRK_FOCUSED]);
    else
    	nvgFillColor(ctx, g_guiColors[COL_BG_DRK]);

    nvgFill(ctx);

//    nvgBeginPath(ctx);
//    nvgRoundedRect(ctx, pos.x + 0.5f, pos.y + 0.5f, size.x - 1,
//                   size.y - 1, 2.5f);
//    nvgStrokeColor(ctx, GUI_COLORA(0, 48));
//    nvgStroke(ctx);

    nvgFontSize(ctx, fontSize());
    nvgFontFace(ctx, "sans");
    Vector2i drawPos(pos.x, pos.y + size.y * 0.5f + 1);

    float xSpacing = size.y * 0.3f;

    float unitWidth = 0;

    if (mUnitsImage > 0) {
        int w, h;
        nvgImageSize(ctx, mUnitsImage, &w, &h);
        float unitHeight = size.y * 0.4f;
        unitWidth = w * unitHeight / h;
        NVGpaint imgPaint = nvgImagePattern(
            ctx, pos.x + size.x - xSpacing - unitWidth,
            drawPos.y - unitHeight * 0.5f, unitWidth, unitHeight, 0,
            mUnitsImage, mEnabled ? 0.7f : 0.35f);
        nvgBeginPath(ctx);
        nvgRect(ctx, pos.x + size.x - xSpacing - unitWidth,
                drawPos.y - unitHeight * 0.5f, unitWidth, unitHeight);
        nvgFillPaint(ctx, imgPaint);
        nvgFill(ctx);
        unitWidth += 2;
    } else if (!mUnits.empty()) {
        unitWidth = nvgTextBounds(ctx, 0, 0, mUnits.c_str(), nullptr, nullptr);
        nvgFillColor(ctx, GUI_COLORA(255, mEnabled ? 64 : 32));
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, pos.x + size.x - xSpacing, drawPos.y,
                mUnits.c_str(), nullptr);
        unitWidth += 2;
    }

    float spinArrowsWidth = 0.f;

    if (mSpinnable && !focused()) {
        spinArrowsWidth = 14.f;

        nvgFontFace(ctx, "icons");
        nvgFontSize(ctx, mFontSize);

//        bool spinning = mMouseDownPos.x != -1;
//
//        /* up button */ {
//            bool hover = mMouseFocus && spinArea(mMousePos) == SpinArea::Top;
//            nvgFillColor(ctx, (mEnabled && (hover || spinning)) ? mTheme->mTextColor : mTheme->mDisabledTextColor);
//            auto icon = utf8(mTheme->mTextBoxUpIcon);
//            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
//            vec2 iconPos(pos.x + 4.f,
//                             pos.y + size.y/2.f - xSpacing/2.f);
//            nvgText(ctx, iconPos.x, iconPos.y, icon.data(), nullptr);
//        }
//
//        /* down button */ {
//            bool hover = mMouseFocus && spinArea(mMousePos) == SpinArea::Bottom;
//            nvgFillColor(ctx, (mEnabled && (hover || spinning)) ? mTheme->mTextColor : mTheme->mDisabledTextColor);
//            auto icon = utf8(mTheme->mTextBoxDownIcon);
//            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
//            vec2 iconPos(pos.x + 4.f,
//                             pos.y + size.y/2.f + xSpacing/2.f + 1.5f);
//            nvgText(ctx, iconPos.x, iconPos.y, icon.data(), nullptr);
//        }

        nvgFontSize(ctx, fontSize());
        nvgFontFace(ctx, "sans");
    }

    switch (mAlignment) {
        case Alignment::Left:
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            drawPos.x += xSpacing + spinArrowsWidth;
            break;
        case Alignment::Right:
            nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            drawPos.x += size.x - unitWidth - xSpacing;
            break;
        case Alignment::Center:
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            drawPos.x += size.x * 0.5f;
            break;
    }

	nvgFontSize(ctx, fontSize());
	NVGcolor mTextColor = GUI_COLORA(5, 160);
	NVGcolor mDisabledTextColor = GUI_COLORA(255, 80);
    nvgFillColor(ctx, mEnabled && (!mCommitted || !mValue.empty()) ? mTextColor : mDisabledTextColor);

    // clip visible text area
    float clipX = pos.x + xSpacing + spinArrowsWidth - 1.0f;
    float clipY = pos.y + 1.0f;
    float clipWidth = size.x - unitWidth - spinArrowsWidth - 2 * xSpacing + 2.0f;
    float clipHeight = size.y - 3.0f;
    if (clipWidth < 1 || clipHeight < 1)
    	return;
    nvgSave(ctx);
    nvgIntersectScissor(ctx, clipX, clipY, clipWidth, clipHeight);

    Vector2i oldDrawPos(drawPos);
    drawPos.x += mTextOffset;

    if (mCommitted) {
        nvgText(ctx, drawPos.x, drawPos.y,
            mValue.empty() ? mPlaceholder.c_str() : mValue.c_str(), nullptr);
    } else {
        const int maxGlyphs = 1024;
        NVGglyphPosition glyphs[maxGlyphs];
        float textBound[4];
        nvgTextBounds(ctx, drawPos.x, drawPos.y, mValueTemp.c_str(),
                      nullptr, textBound);
        float lineh = textBound[3] - textBound[1];

        // find cursor positions
        int nglyphs =
            nvgTextGlyphPositions(ctx, drawPos.x, drawPos.y,
                                  mValueTemp.c_str(), nullptr, glyphs, maxGlyphs);
        updateCursor(ctx, textBound[2], glyphs, nglyphs);

        // compute text offset
        int prevCPos = mCursorPos > 0 ? mCursorPos - 1 : 0;
        int nextCPos = mCursorPos < nglyphs ? mCursorPos + 1 : nglyphs;
        float prevCX = cursorIndex2Position(prevCPos, textBound[2], glyphs, nglyphs);
        float nextCX = cursorIndex2Position(nextCPos, textBound[2], glyphs, nglyphs);

        if (nextCX > clipX + clipWidth)
            mTextOffset -= nextCX - (clipX + clipWidth) + 1;
        if (prevCX < clipX)
            mTextOffset += clipX - prevCX + 1;

        drawPos.x = oldDrawPos.x + mTextOffset;

        // draw text with offset
        nvgText(ctx, drawPos.x, drawPos.y, mValueTemp.c_str(), nullptr);
        nvgTextBounds(ctx, drawPos.x, drawPos.y, mValueTemp.c_str(),
                      nullptr, textBound);

        // recompute cursor positions
        nglyphs = nvgTextGlyphPositions(ctx, drawPos.x, drawPos.y,
                mValueTemp.c_str(), nullptr, glyphs, maxGlyphs);

        if (mCursorPos > -1) {
            if (mSelectionPos > -1) {
                float caretx = cursorIndex2Position(mCursorPos, textBound[2],
                                                    glyphs, nglyphs);
                float selx = cursorIndex2Position(mSelectionPos, textBound[2],
                                                  glyphs, nglyphs);

                if (caretx > selx)
                    std::swap(caretx, selx);

                // draw selection
                nvgBeginPath(ctx);
                nvgFillColor(ctx, nvgRGBA(255, 255, 255, 80));
                nvgRect(ctx, caretx, drawPos.y - lineh * 0.5f, selx - caretx,
                        lineh);
                nvgFill(ctx);
            }

            float caretx = cursorIndex2Position(mCursorPos, textBound[2], glyphs, nglyphs);

            // draw cursor
            nvgBeginPath(ctx);
            nvgMoveTo(ctx, caretx, drawPos.y - lineh * 0.5f);
            nvgLineTo(ctx, caretx, drawPos.y + lineh * 0.5f);
            nvgStrokeColor(ctx, nvgRGBA(255, 192, 0, 255));
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
        }
    }
    nvgRestore(ctx);
}

bool gui_textfield::focusEvent(bool focused) {
//    Widget::focusEvent(focused);

    std::string backup = mValue;
    mFocused = focused;
    if (mEditable) {
        if (focused) {
            mValueTemp = mValue;
            mCommitted = false;
            mCursorPos = 0;
        } else {
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

        mValidFormat = (mValueTemp == "") || checkFormat(mValueTemp, mFormat);
    }

    return true;
}

bool gui_textfield::keyboardEvent(int key, int /* scancode */, KeyEventType action, int modifiers) {
    if (mEditable && mFocused) {
        if (action == KeyEventType::K_PRESS || action == KeyEventType::K_REPEAT) {
            if (key == GLFW_KEY_LEFT) {
                if (modifiers == GLFW_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                if (mCursorPos > 0)
                    mCursorPos--;
            } else if (key == GLFW_KEY_RIGHT) {
                if (modifiers == GLFW_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                if (mCursorPos < (int) mValueTemp.length())
                    mCursorPos++;
            } else if (key == GLFW_KEY_HOME) {
                if (modifiers == GLFW_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                mCursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                if (modifiers == GLFW_MOD_SHIFT) {
                    if (mSelectionPos == -1)
                        mSelectionPos = mCursorPos;
                } else {
                    mSelectionPos = -1;
                }

                mCursorPos = (int) mValueTemp.size();
            } else if (key == GLFW_KEY_BACKSPACE) {
                if (!deleteSelection()) {
                    if (mCursorPos > 0) {
                        mValueTemp.erase(mValueTemp.begin() + mCursorPos - 1);
                        mCursorPos--;
                    }
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (!deleteSelection()) {
                    if (mCursorPos < (int) mValueTemp.length())
                        mValueTemp.erase(mValueTemp.begin() + mCursorPos);
                }
            } else if (key == GLFW_KEY_ENTER) {
//                if (!mCommitted)
//                    focusEvent(false);
            } else if (key == GLFW_KEY_A && isCtrl(modifiers)) {
                mCursorPos = (int) mValueTemp.length();
                mSelectionPos = 0;
            } else if (key == GLFW_KEY_X && isCtrl(modifiers)) {
                copySelection();
                deleteSelection();
            } else if (key == GLFW_KEY_C && isCtrl(modifiers)) {
                copySelection();
            } else if (key == GLFW_KEY_V && isCtrl(modifiers)) {
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

bool gui_textfield::copySelection() {
	if (mSelectionPos > -1) {
		int begin = mCursorPos;
		int end = mSelectionPos;

		if (begin > end)
			std::swap(begin, end);
		if (mValueTemp.length() >= end-begin)
		MainCtrl::get()->setClipboardText(mValueTemp.substr(begin, end).c_str());
		onChange();
		return true;
	}

    return false;
}

void gui_textfield::pasteFromClipboard() {
	if (mCursorPos >= 0 && mCursorPos <= (int)mValueTemp.size()) {
		String str = std::string(MainCtrl::get()->getClipboardText());
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

void gui_textfield::updateCursor(NVGcontext *, float lastx,
                           const NVGglyphPosition *glyphs, int size) {
    // handle mouse cursor events
    if (mMouseDownPos.x != -1) {
        if (mMouseDownModifier == GLFW_MOD_SHIFT) {
            if (mSelectionPos == -1)
                mSelectionPos = mCursorPos;
        } else
            mSelectionPos = -1;

        mCursorPos =
            position2CursorIndex(mMouseDownPos.x, lastx, glyphs, size);

        mMouseDownPos = Vector2i(-1, -1);
    } else if (mMouseDragPos.x != -1) {
        if (mSelectionPos == -1)
            mSelectionPos = mCursorPos;

        mCursorPos =
            position2CursorIndex(mMouseDragPos.x, lastx, glyphs, size);
    } else {
        // set cursor to last character
        if (mCursorPos == -2)
            mCursorPos = size;
    }

    if (mCursorPos == mSelectionPos)
        mSelectionPos = -1;
}

float gui_textfield::cursorIndex2Position(int index, float lastx,
                                    const NVGglyphPosition *glyphs, int size) {
    float pos = 0;
    if (index == size)
        pos = lastx; // last character
    else
        pos = glyphs[index].x;

    return pos;
}

int gui_textfield::position2CursorIndex(float posx, float lastx,
                                  const NVGglyphPosition *glyphs, int size) {
    int mCursorId = 0;
    float caretx = glyphs[mCursorId].x;
    for (int j = 1; j < size; j++) {
        if (std::abs(caretx - posx) > std::abs(glyphs[j].x - posx)) {
            mCursorId = j;
            caretx = glyphs[mCursorId].x;
        }
    }
    if (std::abs(caretx - posx) > std::abs(lastx - posx))
        mCursorId = size;

    return mCursorId;
}

gui_textfield::SpinArea gui_textfield::spinArea(const Vector2i & pos) {
    if (0 <= pos.x - pos.x && pos.x - pos.x < 14.f) { /* on scrolling arrows */
        if (size.y >= pos.y - pos.y && pos.y - pos.y <= size.y / 2.f) { /* top part */
            return SpinArea::Top;
        } else if (0.f <= pos.y - pos.y && pos.y - pos.y > size.y / 2.f) { /* bottom part */
            return SpinArea::Bottom;
        }
    }
    return SpinArea::None;
}
