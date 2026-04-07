#include "pluginscanner.hpp"
#include "appconfig.hpp"
#include "appsettings.hpp"
#include "assert_dbg.h"
#include "buildinfo.h"
#include "exceptions.hpp"
#include "fileio.hpp"
#include "fileio.hpp"
#include "host/host_plugin_loadresult.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/host.hpp"
#include "host/plugin/clap/clap-plugin.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/vst/vstplugin-handles.hpp"
#include "host/plugin/vst/vstplugin.hpp"
#include "host/plugin/vst3/vst3plugin.hpp"
#include "ipc.hpp"
#include "logging.hpp"
#include "platform.hpp"
#include "str_util.hpp"
#include "thread.hpp"
#include "threads/childprocessthread.hpp"
#include "tls.hpp"
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <public.sdk/source/vst/hosting/processdata.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include <vstsdk-host-2.4/aeffectx.h>

#ifdef _WIN32
#include "platform/win/platform_win.hpp"
#include <windows.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <climits>
#include <utility>
#include <csignal>
#endif

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

void createTables(SQLite::Database& db);

namespace DAW::Host::PluginScanner {

#define PROC_SIDE_NONE 0
#define PROC_SIDE_SERVER 1
#define PROC_SIDE_CLIENT 2
static const char* szLogPrefixes[3] = {
    "",
    "SRV: ",
    "CLI: ",
};

static int logPrefixIdx = PROC_SIDE_NONE;

#define log_message(...)                                                                      \
    do {                                                                                      \
        String msg = String(::DAW::Host::PluginScanner::szLogPrefixes[::DAW::Host::PluginScanner::logPrefixIdx]); \
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
#define CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGIN 10
#define CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGINSHELL_SHELL 11
#define CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGINSHELL_PLUGIN 12
#define CMD_PLUGIN_END_SUCCESS 13
#define SCAN_IPC_BUFFER_SIZE (16 * 1024)
#define SCAN_IPC_PIPE_NAME "DAW1pipc"

int32_t timeoutdefault = 120;


bool inConnectNamedPipe  = false;

enum vst_metadata_flags_e : int32_t {
    VST_FLAGS_NONE            = 0,
    VST_FLAGS_LOADED_PLUGIN   = 1,
    VST_FLAGS_IS_SHELL_PLUGIN = 2
};

struct pipe_msg_hdr {
    uint32_t cmd;
};
struct response_type_t {
    char szPath[1025]{ 0 };
    char szName[257]{ 0 };
};
struct request_type_load_plugin_t final : response_type_t {
    uint32_t moduleType{ 0 };
};
struct response_type_vst24_t : response_type_t {
    vst_metadata_flags_e flags{ VST_FLAGS_NONE };
    uint32_t uniqueID{ 0 };
    uint32_t version{ 0 };
    uint32_t vstVersion{ 0 };
    uint32_t pluginCategory{ 0 };
    bool isSynth{ false };
    char szVendorName[257]{ 0 };
    char szProductName[257]{ 0 };
    char szEffectName[257]{ 0 };
    char szShellPluginName[257]{ 0 };
};
struct response_type_clapplugin_t final : response_type_t {
    uint32_t pluginIndex{ 0 };
    uint32_t pluginCategory{ 0 };
    bool isSynth{ false };
    char szVersion[257]{ 0 };
    char szVendorName[257]{ 0 };
    char szProductName[257]{ 0 };
    char szEffectName[257]{ 0 };
};
struct response_type_vst3plugin_t final : response_type_t {
    char vst3UUid[33]{ 0 };
    uint32_t pluginCategory{ 0 };
    bool isSynth{ false };
    uint32_t version{ 0 };
    uint32_t vstVersion{ 0 };
    char szVendorName[257]{ 0 };
};
struct response_type_vst24_plugin_t final : response_type_vst24_t {
};
struct response_type_shell_plugin_begin_t final : response_type_t {
    int numPlugins{};
};
struct recvbuf_t {
    char buf[SCAN_IPC_BUFFER_SIZE]{};
    char* pos = nullptr;
    char* end = nullptr;
};

template<typename T>
bool writeToBuffer(recvbuf_t& buf, T& hdr) {
    dbgassert(buf.pos);
    if (static_cast<size_t>(buf.pos - buf.buf) + sizeof(hdr) <= SCAN_IPC_BUFFER_SIZE) {
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
    auto lenRcvd = conn.readData(bufferRecv.buf, SCAN_IPC_BUFFER_SIZE);
    bufferRecv.end = bufferRecv.buf + lenRcvd;
    return SCAN_IPC_BUFFER_SIZE == lenRcvd;
}

template<typename IPC>
bool IPCsendBuffer(IPC& conn, recvbuf_t& bufferRecv) {
    bufferRecv.end = bufferRecv.pos;
    bufferRecv.pos = bufferRecv.buf;
    return SCAN_IPC_BUFFER_SIZE == conn.sendData(bufferRecv.buf, SCAN_IPC_BUFFER_SIZE);
}

/** uses function scope static buffer: NOT THREADSAFE */
template<typename IPC, typename T>
int readFromIPC(IPC& ipcConnection, T& hdr) {
    static recvbuf_t recvBuf;
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

static void getVSTPluginData(DAW::Host::LoadResultPluginImpl& res, response_type_vst24_t* _out) {
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

static void getClapPluginData(DAW::Host::LoadResultPluginImpl& res, response_type_clapplugin_t* _out) {
    auto plugin = res.clapPlugin;
    _out->pluginIndex = 0;
    _out->pluginCategory = plugin->getModuleCategory();
    safe_strcpy(_out->szName, plugin->sName);
    safe_strcpy(_out->szPath, res.path);
    _out->isSynth = plugin->isSynth;
    auto clapPlugDesc = plugin->getDescription();
    safe_strcpy(_out->szProductName, clapPlugDesc.id);
    safe_strcpy(_out->szVendorName, clapPlugDesc.vendor);
    safe_strcpy(_out->szEffectName, clapPlugDesc.name);
    safe_strcpy(_out->szVersion, clapPlugDesc.version);
}

static void getVst3PluginDataFromClassInfo(const String& path, const VST3::Hosting::ClassInfo& classInfo, response_type_vst3plugin_t* _out) {
    safe_strcpy(_out->vst3UUid, classInfo.ID().toString(true));
    _out->pluginCategory = 0;
    safe_strcpy(_out->szName, classInfo.name());
    safe_strcpy(_out->szPath, path);
    _out->isSynth = false;
    for (auto& subcat : classInfo.subCategories()) {
        if (subcat == "Instrument" || subcat == "Synth" || subcat == "Sampler" || subcat == "Drum") {
            _out->isSynth = true;
            break;
        }
    }
    String version = classInfo.version();
    StrUtil::StringReplace(version, ".", "");
    StrUtil::StringReplace(version, ",", "");
    _out->version = atoi(version.c_str());
    String vstVersion = classInfo.sdkVersion();
    StrUtil::StringReplace(vstVersion, "VST", "");
    StrUtil::StringReplace(vstVersion, ".", "");
    StrUtil::StringReplace(vstVersion, ",", "");
    StrUtil::StringReplace(vstVersion, " ", "");
    _out->vstVersion = atoi(vstVersion.c_str());
    safe_strcpy(_out->szVendorName, classInfo.vendor());
}

static int waitTimeout(ipc_server& server, ProcessThread* thread, bool& userSentQuitRequest, const char* plugName, int minReadBuffSize, int64_t timeStartScan_ms, int64_t timeoutPluginScan_ms) {
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
            seqthreads::threadSleep(300);
            continue;
        }
        break;
    }
    return 0;
}

static int handleClientResponse(const pluginscanner_server_options& options, ipc_server& server, ProcessThread* thread, bool& userSentQuitRequest, const request_type_load_plugin_t& req, SQLite::Statement& queryInsertPlugin, FileFound& file, int64_t timeDisk, bool forcedisable, const String& pluginPath) {
    auto timeStartScan_ms     = getTimeMillis();
    int64_t timeoutPluginScan_ms = options.unresponsiveTimeoutSeconds * int64_t(1000);
    int nPluginsScanned           = 0;
    while (!userSentQuitRequest) {
        int32_t responseType    = 0;
        if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(responseType)), timeStartScan_ms, timeoutPluginScan_ms)) {
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
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respLoadSinglePlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respLoadSinglePlugin)) {
                    log_message("failed reading response_type_clapplugin_t");
                    return -3;
                }

