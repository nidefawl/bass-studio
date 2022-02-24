#include "str_util.h"

#include "plugindatabase.h"
#include "assert_dbg.h"
#include "fileio.h"
#include "platform.h"
#include "snapshot.h"
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>

void createTables(SQLite::Database& db) {
    if (!db.tableExists("plugins")) {
        const char* queryCreate =
            "CREATE TABLE `plugins` (\n"
              " `id` INTEGER PRIMARY KEY AUTOINCREMENT,\n"
              " `moduleFormat` INTEGER DEFAULT 0,\n"
              " `shellplugin` INTEGER DEFAULT 0,\n"
              " `isSynth` INTEGER DEFAULT 0,\n"
              " `uid` INTEGER NOT NULL,\n"
              " `version` INTEGER NOT NULL,\n"
              " `vstVersion` INTEGER NOT NULL,\n"
              " `category` TEXT NOT NULL,\n"
              " `moddate` TEXT NOT NULL,\n"
              " `state` INTEGER DEFAULT 0,\n"
              " `path` TEXT NOT NULL,\n"
              " `relPath` TEXT NOT NULL,\n"
              " `name` TEXT NOT NULL,\n"
              " `vendorName` TEXT NOT NULL,\n"
              " `productName` TEXT NOT NULL,\n"
              " `effectName` TEXT NOT NULL,\n"
              " `requestRescan` INTEGER DEFAULT 0,\n"
              " `forcedisable` INTEGER DEFAULT 0\n"
           ");";
        db.exec(queryCreate);
    }
}

