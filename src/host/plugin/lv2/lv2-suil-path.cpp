#include "host/plugin/lv2/lv2-suil-path.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#ifdef PROJECT_ENABLE_LV2
#include <suil/suil.h>
#endif

namespace lv2_suil_path {

namespace {

bool dir_has_x11_module(const char* dir) {
    if (!dir || !dir[0]) {
        return false;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, "libsuil_x11.so");
    return access(path, F_OK) == 0;
}

#ifdef PROJECT_ENABLE_LV2
bool resolve_from_libsuil(char* out, size_t outSize) {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&suil_init), &info) == 0 || !info.dli_fname) {
        return false;
    }
    char libPath[512];
    snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
    char* slash = strrchr(libPath, '/');
    if (!slash) {
        return false;
    }
    *slash = '\0';
    snprintf(out, outSize, "%s/suil-0", libPath);
    return dir_has_x11_module(out);
}
#endif

} // namespace

void ensure_module_dir() {
#ifdef PROJECT_ENABLE_LV2
    if (const char* cur = getenv("SUIL_MODULE_DIR"); dir_has_x11_module(cur)) {
        return;
    }
#if defined(SUIL_MODULE_DIR_DEFAULT)
    if (dir_has_x11_module(SUIL_MODULE_DIR_DEFAULT)) {
        setenv("SUIL_MODULE_DIR", SUIL_MODULE_DIR_DEFAULT, 1);
        return;
    }
#endif
    char resolved[512];
    if (resolve_from_libsuil(resolved, sizeof(resolved))) {
        setenv("SUIL_MODULE_DIR", resolved, 1);
    }
#endif
}

} // namespace lv2_suil_path
