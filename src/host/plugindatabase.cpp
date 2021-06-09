#include "str_util.h"

#include "plugindatabase.h"
#include "assert_dbg.h"
#include "fileio.h"
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
    			"	`shellplugin`	INTEGER DEFAULT 0,\n"
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
    			"	`productName`	TEXT NOT NULL,\n"
    			"	`effectName`	TEXT NOT NULL,\n"
    			"	`requestRescan`	INTEGER DEFAULT 0,\n"
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
    bool resolve(String name, int32_t uId, String* _outPath, int loadFlags)
    {
        static const char* queryBy_NameAndUUID = "SELECT path FROM plugins where state == 1 and name == ? and uid == ?";
		static const char* queryBy_Name = "SELECT path FROM plugins where state == 1 and name == ?";
		static const char* queryBy_UUID = "SELECT path FROM plugins where state == 1 and uid == ?";
        const char* queries[3] = {queryBy_NameAndUUID, queryBy_UUID, queryBy_Name};
		for (int i = 0; i < 3; i++) {
			String query = String();
			
			SQLite::Statement queryPlugin(db, queries[i]);
            switch (i) {
            case 0:
                queryPlugin.bind(2, uId);
                queryPlugin.bind(1, name);
                break;
            case 1:
                queryPlugin.bind(1, name);
			case 2:
                queryPlugin.bind(1, uId);
                break;
			}
			if ((loadFlags&1)==0) {
				query += " and forcedisable == 0";
			}
			if (queryPlugin.executeStep()) {
				*_outPath = queryPlugin.getColumn("path").getString();
				return true;
			}
		}
		return false;
	}
	void query(String str, std::vector<pluginentry_t>& _out) {
		String strSQLCond = "1";
		bool customQuery = str.length() > 1 && str.at(0) == '.';
		if (customQuery) {
			strSQLCond = str.substr(1);
		} else {
			strSQLCond = " state == 1 and forcedisable == 0 and name like ? ESCAPE '#' ";
		}
		if (str.empty()) {
			str = "%";
		} else {
			replaceString(str, "#", "##");
			replaceString(str, "%", "#%");
			replaceString(str, "_", "#_");
			str = StringFormat("%%%s%%", StringAsCStr(str));
		}
		String strSQLOrder = "ORDER by name COLLATE NOCASE ASC";
		String strSQLQuery = "SELECT * FROM plugins where " + strSQLCond + " " + strSQLOrder;
		log_printf("str: %s\n", StringAsCStr(strSQLQuery));
		SQLite::Statement   queryPlugin(db, strSQLQuery);
		if (!customQuery) {
			queryPlugin.bind(1, str);
		}

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
	revision++;
	dbgassert(!_M_Impl);
	String cwdPathDB = toCWDPath("data/plugins.db3");
	_M_Impl = new plugindatabase_t::Impl{cwdPathDB};
}
void plugindatabase_t::closeDatabase() {
	dbgassert(_M_Impl);
	delete _M_Impl;
	_M_Impl = NULL;
}
void plugindatabase_t::reopen() {
//	closeDatabase();
//	openDatabase();
	revision++;
}
int plugindatabase_t::getRevision() {
	return revision;
}



