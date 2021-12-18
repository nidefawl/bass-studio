#pragma once
#include "str_util.h"
#include <stdlib.h>
#include <vector>

struct pluginentry_t {
	int moduleFormat;
	int id;
	int uid;
	bool isSynth;
	String name;
	String path;
};

struct plugin_snapshot_t;

class plugindatabase_t {
	class Impl;
	Impl* _M_Impl = NULL;
	int revision = -1;
public:
	plugindatabase_t();
	~plugindatabase_t();
	bool resolve(const plugin_snapshot_t& pluginSnapshot, String* _outPath, int loadFlags);
	void query(String q, std::vector<pluginentry_t>& _out);
	void openDatabase();
	void closeDatabase();
	void reopen();
	int getRevision();
	static plugindatabase_t* getInstance();
};
