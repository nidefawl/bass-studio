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

#include "host/plugin/vst_plugin.h"
#include "host/plugin/vst_plugin_handles.h"
#include "host/vst_host.h"
#include "threads/childprocessthread.h"
#include "vstsdk-host-2.4/aeffectx.h"
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
#include <iostream>
#include <memory>
#ifdef _WIN32
#include "platform/win/windowsize.h"
#include "platform/win/platform_win.h"
#include <Windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <climits>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
inline String APPLE_getExecutablePath() {
    String ret = "plugin_scan";
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        ret = path;
    }
    return ret;
}
#endif

static const char* szLogPrefixes[2] = {
    "SRV: ",
    "CLI: ",
};

constexpr int32_t timeoutdefault = 120;
static int logPrefixIdx = 0;

#define LOG_MSG(prefix, fmtString, ...) printf("%s" fmtString "\n", prefix, ##__VA_ARGS__);
#define LOG(fmtString, ...)                                         \
    LOG_MSG(szLogPrefixes[logPrefixIdx], fmtString, ##__VA_ARGS__); \
    fflush(stdout)

#define E_WRITE_OK 0
#define E_WRITE_ERR_PIPE 1
#define E_WRITE_ERR_BUF_SIZE 2
#define E_READ_OK 0
#define E_READ_ERR_PIPE 1
#define E_READ_ERR_BUF_SIZE 2


#define CMD_PLUGIN_LOAD_ERROR 1
#define CMD_PLUGIN_LOAD_REQUEST 2
#define CMD_PLUGIN_THREAD_QUIT 3
#define CMD_PLUGIN_LOAD_SUCCESS_PLUGIN 4
#define CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_SHELL 5
#define CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_PLUGIN 6
#define CMD_PLUGIN_END_SUCCESS 7
#define NUM_BUFS (16 * 1024)
#define SCAN_IPC_PIPE_NAME "DAW1pipc"

using seqthreads::threadSleep;

void deleteApp() {
}

std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) {
    return nullptr;
}

void startApp(std::shared_ptr<AppCtrl>& app) {
}

bool userSentQuitRequest = false;
bool inConnectNamedPipe  = false;
#ifdef _WIN32
static BOOL WINAPI ConsoleHandler(DWORD dwType) {
    userSentQuitRequest = true;
    if (dwType == CTRL_C_EVENT) {
        if (inConnectNamedPipe) {
            exit(0);
        }
        log_printf("CTRL_C\n", 0);
        return true;
    }
    return false;
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
    bool isSynth{ 0 };
    char szVendorName[256]{ 0 };
    char szProductName[256]{ 0 };
    char szEffectName[256]{ 0 };
    char szShellPluginName[256]{ 0 };
};
struct response_type_vst24_plugin_t : response_type_vst24_t {
};
struct response_type_vst24_shell_plugin_t : response_type_t {
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
        LOG("IPCrecvBuffer failed");
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
        LOG("writeToBuffer failed");
        return E_WRITE_ERR_BUF_SIZE;
    }
    if (!IPCsendBuffer(ipcConnection, sendBuffer)) {
        LOG("IPCsendBuffer failed");
        return E_WRITE_ERR_PIPE;
    }
    return E_WRITE_OK;
}

void createTables(SQLite::Database& db);

struct vstscanner_server_options {
    String vstPlugPath;
    bool dryRun             = false;
    bool fullRescan         = false;
    bool launchProcess      = true;
    bool checkDiskTimestamp = true;
    String updatePattern;
    int32_t unresponsiveTimeoutSeconds = timeoutdefault;
};

