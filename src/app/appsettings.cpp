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


appwindowsettings::~appwindowsettings() = default;

appwindowsettings::appwindowsettings(appwindowsettings &&t) = default;

appwindowsettings &appwindowsettings::operator=(appwindowsettings &&t) = default;


appwindowsettings::appwindowsettings()
#ifdef _WIN32
: size(new windowsize)
#endif
{
}

appwindowsettings::appwindowsettings(const appwindowsettings& other)
#ifdef _WIN32
: size(new windowsize)
#endif
{
	*this = other;
}
appwindowsettings& appwindowsettings::operator=(const appwindowsettings& other) {
#ifdef _WIN32
	*this->size = *other.size;
#endif
	this->dens = other.dens;
	return *this;
}
appsettings::appsettings()
{
}

appsettings::appsettings(const appsettings& other)
{
	*this = other;
}
appsettings& appsettings::operator=(const appsettings& other) {

	this->wndMain = other.wndMain;
	this->wndCompanion = other.wndCompanion;
	this->iosettings = other.iosettings;
	this->startEngine = other.startEngine;
	this->vmmode = other.vmmode;
	this->pluginPath = other.pluginPath;
	this->recentfiles = other.recentfiles;
	return *this;
}
