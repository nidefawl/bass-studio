/**
 *  (VST) Plugin Scanner
 *
 *  vstPlugPath is user configuration: Right now system paths are ignored and have to be configured manually (data/settings.json)
 *
 *  server scans for vst files and launches client processes.
 *  client processes load single plugins and return status and plugin information.
 *  server monitors client processes for status response and possible crashes.
 *  server writes information about plugin and its status (valid, invalid) to SQLite database.
 *
 */

#include "assert_dbg.h"
#include "host/plugin/clap/clap-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "logging.h"
#include "seq_util.h"
#include "threads/childprocessthread.h"
#include "appsettings.h"
#include "buildinfo.h"
#include "exceptions.h"
#include "fileio.h"
#include "ipc.h"
#include "platform.h"
#include "str_util.h"
#include "thread.h"
#include "tls.h"
#include "appconfig.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vstsdk-host-2.4/aeffectx.h>

#ifdef _WIN32
#include "platform/win/platform_win.h"
#include <windows.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <climits>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
namespace PluginScannerImplementation {
inline String APPLE_getExecutablePath() {
    String ret = "plugin_scan";
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        ret = path;
    }
    return ret;
}
} // namespace PluginScannerImplementation
#endif

void createTables(SQLite::Database& db);

namespace PluginScannerImplementation {

#define PROC_SIDE_NONE 0
#define PROC_SIDE_SERVER 1
#define PROC_SIDE_CLIENT 2
static const char* szLogPrefixes[3] = {
    "",
    "SRV: ",
    "CLI: ",
};

constexpr int32_t timeoutdefault = 120;
static int logPrefixIdx = PROC_SIDE_NONE;

#define log_message(...)                                                                      \
    do {                                                                                      \
        String msg = String(::PluginScannerImplementation::szLogPrefixes[::PluginScannerImplementation::logPrefixIdx]); \
        msg += StringFormat(__VA_ARGS__);                                                     \
        msg += "\n";                                                                          \
        getGlobalLogger()->log(Log::L_INFO, msg.c_str(), msg.length());                       \
    } while (0)

#define E_WRITE_OK 0
#define E_WRITE_ERR_PIPE 1
#define E_WRITE_ERR_BUF_SIZE 2
#define E_READ_OK 0
#define E_READ_ERR_PIPE 1
#define E_READ_ERR_BUF_SIZE 2


#define CMD_PLUGIN_LOAD_ERROR 1
#define CMD_PLUGIN_LOAD_REQUEST 2
#define CMD_PLUGIN_THREAD_QUIT 3
#define CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN 4
#define CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_SHELL 5
#define CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_PLUGIN 6
#define CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN 7
#define CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_SHELL 8
#define CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_PLUGIN 9
#define CMD_PLUGIN_END_SUCCESS 10
#define NUM_BUFS (16 * 1024)
#define SCAN_IPC_PIPE_NAME "DAW1pipc"

using seqthreads::threadSleep;

bool userSentQuitRequest = false;
bool inConnectNamedPipe  = false;
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD) {
    log_printf("CTRL_C\n");
    userSentQuitRequest = true;
    if (inConnectNamedPipe) {
        exit(0);
    }
    return true;
}
#endif

enum vst_metadata_flags_e : int32_t {
    VST_FLAGS_NONE            = 0,
    VST_FLAGS_LOADED_PLUGIN   = 1,
    VST_FLAGS_IS_SHELL_PLUGIN = 2
};

struct pipe_msg_hdr {
    uint32_t cmd;
};
struct response_type_t {
    char szPath[1024]{ 0 };
    char szName[256]{ 0 };
};
struct request_type_vst24_t : response_type_t {
    uint32_t uniqueID{ 0 };
};
struct response_type_vst24_t : response_type_t {
    vst_metadata_flags_e flags{ VST_FLAGS_NONE };
    uint32_t uniqueID{ 0 };
    uint32_t version{ 0 };
    uint32_t vstVersion{ 0 };
    uint32_t pluginCategory{ 0 };
    bool isSynth{ false };
    char szVendorName[256]{ 0 };
    char szProductName[256]{ 0 };
    char szEffectName[256]{ 0 };
    char szShellPluginName[256]{ 0 };
};
struct response_type_clapplugin_t : response_type_t {
    uint32_t pluginIndex{ 0 };
    uint32_t pluginCategory{ 0 };
    bool isSynth{ false };
    char szVersion[256]{ 0 };
    char szVendorName[256]{ 0 };
    char szProductName[256]{ 0 };
    char szEffectName[256]{ 0 };
};
struct response_type_vst24_plugin_t : response_type_vst24_t {
};
struct response_type_shell_plugin_begin_t : response_type_t {
    int numPlugins{};
};
struct recvbuf_t {
    char buf[NUM_BUFS]{};
    char* pos = nullptr;
    char* end = nullptr;
};

template<typename T>
bool writeToBuffer(recvbuf_t& buf, T& hdr) {
    dbgassert(buf.pos);
    if (static_cast<size_t>(buf.pos - buf.buf) + sizeof(hdr) <= NUM_BUFS) {
        memcpy(buf.pos, &hdr, sizeof(T));
        buf.pos += sizeof(hdr);
        return true;
    }
    return false;
}

