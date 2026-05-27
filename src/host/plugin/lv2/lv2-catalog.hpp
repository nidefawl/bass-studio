#pragma once

#include "str_util.hpp"
#include "types.hpp"
#include <lilv/lilv.h>
#include <vector>

/** One installable LV2 effect/instrument entry (identified by instance URI). */
struct lv2_catalog_entry {
    String instanceUri;
    String title;
    String maker;
    bool isInstrument{ false };
    uint32_t catalogKey{ 0 };
};

namespace lv2_catalog {

/** Scan a single *.lv2 bundle directory; does not touch the global host catalog. */
bool list_plugins_in_bundle(const String& bundleDirectory, std::vector<lv2_catalog_entry>& entries, String& errorOut);

/** Process-wide Lilv index used for hosting. */
LilvWorld* process_world();

/** Load LV2 bundles from a host-configured directory (expanded path, may be ~/.lv2). */
void load_host_search_path(const String& pathLv2);

/** Ensure default + configured LV2 search paths are indexed (safe to call repeatedly). */
void ensure_host_plugin_paths_loaded();

/** Resolve a plugin descriptor for hosting (loads Lilv world index if needed). */
const LilvPlugin* find_plugin(const char* instanceUri);

uint32_t fingerprint_uri(const char* instanceUri);

bool bundle_path_filter(const String& path);

/** Local filesystem path for a plugin's bundle directory. */
String plugin_bundle_path(const LilvPlugin* plugin);

/** True only for actual filesystem paths / file:// URIs ending in `.lv2`.
 *  Plugin URIs that merely contain the substring `.lv2` (e.g.
 *  `http://theusualsuspects.lv2.OsTIrus`) are NOT bundle paths. */
bool is_lv2_bundle_path(const String& s);

/** If @p uriOrBundlePath is a *.lv2 directory, return the plugin instance URI inside it. */
String resolve_instance_uri(const String& uriOrBundlePath, const String& nameHint = {});

} // namespace lv2_catalog