                log_lf(Log::L_INFO, "Plugin '%s': Status: %s\n", respLoadSinglePlugin.szPath, "GOOD");
                auto& data = respLoadSinglePlugin;
                String relPath = file.name;
                if (file.path.length() > pluginPath.length()) {
                    relPath = file.path.substr(pluginPath.length());
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
                    queryInsertPlugin.bind(bndIdx++, timeDisk);
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
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respLoadSinglePlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respLoadSinglePlugin)) {
                    log_message("failed reading response_type_vst24_plugin_t");
                    return -3;
                }

                log_lf(Log::L_INFO, "Plugin '%s': Status: %s\n", respLoadSinglePlugin.szPath, "GOOD");
                auto& data = respLoadSinglePlugin;
                String relPath = file.name;
                if (file.path.length() > pluginPath.length()) {
                    relPath = file.path.substr(pluginPath.length());
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
                    queryInsertPlugin.bind(bndIdx++, timeDisk);
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
            case CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGIN: {
                response_type_vst3plugin_t respLoadSinglePlugin;
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respLoadSinglePlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respLoadSinglePlugin)) {
                    log_message("failed reading response_type_vst3plugin_t");
                    return -3;
                }

                log_lf(Log::L_INFO, "Plugin '%s': Status: %s\n", respLoadSinglePlugin.szPath, "GOOD");
                auto& data = respLoadSinglePlugin;
                String relPath = file.name;
                if (file.path.length() > pluginPath.length()
                    && file.path.find(pluginPath) == 0) {
                    relPath = file.path.substr(pluginPath.length());
                    replaceString(relPath, FILE_PATHSEP_STR, "/");
                }
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, 2); // vst3 plugin
                    queryInsertPlugin.bind(bndIdx++, 0);
                    queryInsertPlugin.bind(bndIdx++, data.version);
                    queryInsertPlugin.bind(bndIdx++, data.vstVersion);
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);
                    queryInsertPlugin.bind(bndIdx++, data.vst3UUid);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
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
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respShellPlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    log_message("failed reading response_type_shell_plugin_begin_t");
                    return -3;
                }
                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGINSHELL_SHELL: {
                response_type_shell_plugin_begin_t respShellPlugin;
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respShellPlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    log_message("failed reading response_type_shell_plugin_begin_t");
                    return -3;
                }
                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGINSHELL_PLUGIN: {
                response_type_vst3plugin_t respLoadSinglePlugin;
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respLoadSinglePlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respLoadSinglePlugin)) {
                    log_message("failed reading response_type_vst3plugin_t");
                    return -3;
                }

                log_lf(Log::L_INFO, "Plugin '%s': Status: %s\n", respLoadSinglePlugin.szPath, "GOOD");
                auto& data = respLoadSinglePlugin;
                String relPath = file.name;
                if (file.path.length() > pluginPath.length()
                    && file.path.find(pluginPath) == 0) {
                    relPath = file.path.substr(pluginPath.length());
                    replaceString(relPath, FILE_PATHSEP_STR, "/");
                }
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, 2); // vst3 plugin
                    queryInsertPlugin.bind(bndIdx++, 0);
                    queryInsertPlugin.bind(bndIdx++, data.version);
                    queryInsertPlugin.bind(bndIdx++, data.vstVersion);
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);
                    queryInsertPlugin.bind(bndIdx++, data.vst3UUid);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
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
            }
            case CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_SHELL: {
                response_type_shell_plugin_begin_t respShellPlugin;
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(respShellPlugin)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, respShellPlugin)) {
                    log_message("failed reading response_type_shell_plugin_begin_t");
                    return -3;
                }
                log_lf(Log::L_INFO, "Loaded clap plugin shell with %d plugins\n", respShellPlugin.numPlugins);
                break;
            } break;
            case CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGINSHELL_PLUGIN: {
                response_type_clapplugin_t data;
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(data)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, data)) {
                    log_message("failed reading response_type_clapplugin_t");
                    return -3;
                }
                String relPath = file.name;
                if (file.path.length() > pluginPath.length()) {
                    relPath = file.path.substr(pluginPath.length());
                    replaceString(relPath, FILE_PATHSEP_STR, "/");
                }
                log_lf(Log::L_INFO, "Found CLAP shell plugin %s: Status: GOOD\n", StringAsCStr(file.path));
                try {
                    queryInsertPlugin.reset();
                    int bndIdx = 1;
                    queryInsertPlugin.bind(bndIdx++, data.isSynth);
                    queryInsertPlugin.bind(bndIdx++, 1); // clap plugin
                    queryInsertPlugin.bind(bndIdx++, data.pluginIndex);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, 0); // vstVersion
                    queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
                    queryInsertPlugin.bind(bndIdx++, timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
                    queryInsertPlugin.bind(bndIdx++, data.szName);
                    queryInsertPlugin.bind(bndIdx++, data.szVendorName);  // vendor
                    queryInsertPlugin.bind(bndIdx++, data.szProductName); // product
                    queryInsertPlugin.bind(bndIdx++, data.szEffectName);  // effect
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
            case CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_PLUGIN: {
                response_type_vst24_t data;
                if (waitTimeout(server, thread, userSentQuitRequest, req.szPath, static_cast<int>(sizeof(data)), timeStartScan_ms, timeoutPluginScan_ms)) {
                    return -4;
                }
                if (E_READ_OK != readFromIPC(server, data)) {
                    log_message("failed reading response_type_vst24_t");
                    return -3;
                }
                log_lf(Log::L_INFO, "Shell plugin %s %s %s isSynth: %d, uid %08X\n", StringAsCStr(file.path), data.szName, "GOOD", data.isSynth, data.uniqueID);
                String relPath = file.name;
                if (file.path.length() > pluginPath.length()) {
                    relPath = file.path.substr(pluginPath.length());
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
                    queryInsertPlugin.bind(bndIdx++, timeDisk);
                    queryInsertPlugin.bind(bndIdx++, 1);
                    queryInsertPlugin.bind(bndIdx++, file.path);
                    queryInsertPlugin.bind(bndIdx++, relPath);
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
            case CMD_PLUGIN_LOAD_ERROR: {
                log_message("Failed to load plugin %s", req.szPath);
                return -2;
            } break;
            case CMD_PLUGIN_END_SUCCESS:
                return nPluginsScanned;
            default:
                dbgassert(0);
                break;
        }
    }
    return -1;
}