template<typename T>
bool readFromBuffer(recvbuf_t& buf, T& hdr) {
    dbgassert(nullptr != buf.end);
    dbgassert(buf.pos < buf.end);
    if (buf.pos + sizeof(hdr) <= buf.end) {
        memcpy(&hdr, buf.pos, sizeof(T));
        buf.pos += sizeof(hdr);
        return true;
    }
    return false;
}

template<typename IPC>
bool IPCrecvBuffer(IPC& conn, recvbuf_t& bufferRecv) {
    bufferRecv.pos = bufferRecv.buf;
    auto lenRcvd   = conn.readData(bufferRecv.pos, NUM_BUFS);
    bufferRecv.end = bufferRecv.pos + lenRcvd;
    return NUM_BUFS == lenRcvd;
}

template<typename IPC>
bool IPCsendBuffer(IPC& conn, recvbuf_t& bufferRecv) {
    bufferRecv.pos = bufferRecv.buf;
    auto lenRcvd   = conn.sendData(bufferRecv.pos, NUM_BUFS);
    bufferRecv.end = bufferRecv.pos + lenRcvd;
    return NUM_BUFS == lenRcvd;
}

/** uses function scope static buffer: NOT THREADSAFE */
template<typename IPC, typename T>
int readFromIPC(IPC& ipcConnection, T& hdr) {
    static recvbuf_t recvBuf;
    recvBuf.pos = recvBuf.buf;
    recvBuf.end = nullptr;
    if (!IPCrecvBuffer(ipcConnection, recvBuf)) {
        log_message("IPCrecvBuffer failed");
        return E_READ_ERR_PIPE;
    }
    if (!readFromBuffer(recvBuf, hdr)) {

        return E_READ_ERR_BUF_SIZE;
    }
    return E_READ_OK;
}

/** uses function scope static buffer: NOT THREADSAFE */
template<typename IPC, typename T>
int writeToIPC(IPC& ipcConnection, T& hdr) {
    static recvbuf_t sendBuffer;
    sendBuffer.pos = sendBuffer.buf;
    sendBuffer.end = nullptr;
    writeToBuffer(sendBuffer, hdr);
    if (!writeToBuffer(sendBuffer, hdr)) {
        log_message("writeToBuffer failed");
        return E_WRITE_ERR_BUF_SIZE;
    }
    if (!IPCsendBuffer(ipcConnection, sendBuffer)) {
        log_message("IPCsendBuffer failed");
        return E_WRITE_ERR_PIPE;
    }
    return E_WRITE_OK;
}

struct pluginscanner_server_options {
    String clapPluginPath;
    String vstPlugPath;
    bool dryRun             = false;
    bool fullRescan         = false;
    bool launchProcess      = true;
    bool checkDiskTimestamp = true;
    String updatePattern;
    int32_t unresponsiveTimeoutSeconds = timeoutdefault;
};

static void getVSTPluginData(DAW::Host::LoadResultPlugin& res, response_type_vst24_t* _out) {
    auto plugin = res.vstPlugin;
    AEffect* aeffect     = plugin->handle->aeffect;
    _out->uniqueID       = aeffect->uniqueID;
    _out->version        = aeffect->version;
    _out->vstVersion     = plugin->vstVersion;
    _out->pluginCategory = plugin->pluginCategory;
    safe_strcpy(_out->szName, plugin->sName);
    safe_strcpy(_out->szPath, res.path);
    if (!plugin->dispatch(effGetVendorString, 0, 0, (void*) _out->szVendorName)) {
        _out->szVendorName[0] = 0;
    }
    if (!plugin->dispatch(effGetProductString, 0, 0, (void*) _out->szProductName)) {
        _out->szProductName[0] = 0;
    }
    if (!plugin->dispatch(effGetEffectName, 0, 0, (void*) _out->szEffectName)) {
        _out->szEffectName[0] = 0;
    }
    _out->isSynth = plugin->isSynth;
}

static void getClapPluginData(DAW::Host::LoadResultPlugin& res, response_type_clapplugin_t* _out) {
    auto plugin = res.clapPlugin;
    _out->pluginIndex = 0;
    _out->pluginCategory = plugin->pluginCategory;
    safe_strcpy(_out->szName, plugin->sName);
    safe_strcpy(_out->szPath, res.path);
    _out->isSynth = plugin->isSynth;
    auto clapPlugDesc = plugin->getDescription();
    safe_strcpy(_out->szProductName, clapPlugDesc.id);
    safe_strcpy(_out->szVendorName, clapPlugDesc.vendor);
    safe_strcpy(_out->szEffectName, clapPlugDesc.name);
    safe_strcpy(_out->szVersion, clapPlugDesc.version);
}
static int waitTimeout(ipc_server& server, ProcessThread* thread, const char* plugName, int minReadBuffSize, int64_t timeStartScan_ms, int64_t timeoutPluginScan_ms) {
    uint32_t notificationStep     = 0;
    while (true) {
        int peakRdBufSizeResult = server.peekReadBufferSize();
        if (peakRdBufSizeResult < minReadBuffSize) {
            auto timeSince_ms = getTimeMillis() - timeStartScan_ms;
            if (-1 == peakRdBufSizeResult || (timeSince_ms > timeoutPluginScan_ms)) {
                log_message("TIMEOUT: Plugin %s timed out after %zd ms", plugName, timeoutPluginScan_ms);
                return 1;
            }
            if (notificationStep != (timeSince_ms / 1000)) {
                int64_t secondsLeft = math::max<int64_t>(0, timeoutPluginScan_ms - timeSince_ms) / 1000;
                notificationStep     = timeSince_ms / 1000;
                log_message("Waiting for Plugin %s to respond... %zds left", plugName, secondsLeft);
            }
            if (userSentQuitRequest) {
                log_message("User requested quit");
                return 1;
            }
            if (thread && !thread->isRunning()) {
                log_message("Client died");
                return 2;
            }
            threadSleep(300);
            continue;
        }
        break;
    }
    return 0;
}

