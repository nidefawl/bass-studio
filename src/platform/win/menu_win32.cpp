#ifdef _WIN32
#include "menu.h"
#include "str_util.h"
#include "str_win32.h"
#include <windows.h>
int findSub(HMENU parent, void* ptr) {
	int cnt = GetMenuItemCount(parent);
    for (int i = 0; i < cnt; i++) {
		MENUITEMINFO menuItemInfo;
		menuItemInfo.cbSize = sizeof(MENUITEMINFO);
		menuItemInfo.fMask = MIIM_DATA;
		menuItemInfo.dwItemData = (ULONG_PTR)NULL;
		if (GetMenuItemInfo(parent, (UINT)i, true, &menuItemInfo)) {
			if (menuItemInfo.dwItemData == (ULONG_PTR)ptr) {
				return i;
			}
		}
	}
	return -1;
}
void syncMenuEntry(bool disabledALL, HMENU menuParent, ngui::Menu* menu, int idx) {
    int hMenuIdx = findSub(menuParent, menu);
	bool regMenu2 = false;
	HMENU hMenu = NULL;
	if (menu->type == ngui::menu_type::submenu) {
	    if (hMenuIdx < 0) {
	    	regMenu2 = true;
	    	hMenu = CreateMenu();
	    	hMenuIdx = idx;
	    } else {
	    	hMenu = GetSubMenu(menuParent, hMenuIdx);
	    }
	    int idxSub = 0;
		for (ngui::Menu* entry : menu->children) {
		    syncMenuEntry(disabledALL, hMenu, entry, idxSub++);
		}
		if (regMenu2) {
		    AppendMenu(menuParent, MF_POPUP, (UINT_PTR) hMenu, StringAsCStr(menu->title));
    		MENUITEMINFO menuInfo;
    		menuInfo.cbSize = sizeof(MENUITEMINFO);
    		menuInfo.fMask = MIIM_DATA;
    		menuInfo.dwItemData = reinterpret_cast<ULONG_PTR>(menu);
    		SetMenuItemInfo(menuParent, idx, true, &menuInfo);
		}
	} else {
        if (hMenuIdx < 0) {
	    	hMenuIdx = idx;
			if (menu->type == ngui::menu_type::seperator) {
				AppendMenu(menuParent, MF_SEPARATOR, 0, NULL);
			} else {
				AppendMenu(menuParent, MF_STRING, menu->command.command, StringAsCStr(menu->title));
			}
    		MENUITEMINFO menuInfo;
    		menuInfo.cbSize = sizeof(MENUITEMINFO);
    		menuInfo.fMask = MIIM_DATA;
    		menuInfo.dwItemData = reinterpret_cast<ULONG_PTR>(menu);
    		SetMenuItemInfo(menuParent, idx, true, &menuInfo);
        } else {

    	}
	}
	int flags = GetMenuState(menuParent, idx, MF_BYPOSITION);
	bool needsDisable = menu->disabled;
	if (menu->type != ngui::menu_type::submenu) {
		needsDisable |= disabledALL;
	}
	if (needsDisable != (flags&MF_GRAYED)) {
		EnableMenuItem(menuParent, hMenuIdx, MF_BYPOSITION | (needsDisable?MF_GRAYED:0));
	}
	if (menu->checked != ((flags&MF_CHECKED)!=0)) {
		CheckMenuItem(menuParent, hMenuIdx, MF_BYPOSITION | (menu->checked?MF_CHECKED:0));
	}
	TCHAR strBuf[512];
	GetMenuString(menuParent, hMenuIdx, strBuf, 512, MF_BYPOSITION);
	String s = (strBuf);
	if (s != menu->title) {
		MENUITEMINFO menuInfo;
		menuInfo.cbSize = sizeof(MENUITEMINFO);
		menuInfo.fMask = MIIM_STRING;
		menuInfo.dwTypeData = (char*)StringAsCStr(menu->title);
		SetMenuItemInfo(menuParent, hMenuIdx, true, &menuInfo);
	}

}
ngui::Menu* getUserDataFromMenu(HMENU hmenu, UINT uPos) {
	MENUITEMINFO menuItemInfo;
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_DATA;
	menuItemInfo.dwItemData = (ULONG_PTR)NULL;
	if (GetMenuItemInfo(hmenu, (UINT)uPos, true, &menuItemInfo)) {
		return reinterpret_cast<ngui::Menu*>(menuItemInfo.dwItemData);
	}
	return NULL;
}
void syncMenu(HWND hwnd, ngui::MenuBar& menubar) {

	HMENU hMenubar = GetMenu(hwnd);
	bool regMenu = false;
	if (hMenubar == NULL) {
		hMenubar = CreateMenu();
		regMenu = true;
	}
	bool disabledALL = menubar.disableAll;
	int idx = 0;
    for (ngui::Menu* menu : menubar.children) {
    	syncMenuEntry(disabledALL, hMenubar, menu, idx);
    	idx++;
    }
	if (regMenu) {
	    SetMenu(hwnd, hMenubar);
	} else {
		DrawMenuBar(hwnd);
	}
}

#endif
