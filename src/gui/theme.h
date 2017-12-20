#pragma once
#include <nanovg.h>
#include "guicolors.h"
using Color = NVGcolor;
struct guitheme_t {
	/* Fonts */
    /// The standard font face (default: ``"sans"`` from ``resources/roboto_regular.ttf``).
//    int mFontNormal;
    /// The bold font face (default: ``"sans-bold"`` from ``resources/roboto_regular.ttf``).
//    int mFontBold;
    /// The icon font face (default: ``"icons"`` from ``resources/entypo.ttf``).
//    int mFontIcons;
    /**
     * The amount of scaling that is applied to each icon to fit the size of
     * NanoGUI widgets.  The default value is ``0.77f``, setting to e.g. higher
     * than ``1.0f`` is generally discouraged.
     */
    float mIconScale;

    /* Spacing-related parameters */
    /// The font size for all widgets other than buttons and textboxes (default: `` 16``).
    int mStandardFontSize;
    /// The font size for buttons (default: ``20``).
    int mButtonFontSize;
    /// The font size for text boxes (default: ``20``).
    int mTextBoxFontSize;
    /// Rounding radius for Window widget corners (default: ``2``).
    int mWindowCornerRadius;
    /// Default size of Window widget titles (default: ``30``).
    int mWindowHeaderHeight;
    /// Size of drop shadow rendered behind the Window widgets (default: ``10``).
    int mWindowDropShadowSize;
    /// Rounding radius for Button (and derived types) widgets (default: ``2``).
    int mButtonCornerRadius;
    /// The border width for TabHeader widgets (default: ``0.75f``).
    float mTabBorderWidth;
    /// The inner margin on a TabHeader widget (default: ``5``).
    int mTabInnerMargin;
    /// The minimum size for buttons on a TabHeader widget (default: ``20``).
    int mTabMinButtonWidth;
    /// The maximum size for buttons on a TabHeader widget (default: ``160``).
    int mTabMaxButtonWidth;
    /// Used to help specify what lies "in bound" for a TabHeader widget (default: ``20``).
    int mTabControlWidth;
    /// The amount of horizontal padding for a TabHeader widget (default: ``10``).
    int mTabButtonHorizontalPadding;
    /// The amount of vertical padding for a TabHeader widget (default: ``2``).
    int mTabButtonVerticalPadding;

    /* Generic colors */
    /**
     * The color of the drop shadow drawn behind widgets
     * (default: intensity=``0``, alpha=``128``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mDropShadow;
    /**
     * The transparency color
     * (default: intensity=``0``, alpha=``0``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mTransparent;
    /**
     * The dark border color
     * (default: intensity=``29``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mBorderDark;
    /**
     * The light border color
     * (default: intensity=``92``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mBorderLight;
    /**
     * The medium border color
     * (default: intensity=``35``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mBorderMedium;
    /**
     * The text color
     * (default: intensity=``255``, alpha=``160``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mTextColor;
    /**
     * The disable dtext color
     * (default: intensity=``255``, alpha=``80``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mDisabledTextColor;
    /**
     * The text shadow color
     * (default: intensity=``0``, alpha=``160``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mTextColorShadow;
    /// The icon color (default: \ref nanogui::Theme::mTextColor).
    Color mIconColor;

    /* Button colors */
    /**
     * The top gradient color for buttons in focus
     * (default: intensity=``64``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mButtonGradientTopFocused;
    /**
     * The bottom gradient color for buttons in focus
     * (default: intensity=``48``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mButtonGradientBotFocused;
    /**
     * The top gradient color for buttons not in focus
     * (default: intensity=``74``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mButtonGradientTopUnfocused;
    /**
     * The bottom gradient color for buttons not in focus
     * (default: intensity=``58``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mButtonGradientBotUnfocused;
    /**
     * The top gradient color for buttons currently pushed
     * (default: intensity=``41``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mButtonGradientTopPushed;
    /**
     * The bottom gradient color for buttons currently pushed
     * (default: intensity=``29``, alpha=``255``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mButtonGradientBotPushed;

    /* Window colors */
    /**
     * The fill color for a Window that is not in focus
     * (default: intensity=``43``, alpha=``230``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mWindowFillUnfocused;
    /**
     * The fill color for a Window that is in focus
     * (default: intensity=``45``, alpha=``230``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mWindowFillFocused;
    /**
     * The title color for a Window that is not in focus
     * (default: intensity=``220``, alpha=``160``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mWindowTitleUnfocused;
    /**
     * The title color for a Window that is in focus
     * (default: intensity=``255``, alpha=``190``; see \ref nanogui::Color::Color(int,int)).
     */
    Color mWindowTitleFocused;