static int readClientResponses(const pluginscanner_server_options& options, ipc_server& server, ProcessThread* thread, const request_type_vst24_t& req, SQLite::Statement& queryInsertPlugin, FileFound& file, int64_t timeDisk, bool forcedisable) {
    auto timeStartScan_ms     = getTimeMillis();
    int64_t timeoutPluginScan_ms = options.unresponsiveTimeoutSeconds * int64_t(1000);
    int nPluginsScanned           = 0;
    while (!userSentQuitRequest) {
        int32_t responseType    = 0;
        if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(responseType)), timeStartScan_ms, timeoutPluginScan_ms)) {
            return -4;
        }
        if (E_READ_OK != readFromIPC(server, responseType)) {
            log_message("failed reading responseType int32_t");
            return -3;
        }
        timeStartScan_ms = getTimeMillis();
        switch (responseType) {
            case CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN: {
                response_type_clapplugin_t respLoadSinglePlugin;
                if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(respLoadSinglePlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respLoadSinglePlugin)) {
                    log_message("failed reading response_type_clapplugin_t");
                    return -3;
                }

                log_lf(Log::L_INFO, "Plugin '%s': Status: %s\n", respLoadSinglePlugin.szPath, "GOOD");
                auto& data = respLoadSinglePlugin;
                String relPath = file.name;
                if (file.path.length() > options.clapPluginPath.length()) {
                    relPath = file.path.substr(options.clapPluginPath.length());
                    replaceString(relPath, FILE_PATHSEP_STR, "/");
                }
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, 1); // clap plugin
                    queryInsertPlugin.bind(bndIdx++, data.pluginIndex);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, 0); // vstVersion
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, (long long int) timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);  // vendor
                    queryInsertPlugin.bind(bndIdx++, data.szProductName); // product
                    queryInsertPlugin.bind(bndIdx++, data.szEffectName);  // effect
                    queryInsertPlugin.bind(bndIdx++, 0);
                    queryInsertPlugin.bind(bndIdx++, forcedisable ? 1 : 0);
                    queryInsertPlugin.bind(bndIdx++, 0);
                    /*int insertRowsAffected = */ queryInsertPlugin.exec();
                    nPluginsScanned++;
                } catch (SQLite::Exception& e) {
                    log_message("queryInsertPlugin failed with SQLite exception: %s (%d)", e.getErrorStr(), e.getErrorCode());
                    return -5;
                }

                break;
            }
            case CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN: {
                response_type_vst24_plugin_t respLoadSinglePlugin;
                if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(respLoadSinglePlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respLoadSinglePlugin)) {
                    log_message("failed reading response_type_vst24_plugin_t");
                    return -3;
                }

                log_lf(Log::L_INFO, "Plugin '%s': Status: %s\n", respLoadSinglePlugin.szPath, "GOOD");
                auto& data = respLoadSinglePlugin;
                String relPath = file.name;
                if (file.path.length() > options.vstPlugPath.length()) {
                    relPath = file.path.substr(options.vstPlugPath.length());
                    replaceString(relPath, FILE_PATHSEP_STR, "/");
                }
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, 0); // vst plugin
                    queryInsertPlugin.bind(bndIdx++, data.uniqueID);
                    queryInsertPlugin.bind(bndIdx++, data.version);
                    queryInsertPlugin.bind(bndIdx++, data.vstVersion);
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, (long long int) timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);
                    queryInsertPlugin.bind(bndIdx++, data.szProductName);
                    queryInsertPlugin.bind(bndIdx++, data.szEffectName);
                    queryInsertPlugin.bind(bndIdx++, 0);
                    queryInsertPlugin.bind(bndIdx++, forcedisable ? 1 : 0);
                    queryInsertPlugin.bind(bndIdx++, 0);
                    /*int insertRowsAffected = */ queryInsertPlugin.exec();
                    nPluginsScanned++;
                } catch (SQLite::Exception& e) {
                    log_message("queryInsertPlugin failed with SQLite exception: %s (%d)", e.getErrorStr(), e.getErrorCode());
                    return -5;
                }

                break;
            }
            case CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_SHELL: {
                response_type_shell_plugin_begin_t respShellPlugin;
                if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(respShellPlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    log_message("failed reading response_type_shell_plugin_begin_t");
                    return -3;
                }
                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_SHELL: {
                response_type_shell_plugin_begin_t respShellPlugin;
                if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(respShellPlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    log_message("failed reading response_type_shell_plugin_begin_t");
                    return -3;
                }
                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_PLUGIN: {
                response_type_clapplugin_t data;
                if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(data)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, data)) {
                    log_message("failed reading response_type_vst24_t");
                    return -3;
                }
                String relPath = file.name;
                if (file.path.length() > options.clapPluginPath.length()) {
                    relPath = file.path.substr(options.clapPluginPath.length());
                    replaceString(relPath, FILE_PATHSEP_STR, "/");
                }
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, 1); // clap plugin
                    queryInsertPlugin.bind(bndIdx++, data.pluginIndex);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, 0); // vstVersion
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, (long long int) timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);  // vendor
                    queryInsertPlugin.bind(bndIdx++, data.szProductName); // product
                    queryInsertPlugin.bind(bndIdx++, data.szEffectName);  // effect
                    queryInsertPlugin.bind(bndIdx++, 0);
                    queryInsertPlugin.bind(bndIdx++, forcedisable ? 1 : 0);
                    queryInsertPlugin.bind(bndIdx++, 0);
                    /*int insertRowsAffected = */ queryInsertPlugin.exec();
                    nPluginsScanned++;
                } catch (SQLite::Exception& e) {
                    log_message("queryInsertPlugin failed with SQLite exception: %s (%d)", e.getErrorStr(), e.getErrorCode());
                    return -5;
                }

                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_PLUGIN: {
                response_type_vst24_t data;
                if (waitTimeout(server, thread, req.szPath, static_cast<int>(sizeof(data)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, data)) {
                    log_message("failed reading response_type_vst24_t");
                    return -3;
                }
                log_lf(Log::L_INFO, "Shell plugin %s %s %s isSynth: %d, uid %08X\n", StringAsCStr(file.path), data.szName, "GOOD", data.isSynth, data.uniqueID);
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, data.uniqueID);
                    queryInsertPlugin.bind(bndIdx++, data.version);
                    queryInsertPlugin.bind(bndIdx++, data.vstVersion);
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, (long long int) timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);
                    queryInsertPlugin.bind(bndIdx++, data.szProductName);
                    queryInsertPlugin.bind(bndIdx++, data.szEffectName);
                    queryInsertPlugin.bind(bndIdx++, 0);
                    queryInsertPlugin.bind(bndIdx++, forcedisable ? 1 : 0);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    /*int insertRowsAffected = */ queryInsertPlugin.exec();
                    nPluginsScanned++;
                } catch (SQLite::Exception& e) {
                    log_message("queryInsertPlugin failed with SQLite exception: %s (%d)", e.getErrorStr(), e.getErrorCode());
                    return -5;
                }

                break;
            } break;
            case CMD_PLUGIN_LOAD_ERROR:
                return -2;
            case CMD_PLUGIN_END_SUCCESS:
                return nPluginsScanned;
            default:
                dbgassert(0);
                break;
        }
    }
    return -1;
}