class PluginScannerServer::Impl {
    struct PluginFileFound : public FileFound {
        String pluginPath; // path to the plugin directory
    };

    pluginscanner_server_options options;
    bool userSentQuitRequest = false;
    String pluginscannerExecPath;
    SQLite::Database db;
    std::unique_ptr<ProcessThread> thread = nullptr;
    std::map<ModuleType, std::vector<PluginFileFound>> filesByType;
public:
    Impl(pluginscanner_server_options options, std::optional<String> pluginscannerExecPath)
        : options(std::move(options)),
        db(App::Platform::toUserdataPath("data/plugins.db3"), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
    {
        if (!pluginscannerExecPath.has_value()) {
            auto scannerNames = {
                "daw-pluginscanner", 
                "pluginscanner", 
                "pluginscanner-debug",
                "pluginscanner-release",
                "pluginscanner-clang-debug",
                "pluginscanner-clang-relwithdebinfo",
                "pluginscanner-clang-release",
                "pluginscanner-msvc-debug",
                "pluginscanner-msvc-release",
                "daw-pluginscanner", 
            };
            String binaryPath = App::Platform::GetExecutablePath();
            // look in directory of the executable first
            String path;
            SplitPath(binaryPath, &path, nullptr, nullptr, nullptr);

            String filename = "";
            for (auto* name : scannerNames) {
                filename = path;
                filename += FILE_PATHSEP_STR;
                filename += name;
#ifdef _WIN32 
                filename += ".exe";
#endif //_WIN32
                if (FileExists(filename)) {
                    break;
                }
            }
            if (!FileExists(filename)) {
                log_lf(Log::L_ERROR, "Failed to find pluginscanner executable in current working directory: %s\n", StringAsCStr(App::Platform::getCurrentWorkingDirectory()));
                throw appexception("Failed to find pluginscanner executable in current working directory");
            } else {
                log_lf(Log::L_INFO, "Using pluginscanner executable: %s\n", StringAsCStr(filename));
                this->pluginscannerExecPath = std::move(filename);
            }
            
        } else {
            this->pluginscannerExecPath = std::move(pluginscannerExecPath.value());
        }
        logPrefixIdx = PROC_SIDE_SERVER;
        createTables(db);
    }
    
    void findFiles() {
        for (auto& [moduleType, pluginPaths] : options.pluginPathLists) {
            for (String pluginPath : pluginPaths) {
                App::Platform::shellExpandPath(pluginPath);
                App::Platform::sanitizePathToDirectory(pluginPath);
                if (pluginPath.empty()) {
                    continue;
                }
                log_message("Searching for plugins in %s", StringAsCStr(pluginPath));
                std::vector<FileFound> files;
                String ext = "";
                switch (moduleType) {
                    case MODULE_TYPE_VST2:
                        ext = PLATFORM_PLUGIN_EXT;
                        findFilesWithExt(pluginPath, ext, true, files);
                        break;
                    case MODULE_TYPE_VST3:
                        ext = PLATFORM_VST3_PLUGIN_EXT;
                        findDirectoriesWithExt(pluginPath, ext, files);
#ifdef _WIN32
                        // on windows, we also look for .vst3 files
                        {
                            std::vector<FileFound> filesVST3;
                            findFilesWithExt(pluginPath, ext, true, filesVST3);
                            // now remove all .vst3 files that are bundles and already found as directories
                            for (auto& file : filesVST3) {
                                bool bParentFound = false;
                                for (auto& dir : files) {
                                    // check if file starts with dir
                                    if (file.path.length() > dir.path.length() && file.path.find(dir.path) == 0) {
                                        bParentFound = true;
                                        break;
                                    }
                                }
                                if (!bParentFound) {
                                    files.emplace_back(file);
                                }
                            }
                        }
#endif
                        break;
                    case MODULE_TYPE_CLAP:
                        ext = PLATFORM_CLAP_PLUGIN_EXT;
                        findFilesWithExt(pluginPath, ext, true, files);
                        break;
                    default:
                        log_message("Unknown module type %d", moduleType);
                        continue;
                }
                log_message("Found %u .%s files in %s", CtrSize(files), StringAsCStr(ext), StringAsCStr(pluginPath));
                auto& filesByType = this->filesByType[moduleType];
                for (FileFound& fileFound : files) {
                    filesByType.emplace_back(fileFound, pluginPath);
                }
            }
        }
    }
    int runScannerServer() {
        using seqthreads::threadSleep;
        if (!options.updatePattern.empty()) {
            log_message("Update *%s*", StringAsCStr(options.updatePattern));
        }
        ipc_server server;
        int ipc_status = server.server_open(SCAN_IPC_PIPE_NAME);
        if (ipc_status) {
            log_message("Failed opening ipc_server: %d", ipc_status);
            return 1;
        }
        try {
            bool pipeConnected = false;
            SQLite::Statement queryPlugin(db, "SELECT id, moddate, forcedisable, requestRescan, uid, shellplugin FROM plugins where path == ?");
            SQLite::Statement queryInsertPlugin(db, "INSERT INTO "
                                                    "plugins(isSynth, moduleFormat, uid, version, vstVersion, category, moddate, state, path, relPath, name, vendorName, productName, effectName, requestRescan, forcedisable, shellplugin) "
                                                    "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            SQLite::Statement queryDelete(db, "DELETE from plugins where id == ? or path == ?");

            SQLite::Statement queryAll(db, "SELECT id, path, moduleFormat from plugins");
            while (queryAll.executeStep() && !userSentQuitRequest) {
                String path = queryAll.getColumn(1).getString();
                ModuleType moduleType = static_cast<ModuleType>(queryAll.getColumn(2).getInt() + 1);
                // make sure that the parent path is in the plugin directory list
                bool foundPluginPath = false;
                for (String pluginPath : options.pluginPathLists[moduleType]) {
                    App::Platform::shellExpandPath(pluginPath);
                    App::Platform::sanitizePathToDirectory(pluginPath);
                    // check if the path is a subdirectory of the pluginPath
                    if (path.length() > pluginPath.length() && path.find(pluginPath) == 0) {
                        foundPluginPath = true;
                        break;
                    }
                }
                if (!foundPluginPath) {
                    log_message("REMOVE plugin: %s is not in the plugin path list", StringAsCStr(path));
                    if (!options.dryRun) {
                        queryDelete.reset();
                        queryDelete.bind(1, queryAll.getColumn(0).getInt());
                        queryDelete.bind(2, path);
                        queryDelete.exec();
                    }
                    continue;
                }
                try {
                    if (moduleType == ModuleType::MODULE_TYPE_VST3) {
                        if (PathIsDirectory(path)) {
                            continue;
                        }
                    }
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

            for (auto& [moduleType, files] : filesByType) {
                if (userSentQuitRequest) {
                    break;
                }
                for (PluginFileFound& file : files) {
                    FileTimeGetter filetime(file.path);
                    int64_t timeDisk   = filetime.getWriteTimeI64();
                    int id             = -1;
                    bool needScan      = true;
                    bool forcedisable  = false;
                    queryPlugin.reset();
                    queryPlugin.bind(1, file.path);
                    String reason = "New plugin (or not a supported file)";
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
                        String lastCmd = StringFormat("%s %s", StringAsCStr(pluginscannerExecPath), StringAsCStr(arg1));
                        thread->startProcess(pluginscannerExecPath, "-client", "");
                        threadSleep(250);
                        if (!thread->isRunning()) {
                            thread->checkException();
                            log_message("Failed starting client");
                            break;
                        }
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
                        request_type_load_plugin_t req;
                        req.moduleType = moduleType;
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
                        int ret = handleClientResponse(options, server, thread.get(), userSentQuitRequest, req, queryInsertPlugin, file, timeDisk, forcedisable, file.pluginPath);

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
                        server.server_disconnect();
                        pipeConnected = false;
                        if (thread && thread->isRunning()) {
                            thread->killProcess();
                            thread->joinProcess();
                        }
                        thread = nullptr;
                        continue;
                    }
                }
            }
        } catch (SQLite::Exception& e) {
            log_message("SQLite exception: %s (%d)", e.getErrorStr(), e.getErrorCode());
        } catch (std::exception& e) {
            log_message("exception %s", e.what());
        } catch (...) {
            log_message("Unhandled exception");
        }
        server.server_disconnect();
        if (thread) {
            if (thread->isRunning()) {
                /*if (pipeConnected) {
                    pipe_msg_hdr hdr = {CMD_PLUGIN_THREAD_QUIT};
                    writeToIPC(server, hdr);
                    threadSleep(120);
                }*/
            }
            if (thread->isRunning()) {
                thread->killProcess();
                thread->joinProcess();
            }
            thread = nullptr;
        }
        return 0;
    }
    void requestQuit() {
        userSentQuitRequest = true;
    }
};
PluginScannerServer::PluginScannerServer(const pluginscanner_server_options& options, std::optional<String> pluginscannerExecPath)
    : impl(std::make_shared<Impl>(options, std::move(pluginscannerExecPath))) {
}
void PluginScannerServer::findFiles() {
    impl->findFiles();
}
int PluginScannerServer::runScannerServer() {
    return impl->runScannerServer();
}

int runPluginTest(request_type_load_plugin_t req, response_type_vst24_plugin_t& respVstPlugin, response_type_clapplugin_t& respClapPlugin) {
    log_message("runPluginTest");

    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(44100), 512, sampleformat_bits_t::FLOAT_32 });
    auto& tls = daw_tls::getTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);

