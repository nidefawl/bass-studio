#pragma once
#include "gui/gui.h"
#include "guicolors.h"
#include "theme.h"
#include <functional>
#include "assert_dbg.h"

class input_filter {
public:
    virtual ~input_filter() = default;
    virtual bool isAllowedChar(uint32_t codepoint)              = 0;
    virtual void setString(String string, bool trigger = false) = 0;
    virtual bool isReplaceInput()                               = 0;
    virtual String parse(String string)                         = 0;
};

class input_filter_hex32 final : public input_filter {
    uint32_t toInt(uint32_t c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        return 16;
    };

public:
    bool isAllowedChar(uint32_t codepoint) override {
        return toInt(codepoint) < 16;
    }
    void setString(String string, bool trigger = false) override {
    }
    bool isReplaceInput() override {
        return true;
    }
    String formatNumber(uint32_t number) {
        return StringFormat("%08X", number);
    }
    uint32_t parseString(String string) {
        for (auto it = string.begin(); it != string.end();) {
            if (toInt(*it) > 15) {
                it = string.erase(it);
            } else {
                it++;
            }
        }
        uint32_t nr = 0;
        for (auto it = string.begin(); it - string.begin() < 8 && it != string.end(); it++) {
            auto nInt = toInt(*it);
            nr <<= 4;
            nr |= nInt;
        }
        return nr;
    }
    String parse(String string) override {
        return formatNumber(parseString(string));
    }
};
class gui_textfield : public guibase {
public:
    enum class Alignment {
        Left,
        Center,
        Right
    };
private:
    struct text_metrics_t {
        std::vector<NVGglyphPosition> glyphPositions;
        int numGlyphs{ 0 };
        float textBounds[4]{ 0 };
        float lineH{ 0 };
    };
    std::string mValue;
    std::string mDefaultValue;
    std::string mUnits;
    std::string mFormat;
    std::string mValueTemp;
    std::string mPlaceholder;

    bool mCommitted = true;
    bool mFocused = false;
    bool mEditable = true;
    bool mValidFormat = true;
    bool mReturnCommits = false;
    bool mInputActivates = false;

    text_metrics_t metrics;
    Alignment mAlignment = Alignment::Left;
    float mFontSize = -1.0f;
    GuiColor::constant_t mColor = GuiColor::COL_TEXTBOX_TEXT;

    int mCursorPos    = -1;
    int mSelectionPos = -1;
    float mTextOffset = 0;
    vec2 drawPos{};
    vec2 clipPos{};
    vec2 clipSize{};
    ivec2 mMouseDownPos{-1, -1};
    ivec2 mMouseDragPos{-1, -1};
    int mMouseDownModifier = 0;
    int64_t m_tmLastClick = 0;

public:
    input_filter* filter = nullptr;
    std::function<bool(const std::string& str)> mCallback    = nullptr;
    std::function<bool(const std::string& str)> mCallbackEnd = nullptr;
    std::function<void(MouseHitEvt&, bool)> fnFocus          = nullptr;
    gui_textfield() {
        setFlag(FLG_RENDER_BACKGROUND_INSET, true);
        setFlag(FLG_BG_SHADING, true);
        setCanMouseHit(true);
    };

    float fontSize() const {
        return mFontSize;
    }
    void setFontSize(float f) {
        mFontSize = f;
    }

    void setTextfieldColor(GuiColor::constant_t color) {
        mColor = color;
    }

    void setInputActivates(bool b) {
        mInputActivates = b;
    }

    bool editable() const { return isEnabled(); }

    bool isTextCommitted() const { return mCommitted; }

    bool returnCommits() const { return mReturnCommits; }
    void setReturnCommits(bool bReturnCommits) { mReturnCommits = bReturnCommits; }

    const std::string& value() const { return mValue; }
    std::string getEditValue() const {
        if (!mCommitted)
            return mValueTemp;
        return mValue;
    }
    void setValue(const std::string& value) { mValue = value; }
    void setSelectionRange(int start, int end) {
        if (this->mValue.empty()) {
            dbgassert(start < 0 && end < 0);
            this->mSelectionPos = -1;
            this->mCursorPos    = -1;
        } else {
            this->mSelectionPos = math::max(start, 0);
            this->mCursorPos    = end < 0 ? (int) this->mValue.length() : math::min(end, (int) this->mValue.length());
        }
    }
    void clearSelection() { this->mSelectionPos = -1; }
    bool isEditing() const { return !mCommitted; }

    const std::string& defaultValue() const { return mDefaultValue; }
    void setDefaultValue(const std::string& defaultValue) { mDefaultValue = defaultValue; }

    Alignment alignment() const { return mAlignment; }
    void setAlignment(Alignment align) { mAlignment = align; }

    const std::string& units() const { return mUnits; }
    void setUnits(const std::string& units) { mUnits = units; }

    /// Return the underlying regular expression specifying valid formats
    const std::string& format() const { return mFormat; }
    /// Specify a regular expression specifying valid formats
    void setFormat(const std::string& format) { mFormat = format; }

    /// Return the placeholder text to be displayed while the text box is empty.
    const std::string& placeholder() const { return mPlaceholder; }
    /// Specify a placeholder text to be displayed while the text box is empty.
    void setPlaceholder(const std::string& placeholder) { mPlaceholder = placeholder; }

    /// Sets the callback to execute when the value of this TextBox has changed.
    void setChangeCallback(const std::function<bool(const std::string& str)>& callback) { mCallback = callback; }
    void setEndEditCallback(const std::function<bool(const std::string& str)>& callback) { mCallbackEnd = callback; }

    bool focusEvent(MouseHitEvt& evt, bool focused) override;
    virtual bool keyboardEvent(KeyboardKey key, int scancode, KeyboardState action, KeyboardMods modifiers);
    bool handleCharInput(uint32_t codepoint) override;
    bool canHandleCharInput(uint32_t codepoint);

    virtual ivec2 preferredSize(NVGcontext* ctx) const;


    void beginEdit();
    void endEdit(bool success);

    void layout() override;
    void render(NVGcontext* ctx) override;
    void updateTextLayout(NVGcontext* ctx);
    void renderTextField(NVGcontext* ctx) const;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    void handleRightClick(MouseEvent& evt) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    virtual void onTextChange();
    virtual void onTextEndEdit();


    bool copySelectionString(std::string& output);
    void setFilter(input_filter* filter) {
        this->filter = filter;
    }

protected:
    void onChange();
    bool checkFormat(const std::string& input, const std::string& format);
    bool copySelection();//move direct copy out
    void pasteFromClipboard();
    bool deleteSelection();


    void updateCursor(NVGcontext* ctx, float lastx);
    void updateShiftCursorVisible();
    float cursorIndex2Position(int index, float lastx) const;
    int position2CursorIndex(float posx, float lastx) const;
};
