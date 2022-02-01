#if defined(__linux__) || defined (__APPLE__)
#include "fileio.h"
#include "exceptions.h"
#include "str_util.h"
#include "assert_dbg.h"
#include "window.h"
#include "platform.h"
#include "logging.h"

#include <stb_image.h>
#include <vector>
#include <iostream>
#include <string>

#include <limits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fts.h>

#ifdef __linux__
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

Display* getX11Display();
Window getX11FromWindowBase(window_base* w);

int64_t ReadImage( const String &Filename, ImageBuf& ref)
{
	String path = toResourcePath(Filename);
	if (!FileExists(path)) {
		throw appexception(StringAsCStr(StringFormat("File not found: %s", StringAsCStr(path))));
	}
	 unsigned char *data = stbi_load(StringAsCStr(path), &ref.w, &ref.h, &ref.bitdepth, 0);
	 int64_t bufSize = -1;
	 if (data) {
		 bufSize = ref.w*ref.h*ref.bitdepth;
		 ref.bytes.reserve(bufSize);
		 ref.bytes.assign(data, data+bufSize);
	 }
	 stbi_image_free(data);
	 return bufSize;
}

using namespace std;


void ThrowLastErrorIf(bool expression, const String& msg)
{
	if (expression) {
		throw FileIOException(errno, msg);
	}
}

class File
{
private:

	// Declared but not defined, to avoid double closing.
	File& operator=(const File&);
	File(const File&);
	int handle;
public:
	explicit File(const String& filename, int mode, int perms)
	{
		handle = open(StringAsCStr(filename), mode, perms);
		ThrowLastErrorIf(handle < 0, "open call failed on file named " + filename);
	}

	~File() {
		if (handle > -1) {
			int ret = close(handle);
			dbgassert(ret == 0);
		}
	}

	int GetHandle() { return handle; }
};

size_t GetFileSizeSafe(const String& filename)
{
	struct stat fStat;
	if (stat(StringAsCStr(filename), &fStat) == 0) {
		return fStat.st_size;
	}
	return 0;
}

int32_t WriteFileVector(const String& filename, vector<uint8_t>& writebuffer)
{
	File fobj(filename, O_CREAT|O_WRONLY, 0644);
	ssize_t written = 0;
	while (written < writebuffer.size()) {
		ssize_t len =write(fobj.GetHandle(), writebuffer.data(), writebuffer.size());
		if (len < 0) {
			int err = errno;
			if (err == EAGAIN) {
				continue;
			}
			if (err == EINTR) {
				continue;
			}
			throw FileIOException(errno, "WriteFile failed: " + filename);
		}
		written += len;
	}
	return (int32_t) written;
}

void ReadFileVector(const String& filename, vector<uint8_t>& out)
{
	File fobj(filename, O_RDONLY, 0);
	size_t filesize = GetFileSizeSafe(filename);
	ssize_t bytesRead = 0;

	out.resize(filesize);

	while (bytesRead < filesize) {
		ssize_t len = read(fobj.GetHandle(), out.data(), filesize);
		if (len < 0) {
			int err = errno;
			if (err == EAGAIN) {
				continue;
			}
			if (err == EINTR) {
				continue;
			}
			throw FileIOException(errno, "ReadFile failed: " + filename);
		}
		bytesRead += len;
	}
}

void findFilesWithExt(
		const String& strPath,
		const String& strExt,
		const bool& bRecursive,
		std::vector<FileFound>& _out, int depth)
{
    FTS* file_system = NULL;
    FTSENT* child = NULL;
    FTSENT* parent = NULL;
	String localCopy = strPath;
	const char* ptr = StringAsCStr(localCopy);
    char* args[2] = { (char*)ptr, NULL };
    file_system = fts_open(args, FTS_LOGICAL | FTS_COMFOLLOW | FTS_NOCHDIR, NULL);

    if (NULL != file_system)
    {
        while( (parent = fts_read(file_system)) != NULL)
        {
            child = fts_children(file_system,0);

            if (errno == 0)
            {
				while ((NULL != child))
				{
					String fileName, ext;
					SplitPath(child->fts_name, NULL, NULL, &ext, &fileName);
					if (ext == strExt) {

						String path = String(child->fts_path) + child->fts_name;
						const FileFound f = {std::move(path), child->fts_name, ext};
						_out.push_back(f);
					}
					child = child->fts_link;
				}
            }
        }
        fts_close(file_system);
    }
}


#ifdef __linux__



static void AddFiltersToDialog( GtkWidget *dialog, std::vector<SupportedFileType>& fileTypes )
{
    for (SupportedFileType& ft : fileTypes)
    {
    	GtkFileFilter* filter = gtk_file_filter_new();
        gtk_file_filter_set_name( filter, StringAsCStr(String("*.")+ft.ext));
        gtk_file_filter_add_pattern( filter, StringAsCStr(String("*.")+ft.ext));
        gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter );
    }

    /* always append a wildcard option to the end*/

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name( filter, "*.*" );
    gtk_file_filter_add_pattern( filter, "*" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter );
}


static void WaitForCleanup(void)
{
    while (gtk_events_pending())
        gtk_main_iteration();
}


struct DialogResult {
	int result = 0;
	String selected;
};

void response_cb (DialogResult *ctxt,
                  gint response,
				  GtkWidget *dialog)
{
	ctxt->result = response;
	if (response == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
    	ctxt->selected = filename;
        g_free(filename);
	}
    gtk_widget_hide(dialog);

}

