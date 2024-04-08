#pragma once
#include "str_util.h"
#include <vector>

struct pluginentry_t {
    int32_t moduleFormat;
    int32_t localDbId;
    uint32_t uid;
    String clapId;
    bool isSynth;
    String name;
    String path;
    String relPath;
    uint64_t bugfixFlags;
    String vendorName;
};

struct plugin_snapshot_t;

class plugindatabase_t {
    class Impl;
    Impl* m_impl = nullptr;
    int revision = -1;

public:
    plugindatabase_t()  = default;
    ~plugindatabase_t();

    bool resolvePlugin(const plugin_snapshot_t& pluginSnapshot, pluginentry_t& _outResult, int loadFlags);
    void query(const String& q, std::vector<pluginentry_t>& _out);
    void openDatabase();
    void closeDatabase();
    void reopen();
    int getRevision() const;
    static plugindatabase_t* getInstance();
};
