#pragma once
#include "gui.h"
#include "theme.h"
#include <functional>

using Vector2i = glm::ivec2;
class gui_textfield : public guibase {
public:
    /// How to align the text in the text box.
    enum class Alignment {
        Left,
        Center,
        Right
    };

    gui_textfield();

    bool editable() const { return mEditable; }
    void setEditable(bool editable);

    bool spinnable() const { return mSpinnable; }
    void setSpinnable(bool spinnable) { mSpinnable = spinnable; }

    const std::string &value() const { return mValue; }
    void setValue(const std::string &value) { mValue = value; }

    const std::string &defaultValue() const { return mDefaultValue; }
    void setDefaultValue(const std::string &defaultValue) { mDefaultValue = defaultValue; }

    Alignment alignment() const { return mAlignment; }
    void setAlignment(Alignment align) { mAlignment = align; }

    const std::string &units() const { return mUnits; }
    void setUnits(const std::string &units) { mUnits = units; }

    int unitsImage() const { return mUnitsImage; }
    void setUnitsImage(int image) { mUnitsImage = image; }

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

    virtual bool focusEvent(bool focused);
    virtual bool keyboardEvent(int key, int scancode, KeyEventType action, int modifiers);
    virtual bool handleCharInput(unsigned int codepoint) override;

    virtual Vector2i preferredSize(NVGcontext *ctx) const;



    virtual void render(NVGcontext* ctx) override;
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	virtual bool handleKeyInput(KeyEvent& kevt) override;
	virtual void handleRightClick(MouseEvent& evt) override;
	virtual void handleDraggedBegin(MouseEvent& evt) override;
	virtual void handleDraggedMove(MouseEvent& evt) override;
	virtual void handleDraggedRelease(MouseEvent& evt) override;



protected:
	void onChange();
    bool checkFormat(const std::string& input,const std::string& format);
    bool copySelection();
    void pasteFromClipboard();
    bool deleteSelection();

    void updateCursor(NVGcontext *ctx, float lastx,
                      const NVGglyphPosition *glyphs, int size);
    float cursorIndex2Position(int index, float lastx,
                               const NVGglyphPosition *glyphs, int size);
    int position2CursorIndex(float posx, float lastx,
                             const NVGglyphPosition *glyphs, int size);

    /// The location (if any) for the spin area.
    enum class SpinArea { None, Top, Bottom };
    SpinArea spinArea(const Vector2i & pos);

protected:
    bool mEditable;
    bool mSpinnable;
    bool mCommitted;
    std::string mValue;
    std::string mDefaultValue;
    Alignment mAlignment;
    std::string mUnits;
    std::string mFormat;
    int mUnitsImage;
    std::function<bool(const std::string& str)> mCallback;
    bool mValidFormat;
    std::string mValueTemp;
    std::string mPlaceholder;
    int mCursorPos;
    int mSelectionPos;
    Vector2i mMousePos;
    Vector2i mMouseDownPos;
    Vector2i mMouseDragPos;
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
    guitheme_t* mTheme;
public:

	virtual bool hovered() {
		return this == MainCtrl::get()->guiOver;
	}
	virtual bool pressed() {
		return this == MainCtrl::get()->guiDragged;
	}
	virtual bool focused() {
		return this == MainCtrl::get()->guiFocused;
	}
	float fontSize() const {
		return mFontSize;
	}
};