void handleGuiEvents(window_base* w, GtkWidget *dialog) {
	while (gtk_widget_is_visible(dialog)) {
		glfwWaitEventsTimeout(0.001);
		if (gtk_events_pending())
			gtk_main_iteration();
		w->updateWindowFromDlg();
//		GdkWindow *gtk_window = gtk_widget_get_window(dialog);
//		if (gtk_window) {
//			GtkWindow *parent = NULL;
//			gdk_window_get_user_data(gtk_window, (gpointer *)&parent);
//			if (parent && !gtk_window_is_active(parent)) {
//				gtk_window_present_with_time(parent, GDK_CURRENT_TIME);
//			}
//		}

//	    GtkWidget *toplevel = gtk_widget_get_toplevel (dialog);

	}
    gtk_widget_destroy(dialog);
    WaitForCleanup();
	my_printf("Exit loop\n", 0);
}


int browseForFolder(const String& title, const String& pathStart, String& _out)
{
	log_printf("not implemented\n", 0);
	return 0;
}

int promptUserFilePath(window_base* w, int mode,
		std::vector<SupportedFileType> fileTypes, String& _out) {
	GtkWidget *dialog;

	if (!gtk_init_check( NULL, NULL)) {
		return 0;
	}

	GtkFileChooserAction dlgMode;
	String dlgTitle;
	String actionConfrimStr;
	switch (mode) {
	default:
	case 0:
		dlgMode = GTK_FILE_CHOOSER_ACTION_OPEN;
		dlgTitle = "Open File";
		actionConfrimStr = "_Open";
		break;
	case 1:
		dlgMode = GTK_FILE_CHOOSER_ACTION_SAVE;
		dlgTitle = "Save File";
		actionConfrimStr = "_Save";
		break;
	}
	dialog = gtk_file_chooser_dialog_new(StringAsCStr(dlgTitle),
	NULL, dlgMode, "_Cancel", GTK_RESPONSE_CANCEL,
			StringAsCStr(actionConfrimStr), GTK_RESPONSE_ACCEPT,
			NULL);
	switch (mode) {
	default:
	case 0:
		break;
	case 1:
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
		break;
	}

	AddFiltersToDialog(dialog, fileTypes);


	DialogResult res;
	g_signal_connect_swapped(dialog, "response", G_CALLBACK (response_cb),
			&res);

	gtk_widget_show_all(dialog);
	GdkWindow *gtk_window = gtk_widget_get_window(dialog);
	if (gtk_window) {
		Window fopenx11w = gdk_x11_window_get_xid(gtk_window);
		Window x11w = getX11FromWindowBase(w);
		XSetTransientForHint(getX11Display(), fopenx11w, x11w);
	}

	handleGuiEvents(w, dialog);
	if (res.result == GTK_RESPONSE_ACCEPT) {
		_out = res.selected;
		my_printf("SELECTED PATH: %s\n", StringAsCStr(_out));
		return 1;
	}
	_out = "";
	return 0;
}

#endif

class FileTimeGetter::Impl {
	struct stat fStat;
    bool ok = false;
public:
    int64_t getWriteTimeI64() {
    	if (!ok) {
    		return 0;
    	}
#ifdef __APPLE__
    	return fStat.st_mtimespec.tv_sec * 1000L + fStat.st_mtimespec.tv_nsec / 1000000L;
#else
    	return fStat.st_mtim.tv_sec * 1000L + fStat.st_mtim.tv_nsec / 1000000L;
#endif
    }
    Impl(String path) {
    	ok = stat(StringAsCStr(path), &fStat) == 0;
	}
	~Impl() {
	}
};
FileTimeGetter::FileTimeGetter(const String& path) : m_impl{new FileTimeGetter::Impl{path}} {

}
FileTimeGetter::~FileTimeGetter() {
	delete m_impl;
}
int64_t FileTimeGetter::getWriteTimeI64() {
	return m_impl->getWriteTimeI64();
}
class FileImpl
{
private:
	FILE* m_handle;

	// Declared but not defined, to avoid double closing.
	FileImpl& operator=(const FileImpl&);
	FileImpl(const FileImpl&);
public:
	explicit FileImpl(const String& filename, OpenFileMode mode)
	{
		String strFileOpenMode = "";
		switch (mode) {
			case OpenFileMode::READ:
				strFileOpenMode = "rb";
				break;
			case OpenFileMode::WRITE:
				strFileOpenMode = "wb";
				break;
			case OpenFileMode::READWRITE:
				strFileOpenMode = "wb";
				break;
		}
		m_handle = fopen64(filename.c_str(), strFileOpenMode.c_str());

		ThrowLastErrorIf(m_handle == NULL,
			"fopen64 call failed on file named " + filename);
	}

	~FileImpl() { fclose(m_handle); }

	FILE* GetHandle() { return m_handle; }
};
IOFile::IOFile(FileImpl* _impl) noexcept : impl(_impl) {
	this->validHandle = true;
}
IOFile::~IOFile() {
	delete impl;
}
void IOFile::write(const char* data, size_t len) {
	if (this->validHandle) {
		fwrite(data, len, 1, impl->GetHandle());
	}
}
void IOFile::flush() {
	if (this->validHandle) {
		fflush(impl->GetHandle());
	}
}

IOFile* IOFile::openFile(const String& filename, OpenFileMode mode) {
	FileImpl* impl = new FileImpl(filename, mode);
	if (!impl->GetHandle()) {
		delete impl;
		return nullptr;
	}
	return new IOFile(impl);
}

bool CreateDirectoryIfNotExists( const String &DirPath )
{
	int mkdRet = mkdir(StringAsCStr(DirPath), 0755);
	ThrowLastErrorIf((mkdRet != 0) && (errno != EEXIST),
		"mkdir call failed on file named " + DirPath);
	return true;
}

#endif
