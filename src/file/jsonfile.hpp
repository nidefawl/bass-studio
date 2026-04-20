#pragma once

#include "config.hpp"
#include "str_util.hpp"
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>
#include <sstream>

using json = nlohmann::json;
using Stringstream = std::stringstream;

/**
 * @brief Common JSON file I/O utilities for v2 serialization formats
 * 
 * Provides centralized, reusable functions for reading/writing JSON files
 * used across appsettingsfile-v2, themefile-v2, projectfile-v2, and other v2 implementations.
 * 
 * Eliminates code duplication and ensures consistent error handling and formatting.
 */
namespace DAW::JsonFileIO {

// ============================================================================
// File I/O - JSON from/to disk
// ============================================================================

/**
 * Read JSON from a file, returning the parsed JSON object.
 * Returns std::nullopt if file doesn't exist (not treated as error).
 * Logs warnings on parse failures but returns nullopt (graceful degradation).
 * 
 * @param path File path to read
 * @return Parsed JSON object, or std::nullopt if file missing/unparseable
 */
std::optional<json> readJsonFromFile(const String& path);

/**
 * Write JSON to a file with pretty-printing.
 * 
 * @param j JSON object to write
 * @param path File path destination
 * @param indent Indentation width (default: 2)
 * @return std::nullopt on success, error message string on failure
 */
std::optional<String> writeJsonToFile(const json& j, const String& path, int indent = 2);

// ============================================================================
// Buffer Conversions - JSON to/from memory
// ============================================================================

/**
 * Convert JSON to a byte buffer with optional pretty-printing.
 * 
 * @param j JSON object to serialize
 * @param indent Indentation width (default: 2). Use -1 for compact format.
 * @return Vector of bytes representing the JSON
 */
std::vector<uint8_t> jsonToBuffer(const json& j, int indent = 2);

/**
 * Parse JSON from a byte buffer.
 * Returns std::nullopt on parse failure and logs warning.
 * 
 * @param buffer Byte buffer to parse
 * @return Parsed JSON object, or std::nullopt on failure
 */
std::optional<json> bufferToJson(const std::vector<uint8_t>& buffer);

// ============================================================================
// Stringstream Helpers
// ============================================================================

/**
 * Write JSON to a stringstream with pretty-printing.
 * Caller should call sstream.flush() after this if needed.
 * 
 * @param j JSON object to write
 * @param sstream Target stringstream
 * @param indent Indentation width (default: 2)
 * @return std::nullopt on success, error message on failure
 */
std::optional<String> writeJsonToStream(const json& j, Stringstream& sstream, int indent = 2);

// ============================================================================
// Fallback File Loading
// ============================================================================

/**
 * Read JSON from primary path, falling back to secondary path if primary doesn't exist.
 * Useful for user data paths with template/default file fallbacks.
 * 
 * @param primaryPath First path to try (e.g., user data directory)
 * @param fallbackPath Second path to try (e.g., default template)
 * @param out Output JSON object (assigned if successful)
 * @return std::nullopt on success, error message on failure
 * 
 * Example:
 *   auto err = readJsonFromFileFallback(
 *       App::Platform::toUserdataPath("data/layout.json"),
 *       App::Platform::toDefaultSettingFilesPath("layout.json"),
 *       j
 *   );
 */
std::optional<String> readJsonFromFileFallback(
    const String& primaryPath,
    const String& fallbackPath,
    json& out
);

} // namespace DAW::JsonFileIO
