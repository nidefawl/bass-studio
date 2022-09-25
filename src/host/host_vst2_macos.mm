#include "str_util.h"
#include "pluginmanager.h"
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule) {
    const char *pluginPath = filepath.c_str();

    CFStringRef pluginPathStringRef = CFStringCreateWithCString(NULL, pluginPath, kCFStringEncodingASCII);
    CFURLRef bundleUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pluginPathStringRef, kCFURLPOSIXPathStyle, true);
    CFRelease(pluginPathStringRef);
    if(bundleUrl == NULL) {
        printf("Couldn't make URL reference for plugin\n");
        return -2;
    }

    // Open the bundle
    CFBundleRef bundle;
    bundle = CFBundleCreate(kCFAllocatorDefault, bundleUrl);
    CFRelease(bundleUrl);
    if(bundle == NULL) {
        printf("Couldn't create bundle reference\n");
        return -3;
    }


    VSTPluginMain_t* fnPluginMain = (VSTPluginMain_t*)CFBundleGetFunctionPointerForName(bundle, CFSTR("VSTPluginMain"));
    // VST plugins previous to the 2.4 SDK used main_macho for the entry point name
    if(fnPluginMain == NULL) {
        fnPluginMain = (VSTPluginMain_t*)CFBundleGetFunctionPointerForName(bundle, CFSTR("main_macho"));
    }
    if (fnPluginMain == NULL)
    {
        CFBundleUnloadExecutable(bundle);
        CFRelease(bundle);
        return -4;
    }
    *out_hmodule = bundle;
    *out_fn = fnPluginMain;

    return 0;
}
