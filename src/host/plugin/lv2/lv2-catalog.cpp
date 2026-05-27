#include "host/plugin/lv2/lv2-catalog.hpp"

#include "fileio.hpp"
#include "logging.hpp"
#include "platform.hpp"

#include <cstring>
#include <lilv/lilv.h>
#include <lv2/core/lv2.h>

namespace lv2_catalog {

namespace {

LilvWorld* hosting_world() {
    static LilvWorld* world = []() {
        LilvWorld* w = lilv_world_new();
        if (w) {
            lilv_world_load_all(w);
        }
        return w;
    }();
    return world;
}

bool has_inplace_broken(LilvWorld* w, const LilvPlugin* plugin) {
    LilvNode* node = lilv_new_uri(w, LV2_CORE__inPlaceBroken);
    const bool broken = lilv_plugin_has_feature(plugin, node);
    lilv_node_free(node);
    return broken;
}

bool is_instrument_class(LilvWorld* w, const LilvPlugin* plugin) {
    const LilvPluginClass* cls = lilv_plugin_get_class(plugin);
    if (!cls) {
        return false;
    }
    const LilvNode* clsUri = lilv_plugin_class_get_uri(cls);
    if (!clsUri) {
        return false;
    }
    LilvNode* instrument = lilv_new_uri(w, LV2_CORE__InstrumentPlugin);
    const bool match = lilv_node_equals(clsUri, instrument);
    lilv_node_free(instrument);
    return match;
}

} // namespace

uint32_t fingerprint_uri(const char* instanceUri) {
    uint32_t h = 2166136261u;
    for (const char* p = instanceUri; p && *p; ++p) {
        h ^= static_cast<uint8_t>(*p);
        h *= 16777619u;
    }
    return h;
}

bool bundle_path_filter(const String& path) {
    if (path.empty() || path[0] == '.') {
        return false;
    }
    return path.length() > 4 && path.find(".lv2") == path.length() - 4;
}

bool list_plugins_in_bundle(const String& bundleDirectory, std::vector<lv2_catalog_entry>& entries, String& errorOut) {
    entries.clear();
    LilvWorld* world = lilv_world_new();
    if (!world) {
        errorOut = "Failed to create Lilv world for bundle scan";
        return false;
    }

    LilvNode* bundleNode = lilv_new_file_uri(world, nullptr, StringAsCStr(bundleDirectory));
    if (!bundleNode) {
        lilv_world_free(world);
        errorOut = "Invalid bundle path";
        return false;
    }
    lilv_world_load_bundle(world, bundleNode);
    lilv_node_free(bundleNode);
    const LilvPlugins* pluginsInBundle = lilv_world_get_all_plugins(world);
    if (!pluginsInBundle || lilv_plugins_size(pluginsInBundle) == 0) {
        lilv_world_free(world);
        errorOut = "Could not load LV2 bundle";
        return false;
    }

    const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
    LILV_FOREACH(plugins, i, plugins) {
        const LilvPlugin* plugin = lilv_plugins_get(plugins, i);
        if (has_inplace_broken(world, plugin)) {
            continue;
        }

        lv2_catalog_entry entry;
        entry.instanceUri = lilv_node_as_uri(lilv_plugin_get_uri(plugin));
        LilvNode* title = lilv_plugin_get_name(plugin);
        if (title) {
            entry.title = lilv_node_as_string(title);
            lilv_node_free(title);
        }
        LilvNode* maker = lilv_plugin_get_author_name(plugin);
        if (maker) {
            entry.maker = lilv_node_as_string(maker);
            lilv_node_free(maker);
        }
        entry.isInstrument = is_instrument_class(world, plugin);
        entry.catalogKey   = fingerprint_uri(entry.instanceUri.c_str());
        entries.push_back(std::move(entry));
    }

    lilv_world_free(world);
    return true;
}

LilvWorld* process_world() {
    return hosting_world();
}

void load_host_search_path(const String& pathLv2) {
    LilvWorld* world = hosting_world();
    if (!world || pathLv2.empty()) {
        return;
    }
    String path = pathLv2;
    App::Platform::shellExpandPath(path);
    App::Platform::sanitizePathToDirectory(path);
    if (path.empty()) {
        return;
    }
    if (path.length() > 4 && path.find(".lv2") == path.length() - 4) {
        LilvNode* bundle = lilv_new_file_uri(world, nullptr, StringAsCStr(path));
        if (bundle) {
            lilv_world_load_bundle(world, bundle);
            lilv_node_free(bundle);
        }
        return;
    }
    std::vector<FileFound> bundles;
    findDirectoriesWithExt(path, "lv2", bundles);
    for (const FileFound& bundle : bundles) {
        LilvNode* bundleNode = lilv_new_file_uri(world, nullptr, StringAsCStr(bundle.path));
        if (bundleNode) {
            lilv_world_load_bundle(world, bundleNode);
            lilv_node_free(bundleNode);
        }
    }
}

void ensure_host_plugin_paths_loaded() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    load_host_search_path(DAW_PLATFORM_LV2_PATH_DEFAULT);
}

String plugin_bundle_path(const LilvPlugin* plugin) {
    if (!plugin) {
        return {};
    }
    const LilvNode* bundle = lilv_plugin_get_bundle_uri(plugin);
    if (!bundle) {
        return {};
    }
    String path = lilv_node_as_uri(bundle);
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    return path;
}

const LilvPlugin* find_plugin(const char* instanceUri) {
    LilvWorld* world = hosting_world();
    if (!world || !instanceUri) {
        return nullptr;
    }
    LilvNode* uri = lilv_new_uri(world, instanceUri);
    if (!uri) {
        return nullptr;
    }
    const LilvPlugins* all = lilv_world_get_all_plugins(world);
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(all, uri);
    lilv_node_free(uri);
    return plugin;
}

static String local_bundle_path(String path) {
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    App::Platform::shellExpandPath(path);
    return path;
}

bool is_lv2_bundle_path(const String& s) {
    if (s.empty()) {
        return false;
    }
    // Plugin URIs (http://..., https://..., urn:...) never name a filesystem bundle,
    // even if they contain the substring ".lv2" (theusualsuspects.lv2.OsTIrus etc.).
    if (s.find("http://") == 0 || s.find("https://") == 0 || s.find("urn:") == 0) {
        return false;
    }
    String p = s;
    if (p.find("file://") == 0) {
        p = p.substr(7);
    }
    while (p.length() > 1 && (p.back() == '/' || p.back() == '\\')) {
        p.pop_back();
    }
    return p.length() >= 4 && p.compare(p.length() - 4, 4, ".lv2") == 0;
}

String resolve_instance_uri(const String& uriOrBundlePath, const String& nameHint) {
    if (uriOrBundlePath.empty()) {
        return {};
    }
    if (!is_lv2_bundle_path(uriOrBundlePath)) {
        return uriOrBundlePath;
    }
    const String bundlePath = local_bundle_path(uriOrBundlePath);
    std::vector<lv2_catalog_entry> entries;
    String err;
    if (!list_plugins_in_bundle(bundlePath, entries, err) || entries.empty()) {
        return {};
    }
    if (nameHint.length()) {
        for (const lv2_catalog_entry& e : entries) {
            if (e.title == nameHint || e.instanceUri.find(nameHint) != String::npos) {
                return e.instanceUri;
            }
        }
    }
    return entries.front().instanceUri;
}

} // namespace lv2_catalog
