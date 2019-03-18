#include <vector>
#include <stdint.h>
#include <memory>
#include "str_util.h"
#include "fileio.h"
#include "mouse.h"
#include "mousecursor.h"

#ifdef _WIN32
#include "windows.h"
#endif
#ifndef NO_GLFW_LIB
#include <GLFW/glfw3.h>
#endif

#include <assert.h>


using ImgData = std::shared_ptr<uint8_t>;
//#define NO_GLFW_LIB 1
namespace MouseCursors {
#ifdef NO_GLFW_LIB
struct AppMouseCursor {
#ifdef _WIN32
	HCURSOR             handle;
#endif
};
#ifdef _WIN32

	// Creates an RGBA icon or cursor
	//
	static HICON createIcon(const ImageBuf* image, int xhot, int yhot, bool icon)
	{
		int i;
		HDC dc;
		HICON handle;
		HBITMAP color, mask;
		BITMAPV5HEADER bi;
		ICONINFO ii;
		unsigned char* target = NULL;
		const unsigned char* source = image->bytes.data();

		ZeroMemory(&bi, sizeof(bi));
		bi.bV5Size        = sizeof(bi);
		bi.bV5Width       = image->w;
		bi.bV5Height      = -image->h;
		bi.bV5Planes      = 1;
		bi.bV5BitCount    = 32;
		bi.bV5Compression = BI_BITFIELDS;
		bi.bV5RedMask     = 0x00ff0000;
		bi.bV5GreenMask   = 0x0000ff00;
		bi.bV5BlueMask    = 0x000000ff;
		bi.bV5AlphaMask   = 0xff000000;

		dc = GetDC(NULL);
		color = CreateDIBSection(dc,
								 (BITMAPINFO*) &bi,
								 DIB_RGB_COLORS,
								 (void**) &target,
								 NULL,
								 (DWORD) 0);
		ReleaseDC(NULL, dc);

		if (!color)
		{
			my_printf("Win32: Failed to create RGBA bitmap", 0);
			return NULL;
		}

		mask = CreateBitmap(image->w, image->h, 1, 1, NULL);
		if (!mask)
		{
			my_printf("Win32: Failed to create mask bitmap", 0);
			DeleteObject(color);
			return NULL;
		}

		for (i = 0;  i < image->w * image->h;  i++)
		{
			target[0] = source[2];
			target[1] = source[1];
			target[2] = source[0];
			target[3] = source[3];
			target += 4;
			source += 4;
		}

		ZeroMemory(&ii, sizeof(ii));
		ii.fIcon    = icon;
		ii.xHotspot = xhot;
		ii.yHotspot = yhot;
		ii.hbmMask  = mask;
		ii.hbmColor = color;

		handle = CreateIconIndirect(&ii);

		DeleteObject(color);
		DeleteObject(mask);

		if (!handle)
		{
			if (icon)
			{
				my_printf("Win32: Failed to create icon", 0);
			}
			else
			{
				my_printf("Win32: Failed to create cursor", 0);
			}
		}

		return handle;
	}

	AppMouseCursor* createPlatformCursor(const ImageBuf* image, int xhot, int yhot) {
		 HCURSOR handle = (HCURSOR) createIcon(image, xhot, yhot, false);
		 if (handle) {
			 return new AppMouseCursor{handle};
		 }
		 return nullptr;
	}

	int setCursorWin32(AppMouseCursor* cursorHandle) {
	    if (cursorHandle && cursorHandle->handle)
	        SetCursor(cursorHandle->handle);
	    else
	        SetCursor(LoadCursor(NULL, IDC_ARROW));
	    return 0;
	}
#endif
#endif

	MouseCursorIcon* cursors[NUM_CURSORS]{0};
	void load(String path, ImageBuf& out) {
		if (ReadImage(path, out) < 0) {
			my_printf("Error loading image %s\n", StringAsCStr(path));
		} else {
			my_printf("%s loaded: %dx%d %d-channel, bufsize: %d\n", StringAsCStr(path), out.w, out.h, out.bitdepth, out.bytes.size());
		}
	}
	void init() {
	{
			ImageBuf imgCursors[NUM_CURSORS];
			for (int i = 0; i < NUM_CURSORS; i++) {
				ImageBuf& buf = imgCursors[i];
				assert((int)buf.bytes.size() == buf.w*buf.h * 4);
			}
			for (int i = 0; i < NUM_CURSORS; i++) {
				ImageBuf& buf = imgCursors[i];
				assert((int)buf.bytes.size() == buf.w*buf.h * 4);
			}
			for (int i = 0; i < 6; i++) {
				load(StringFormat("res/cursors/cursor%02d.png", i), imgCursors[i]);
			}
			cursors[0] = NULL;
			for (int i = 0; i < NUM_CURSORS; i++) {
				ImageBuf& buf = imgCursors[i];
				if (buf.w*buf.h == 0) {
					continue;
				}
				int posx = buf.w/2;
				int posy = buf.h/2;
				if (i+1 == CURSOR_DUPLICATE) {
					posx = 0;
					posy = 0;
				}
				if (i+1 == CURSOR_CLIP_SIZE_LEFT) {
					posx = 4;
				}
				if (i+1 == CURSOR_CLIP_SIZE_RIGHT) {
					posx = 12;
				}
#ifdef NO_GLFW_LIB
				cursors[i+1] = createPlatformCursor(&buf, posx, posy);
#else
				GLFWimage image;
				image.width = buf.w;
				image.height = buf.h;
				image.pixels = &buf.bytes[0];
				cursors[i+1] = glfwCreateCursor(&image, posx, posy);
#endif

			}
		}
	}
	void destroy() {
#ifdef NO_GLFW_LIB
		for (int i = 0; i < NUM_CURSORS; i++) {
			if (cursors[i]) {
				delete cursors[i];
				cursors[i] = nullptr;
			}
		}
#else
		//nothing
		// GLFW keeps reference and cleans stuff up internally
#endif
	}

}