    int response = 0;
    log_message("Load plugin %s", req.szPath);
    try {
        DAW::Host::PluginLoadParameters params{
            .filepath = req.szPath,
            .uId = 0,
            .globalId = 0,
            .bugfixFlags = 0,
            .moduleType = ModuleType::MODULE_TYPE_UNKNOWN,
            .uIdVst3 = ""
        };
        auto loadresult = pluginMgr->loadPlugin(params);
        auto res = *loadresult;
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
            } else if (res.vst3Plugin) {
                response = CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGIN;
            }
            pluginMgr->unloadPlugin(res.plugin);
        }
    } catch (...) {
        log_message("exception while loading %s", req.szPath);
        response = CMD_PLUGIN_LOAD_ERROR;
    }

    log_message("runPluginTest end");
    seqthreads::threadSleep(25);
    pluginMgr->destroy();
    return response;
}

int runScannerClient() {

    // Open the named pipe
    ipc_client client;
    int ipcstatus = client.client_connect(SCAN_IPC_PIPE_NAME);
    if (ipcstatus) {
        log_message("Failed opening ipc_client: %d", ipcstatus);
        return 1;
    }

    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(44100), 512, sampleformat_bits_t::FLOAT_32 });
    auto& tls = daw_tls::getTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);

    pipe_msg_hdr hdr{};
    while (true) {
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
            request_type_load_plugin_t req;
            if (E_READ_OK != readFromIPC(client, req)) {
                log_message("req readFromIPC failed");
                break;
            }
            ModuleType moduleType = static_cast<ModuleType>(req.moduleType);
            String moduleTypeStr = ModuleTypeToString(moduleType);
            DAW::Host::PluginLoadParameters params{
                .filepath = req.szPath,
                .uId = 0,
                .globalId = 0,
                .bugfixFlags = 0,
                .moduleType = moduleType,
                .uIdVst3 = ""
            };
            log_message("Load %s plugin %s", StringAsCStr(moduleTypeStr), req.szPath);
            auto loadresult = pluginMgr->loadPlugin(params);
            auto res = *loadresult;
            if (res.library.state != DAW::Host::SharedLibState::SUCCESS) {
                int response = CMD_PLUGIN_LOAD_ERROR;
                writeToIPC(client, response);
            } else {
                if (res.clapPlugin && res.clapPlugin->getPluginCount() > 0) {
                    String nameShellPlugin = res.name;
                    log_message("loading clap plugin: %s", StringAsCStr(nameShellPlugin));

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
                            log_message("Found clap plugin: %s '%s'", desc->id, desc->name);
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
                    dbgassert(handles && handles->aeffect);
                    dbgassert(handles->aeffect->dispatcher);
                    String nameShellPlugin = res.name;
                    log_message("Loading shell plugin: %s", StringAsCStr(nameShellPlugin));

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
                    VstIntPtr dispatchRet = 0;
                    while ((dispatchRet = handles->aeffect->dispatcher(handles->aeffect, effShellGetNextPlugin, 0, 0, tempName, 0)) != 0) {
                        if (dispatchRet < 0)
                            log_message("WARN: expected positive value for VST UID %zd", dispatchRet);

                        auto plugUniqueID = (VstInt32) (static_cast<uint64_t>(dispatchRet) & 0xFFFFFFFFULL);
                        // subplug needs a name
                        if (tempName[0] != 0) {
                            log_message("New VST2 Shell Plugin: %04X\t%s", plugUniqueID, tempName);
                            entries.push_back(shell_plugin_entry_t{ tempName, plugUniqueID });
                        } else {
                            log_message("effShellGetNextPlugin returned empty name for plugin UID %04X", plugUniqueID);
                        }
                        tempName[0] = 0;
                    }
                    int32_t response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_SHELL;
                    writeToIPC(client, response);
                    respShellPlugin.numPlugins = (int) entries.size();
                    writeToIPC(client, respShellPlugin);
                    log_message("-- begin of shell plugin list --");
                    for (auto& entry : entries) {
                        log_message("load shell entry: %08X", entry.pluginUID);
                        DAW::Host::PluginLoadParameters params{
                            .filepath = req.szPath,
                            .uId = static_cast<uint32_t>(entry.pluginUID),
                            .globalId = 0,
                            .bugfixFlags = 0,
                            .moduleType = ModuleType::MODULE_TYPE_VST2,
                            .uIdVst3 = ""
                        };
                        auto loadresult = pluginMgr->loadPlugin(params);
                        auto resShellPluginEntry = *loadresult;
                        if (resShellPluginEntry.library.state != DAW::Host::SharedLibState::SUCCESS) {
                            log_message("Failed loading shell plugin %s: %s (%d)", req.szPath, StringAsCStr(res.library.error), static_cast<int32_t>(res.library.state));
                        } else {
                            dbgassert(resShellPluginEntry.plugin);
                            dbgassert(resShellPluginEntry.vstPlugin);
                            response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGINSHELL_PLUGIN;
                            response_type_vst24_t respShellPluginEntry;
                            if (resShellPluginEntry.vstPlugin)
                                getVSTPluginData(resShellPluginEntry, &respShellPluginEntry);
                            safe_strcpy(respShellPluginEntry.szName, entry.name);
                            writeToIPC(client, response);
                            writeToIPC(client, respShellPluginEntry);
                            log_message("unload shell entry: %08X", entry.pluginUID);
                            pluginMgr->unloadPlugin(resShellPluginEntry.plugin);
                        }
                    }
                    log_message("-- end of shell plugin list --");
                    response = CMD_PLUGIN_END_SUCCESS;
                    writeToIPC(client, response);
                } else  if (res.library.type == DAW::Host::SharedLibPluginType::VST3_SHELL) {
                    VST3::Hosting::PluginFactory factory = res.library.vst3Module->getFactory();
                    String nameShellPlugin = res.name;
                    log_message("Loading VST3 Shell Plugin: %s\n", StringAsCStr(nameShellPlugin));
                    response_type_shell_plugin_begin_t respShellPlugin;
                    safe_strcpy(respShellPlugin.szName, res.name);
                    int32_t response = CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGINSHELL_SHELL;
                    writeToIPC(client, response);
                    respShellPlugin.numPlugins = 0; // not used anymore
                    writeToIPC(client, respShellPlugin);
                    // loop over all shell plugin entries
                    int32_t classCount = 0;
                    response = CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGINSHELL_PLUGIN;
                    for (auto &classInfo : factory.classInfos()) {
                        if (classInfo.category() == kVstAudioEffectClass) {
                            log_message("VST3 %d: %s, %s, %s, %s, %s, %s, %s\n", classCount, StringAsCStr(classInfo.name()), classInfo.ID().toString(true).c_str(), classInfo.category().c_str(), classInfo.subCategoriesString().c_str(), classInfo.vendor().c_str(), classInfo.version().c_str(), classInfo.sdkVersion().c_str());
                            classCount++;
                            response_type_vst3plugin_t respShellPluginEntry;
                            getVst3PluginDataFromClassInfo(req.szPath, classInfo, &respShellPluginEntry);
                            writeToIPC(client, response);
                            writeToIPC(client, respShellPluginEntry);
                        }
                    }
                    response = CMD_PLUGIN_END_SUCCESS;
                    writeToIPC(client, response);
                } else if (res.library.state == DAW::Host::SharedLibState::SUCCESS) {
                    if (res.vstPlugin) {
                        response_type_vst24_t resp;
                        getVSTPluginData(res, &resp);
                        int response = CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN;
                        writeToIPC(client, response);
                        writeToIPC(client, resp);
                    } else if (res.clapPlugin) {
                        response_type_clapplugin_t resp;
                        getClapPluginData(res, &resp);
                        int response = CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN;
                        writeToIPC(client, response);
                        writeToIPC(client, resp);
                    } else if (res.library.vst3Module) {
                        VST3::Hosting::PluginFactory factory = res.library.vst3Module->getFactory();
                        for (auto &classInfo : factory.classInfos()) {
                            if (classInfo.category() == kVstAudioEffectClass) {
                                log_message("VST3: %s, %s, %s, %s, %s, %s, %s\n", StringAsCStr(classInfo.name()), classInfo.ID().toString(true).c_str(), classInfo.category().c_str(), classInfo.subCategoriesString().c_str(), classInfo.vendor().c_str(), classInfo.version().c_str(), classInfo.sdkVersion().c_str());
                                response_type_vst3plugin_t resp;
                                getVst3PluginDataFromClassInfo(req.szPath, classInfo, &resp);
                                int response = CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGIN;
                                writeToIPC(client, response);
                                writeToIPC(client, resp);
                            }
                        }
                    }
                    if (res.plugin)
                        pluginMgr->unloadPlugin(res.plugin);
                    int response = CMD_PLUGIN_END_SUCCESS;
                    writeToIPC(client, response);
                }
            }
        }
        seqthreads::threadSleep(25);
    }
    client.client_close();
    seqthreads::threadSleep(25);
    host->destroy();
    return 0;
}