static int runScannerServer(const pluginscanner_server_options& options) {

    if (!options.updatePattern.empty()) {
        log_message("Update *%s*", StringAsCStr(options.updatePattern));
    }
    std::unique_ptr<ProcessThread> thread = nullptr;
    try {
        String cwdPathDB = App::Platform::toUserdataPath("data/plugins.db3");
        SQLite::Database db(cwdPathDB, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";
        createTables(db);

        threadSleep(1000);

        std::vector<FileFound> filesClap;
        std::vector<FileFound> filesVst_;
        if (!options.vstPlugPath.empty()) {
            findFilesWithExt(options.vstPlugPath, PLATFORM_PLUGIN_EXT, true, filesVst_);
            log_message("Found %u .%s files in %s", CtrSize(filesVst_), PLATFORM_PLUGIN_EXT, StringAsCStr(options.vstPlugPath));
        }
        if (!options.clapPluginPath.empty()) {
            findFilesWithExt(options.clapPluginPath, PLATFORM_CLAP_PLUGIN_EXT, true, filesClap);
            log_message("Found %u .%s files in %s", CtrSize(filesClap), PLATFORM_CLAP_PLUGIN_EXT, StringAsCStr(options.clapPluginPath));
        }
        if (filesVst_.empty() && filesClap.empty()) {
            return 1;
        }
        auto& allFiles = filesClap;
        allFiles.insert(allFiles.end(), filesVst_.begin(), filesVst_.end());
#ifdef _WIN32
        TCHAR szFileName[MAX_PATH + 1];
        GetModuleFileName(nullptr, szFileName, MAX_PATH + 1);
        String exeName = szFileName;
#endif
#ifdef __APPLE__
        String exeName = APPLE_getExecutablePath();
#endif
#ifdef __linux__
        String exeName = "plugin_scan";
        char buff[4096];
        ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
        if (len != -1) {
            buff[len] = '\0';
            exeName   = buff;
        }
#endif

        ipc_server server;
        int ipc_status = server.server_open(SCAN_IPC_PIPE_NAME);
        if (ipc_status) {
            log_message("Failed opening ipc_server: %d", ipc_status);
            return 1;
        }
        bool pipeConnected                    = false;
        SQLite::Statement queryPlugin(db, "SELECT id, moddate, forcedisable, requestRescan, uid, shellplugin FROM plugins where path == ?");
        SQLite::Statement queryInsertPlugin(db, "INSERT INTO "
                                                "plugins(isSynth, moduleFormat, uid, version, vstVersion, category, moddate, state, path, relPath, name, vendorName, productName, effectName, requestRescan, forcedisable, shellplugin) "
                                                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        SQLite::Statement queryDelete(db, "DELETE from plugins where id == ? or path == ?");

        SQLite::Statement queryAll(db, "SELECT id, path from plugins");
        while (queryAll.executeStep() && !userSentQuitRequest) {
            String path = queryAll.getColumn(1).getString();

            try {
                size_t size = GetFileSizeSafe(path);
                if (size > 0) {
                    continue;
                }
            } catch (std::exception& /*e*/) {
                log_message("REMOVE plugin: File at %s is missing", StringAsCStr(path));
            }
            if (!options.dryRun) {
                queryDelete.reset();
                queryDelete.bind(1, queryAll.getColumn(0).getInt());
                queryDelete.bind(2, path);
                queryDelete.exec();
            }
        }

        for (FileFound& file : allFiles) {
            if (userSentQuitRequest) {
                break;
            }
            FileTimeGetter filetime(file.path);
            int64_t timeDisk   = filetime.getWriteTimeI64();
            int id             = -1;
            bool needScan      = true;
            bool forcedisable  = false;
            queryPlugin.reset();
            queryPlugin.bind(1, file.path);
            String reason = "New plugin (or not a plugin .dll)";
            if (queryPlugin.executeStep()) {
                id            = queryPlugin.getColumn(0).getInt();
                forcedisable  = queryPlugin.getColumn(2).getInt() > 0;
                if (options.checkDiskTimestamp) {

                    int64_t timeDB = queryPlugin.getColumn(1).getInt64();
                    if (timeDisk == timeDB) {
                        needScan = false;
                    } else {
                        reason = "File date changed";
                    }
                } else {
                    needScan = false;
                }
                if (queryPlugin.getColumn(3).getInt()) {
                    needScan = true;
                    reason   = "Force rescan";
                }
            }
            if (!needScan && options.fullRescan) {
                needScan = true;
                reason   = "Full rescan";
            }
            if (options.updatePattern.length()) {
                needScan = file.name.find(options.updatePattern) != String::npos;
                if (needScan) {
                    reason = "Filename matched pattern";
                }
            }
            if (!needScan) {
                //if (options.updatePattern.empty())
                //    log_message("%s is up to date", StringAsCStr(file.name));
                continue;
            }
            log_message("%s needs update: %s", StringAsCStr(file.path), StringAsCStr(reason));
            if (options.dryRun) {
                continue;
            }
            if (options.launchProcess && (!thread || !thread->isRunning())) {
                if (thread)
                    thread->joinProcess();
                if (pipeConnected) {
                    server.server_disconnect();
                }
                threadSleep(250);
                pipeConnected  = false;
                thread         = nullptr;
                thread         = std::make_unique<ProcessThread>();
                String arg1    = "-client";
                String lastCmd = StringFormat("%s %s", StringAsCStr(exeName), StringAsCStr(arg1));
                thread->startProcess(exeName, "-client", "");
                threadSleep(250);
                if (!thread->isRunning()) {
                    thread->checkException();
                    log_message("Failed starting client");
                    break;
                }
                log_message("Thread is up");
            }
            bool resetConnection = false;
            if (!pipeConnected && (!options.launchProcess || (thread && thread->isRunning()))) {
                inConnectNamedPipe    = true;
                int ipcstatus_connect = server.server_accept();
                inConnectNamedPipe    = false;
                if (0 != ipcstatus_connect) {
                    log_message("ipc_server::server_accept() failed: %d", ipcstatus_connect);
                    resetConnection = true;
                } else {
                    pipeConnected = true;
                }
            }
            if (pipeConnected) {
                pipe_msg_hdr hdr = { CMD_PLUGIN_LOAD_REQUEST };
                if (E_WRITE_OK != writeToIPC(server, hdr)) {
                    log_message("error writeToIPC pipe_msg_hdr");
                    resetConnection = true;
                }
                request_type_vst24_t req;
                safe_strcpy(req.szPath, file.path);
                if (E_WRITE_OK != writeToIPC(server, req)) {
                    log_message("error writeToIPC request_type_vst24_t");
                    resetConnection = true;
                }
                if (id > 0) {
                    queryDelete.reset();
                    queryDelete.bind(1, id);
                    queryDelete.bind(2, req.szPath);
                    queryDelete.exec();
                }
                int ret = readClientResponses(options, server, thread.get(), req, queryInsertPlugin, file, timeDisk, forcedisable);

                if (ret < 0) {
                    resetConnection = true;
                }
            } else {
                resetConnection = true;
            }
            if (userSentQuitRequest)
                break;
            if (resetConnection) {
                resetConnection = false;
                log_message("Reset scanner client process");
                server.server_disconnect();
                pipeConnected = false;
                if (thread && thread->isRunning()) {
                    thread->killProcess();
                }
                thread = nullptr;
                continue;
            }
        }
        if (thread) {
            if (thread->isRunning()) {
                /*if (pipeConnected) {
                    pipe_msg_hdr hdr = {CMD_PLUGIN_THREAD_QUIT};
                    writeToIPC(server, hdr);
                    threadSleep(120);
                }*/
            }
            server.server_disconnect();
            threadSleep(120);
            if (thread->isRunning()) {
                thread->killProcess();
            }
            thread = nullptr;
        }
    } catch (SQLite::Exception& e) {
        log_message("SQLite exception: %s (%d)", e.getErrorStr(), e.getErrorCode());
    } catch (std::exception& e) {
        log_message("exception %s", e.what());
    } catch (...) {
        log_message("Unhandled exception");
    }
    return 0;
}

static int runPluginTest(request_type_vst24_t req, response_type_vst24_plugin_t& respVstPlugin, response_type_clapplugin_t& respClapPlugin) {
    log_message("runPluginTest");

    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32 });
    auto& tls = daw_tls::getTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);

    int response = 0;
    log_message("Load plugin %s", req.szPath);
    try {
        auto res = pluginMgr->loadPlugin(req.szPath, 0);
        if (res.library.state != DAW::Host::SharedLibState::SUCCESS) {
            log_message("Failed loading %s: %s (%d)", req.szPath, StringAsCStr(res.library.error), static_cast<int32_t>(res.library.state));
            response = CMD_PLUGIN_LOAD_ERROR;
        } else {
            dbgassert(res.plugin);
            if (res.vstPlugin) {
                response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN;
                getVSTPluginData(res, &respVstPlugin);
            } else if (res.clapPlugin) {
                response = CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN;
                getClapPluginData(res, &respClapPlugin);
            }
            pluginMgr->unloadPlugin(res.plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
        }
    } catch (...) {
        log_message("exception while loading %s", req.szPath);
        response = CMD_PLUGIN_LOAD_ERROR;
    }

    log_message("runPluginTest end");
    threadSleep(25);
    pluginMgr->destroy();
    return response;
}

