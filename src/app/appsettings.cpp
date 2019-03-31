#include "appsettings.h"

#ifdef _WIN32
#include "platform/win/windowsize.h"
#include <windows.h>
void saveWindowPos(HWND hwnd, windowsize* size) {
	size->valid = GetWindowPlacement(hwnd, &(size->p)) != 0;
}
bool restoreWindowPos(HWND hwnd, windowsize* size) {
	if (size->valid) {
		return SetWindowPlacement(hwnd, &(size->p)) != 0;
	}
	return false;
}
#endif

appsettings::appsettings()
#ifdef _WIN32
: size(new windowsize())
#endif
{
}

appsettings::~appsettings()
{
#ifdef _WIN32
	delete size;
#endif
}
