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
class plugindatabase_t {
	class Impl;
	Impl* _M_Impl = NULL;
public:
	plugindatabase_t();
	~plugindatabase_t();
	bool resolve(String name, int32_t uId, String* _outPath);
	void query(String q, std::vector<pluginentry_t>& _out);
	void openDatabase();
	void closeDatabase();
	static plugindatabase_t* getInstance();
};
