
#include "plugindatabase.h"
#include "host/plugin/modules.h"
#include "logging.h"
#include "tls.h"
#include "types.h"
#include "str_util.h"
#include "assert_dbg.h"
#include "fileio.h"
#include "platform.h"
#include "appsettings.h"
#include "snapshot/snapshot.h"
#include "snapshot/plugin-snapshot.h"
#include <exception>
#include <memory>
#include <utility>
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
              " `forcedisable` INTEGER DEFAULT 0,\n"
              " `bugfixFlags` INTEGER DEFAULT 0\n"
           ");";
        db.exec(queryCreate);
    }
}

class plugindatabase_t::Impl {
    String path;
    std::shared_ptr<SQLite::Database> db;
    std::map<uint32_t, uint32_t> remapVst2;
public:
    explicit Impl(String path) : path(std::move(path)) {
        auto& settings = daw_tls::getSettings();
        remapVst2 = settings.pluginsettings.configVst2.uidRemapping;
    }
    ~Impl() = default;
    bool ensureOpen() {
        if (!db) {
            try {
                String parentPath;
                SplitPath(path, &parentPath, nullptr, nullptr);
                CreateDirectoryIfNotExists(parentPath);
                db = std::make_shared<SQLite::Database>(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
                if (db) {
                    createTables(*db);
                }
                return true;
            } catch (const std::exception& e) {
                log_lf(Log::L_ERROR, "Failed to open plugin database: %s\n", e.what());
            }
        }
        return db.get() != nullptr;
    }
    bool resolvePlugin(const plugin_snapshot_t& pluginSnapshot, pluginentry_t& _outResult, int loadFlags) {
        if (!ensureOpen()) {
            return false;
        }
        _outResult = {};
        enum query_type : uint32_t {
            BY_LOCALID_AND_UUID = 0,
            BY_NAME_AND_UUID,
            BY_CLAP_UUID,
            BY_VST_UUID,
            BY_NAME,
            NUM_QUERY_TYPES = 5
        };
        auto name              = pluginSnapshot.name;
        auto uId               = pluginSnapshot.uId;
        auto localId           = pluginSnapshot.localDbId;
        auto pluginType        = pluginSnapshot.pluginType;
        auto clapId            = pluginSnapshot.clapId;
        bool loadForceDisabled = (loadFlags & 1) != 0;

        if (pluginType == PluginType::PLUGIN_TYPE_VST) {
            auto it = remapVst2.find(uId);
            if (it != remapVst2.end()) {
                uId = it->second;
            }
        }

        static const char* queryBy_LocalIdAndUUID = "SELECT * FROM plugins where moduleFormat == ? and state == 1 and id == ? and uid == ? and __COND__";
        static const char* queryBy_NameAndUUID    = "SELECT * FROM plugins where moduleFormat == ? and state == 1 and name == ? and uid == ? and __COND__ order by forcedisable ASC, version DESC, id DESC";
        static const char* queryBy_VST_UUID       = "SELECT * FROM plugins where moduleFormat == ? and state == 1 and uid == ? and __COND__ order by id DESC, forcedisable ASC, version DESC, productName DESC";
        static const char* queryBy_CLAP_UUID      = "SELECT * FROM plugins where moduleFormat == ? and state == 1 and productName == ? and __COND__ order by id DESC, forcedisable ASC, version DESC, productName DESC";
        static const char* queryBy_Name           = "SELECT * FROM plugins where moduleFormat == ? and state == 1 and name == ? and __COND__ order by id DESC, forcedisable ASC, version DESC, productName DESC";
        const char* queries[NUM_QUERY_TYPES]      = { queryBy_LocalIdAndUUID, queryBy_NameAndUUID, queryBy_CLAP_UUID, queryBy_VST_UUID, queryBy_Name };
        for (size_t i = 0; i < NUM_QUERY_TYPES; i++) {
            if (i == BY_LOCALID_AND_UUID && localId <= 0) {
                continue;
            }
            if (i == BY_CLAP_UUID && pluginType == PluginType::PLUGIN_TYPE_VST) {
                continue;
            }
            if (i == BY_VST_UUID && pluginType == PluginType::PLUGIN_TYPE_CLAP) {
                continue;
            }
            String query = queries[i];
            if (loadForceDisabled) {
                replaceString(query, "__COND__", "1");
            } else {
                replaceString(query, "__COND__", "forcedisable == 0");
            }

            SQLite::Statement queryPlugin(*db, query);
            queryPlugin.bind(1, pluginType == PluginType::PLUGIN_TYPE_VST ? 0 : 1);
            switch (i) {
                case BY_LOCALID_AND_UUID:
                    queryPlugin.bind(2, localId);
                    queryPlugin.bind(3, uId);
                    break;
                case BY_NAME_AND_UUID:
                    queryPlugin.bind(2, name);
                    queryPlugin.bind(3, uId);
                    break;
                case BY_CLAP_UUID:
                    //TODO: let user pick if multiple
                    queryPlugin.bind(2, clapId);
                    break;
                case BY_VST_UUID:
                    //TODO: let user pick if multiple
                    queryPlugin.bind(2, uId);
                    break;
                default:
                case BY_NAME:
                    //TODO: let user pick if multiple
                    queryPlugin.bind(2, name);
                    break;
            }

            while (queryPlugin.executeStep()) {
                pluginentry_t entry;
                entry.localDbId    = queryPlugin.getColumn("id").getInt();
                entry.moduleFormat = queryPlugin.getColumn("moduleFormat").getInt();
                entry.uid          = queryPlugin.getColumn("uid").getUInt();
                entry.clapId       = pluginType == PluginType::PLUGIN_TYPE_CLAP ? queryPlugin.getColumn("productName").getString() : "";
                entry.isSynth      = queryPlugin.getColumn("isSynth").getInt() != 0;
                entry.name         = queryPlugin.getColumn("name").getString();
                entry.path         = queryPlugin.getColumn("path").getString();
                entry.relPath      = queryPlugin.getColumn("relPath").getString();
                entry.bugfixFlags  = queryPlugin.getColumn("bugfixFlags").getUInt();
                _outResult = std::move(entry);
                return true;
            }
        }
        return false;
    }
    void query(const String& strQuery, std::vector<pluginentry_t>& _out) {
        if (!ensureOpen()) {
            return;
        }
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
            strSQLCond = "";
            bool customQuery2 = strSearchQuery.length() > 1 && strSearchQuery.at(0) == ':';
            if (customQuery2) {
                // find end (space)
                size_t pos = strSearchQuery.find(' ');
                if (pos == String::npos) {
                    pos = strSearchQuery.length();
                }
                String tag = strSearchQuery.substr(1, pos - 1);
                if (tag == "vst") {
                    strSQLCond += " moduleFormat == 0 and ";
                } else if (tag == "clap") {
                    strSQLCond += " moduleFormat == 1 and ";
                } else if (tag == "synth") {
                    strSQLCond += " isSynth == 1 and ";
                } else if (tag == "fx") {
                    strSQLCond += " isSynth == 0 and ";
                }
                if (pos == String::npos) {
                    pos = 0;
                }
                if (strSearchQuery.length() > pos) {
                    strSearchQuery = strSearchQuery.substr(pos + 1);
                } else {
                    strSearchQuery.clear();
                }

            }
            // else
            strSQLCond += " state == 1 and forcedisable == 0 and name like ? ESCAPE '#' ";
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
            log_lf(Log::L_DEBUG, "str: %s\n", StringAsCStr(strSQLQuery));
        SQLite::Statement queryPlugin(*db, strSQLQuery);
        if (!customQuery) {
            queryPlugin.bind(1, strSearchQuery);
        }

        pluginentry_t entry;
        while (queryPlugin.executeStep()) {
            entry.localDbId    = queryPlugin.getColumn("id").getInt();
            entry.moduleFormat = queryPlugin.getColumn("moduleFormat").getInt();
            entry.uid          = queryPlugin.getColumn("uid").getUInt();
            entry.clapId       = entry.moduleFormat == 1 ? queryPlugin.getColumn("productName").getString() : "";
            entry.isSynth      = queryPlugin.getColumn("isSynth").getInt() != 0;
            entry.name         = queryPlugin.getColumn("name").getString();
            entry.path         = queryPlugin.getColumn("path").getString();
            entry.relPath      = queryPlugin.getColumn("relPath").getString();
            entry.bugfixFlags  = queryPlugin.getColumn("bugfixFlags").getUInt();
            _out.push_back(entry);
        }
    }
};

bool plugindatabase_t::resolvePlugin(const plugin_snapshot_t& pluginSnapshot, pluginentry_t& _outResult, int loadFlags) {
    return m_impl->resolvePlugin(pluginSnapshot, _outResult, loadFlags);
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