static int runScannerClient() {

    // Open the named pipe
    ipc_client client;
    log_message("client_connect");
    int ipcstatus = client.client_connect(SCAN_IPC_PIPE_NAME);
    if (ipcstatus) {
        log_message("Failed opening ipc_client: %d", ipcstatus);
        return 1;
    }


    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32 });
    auto& tls = daw_tls::getTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);

    pipe_msg_hdr hdr{};
    log_message("listening...");
    while (!userSentQuitRequest) {
        int retRead = readFromIPC(client, hdr);
        dbgassert(E_READ_ERR_BUF_SIZE != retRead);
        if (E_READ_OK != retRead) {
            log_message("hdr readFromIPC E_READ_ERR_PIPE");
            break;
        }
        if (hdr.cmd == CMD_PLUGIN_THREAD_QUIT) {
            break;
        }
        if (hdr.cmd == CMD_PLUGIN_LOAD_REQUEST) {
            request_type_vst24_t req;
            if (E_READ_OK != readFromIPC(client, req)) {
                log_message("req readFromIPC failed");
                break;
            }

            log_message("Load plugin %s", req.szPath);
            auto res = pluginMgr->loadPlugin(req.szPath, 0);
            if (res.library.state != DAW::Host::SharedLibState::SUCCESS) {
                log_message("Failed loading %s: %s (%d)", req.szPath, StringAsCStr(res.library.error), static_cast<int32_t>(res.library.state));
                int response = CMD_PLUGIN_LOAD_ERROR;
                writeToIPC(client, response);
            } else {
                if (res.clapPlugin && res.clapPlugin->pluginCount > 0) {
                    handles_t* handles     = res.shellPluginHandle;
                    String nameShellPlugin = res.name;
                    log_printf("loading clap plugin: %s\n", StringAsCStr(nameShellPlugin));

                    std::vector<response_type_clapplugin_t> entries;
                    response_type_shell_plugin_begin_t respShellPlugin;
                    safe_strcpy(respShellPlugin.szName, res.name);
                    respShellPlugin.szName[255] = 0;
                    auto fac = res.clapPlugin->getPluginFactory();
                    auto plugCount = fac->get_plugin_count(fac);
                    for (uint32_t i = 0; i < plugCount; ++i) {
                        const clap_plugin_descriptor_t* desc = fac->get_plugin_descriptor(fac, i);
                        if (desc && desc->name && desc->id) {
                            response_type_clapplugin_t _out{};
                            _out.pluginIndex = i;
                            safe_strcpy(_out.szName, desc->name);
                            safe_strcpy(_out.szEffectName, desc->name);
                            safe_strcpy(_out.szProductName, desc->id);
                            safe_strcpy(_out.szPath, req.szPath);
                            if (desc->vendor) 
                                safe_strcpy(_out.szVendorName, desc->vendor);
                            if (desc->version)
                                safe_strcpy(_out.szVersion, desc->version);
                            for (int i = 0; desc->features && desc->features[i]; ++i) {
                                auto entry = desc->features[i];
                                if (!strcmp(entry, CLAP_PLUGIN_FEATURE_INSTRUMENT)) {
                                    _out.isSynth = true;
                                }
                            }
                            _out.pluginCategory = _out.isSynth ? 1 : 0;
                            log_printf("Found clap plugin: %s '%s'\n", desc->id, desc->name);
                            entries.push_back(_out);
                        }
                    }
                    int32_t response = CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_SHELL;
                    writeToIPC(client, response);
                    respShellPlugin.numPlugins = (int) entries.size();
                    writeToIPC(client, respShellPlugin);
                    for (auto& entry : entries) {
                        response = CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_PLUGIN;
                        writeToIPC(client, response);
                        writeToIPC(client, entry);
                    }
                    response = CMD_PLUGIN_END_SUCCESS;
                    writeToIPC(client, response);
                } else if (res.library.type == DAW::Host::SharedLibPluginType::VST2_SHELL) {
                    handles_t* handles     = res.shellPluginHandle;
                    String nameShellPlugin = res.name;
                    log_printf("loading shell plugin: %s\n", StringAsCStr(nameShellPlugin));

                    char tempName[64] = { 0 };
                    struct shell_plugin_entry_t {
                        String name;
                        int32_t pluginUID;
                    };

                    std::vector<shell_plugin_entry_t> entries;

                    response_type_shell_plugin_begin_t respShellPlugin;
                    safe_strcpy(respShellPlugin.szName, res.name);
                    respShellPlugin.szName[255] = 0;
                    // loop over all shell plugin entries
                    VstIntPtr dispatchRet;
                    while ((dispatchRet = handles->aeffect->dispatcher(handles->aeffect, effShellGetNextPlugin, 0, 0, tempName, 0)) != 0) {
                        if (dispatchRet < 0)
                            log_printf("WARN: expected positive value for VST UID %zd\n", dispatchRet);

                        auto plugUniqueID = (VstInt32) (static_cast<uint64_t>(dispatchRet) & 0xFFFFFFFFULL);
                        // subplug needs a name
                        if (tempName[0] != 0) {
                            log_printf("plugUniqueID %d\t%s\n", plugUniqueID, tempName);
                            entries.push_back(shell_plugin_entry_t{ tempName, plugUniqueID });
                        } else {
                            log_printf("plugUniqueID %d\tNAME == NULL!\n", plugUniqueID);
                        }
                        tempName[0] = 0;
                    }
                    int32_t response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_SHELL;
                    writeToIPC(client, response);
                    respShellPlugin.numPlugins = (int) entries.size();
                    writeToIPC(client, respShellPlugin);
                    log_message("-- begin of shell plugin list --");
                    for (auto& entry : entries) {
                        if (userSentQuitRequest) break;
                        log_message("load shell entry: %08X", entry.pluginUID);

                        auto resShellPluginEntry = pluginMgr->loadPlugin(req.szPath, entry.pluginUID);
                        if (resShellPluginEntry.library.state != DAW::Host::SharedLibState::SUCCESS) {
                            log_message("Failed loading shell plugin %s: %s (%d)", req.szPath, StringAsCStr(res.library.error), static_cast<int32_t>(res.library.state));
                        } else {
                            dbgassert(resShellPluginEntry.plugin);
                            response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_PLUGIN;
                            writeToIPC(client, response);
                            response_type_vst24_t respShellPluginEntry;
                            if (res.vstPlugin)
                                getVSTPluginData(resShellPluginEntry, &respShellPluginEntry);
                            safe_strcpy(respShellPluginEntry.szName, entry.name);
                            writeToIPC(client, respShellPluginEntry);
                            log_message("unload shell entry: %08X", entry.pluginUID);
                            pluginMgr->unloadPlugin(resShellPluginEntry.plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                        }
                    }
                    log_message("-- end of shell plugin list --");
                    response = CMD_PLUGIN_END_SUCCESS;
                    writeToIPC(client, response);
                } else if (res.library.state == DAW::Host::SharedLibState::SUCCESS) {
                    dbgassert(res.plugin);
                    int response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN;
                    if (res.clapPlugin) {
                        response = CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN;
                    }
                    writeToIPC(client, response);
                    if (res.vstPlugin) {
                        response_type_vst24_t resp;
                        getVSTPluginData(res, &resp);
                        writeToIPC(client, resp);
                    } else if (res.clapPlugin) {
                        response_type_clapplugin_t resp;
                        getClapPluginData(res, &resp);
                        writeToIPC(client, resp);
                    }
                    pluginMgr->unloadPlugin(res.plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                    response = CMD_PLUGIN_END_SUCCESS;
                    writeToIPC(client, response);
                }
            }
        }
        threadSleep(25);
    }
    log_message("client_close()");
    client.client_close();
    threadSleep(25);
    host->destroy();
    return 0;
}

} // namespace PluginScannerImplementation

