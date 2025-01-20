#pragma once
#include "config.hpp"
#include "types.hpp"
#include "str_util.hpp"
#include <public.sdk/source/vst/hosting/module.h>

class effectbase;
class vstplugin;
class vst3plugin;
class clapplugin;
struct handles_t;

namespace DAW::Host {

enum class SharedLibPluginType : int32_t {
    UNKNOWN = -1,
    VST2 = 0,
    VST2_SHELL = 1,
    CLAP = 2,
    VST3 = 3,
    VST3_SHELL = 4,
};

enum class SharedLibState : int32_t {
    FILE_NOT_FOUND = -3,
    DL_OPEN_FAILED = -2,
    DL_UNKNOWN_FORMAT = -1,
    FAILED = 0,
    SUCCESS = 1,
};

struct LoadResultSharedLibrary {
    SharedLibPluginType type = SharedLibPluginType::UNKNOWN;
    SharedLibState state = SharedLibState::FILE_NOT_FOUND;
    String error = "";
    void* module = nullptr;
    void* entryPoint = nullptr;
    VST3::Hosting::Module::Ptr vst3Module{};
    static inline LoadResultSharedLibrary FromError(SharedLibState _state, const String& _error, SharedLibPluginType _type = SharedLibPluginType::UNKNOWN) {
        return {_type, _state, _error, nullptr, nullptr};
    }
    static inline LoadResultSharedLibrary FromSuccess(SharedLibPluginType _type, void* module, void* entryPoint) {
        return {_type, SharedLibState::SUCCESS, "", module, entryPoint};
    }
    static inline LoadResultSharedLibrary FromSuccessVST3(VST3::Hosting::Module::Ptr&& module) {
        return {SharedLibPluginType::VST3, SharedLibState::SUCCESS, "", nullptr, nullptr, module};
    }
    bool isSuccess() const {
        return state >= SharedLibState::SUCCESS && type != SharedLibPluginType::UNKNOWN;
    }
};

struct LoadResultPluginImpl {
    LoadResultSharedLibrary library;
    effectbase* plugin = nullptr;
    vstplugin* vstPlugin = nullptr;
    clapplugin* clapPlugin = nullptr;
    vst3plugin* vst3Plugin = nullptr;
    handles_t* shellPluginHandle = nullptr;
    String path;
    String name;
    explicit LoadResultPluginImpl(LoadResultSharedLibrary _lib) : library(std::move(_lib)){};
    LoadResultPluginImpl() = default;
    LoadResultPluginImpl(LoadResultSharedLibrary _lib, vstplugin* _plugin);
    LoadResultPluginImpl(LoadResultSharedLibrary _lib, vstplugin* _plugin, handles_t* _shellHandle, String _path, String _name);
    LoadResultPluginImpl(LoadResultSharedLibrary _lib, clapplugin* _plugin, String _path, String _name);
    LoadResultPluginImpl(LoadResultSharedLibrary _lib, vst3plugin* _plugin, String _path, String _name);
};

} // namespace DAW::Host
