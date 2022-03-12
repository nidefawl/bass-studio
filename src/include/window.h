#pragma once
#include "math/vec.h"
#include "str_util.h"
#include <memory>

#define WINDOW_BORDERLESS_POPUP (1 << 1)
#define WINDOW_IS_DIALOG (1 << 2)
#define WINDOW_IS_RESIZABLE (1 << 3)
#define WINDOW_IS_MAINWINDOW_MASTER (1 << 31)
#define WINDOW_IS_MAINWINDOW_SLAVE (1 << 30)

class BaseCtrl;
class AppCtrl;
class PopupCtrl;
class window_abstract_t;

class window_base {
public:
    window_base() = default;
    virtual ~window_base() = default;
    virtual bool isShown()                  = 0;
    virtual void getPos(ivec2* pos)         = 0;
    virtual void getSize(ivec2* size)       = 0;
    virtual void setSize(ivec2 size)        = 0;
    virtual void setPos(ivec2 pos)          = 0;
    virtual void requestRedraw()            = 0;
    virtual void setClipboardText(String s) = 0;
    virtual String getClipboardText()       = 0;
    virtual int getKeyMods()                = 0;
    virtual void captureMouse()             = 0;
    virtual void releaseMouse()             = 0;
    virtual bool isMouseCaptured()          = 0;
    virtual void updateWindowFromDlg()      = 0;
    virtual void fireMouseMoved()           = 0;
};
class window_dialog : public window_base {
public:
    ~window_dialog() override = default;
    virtual void show()                                    = 0;
};

class window_overlay : public window_base {
public:
    ~window_overlay() override = default;
    virtual void show()                                  = 0;
    virtual void hide()                                  = 0;
    virtual void positionOnScreen(ivec2 pos, ivec2 size) = 0;
    virtual PopupCtrl* getCtrl()                         = 0;
};
class window_main : public window_base {
public:
    ~window_main() override = default;
    virtual window_dialog* createDialog(const String& sTitle, int w, int h, std::shared_ptr<window_abstract_t>&& windowImpl) = 0;
    virtual window_main* createOverlay(std::shared_ptr<AppCtrl> ctrl, ivec2 windowSize, int flags) = 0;
    virtual void closeOverlay(window_main* wnd)                                  = 0;

    virtual void show()            = 0;
    virtual void hide()            = 0;
    virtual void focus()           = 0;
    virtual void requestClose()    = 0;
    virtual void updateMenu()      = 0;
    virtual void preRender()       = 0;
    virtual void postRender()      = 0;
    virtual AppCtrl* getCtrl()     = 0;
    virtual bool canResize()       = 0;
    virtual int getCreationFlags() = 0;
    virtual void initControl()     = 0;
    virtual void setInvalid()      = 0;
    // virtual void destroy()     = 0;

    virtual void positionOnScreen(ivec2 pos, ivec2 size)     = 0;
    virtual void setSizeLimits(ivec2 minSize, ivec2 maxSize) = 0;
};
