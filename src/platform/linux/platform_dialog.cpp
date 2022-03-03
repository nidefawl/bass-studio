#if defined(__linux__) || defined(__APPLE__)
#include "assert_dbg.h"
#include "str_util.h"
#include "fileio.h"
#include "window.h"
#include "platform.h"
#include <vector>
#include <GLFW/glfw3.h>
#ifdef __linux__
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif


Display* getX11Display();
Window getX11FromWindowBase(window_base* w);


#ifdef __linux__

namespace GTKFileDialogImpl {

    void AddFiltersToDialog(GtkWidget* dialog, std::vector<SupportedFileType>& fileTypes) {
        for (SupportedFileType& ft : fileTypes) {
            GtkFileFilter* filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, StringAsCStr(String("*.") + ft.ext));
            gtk_file_filter_add_pattern(filter, StringAsCStr(String("*.") + ft.ext));
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
        }

        /* always append a wildcard option to the end*/

        GtkFileFilter* filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "*.*");
        gtk_file_filter_add_pattern(filter, "*");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    }


    void WaitForCleanup() {
        while (gtk_events_pending())
            gtk_main_iteration();
    }


    struct DialogResult {
        int result = 0;
        String selected;
    };

    void response_cb(DialogResult* ctxt,
                     gint response,
                     GtkWidget* dialog) {
        ctxt->result = response;
        if (response == GTK_RESPONSE_ACCEPT) {
            char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            ctxt->selected = filename;
            g_free(filename);
        }
        gtk_widget_hide(dialog);
    }

    void handleGuiEvents(window_base* w, GtkWidget* dialog) {
        while (gtk_widget_is_visible(dialog)) {
            glfwWaitEventsTimeout(0.001);
            if (gtk_events_pending())
                gtk_main_iteration();
            w->updateWindowFromDlg();
            // GdkWindow* gtk_window = gtk_widget_get_window(dialog);
            // if (gtk_window) {
            //     GtkWindow* parent = nullptr;
            //     gdk_window_get_user_data(gtk_window, (gpointer*) &parent);
            //     if (parent && !gtk_window_is_active(parent)) {
            //         gtk_window_present_with_time(parent, GDK_CURRENT_TIME);
            //     }
            // }

            // GtkWidget* toplevel = gtk_widget_get_toplevel(dialog);
        }
        gtk_widget_destroy(dialog);
        WaitForCleanup();
        log_printf("Exit loop\n");
    }

}// namespace GTKFileDialogImpl

int browseForFolder(const String& title, const String& pathStart, String& _out) {
    log_printf("not implemented\n");
    return 0;
}

int promptUserFilePath(window_base* w,
                       int mode,
                       std::vector<SupportedFileType> fileTypes,
                       String& _out) {
    GtkWidget* dialog = nullptr;
    if (!gtk_init_check(nullptr, nullptr)) {
        return 0;
    }

    GtkFileChooserAction dlgMode{};
    String dlgTitle;
    String actionConfrimStr;
    switch (mode) {
        case 0:
            dlgMode          = GTK_FILE_CHOOSER_ACTION_OPEN;
            dlgTitle         = "Open File";
            actionConfrimStr = "_Open";
            break;
        case 1:
            dlgMode          = GTK_FILE_CHOOSER_ACTION_SAVE;
            dlgTitle         = "Save File";
            actionConfrimStr = "_Save";
            break;
        default:
            dbgassert(0 && "Invalid file dialog mode");
            return 0;
    }
    dialog = gtk_file_chooser_dialog_new(StringAsCStr(dlgTitle),
                                         nullptr,
                                         dlgMode,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         StringAsCStr(actionConfrimStr), GTK_RESPONSE_ACCEPT,
                                         nullptr);
    switch (mode) {
        default:
        case 0:
            break;
        case 1:
            gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
            break;
    }

    GTKFileDialogImpl::AddFiltersToDialog(dialog, fileTypes);


    GTKFileDialogImpl::DialogResult res;
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(GTKFileDialogImpl::response_cb), &res);

    gtk_widget_show_all(dialog);
    GdkWindow* gtk_window = gtk_widget_get_window(dialog);
    if (gtk_window) {
        Window fopenx11w = gdk_x11_window_get_xid(gtk_window);
        Window x11w      = getX11FromWindowBase(w);
        XSetTransientForHint(getX11Display(), fopenx11w, x11w);
    }

    GTKFileDialogImpl::handleGuiEvents(w, dialog);
    if (res.result == GTK_RESPONSE_ACCEPT) {
        _out = res.selected;
        log_lf(Log::L_DEBUG, "SELECTED PATH: %s\n", StringAsCStr(_out));
        return 1;
    }
    _out = "";
    return 0;
}

#endif
#endif