    /**
     * The top gradient color for Window headings
     * (default: \ref nanogui::Theme::mButtonGradientTopUnfocused).
     */
    Color mWindowHeaderGradientTop;
    /**
     * The bottom gradient color for Window headings
     * (default: \ref nanogui::Theme::mButtonGradientBotUnfocused).
     */
    Color mWindowHeaderGradientBot;
    /// The Window header top separation color (default: \ref nanogui::Theme::mBorderLight).
    Color mWindowHeaderSepTop;
    /// The Window header bottom separation color (default: \ref nanogui::Theme::mBorderDark).
    Color mWindowHeaderSepBot;

    /**
     * The popup window color
     * (default: intensity=``50``, alpha=``255``; see \ref nanogui::Color::Color(int,int))).
     */
    Color mWindowPopup;
    /**
     * The transparent popup window color
     * (default: intensity=``50``, alpha=``0``; see \ref nanogui::Color::Color(int,int))).
     */
    Color mWindowPopupTransparent;


	guitheme_t() {

	    mStandardFontSize                 = 16;
	    mButtonFontSize                   = 20;
	    mTextBoxFontSize                  = 20;
	    mIconScale                        = 0.77f;

	    mWindowCornerRadius               = 2;
	    mWindowHeaderHeight               = 30;
	    mWindowDropShadowSize             = 10;
	    mButtonCornerRadius               = 2;
	    mTabBorderWidth                   = 0.75f;
	    mTabInnerMargin                   = 5;
	    mTabMinButtonWidth                = 20;
	    mTabMaxButtonWidth                = 160;
	    mTabControlWidth                  = 20;
	    mTabButtonHorizontalPadding       = 10;
	    mTabButtonVerticalPadding         = 2;

	    mDropShadow                       = GUI_COLORA(0, 128);
	    mTransparent                      = GUI_COLORA(0, 0);
	    mBorderDark                       = GUI_COLORA(29, 255);
	    mBorderLight                      = GUI_COLORA(92, 255);
	    mBorderMedium                     = GUI_COLORA(35, 255);
	    mTextColor                        = GUI_COLORA(5, 160);
	    mDisabledTextColor                = GUI_COLORA(255, 80);
	    mTextColorShadow                  = GUI_COLORA(0, 160);
	    mIconColor                        = mTextColor;

	    mButtonGradientTopFocused         = GUI_COLORA(64, 255);
	    mButtonGradientBotFocused         = GUI_COLORA(48, 255);
	    mButtonGradientTopUnfocused       = GUI_COLORA(74, 255);
	    mButtonGradientBotUnfocused       = GUI_COLORA(58, 255);
	    mButtonGradientTopPushed          = GUI_COLORA(41, 255);
	    mButtonGradientBotPushed          = GUI_COLORA(29, 255);

	    /* Window-related */
	    mWindowFillUnfocused              = GUI_COLORA(43, 230);
	    mWindowFillFocused                = GUI_COLORA(45, 230);
	    mWindowTitleUnfocused             = GUI_COLORA(220, 160);
	    mWindowTitleFocused               = GUI_COLORA(255, 190);

	    mWindowHeaderGradientTop          = mButtonGradientTopUnfocused;
	    mWindowHeaderGradientBot          = mButtonGradientBotUnfocused;
	    mWindowHeaderSepTop               = mBorderLight;
	    mWindowHeaderSepBot               = mBorderDark;

	    mWindowPopup                      = GUI_COLORA(50, 255);
	    mWindowPopupTransparent           = GUI_COLORA(50, 0);

//	    mFontNormal = nvgCreateFontMem(vg, "sans", roboto_regular_ttf,
//	                                   roboto_regular_ttf_size, 0);
//	    mFontBold = nvgCreateFontMem(ctx, "sans-bold", roboto_bold_ttf,
//	                                 roboto_bold_ttf_size, 0);
//	    mFontIcons = nvgCreateFontMem(ctx, "icons", entypo_ttf,
//	                                  entypo_ttf_size, 0);
	}

};
