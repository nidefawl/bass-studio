#pragma once

namespace ngui {

    /*!
    * Options for styles to apply to a message box
    */
    enum class Style {
        Info,
        Warning,
        Error
    };

    /*!
    * The default style to apply to a message box
    */
    const Style kDefaultStyle = Style::Info;

    /*!
    * Blocking call to create a modal message box with the given message, title, style, and buttons
    */
    void showNotification(Style style, const char* title, const char* message);
}// namespace ngui