static void getPluginData(vstplugin* plugin, response_type_vst24_t* _out) {
    AEffect* aeffect     = plugin->handle->aeffect;
    _out->uniqueID       = aeffect->uniqueID;
    _out->version        = aeffect->version;
    _out->vstVersion     = plugin->vstVersion;
    _out->pluginCategory = plugin->pluginCategory;
    strncpy(_out->szName, StringAsCStr(plugin->sName), plugin->sName.length());
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

static int readClientResponses(vstscanner_server_options& options, ipc_server& server, request_type_vst24_t& req, SQLite::Statement& queryInsertPlugin, FileFound& file, int64_t timeDisk, bool forcedisable) {

    auto timeStartScan_ms     = getTimeMillis();
    auto timeoutPluginScan_ms = options.unresponsiveTimeoutSeconds * 1000;
    uint32_t notificationStep     = 0;
    int nPluginsScanned           = 0;
    while (!userSentQuitRequest) {
        int32_t responseType    = 0;
        int peakRdBufSizeResult = server.peekReadBufferSize();
        if (peakRdBufSizeResult < sizeof(responseType)) {
            auto timeSince_ms = getTimeMillis() - timeStartScan_ms;
            if (-1 == peakRdBufSizeResult || (timeSince_ms > timeoutPluginScan_ms)) {
                LOG("TIMEOUT: Plugin %s timed out after %d ms", req.szPath, timeoutPluginScan_ms);
                return -4;
            } else {
                if (notificationStep != (timeSince_ms / 1000)) {
                    uint64_t secondsLeft = math::max<uint64_t>(0, timeoutPluginScan_ms - timeSince_ms) / 1000;
                    notificationStep     = timeSince_ms / 1000;
                    LOG("Waiting for Plugin %s to respond... %zus left", req.szPath, secondsLeft);
                }
                threadSleep(50);
                continue;
            }
        }
        if (E_READ_OK != readFromIPC(server, responseType)) {
            LOG("failed reading responseType int32_t");
            return -3;
        }
        timeStartScan_ms = getTimeMillis();
        switch (responseType) {
            case CMD_PLUGIN_LOAD_SUCCESS_PLUGIN: {
                response_type_vst24_plugin_t respShellPlugin;
                LOG("READ response_type_vst24_plugin_t");
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    LOG("failed reading response_type_vst24_plugin_t");
                    return -3;
                }

                printf("%s %s\n", respShellPlugin.szPath, "GOOD");
                auto& data = respShellPlugin;
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
                    queryInsertPlugin.bind(bndIdx++, 0);
                    /*int insertRowsAffected = */ queryInsertPlugin.exec();
                    nPluginsScanned++;
                } catch (SQLite::Exception& e) {
                    std::cout << "queryInsertPlugin exception: " << e.getErrorStr() << std::endl;
                    return -5;
                }

                break;
            }
            case CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_SHELL: {
                LOG("READ response_type_vst24_shell_plugin_t");
                response_type_vst24_shell_plugin_t respShellPlugin;
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    LOG("failed reading response_type_vst24_shell_plugin_t");
                    return -3;
                }
                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_PLUGIN: {
                LOG("READ response_type_vst24_t");
                response_type_vst24_t respShellPluginEntry;
                if (E_READ_OK != readFromIPC(server, respShellPluginEntry)) {
                    LOG("failed reading response_type_vst24_t");
                    return -3;
                }


                auto& data = respShellPluginEntry;
                printf("%s %s %s isSynth: %d, uid %08X\n", StringAsCStr(file.path), data.szName, "GOOD", data.isSynth, data.uniqueID);
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
                    std::cout << "queryInsertPlugin exception: " << e.getErrorStr() << std::endl;
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

static int runScannerServer(vstscanner_server_options options) {

    if (!options.updatePattern.empty()) {
        LOG("Update *%s*", StringAsCStr(options.updatePattern));
    }
    try {
        String cwdPathDB = App::Platform::toUserdataPath("data/plugins.db3");
        SQLite::Database db(cwdPathDB, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";
        createTables(db);

        threadSleep(1000);

        std::vector<FileFound> files;
        findFilesWithExt(options.vstPlugPath, PLATFORM_PLUGIN_EXT, true, files);
        LOG("Found %u files", (uint32_t) files.size());
        if (files.empty()) {
            return 1;
        }
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

        recvbuf_t bufferRecv;
        ipc_server server;
        int ipc_status = server.server_open(SCAN_IPC_PIPE_NAME);
        if (ipc_status) {
            LOG("Failed opening ipc_server: %d", ipc_status);
            return 1;
        }
        std::unique_ptr<ProcessThread> thread = nullptr;
        bool pipeConnected                    = false;
        SQLite::Statement queryPlugin(db, "SELECT id, moddate, forcedisable, requestRescan, uid, shellplugin FROM plugins where path == ?");
        SQLite::Statement queryInsertPlugin(db, "INSERT INTO "
                                                "plugins(isSynth, uid, version, vstVersion, category, moddate, state, path, name, vendorName, productName, effectName, requestRescan, forcedisable, shellplugin) "
                                                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
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
                LOG("REMOVE plugin: File at %s is missing", StringAsCStr(path));
            }
            if (!options.dryRun) {
                queryDelete.reset();
                queryDelete.bind(1, queryAll.getColumn(0).getInt());
                queryDelete.bind(2, path);
                queryDelete.exec();
            }
        }

        for (FileFound& file : files) {
            if (userSentQuitRequest) {
                break;
            }
            FileTimeGetter filetime(file.path);
            int64_t timeDisk   = filetime.getWriteTimeI64();
            int id             = -1;
            int32_t uid        = -1;
            bool isShellPlugin = false;
            bool needScan      = true;
            bool forcedisable  = false;
            queryPlugin.reset();
            queryPlugin.bind(1, file.path);
            String reason = "New or not a VST dll";
            if (queryPlugin.executeStep()) {
                id            = queryPlugin.getColumn(0).getInt();
                uid           = queryPlugin.getColumn(4).getInt();
                isShellPlugin = queryPlugin.getColumn(5).getInt() > 0;
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
                //    LOG("%s is up to date", StringAsCStr(file.name));
                continue;
            }
            LOG("%s needs update: %s", StringAsCStr(file.path), StringAsCStr(reason));
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
                    LOG("Failed starting client");
                    break;
                }
                LOG("Thread is up");
            }
            bool resetConnection = false;
            if (!pipeConnected && (!options.launchProcess || (thread && thread->isRunning()))) {
                inConnectNamedPipe    = true;
                int ipcstatus_connect = server.server_accept();
                inConnectNamedPipe    = false;
                if (0 != ipcstatus_connect) {
                    LOG("ipc_server::server_accept() failed: %d", ipcstatus_connect);
                    resetConnection = true;
                } else {
                    pipeConnected = true;
                }
            }
            if (pipeConnected) {
                pipe_msg_hdr hdr = { CMD_PLUGIN_LOAD_REQUEST };
                if (E_WRITE_OK != writeToIPC(server, hdr)) {
                    LOG("error writeToIPC pipe_msg_hdr");
                    resetConnection = true;
                }
                request_type_vst24_t req;
                strncpy(req.szPath, StringAsCStr(file.path), file.path.length());
                if (E_WRITE_OK != writeToIPC(server, req)) {
                    LOG("error writeToIPC request_type_vst24_t");
                    resetConnection = true;
                }
                if (!options.dryRun && pipeConnected) {
                    if (id > 0) {
                        queryDelete.reset();
                        queryDelete.bind(1, id);
                        queryDelete.bind(2, req.szPath);
                        queryDelete.exec();
                    }
                    int ret = readClientResponses(options, server, req, queryInsertPlugin, file, timeDisk, forcedisable);

                    if (ret < 0) {
                        resetConnection = true;
                    }
                }
            } else {
                resetConnection = true;
            }
            if (userSentQuitRequest)
                break;
            if (resetConnection) {
                resetConnection = false;
                LOG("Reset scanner client process");
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
        std::cout << "SQLite exception: " << e.getErrorStr() << std::endl;
        std::cout << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cout << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unhandled exception" << std::endl;
    }
    return 0;
}

static int runPluginTest(request_type_vst24_t req, response_type_vst24_plugin_t& respPlugin) {
    LOG("runPluginTest");

    auto vsthostInstance = std::make_unique<vsthost>();
    vsthost::assignMasterCallback(vsthostInstance.get());
    vsthostInstance->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32 });

    daw_tls::tlsinstance initTls;
    initTls.tlsInitialized = true;
    initTls.config         = new app_config_t{};
    initTls.host           = vsthostInstance.get();
    daw_tls::setTls(initTls);

    int response = 0;
    LOG("Load plugin %s", req.szPath);
    try {

        vstpluginloadres res = vsthostInstance->loadPlugin(req.szPath, 0);
        LOG("result: %d", res.result);
        if (res.result < 0) {
            response = CMD_PLUGIN_LOAD_ERROR;
        } else {
            dbgassert(res.result == 0);
            dbgassert(res.plugin);
            response = CMD_PLUGIN_LOAD_SUCCESS_PLUGIN;
            getPluginData(res.plugin, &respPlugin);
            vsthostInstance->unloadPlugin(res.plugin, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
        }
    } catch (...) {
        LOG("exception while loading %s", req.szPath);
        response = -1;
    }

    LOG("runPluginTest end");
    threadSleep(25);
    vsthost::getInstance()->destroy();
    return response;
}

static int runScannerClient() {

    // Open the named pipe
    ipc_client client;
    LOG("client_connect");
    int ipcstatus = client.client_connect(SCAN_IPC_PIPE_NAME);
    if (ipcstatus) {
        LOG("Failed opening ipc_client: %d", ipcstatus);
        return 1;
    }

    auto vsthostInstance = std::make_unique<vsthost>();
    vsthost::assignMasterCallback(vsthostInstance.get());
    vsthostInstance->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32 });

    daw_tls::tlsinstance initTls;
    initTls.tlsInitialized = true;
    initTls.config         = new app_config_t{};
    initTls.host           = vsthostInstance.get();
    daw_tls::setTls(initTls);

    pipe_msg_hdr hdr{};
    recvbuf_t bufferRecv;
    LOG("listening...");
    while (!userSentQuitRequest) {
        int retRead = readFromIPC(client, hdr);
        dbgassert(E_READ_ERR_BUF_SIZE != retRead);
        if (E_READ_OK != retRead) {
            LOG("hdr readFromIPC E_READ_ERR_PIPE");
            break;
        }
        if (hdr.cmd == CMD_PLUGIN_THREAD_QUIT) {
            break;
        }
        if (hdr.cmd == CMD_PLUGIN_LOAD_REQUEST) {
            request_type_vst24_t req;
            if (E_READ_OK != readFromIPC(client, req)) {
                LOG("req readFromIPC failed");
                break;
            }

            LOG("Load plugin %s", req.szPath);
            try {

                vstpluginloadres res = vsthostInstance->loadPlugin(req.szPath, 0);
                LOG("result: %d", res.result);
                if (res.result < 0) {
                    int response = CMD_PLUGIN_LOAD_ERROR;
                    writeToIPC(client, response);
                } else {
                    if (res.result == 1) {
                        handles_t* handles     = res.shellPluginHandle;
                        String nameShellPlugin = res.name;
                        log_printf("loading shell plugin: %s\n", StringAsCStr(nameShellPlugin));

                        char tempName[64] = { 0 };
                        struct shell_plugin_entry_t {
                            String name;
                            int32_t pluginUID;
                        };

                        std::vector<shell_plugin_entry_t> entries;

                        response_type_vst24_shell_plugin_t respShellPlugin;
                        strncpy(respShellPlugin.szName, StringAsCStr(res.name), math::min<size_t>(255, res.name.length() + 1));
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
                        int32_t response = CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_SHELL;
                        writeToIPC(client, response);
                        respShellPlugin.numPlugins = (int) entries.size();
                        writeToIPC(client, respShellPlugin);
                        LOG("-- begin of shell plugin list --");
                        for (auto& entry : entries) {
                            if (userSentQuitRequest) break;
                            LOG("load shell entry: %08X", entry.pluginUID);

                            vstpluginloadres resShellPluginEntry = vsthostInstance->loadPlugin(req.szPath, entry.pluginUID);
                            if (resShellPluginEntry.result != 0) {
                                LOG("FAILED LOADING SHELL PLUGIN: %d", resShellPluginEntry.result);
                            } else {
                                dbgassert(resShellPluginEntry.plugin);
                                response = CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_PLUGIN;
                                writeToIPC(client, response);
                                response_type_vst24_t respShellPluginEntry;
                                getPluginData(resShellPluginEntry.plugin, &respShellPluginEntry);
                                strncpy(respShellPluginEntry.szName, StringAsCStr(entry.name), math::min<size_t>(255U, entry.name.length() + 1));
                                writeToIPC(client, respShellPluginEntry);
                                LOG("unload shell entry: %08X", entry.pluginUID);
                                vsthostInstance->unloadPlugin(resShellPluginEntry.plugin, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                            }
                        }
                        LOG("-- end of shell plugin list --");
#ifdef _WIN32
                        FreeLibrary((HMODULE) handles->hmodule);
#endif
                        response = CMD_PLUGIN_END_SUCCESS;
                        writeToIPC(client, response);
                    } else if (res.result == 0) {
                        dbgassert(res.plugin);
                        int response = CMD_PLUGIN_LOAD_SUCCESS_PLUGIN;
                        writeToIPC(client, response);
                        response_type_vst24_plugin_t respPlugin;
                        getPluginData(res.plugin, &respPlugin);
                        writeToIPC(client, respPlugin);
                        vsthostInstance->unloadPlugin(res.plugin, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                        response = CMD_PLUGIN_END_SUCCESS;
                        writeToIPC(client, response);
                    }
                }
            } catch (...) {
                LOG("exception while loading %s", req.szPath);
                int response = -1;
                writeToIPC(client, response);
            }
        }
        //LOG("client sendData()");
        //if (!sendData(&client, &hdr, &data)) {
        //    LOG("sendData failed");
        //    break;
        //}
        threadSleep(25);
    }
    LOG("client_close()");
    client.client_close();
    threadSleep(25);
    vsthost::getInstance()->destroy();
    return 0;
}

int main(int argc, char* argv[]) {
    seqthreads::registerThread("mainthread");
    String cwdPath;
    App::Platform::initPlatformEnvironment("daw");
    if (argc <= 1) {
        String cwdPathDB = App::Platform::toUserdataPath("data/plugins.db3");
        printf("Daw VST scanner version %s\n\n", BuildInfo::BUILD_BINARY_VERSION);
        printf("This program can be run in server or client mode.\n");
        printf("The server starts a client process that loads the VST2 DLL and scans it.\n");
        printf("The client automatically connects to the server process via IPC and listens for commands.\n");
        printf("The plugin database is stored here: %s\n", StringAsCStr(cwdPathDB));
        printf("Command line options:\n");
        printf("-test <path>\t\ttest single plugin\n");
        printf("-client \t\trun client\n");
        printf("-server \t\trun server\n");
        printf("-wait   \t\t(server only)\tDo not start client process. allows manual start of clients.\n");
        printf("-dry    \t\t(server only)\tCheck for new plugins but does not scan them\n");
        printf("-update <plugin-name>\t(server only)\tRescan a specific plugin. Does partial name matching, case-insensitive\n");
        printf("-rescan \t\t(server only)\tRescan all registered VST2 plugins, even if their disk timestamp has not changed\n");
        printf("-timeout <seconds>\t(server only)\tSet the timeout for unresponsive plugins. Default is %d seconds\n", timeoutdefault);
        printf("\nThe default command to scan plugins is:\n");
        printf("%s -server\n", argv[0]);
        fflush(stdout);
        return 0;
    }
#ifdef _WIN32
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleHandler, TRUE)) {
        fprintf(stderr, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
#endif
    using DAW::settings;
    settings           = loadSettings();
    String vstPlugPath = settings.pluginPath;
    LOG("pluginPath '%s'", StringAsCStr(vstPlugPath));
    if (vstPlugPath.empty()) {
        fprintf(stderr, "Error: pluginPath not configured\n");
        return EXIT_FAILURE;
    }

    if (argc > 1 && !strcmp("-server", argv[1])) {
        vstscanner_server_options options;
        options.launchProcess              = true;
        options.dryRun                     = false;
        options.updatePattern              = "";
        options.fullRescan                 = false;
        options.checkDiskTimestamp         = true;
        options.vstPlugPath                = vstPlugPath;
        options.unresponsiveTimeoutSeconds = timeoutdefault;
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
                }
                if (!strcmp(argv[i], "-rescan")) {
                    options.fullRescan = true;
                }
                if (!strcmp(argv[i], "-timeout") && i + 1 < argc) {
                    options.unresponsiveTimeoutSeconds = atoi(argv[i + 1]);
                }
            }
        }
        runScannerServer(options);

        LOG("Done.");
        threadSleep(500);
    } else if (argc > 2 && !strcmp("-test", argv[argc - 2])) {
        setExceptionHandler();
        threadSleep(120);
        request_type_vst24_t req;
        String fPath = argv[argc - 1];
        strncpy(req.szPath, StringAsCStr(fPath), fPath.length());
        response_type_vst24_plugin_t respPlugin;
        int retCode = runPluginTest(req, respPlugin);
        if (retCode == CMD_PLUGIN_LOAD_SUCCESS_PLUGIN) {
            log_printf("Plugin %s: Good\n", StringAsCStr(fPath));
        } else {
            log_printf("Plugin %s: Failed %d\n", StringAsCStr(fPath), retCode);
        }
    } else if (argc > 0 && !strcmp("-client", argv[argc - 1])) {
        logPrefixIdx = 1;
        setExceptionHandler();
        threadSleep(120);
        runScannerClient();
    } else {
        log_printf("No command. Use -server to update plugin database\n", 0);
    }
    return 0;
}
