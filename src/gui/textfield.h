#pragma once
#include "math/vec.h"
#include "gui.h"
#include "theme.h"
#include <functional>
#include "assert_dbg.h"

class input_filter {
public:
    virtual ~input_filter() {}
    virtual bool isAllowedChar(uint32_t codepoint) = 0;
    virtual void setString(String string, bool trigger=false) = 0;
    virtual bool isReplaceInput() = 0;
    virtual String parse(String string) = 0;
};

class input_filter_hex32 : public input_filter {
	int32_t toInt(int32_t c)
	{
		if (c >= '0' && c <= '9') return      c - '0';
		if (c >= 'A' && c <= 'F') return 10 + c - 'A';
		if (c >= 'a' && c <= 'f') return 10 + c - 'a';
		return -1;
	};
public:
    bool isAllowedChar(uint32_t codepoint) override {
    	return toInt(codepoint) != -1;
    }
    void setString(String string, bool trigger=false) override {

    }
    bool isReplaceInput() override {
    	return true;
    }
    String formatNumber(int32_t number) {
    	return StringFormat("%08X", number);
    }
    int32_t parseString(String string) {
    	for (auto it = string.begin(); it != string.end(); ) {
    		if (toInt(*it) == -1) {
    			it = string.erase(it);
    		} else {
    			it++;
    		}
    	}
    	int32_t nr = 0;
    	for (auto it = string.begin(); it-string.begin() < 8 && it != string.end(); it++) {
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
	static constexpr int MAX_CHARS = 1024;
    /// How to align the text in the text box.
    enum class Alignment {
        Left,
        Center,
        Right
    };
    gui_textfield();

    bool editable() const { return mEditable; }
    void setEditable(bool editable);

    const std::string &value() const { return mValue; }
    std::string getEditValue() const {
    	if (!mCommitted)
    		return mValueTemp;
    	return mValue;
    }
    void setValue(const std::string &value) { mValue = value; }
	void setSelectionRange(int start, int end) {
		if (this->mValue.empty()) {
			dbgassert(start < 0 && end < 0);
			this->mSelectionPos = -1;
			this->mCursorPos = -1;
		} else {
			this->mSelectionPos = math::max(start, 0);
			this->mCursorPos = end < 0 ? (int) this->mValue.length() : math::min(end, (int) this->mValue.length());
		}
	}
    void clearSelection() { this->mSelectionPos = -1; }

    const std::string &defaultValue() const { return mDefaultValue; }
    void setDefaultValue(const std::string &defaultValue) { mDefaultValue = defaultValue; }

    Alignment alignment() const { return mAlignment; }
    void setAlignment(Alignment align) { mAlignment = align; }

    const std::string &units() const { return mUnits; }
    void setUnits(const std::string &units) { mUnits = units; }

    /// Return the underlying regular expression specifying valid formats
    const std::string &format() const { return mFormat; }
    /// Specify a regular expression specifying valid formats
    void setFormat(const std::string &format) { mFormat = format; }

    /// Return the placeholder text to be displayed while the text box is empty.
    const std::string &placeholder() const { return mPlaceholder; }
    /// Specify a placeholder text to be displayed while the text box is empty.
    void setPlaceholder(const std::string &placeholder) { mPlaceholder = placeholder; }

    /// Set the \ref Theme used to draw this widget
//    virtual void setTheme(Theme *theme) override;

    /// The callback to execute when the value of this TextBox has changed.
    std::function<bool(const std::string& str)> callback() const { return mCallback; }

    /// Sets the callback to execute when the value of this TextBox has changed.
    void setCallback(const std::function<bool(const std::string& str)> &callback) { mCallback = callback; }

    virtual bool focusEvent(MouseHitEvt& evt, bool focused);
    virtual bool keyboardEvent(int key, int scancode, KeyEventType action, int modifiers);
    virtual bool handleCharInput(unsigned int codepoint) override;

    virtual ivec2 preferredSize(NVGcontext *ctx) const;


    void beginEdit();
    void endEdit();

    virtual void render(NVGcontext* ctx) override;
    void updateTextLayout(NVGcontext* ctx);
    void renderTextField(NVGcontext* ctx) const;
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	virtual bool handleKeyInput(KeyEvent& kevt) override;
	virtual void handleRightClick(MouseEvent& evt) override;
	virtual void handleDraggedBegin(MouseEvent& evt) override;
	virtual void handleDraggedMove(MouseEvent& evt) override;
	virtual void handleDraggedRelease(MouseEvent& evt) override;



    bool copySelectionString(std::string& output);
    void setFilter(input_filter* filter) {
    	this->filter = filter;
    }
protected:
	void onChange();
    bool checkFormat(const std::string& input,const std::string& format);
    bool copySelection(); //move direct copy out
    void pasteFromClipboard();
    bool deleteSelection();


	void updateCursor(NVGcontext *ctx, float lastx);
	void updateShiftCursorVisible();
	float cursorIndex2Position(int index, float lastx) const;
	int position2CursorIndex(float posx, float lastx) const;


public:
    bool mEditable;
    bool mCommitted;
    std::string mValue;
    std::string mDefaultValue;
    Alignment mAlignment;
    std::string mUnits;
    std::string mFormat;
    input_filter* filter = nullptr;
    std::function<bool(const std::string& str)> mCallback = nullptr;
    std::function<void(MouseHitEvt&, bool)> fnFocus = nullptr;
    bool mValidFormat;
    std::string mValueTemp;
    std::string mPlaceholder;
    int mCursorPos;
    int mSelectionPos;
    ivec2 mMousePos;
    ivec2 mMouseDownPos;
    ivec2 mMouseDragPos;
    int mMouseDownModifier;
    float mTextOffset;
    double mLastClick;
protected:
    bool mVisible;
    bool mEnabled;
    std::string mTooltip;
    bool mFocused;
    float mFontSize;
    bool mMouseFocus;
    struct text_metrics_t {
        NVGglyphPosition glyphPositions[MAX_CHARS]{{0}};
        int numGlyphs{0};
        float textBounds[4]{0};
        float lineH{0};
    };
    text_metrics_t metrics;
	vec2 drawPos;
	vec2 clipPos;
	vec2 clipSize;
public:

	float fontSize() const {
		return mFontSize;
	}
	void setFontSize(float f) {
		mFontSize = f;
	}
};
