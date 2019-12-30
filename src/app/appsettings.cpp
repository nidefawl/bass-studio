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



appsettings::~appsettings() = default;

appsettings::appsettings(appsettings &&t) = default;

appsettings &appsettings::operator=(appsettings &&t) = default;


appsettings::appsettings()
#ifdef _WIN32
: size(new windowsize)
#endif
{
}

appsettings::appsettings(const appsettings& other)
#ifdef _WIN32
: size(new windowsize)
#endif
{
	*this = other;
}
appsettings& appsettings::operator=(const appsettings& other) {
#ifdef _WIN32
	*this->size = *other.size;
#endif
	this->dens = other.dens;
	this->iosettings = other.iosettings;
	this->startEngine = other.startEngine;
	this->vmmode = other.vmmode;
	this->pluginPath = other.pluginPath;
	return *this;
}
