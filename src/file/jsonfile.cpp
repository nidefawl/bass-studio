#include "jsonfile.hpp"
#include "fileio.hpp"
#include "logging.hpp"
#include "platform.hpp"

namespace DAW::JsonFileIO {

// ============================================================================
// File I/O - JSON from/to disk
// ============================================================================

std::optional<json> readJsonFromFile(const String& path) {
    try {
        if (!FileExists(path)) {
            return std::nullopt; // file doesn't exist, not an error
        }
        
        std::vector<uint8_t> vec;
        ReadFileVector(path, vec);
        String jsonStr(vec.begin(), vec.end());
        json j = json::parse(jsonStr);
        return j;
    } catch (const json::exception& e) {
        log_lf(Log::L_WARN, "Failed to parse JSON file %s: %s\n", StringAsCStr(path), e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        log_lf(Log::L_WARN, "Failed to read JSON file %s: %s\n", StringAsCStr(path), e.what());
        return std::nullopt;
    }
}

std::optional<String> writeJsonToFile(const json& j, const String& path, int indent) {
    try {
        Stringstream sstream;
        sstream << j.dump(indent);
        sstream.flush();
        
        writeStringStream(path, sstream);
        return std::nullopt; // success
    } catch (const std::exception& e) {
        return String("Failed to write JSON file: ") + e.what();
    }
}

// ============================================================================
// Buffer Conversions - JSON to/from memory
// ============================================================================

std::vector<uint8_t> jsonToBuffer(const json& j, int indent) {
    Stringstream sstream;
    if (indent < 0) {
        sstream << j.dump();  // compact format
    } else {
        sstream << j.dump(indent);
    }
    sstream.flush();
    
    std::vector<uint8_t> buffer;
    buffer.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
    return buffer;
}

std::optional<json> bufferToJson(const std::vector<uint8_t>& buffer) {
    try {
        if (buffer.empty()) {
            return std::nullopt;
        }
        
        String jsonStr(buffer.begin(), buffer.end());
        json j = json::parse(jsonStr);
        return j;
    } catch (const json::exception& e) {
        log_lf(Log::L_WARN, "Failed to parse JSON buffer: %s\n", e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        log_lf(Log::L_WARN, "Failed to read JSON buffer: %s\n", e.what());
        return std::nullopt;
    }
}

// ============================================================================
// Stringstream Helpers
// ============================================================================

std::optional<String> writeJsonToStream(const json& j, Stringstream& sstream, int indent) {
    try {
        if (indent < 0) {
            sstream << j.dump();  // compact format
        } else {
            sstream << j.dump(indent);
        }
        return std::nullopt; // success
    } catch (const std::exception& e) {
        return String("Failed to serialize JSON to stream: ") + e.what();
    }
}

// ============================================================================
// Fallback File Loading
// ============================================================================

std::optional<String> readJsonFromFileFallback(
    const String& primaryPath,
    const String& fallbackPath,
    json& out
) {
    try {
        std::vector<uint8_t> vec;
        
        // Try primary path first
        if (FileExists(primaryPath)) {
            ReadFileVector(primaryPath, vec);
        } 
        // Fall back to secondary path if primary doesn't exist
        else if (FileExists(fallbackPath)) {
            ReadFileVector(fallbackPath, vec);
        }
        // Neither path exists
        else {
            return String("File not found (tried: ") + primaryPath + " and " + fallbackPath + ")";
        }
        
        if (vec.empty()) {
            return String("JSON file is empty");
        }
        
        String jsonStr(vec.begin(), vec.end());
        out = json::parse(jsonStr);
        return std::nullopt; // success
        
    } catch (const json::exception& e) {
        return String("Failed to parse JSON: ") + e.what();
    } catch (const std::exception& e) {
        return String("Failed to read JSON file: ") + e.what();
    }
}

} // namespace DAW::JsonFileIO