int main(int argc, char* argv[]) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    App::Platform::initPlatformEnvironment("daw");
    if (argc < 1) {
        String cwdPathDB = App::Platform::toUserdataPath("data/plugins.db3");
        log_message("Daw VST scanner version %s\n", BuildInfo::BUILD_BINARY_VERSION);
        log_message("This program can be run in server or client mode.");
        log_message("The server starts a client process that loads the VST2 DLL and scans it.");
        log_message("The client automatically connects to the server process via IPC and listens for commands.");
        log_message("The plugin database is stored here: %s", StringAsCStr(cwdPathDB));
        log_message("Command line options:");
        log_message("-test <path>\t\ttest single plugin");
        log_message("-client \t\trun client");
        log_message("-server \t\trun server");
        log_message("-wait   \t\t(server only)\tDo not start client process (Allows manual start)");
        log_message("-dry    \t\t(server only)\tCheck for new plugins but does not scan them");
        log_message("-update <plugin-name>\t(server only)\tRescan a specific plugin. Does partial name matching, case-insensitive");
        log_message("-rescan \t\t(server only)\tRescan all registered VST2 plugins, even if their disk timestamp has not changed");
        log_message("-path <directory>\t(server only)\tManually specify directory to scan for plugins");
        log_message("-timeout <seconds>\t(server only)\tSet the timeout for unresponsive plugins. Default is %d seconds", PluginScannerImplementation::timeoutdefault);
        log_message("\nThe default command to scan plugins is:");
        log_message("%s -server\n", argv[0]);
        return 0;
    }