static PluginScanner::PluginScannerServer* serverInstance = nullptr;
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD) {
    if (serverInstance) {
        serverInstance->requestQuit();
    }
    if (inConnectNamedPipe) {
        exit(0);
    }
    return true;
}
#elif defined(__linux__) || defined(__APPLE__)
static void signalHandler(int signum) {
    if (serverInstance) {
        serverInstance->requestQuit();
    }
    if (inConnectNamedPipe) {
        exit(0);
    }
}
#endif

int mainPluginScanner(int argc, char* argv[]) {
    using namespace DAW::Host;
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    getGlobalLogger()->setLevel(Log::LEVEL_ALL);
    if (argc > 1 && (String(argv[1]) == "-h" || String(argv[1]) == "--help")) {
        log_out("Daw VST scanner version %s\n\n", BuildInfo::BUILD_BINARY_VERSION);
        log_out("This program can be run in server or client mode.\n");
        log_out("The server starts a client process that loads the VST2 DLL and scans it.\n");
        log_out("The client automatically connects to the server process via IPC and listens for commands.\n");
        log_out("Command line options:\n");
        log_out("-test <path>\t\ttest single plugin\n");
        log_out("-client \t\trun client\n");
        log_out("-server \t\trun server\n");
        log_out("-wait   \t\t(server only)\tDo not start client process (Allows manual start)\n");
        log_out("-dry    \t\t(server only)\tCheck for new plugins but does not scan them\n");
        log_out("-update <plugin-name>\t(server only)\tRescan a specific plugin. Does partial name matching, case-insensitive\n");
        log_out("-rescan \t\t(server only)\tRescan all registered plugins, even if their disk timestamp has not changed\n");
        log_out("-path <directory>\t(server only)\tManually specify directory to scan for vst plugins\n");
        log_out("-clappath <directory>\t(server only)\tManually specify directory to scan for clap plugins\n");
        log_out("-timeout <seconds>\t(server only)\tSet the timeout for unresponsive plugins. Default is %d seconds\n", PluginScanner::timeoutdefault);
        log_out("\nThe default command to scan plugins is:\n");
        log_out("%s -server\n", argv[0]);
        return 0;
    }
#ifdef _WIN32
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) PluginScanner::ConsoleHandler, TRUE)) {
        log_lf(Log::L_ERROR, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
#elif defined(__linux__) || defined(__APPLE__)
    signal(SIGINT, PluginScanner::signalHandler);
#endif
    auto& tls = daw_tls::initNewTls();
    enum Mode {
        SERVER,
        CLIENT,
        TEST
    };
    Mode mode = SERVER;
    if (argc == 1 || (argc > 1 && !strcmp("-server", argv[1]))) {
        mode = SERVER;
    } else if (argc > 2 && !strcmp("-test", argv[argc - 2])) {
        mode = TEST;
    } else if (argc > 0 && !strcmp("-client", argv[argc - 1])) {
        mode = CLIENT;
    }
    if (mode == SERVER) {
        PluginScanner::logPrefixIdx = PROC_SIDE_SERVER;
        PluginScanner::pluginscanner_server_options options;
        options.launchProcess              = true;
        options.dryRun                     = false;
        options.updatePattern              = "";
        options.fullRescan                 = false;
        options.checkDiskTimestamp         = true;
        options.unresponsiveTimeoutSeconds = PluginScanner::timeoutdefault;
        String vstPlugPath;
        String clapPluginPath;
        String vst3PluginPath;
        for (int i = 1; i < argc; i++) {
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
                if (!strcmp(argv[i], "-vst3path") && i + 1 < argc) {
                    vst3PluginPath = argv[i + 1];
                    i++;
                }
            }
        }
        loadSettings(*tls.settings);
        if (vstPlugPath.empty()) {
            vstPlugPath = tls.settings->pluginsettings.pathVst2;
        }
        if (clapPluginPath.empty()) {
            clapPluginPath = tls.settings->pluginsettings.pathClap;
        }
        if (vst3PluginPath.empty()) {
            vst3PluginPath = tls.settings->pluginsettings.pathVst3;
        }
        if (clapPluginPath.empty() && vstPlugPath.empty() && vst3PluginPath.empty()) {
            log_lf(Log::L_ERROR, "Error: settings.pluginsettings.pathVst2 / settings.pluginsettings.pathClap / settings.pluginsettings.pathVst3 is empty\n");
            return EXIT_FAILURE;
        }
        log_out("VST2 Plugins Path: '%s'\n", StringAsCStr(vstPlugPath));
        log_out("CLAP Plugins Path: '%s'\n", StringAsCStr(clapPluginPath));
        log_out("VST3 Plugins Path: '%s'\n", StringAsCStr(vst3PluginPath));
        options.pluginPathLists[MODULE_TYPE_VST2].push_back(vstPlugPath);
        options.pluginPathLists[MODULE_TYPE_CLAP].push_back(clapPluginPath);
        options.pluginPathLists[MODULE_TYPE_VST3].push_back(vst3PluginPath);
        String pluginscannerExecPath = App::Platform::GetExecutablePath();
        PluginScanner::PluginScannerServer scanner(options, std::move(pluginscannerExecPath));
        serverInstance = &scanner;
        scanner.findFiles();
        scanner.runScannerServer();
    } else if (mode == TEST) {
        setExceptionHandler();
        seqthreads::threadSleep(120);
        PluginScanner::request_type_load_plugin_t req;
        String fPath = argv[argc - 1];
        safe_strcpy(req.szPath, fPath);
        PluginScanner::response_type_vst24_plugin_t respPlugin;
        PluginScanner::response_type_clapplugin_t respClap;
        int retCode = PluginScanner::runPluginTest(req, respPlugin, respClap);
        if (retCode == CMD_PLUGIN_LOAD_SUCCESS_VST_PLUGIN) {
            log_lf(Log::L_INFO, "VST2 Plugin %s: Good", StringAsCStr(fPath));
        } else if (retCode == CMD_PLUGIN_LOAD_SUCCESS_CLAP_PLUGIN) {
            log_lf(Log::L_INFO, "Clap Plugin %s: Good", StringAsCStr(fPath));
        } else if (retCode == CMD_PLUGIN_LOAD_SUCCESS_VST3_PLUGIN) {
            log_lf(Log::L_INFO, "VST3 Plugin %s: Good", StringAsCStr(fPath));
        } else {
            log_lf(Log::L_WARN, "Plugin %s: Failed %d\n", StringAsCStr(fPath), retCode);
        }
    } else if (mode == CLIENT) {
        PluginScanner::logPrefixIdx = PROC_SIDE_CLIENT;
        setExceptionHandler();
        seqthreads::threadSleep(120);
        PluginScanner::runScannerClient();
    } else {
        log_lf(Log::L_ERROR, "No command. Use -server to update plugin database\n");
    }
    return 0;
}

void PluginScannerServer::requestQuit() {
    impl->requestQuit();
}

}// namespace DAW::Host::PluginScanner