class plugindatabase_t::Impl {
    SQLite::Database db;

public:
    explicit Impl(const String& path)
        : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
        createTables(db);
    }
    ~Impl() = default;
    bool resolve(const plugin_snapshot_t& pluginSnapshot, pluginentry_t& _outResult, int loadFlags) {
        _outResult = {};
        enum query_type : uint32_t {
            BY_LOCALID_AND_UUID = 0,
            BY_NAME_AND_UUID,
            BY_UUID,
            BY_NAME,
            NUM_QUERY_TYPES = 4
        };
        auto name              = pluginSnapshot.name;
        auto uId               = pluginSnapshot.uId;
        auto localId           = pluginSnapshot.localDbId;
        bool loadForceDisabled = (loadFlags & 1) != 0;

        static const char* queryBy_LocalIdAndUUID = "SELECT * FROM plugins where state == 1 and id == ? and uid == ? and __COND__";
        static const char* queryBy_NameAndUUID    = "SELECT * FROM plugins where state == 1 and name == ? and uid == ? and __COND__ order by forcedisable ASC, version DESC, id DESC";
        static const char* queryBy_UUID           = "SELECT * FROM plugins where state == 1 and uid == ? and __COND__ order by id DESC, forcedisable ASC, version DESC, productName DESC";
        static const char* queryBy_Name           = "SELECT * FROM plugins where state == 1 and name == ? and __COND__ order by id DESC, forcedisable ASC, version DESC, productName DESC";
        const char* queries[NUM_QUERY_TYPES]      = { queryBy_LocalIdAndUUID, queryBy_NameAndUUID, queryBy_UUID, queryBy_Name };
        for (int i = 0; i < NUM_QUERY_TYPES; i++) {
            if (i == BY_LOCALID_AND_UUID && localId <= 0) {
                continue;
            }
            String query = queries[i];
            if (loadForceDisabled) {
                replaceString(query, "__COND__", "1");
            } else {
                replaceString(query, "__COND__", "forcedisable == 0");
            }

            SQLite::Statement queryPlugin(db, query);
            switch (i) {
                case BY_LOCALID_AND_UUID:
                    queryPlugin.bind(1, localId);
                    queryPlugin.bind(2, uId);
                    break;
                case BY_NAME_AND_UUID:
                    queryPlugin.bind(1, name);
                    queryPlugin.bind(2, uId);
                    break;
                case BY_UUID:
                    //TODO: let user pick if multiple
                    queryPlugin.bind(1, uId);
                    break;
                default:
                case BY_NAME:
                    //TODO: let user pick if multiple
                    queryPlugin.bind(1, name);
                    break;
            }

            while (queryPlugin.executeStep()) {
                pluginentry_t entry;
                entry.localDbId    = queryPlugin.getColumn("id").getInt();
                entry.moduleFormat = queryPlugin.getColumn("moduleFormat").getInt();
                entry.uid          = queryPlugin.getColumn("uid").getInt();
                entry.isSynth      = queryPlugin.getColumn("isSynth").getInt() != 0;
                entry.name         = queryPlugin.getColumn("name").getString();
                entry.path         = queryPlugin.getColumn("path").getString();
                entry.relPath      = queryPlugin.getColumn("relPath").getString();
                _outResult = std::move(entry);
                return true;
            }
        }
        return false;
    }
    void query(const String& strQuery, std::vector<pluginentry_t>& _out) {
        String strSearchQuery = strQuery;
        String strSQLCond     = "1";
        String strSQLOrder    = "ORDER by name COLLATE NOCASE ASC";
        bool customQuery      = strSearchQuery.length() > 1 && strSearchQuery.at(0) == '.';
        if (customQuery) {
            strSQLCond = strSearchQuery.substr(1);
            if (StringContainsCI(strSQLCond, "order")) {
                strSQLOrder.clear();
            }
        } else {
            strSQLCond = " state == 1 and forcedisable == 0 and name like ? ESCAPE '#' ";
            if (strSearchQuery.empty()) {
                strSearchQuery = "%";
            } else {
                replaceString(strSearchQuery, "#", "##");
                replaceString(strSearchQuery, "%", "#%");
                replaceString(strSearchQuery, "_", "#_");
                strSearchQuery = StringFormat("%%%s%%", StringAsCStr(strSearchQuery));
            }
        }
        String strSQLQuery = "SELECT * FROM plugins where " + strSQLCond + " " + strSQLOrder;
        if (customQuery)
            log_printf("str: %s\n", StringAsCStr(strSQLQuery));
        SQLite::Statement queryPlugin(db, strSQLQuery);
        if (!customQuery) {
            queryPlugin.bind(1, strSearchQuery);
        }

        pluginentry_t entry;
        while (queryPlugin.executeStep()) {
            entry.localDbId    = queryPlugin.getColumn("id").getInt();
            entry.moduleFormat = queryPlugin.getColumn("moduleFormat").getInt();
            entry.uid          = queryPlugin.getColumn("uid").getInt();
            entry.isSynth      = queryPlugin.getColumn("isSynth").getInt() != 0;
            entry.name         = queryPlugin.getColumn("name").getString();
            entry.path         = queryPlugin.getColumn("path").getString();
            entry.relPath      = queryPlugin.getColumn("relPath").getString();
            _out.push_back(entry);
        }
    }
};

bool plugindatabase_t::resolve(const plugin_snapshot_t& pluginSnapshot, pluginentry_t& _outResult, int loadFlags) {
    return m_impl->resolve(pluginSnapshot, _outResult, loadFlags);
}

void plugindatabase_t::query(const String& q, std::vector<pluginentry_t>& _out) {
    m_impl->query(q, _out);
}

void plugindatabase_t::openDatabase() {
    revision++;
    dbgassert(!m_impl);
    String cwdPathDB = App::Platform::toUserdataPath("data/plugins.db3");
    m_impl = new plugindatabase_t::Impl{ cwdPathDB };
}

void plugindatabase_t::closeDatabase() {
    dbgassert(m_impl);
    delete m_impl;
    m_impl = nullptr;
}

void plugindatabase_t::reopen() {
    //closeDatabase();
    //openDatabase();
    revision++;
}

int plugindatabase_t::getRevision() const {
    return revision;
}