#ifdef _WIN32
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) PluginScannerImplementation::ConsoleHandler, TRUE)) {
        log_lf(Log::L_ERROR, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
#endif
    auto& tls = daw_tls::initNewTls();

    if (argc == 1 || (argc > 1 && !strcmp("-server", argv[1]))) {
        PluginScannerImplementation::logPrefixIdx = PROC_SIDE_SERVER;
        PluginScannerImplementation::pluginscanner_server_options options;
        options.launchProcess              = true;
        options.dryRun                     = false;
        options.updatePattern              = "";
        options.fullRescan                 = false;
        options.checkDiskTimestamp         = true;
        options.unresponsiveTimeoutSeconds = PluginScannerImplementation::timeoutdefault;
        String vstPlugPath;
        String clapPluginPath;
        for (int i = 2; i < argc; i++) {
            if (argv[i] && strlen(argv[i]) > 2 && argv[i][0] == '-') {
                if (!strcmp(argv[i], "-wait")) {
                    options.launchProcess = false;
                }
                if (!strcmp(argv[i], "-dry")) {
                    options.dryRun = true;
                }
                if (!strcmp(argv[i], "-update") && i + 1 < argc) {
                    options.updatePattern      = argv[i + 1];
                    options.checkDiskTimestamp = false;
                    i++;
                }
                if (!strcmp(argv[i], "-rescan")) {
                    options.fullRescan = true;
                }
                if (!strcmp(argv[i], "-timeout") && i + 1 < argc) {
                    options.unresponsiveTimeoutSeconds = atoi(argv[i + 1]);
                    i++;
                }
                if (!strcmp(argv[i], "-path") && i + 1 < argc) {
                    vstPlugPath = argv[i + 1];
                    i++;
                }
                if (!strcmp(argv[i], "-clappath") && i + 1 < argc) {
                    clapPluginPath = argv[i + 1];
                    i++;
                }
            }
        }
        if (vstPlugPath.empty()) {
            loadSettings(*tls.settings);
            vstPlugPath = tls.settings->pluginsettings.pathVst2;
            log_message("settings.pluginsettings.pathVst2 '%s'", StringAsCStr(vstPlugPath));
        }
        if (clapPluginPath.empty()) {
            loadSettings(*tls.settings);
            clapPluginPath = tls.settings->pluginsettings.pathClap;
            log_message("settings.pluginsettings.pathClap '%s'", StringAsCStr(clapPluginPath));
        }
        App::Platform::shellExpandPath(vstPlugPath);
        App::Platform::sanitizePathToDirectory(vstPlugPath);
        App::Platform::shellExpandPath(clapPluginPath);
        App::Platform::sanitizePathToDirectory(clapPluginPath);
        if (clapPluginPath.empty() && vstPlugPath.empty()) {
            log_lf(Log::L_ERROR, "Error: settings.pluginsettings.pathVst2 / settings.pluginsettings.pathClap not configured\n");
            return EXIT_FAILURE;
        }
        log_message("vstPlugPath '%s'", StringAsCStr(vstPlugPath));
        options.vstPlugPath = vstPlugPath;
        log_message("clapPluginPath '%s'", StringAsCStr(clapPluginPath));
        options.clapPluginPath = clapPluginPath;
        runScannerServer(options);

        seqthreads::threadSleep(500);
    } else if (argc > 2 && !strcmp("-test", argv[argc - 2])) {
        setExceptionHandler();
        seqthreads::threadSleep(120);
        PluginScannerImplementation::request_type_vst24_t req;
        String fPath = argv[argc - 1];
        safe_strcpy(req.szPath, fPath);
        PluginScannerImplementation::response_type_vst24_plugin_t respPlugin;
        PluginScannerImplementation::response_type_clapplugin_t respClap;
        int retCode = PluginScannerImplementation::runPluginTest(req, respPlugin, respClap);
        if (retCode == CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN) {
            log_message("Vst Plugin %s: Good", StringAsCStr(fPath));
        } else if (retCode == CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN) {
            log_message("Clap Plugin %s: Good", StringAsCStr(fPath));
        } else {
            log_lf(Log::L_WARN, "Plugin %s: Failed %d\n", StringAsCStr(fPath), retCode);
        }
    } else if (argc > 0 && !strcmp("-client", argv[argc - 1])) {
        PluginScannerImplementation::logPrefixIdx = PROC_SIDE_CLIENT;
        setExceptionHandler();
        seqthreads::threadSleep(120);
        PluginScannerImplementation::runScannerClient();
    } else {
        log_lf(Log::L_ERROR, "No command. Use -server to update plugin database\n");
    }
    return 0;
}
