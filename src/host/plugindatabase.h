#pragma once
#include "str_util.h"
#include <vector>

struct pluginentry_t {
    int moduleFormat;
    int localDbId;
    int uid;
    bool isSynth;
    String name;
    String path;
};

struct plugin_snapshot_t;

class plugindatabase_t {
    class Impl;
    Impl* m_impl = nullptr;
    int revision = -1;

public:
    plugindatabase_t()  = default;
    ~plugindatabase_t() = default;

    bool resolve(const plugin_snapshot_t& pluginSnapshot, pluginentry_t& _outResult, int loadFlags);
    void query(const String& q, std::vector<pluginentry_t>& _out);
    void openDatabase();
    void closeDatabase();
    void reopen();
    int getRevision() const;
    static plugindatabase_t* getInstance();
};
