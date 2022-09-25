#ifdef _WIN32
#include "config.h"
#include "types.h"
#include "str_util.h"
#include "pluginmanager.h"
#include <windows.h>

// Roughly equal to https://jstuff.wordpress.com/jbridge/how-to-add-direct-support-for-jbridge-in-your-host/

// Name of the proxy DLL to load
#define PROXY_REGKEY "Software\\JBridge"

#ifdef _M_X64
#define PROXY_REGVAL "Proxy64"//use this for x64 builds
#else
#define PROXY_REGVAL "Proxy32"//use this for x86 builds
#endif

// Typedef for BridgeMain proc
using PFNBRIDGEMAIN = AEffect *(*)(audioMasterCallback, char *);

//Check if it’s a plugin_name.xx.dll
bool IsBootStrapDll(const char* path) {
    bool ret = false;
    HMODULE hModule = LoadLibrary(path);
    if (!hModule) {
        return ret;
    }
    //Exported dummy function to identify this as a bootstrap dll.
    if (GetProcAddress(hModule, "JBridgeBootstrap")) {
        //it’s a bootstrap dll
        ret = true;
    }
    FreeLibrary(hModule);
    return ret;
}

//receives the plugin’s path as an argument
int loadPlugin_jbridge(audioMasterCallback audiomasterCallback, const String& filepath, HMODULE* hmodule, AEffect** aeffect, uint64_t bugfixFlags) {
    const char* szPath = StringAsCStr(filepath);
    /* Check for boostrap dll
     * optional, but recommended
     * please note that this will cause the bootstrap dlls (plugin_name.xx.dll) to be ignored,
     * which may not be desirable if your host relies on the plugin’s location rather than its ID
     */
    if (IsBootStrapDll(szPath)) {
        MessageBox(GetActiveWindow(), "This is a bootstrap dll. Therefore, it will be ignored.", "Warning", MB_OK | MB_ICONHAND);
        return 0;
    }

    // Get path to JBridge proxy
    CHAR szProxyPath[MAX_PATH]{};
    HKEY hKey;
    if (RegOpenKey(HKEY_LOCAL_MACHINE, PROXY_REGKEY, &hKey) == ERROR_SUCCESS) {
        DWORD dw = sizeof(szProxyPath);
        RegQueryValueEx(hKey, PROXY_REGVAL, nullptr, nullptr, (LPBYTE) szProxyPath, &dw);
        RegCloseKey(hKey);
    }

    // Check key found and file exists
    if (szProxyPath[0] == 0) {
        MessageBox(GetActiveWindow(), "Unable to locate proxy DLL", "Warning", MB_OK | MB_ICONHAND);
        return -20;
    }

    // Load proxy DLL
    HMODULE hModuleProxy = LoadLibrary(szProxyPath);
    if (!hModuleProxy) {
        MessageBox(GetActiveWindow(), "Failed to load proxy", "Warning", MB_OK | MB_ICONHAND);
        return -21;
    }

    // Get entry point
    PFNBRIDGEMAIN pfnBridgeMain = (PFNBRIDGEMAIN) GetProcAddress(hModuleProxy, "BridgeMain");
    if (!pfnBridgeMain) {
        FreeLibrary(hModuleProxy);
        MessageBox(GetActiveWindow(), "BridgeMain entry point not found", "JBridge", MB_OK | MB_ICONHAND);
        return -22;
    }

    *aeffect = pfnBridgeMain(audiomasterCallback, const_cast<char*>(szPath));
    if (!aeffect) {
        FreeLibrary(hModuleProxy);
        return -23;
    }
    *hmodule = hModuleProxy;
    return 0;
}
#endif
