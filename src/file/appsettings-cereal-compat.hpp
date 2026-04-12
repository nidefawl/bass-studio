#pragma once

#include "appsettings.hpp"
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>

// ============================================================================
// Cereal Serializers for appwindow_size_t
// ============================================================================
// These templates are in the global namespace so cereal's ADL can find them.
// This header is included by projectfile-v1.cpp and appsettingsfile-v2.cpp

template<class Archive>
void save(Archive& archive, appwindow_size_t const& settings, const std::uint32_t version) {
    cereal::size_type size = sizeof(settings.data);
    archive(cereal::make_nvp("valid", settings.valid), cereal::make_nvp("type", settings.type), cereal::make_nvp("size", size));
    if (typeid(Archive) == typeid(cereal::JSONOutputArchive)) {
        ((cereal::JSONOutputArchive*)&archive)->saveBinaryValue(settings.data, size, "data");
    }
}

template<class Archive>
void load(Archive& ar, appwindow_size_t& settings, const std::uint32_t version) {
    if (version < 1) {
        settings = {};
        settings.valid = false;
        return;
    }
    cereal::size_type size = 0;
    ar(cereal::make_nvp("valid", settings.valid), cereal::make_nvp("type", settings.type), cereal::make_nvp("size", size));
    if (typeid(Archive) == typeid(cereal::JSONInputArchive)) {
        std::vector<std::byte> vec;
        vec.resize(size);
        ((cereal::JSONInputArchive*)&ar)->loadBinaryValue((void*)vec.data(), size, "data");
        memcpy(&settings.data[0], vec.data(), size);
    }
}

// Explicit template instantiations for cereal archives
template void save<cereal::JSONOutputArchive>(cereal::JSONOutputArchive& archive, appwindow_size_t const& settings, const std::uint32_t version);
template void load<cereal::JSONInputArchive>(cereal::JSONInputArchive& ar, appwindow_size_t& settings, const std::uint32_t version);
