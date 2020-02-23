#include "str_util.h"

#include "plugindatabase.h"
#include "assert_dbg.h"
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>

void createTables(SQLite::Database& db) {
    const bool bExists = db.tableExists("plugins");
    if (bExists) {
//        db.exec("DROP TABLE `plugins`");
    }

    if (!db.tableExists("plugins")) {
    	const char* queryCreate = "CREATE TABLE `plugins` (\n"
    			"	`id`	INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    			"	`moduleFormat`	INTEGER DEFAULT 0,\n"
    			"	`isSynth`	INTEGER DEFAULT 0,\n"
    			"	`uid`	INTEGER NOT NULL,\n"
    			"	`version`	INTEGER NOT NULL,\n"
    			"	`vstVersion`	INTEGER NOT NULL,\n"
    			"	`category`	TEXT NOT NULL,\n"
    			"	`moddate`	TEXT NOT NULL,\n"
    			"	`state`	INTEGER DEFAULT 0,\n"
    			"	`path`	TEXT NOT NULL,\n"
    			"	`name`	TEXT NOT NULL,\n"
    			"	`vendorName`	TEXT NOT NULL,\n"
    			"	`state`	INTEGER DEFAULT 0,\n"
    			"	`forcedisable`	INTEGER DEFAULT 0\n"
    			");";
    	db.exec(queryCreate);
    }
}
class plugindatabase_t::Impl {
	SQLite::Database db;
public:
	Impl(String path)
		: db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
	{
		createTables(db);
	}
	~Impl() {

	}
	bool resolve(String name, int32_t uId, String* _outPath, int loadFlags) {
		static const char* queryDefault = "SELECT path FROM plugins where state == 1 and name == ? and forcedisable == 0";
		static const char* queryForceLoad = "SELECT path FROM plugins where state == 1 and name == ?";
		const char* query = queryDefault;
		if ((loadFlags&1)!=0) {
			query = queryForceLoad;
		}
		SQLite::Statement   queryPlugin(db, query);
		queryPlugin.bind(1, name);
		if (queryPlugin.executeStep()) {
			*_outPath = queryPlugin.getColumn("path").getString();
			return true;
		}

		return false;
	}
	void query(String str, std::vector<pluginentry_t>& _out) {
		if (str.empty()) {
			str = "%";
		} else {
			replaceString(str, "#", "##");
			replaceString(str, "%", "#%");
			replaceString(str, "_", "#_");
			str = StringFormat("%%%s%%", StringAsCStr(str));
		}
		SQLite::Statement   queryPlugin(db, "SELECT * FROM plugins where state == 1 and forcedisable == 0 and name like ? ESCAPE '#' ORDER by name COLLATE NOCASE ASC");
		queryPlugin.bind(1, str);
		pluginentry_t entry;
		while (queryPlugin.executeStep()) {
			entry.id = queryPlugin.getColumn("id").getInt();
			entry.moduleFormat = queryPlugin.getColumn("moduleFormat").getInt();
			entry.uid = queryPlugin.getColumn("uid").getInt();
			entry.isSynth = queryPlugin.getColumn("isSynth").getInt() != 0;
			entry.name = queryPlugin.getColumn("name").getString();
			entry.path = queryPlugin.getColumn("path").getString();
			_out.push_back(entry);
		}
	}
};
plugindatabase_t::plugindatabase_t() {
}
plugindatabase_t::~plugindatabase_t() {
}
bool plugindatabase_t::resolve(String name, int32_t uId, String* _outPath, int loadFlags) {
	return _M_Impl->resolve(name, uId, _outPath, loadFlags);
}
void plugindatabase_t::query(String q, std::vector<pluginentry_t>& _out) {
	_M_Impl->query(q, _out);
}
void plugindatabase_t::openDatabase() {
	dbgassert(!_M_Impl);
	_M_Impl = new plugindatabase_t::Impl{"data/plugins.db3"};
}
void plugindatabase_t::closeDatabase() {
	dbgassert(_M_Impl);
	delete _M_Impl;
	_M_Impl = NULL;
}



