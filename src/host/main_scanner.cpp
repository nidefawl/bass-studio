/**
 *  (VST) Plugin Scanner
 *
 *	vstPlugPath is user configuration: Right now system paths are ignored and have to be configured manually (data/settings.json)
 *
 *  server scans for vst files and launches client processes.
 *  client processes load single plugins and return status and plugin information.
 *  server monitors client processes for status response and possible crashes.
 *  server writes information about plugin and its status (valid, invalid) to SQLite database.
 *
 */

#include "str_util.h"
#include "../vstsdk-host-2.4/aeffectx.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "fileio.h"
#include "exceptions.h"
#include "../threads/childprocessthread.h"
#include "platform.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include <iostream>
#include <memory>
#include "ipc.h"
#include "appsettings.h"
#include "tls.h"
#ifdef _WIN32
#include "../platform/win/platform_win.h"
#include <windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <limits.h>
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

//#define LOG_SERVER(fmtString,...) printf("SRV:" fmtString "\n", ##__VA_ARGS__); fflush(stdout)
//#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)
static const char* szLogPrefixes[2] = {
	"SRV: ",
	"CLI: ",
};
static int logPrefixIdx = 0;
#define LOG_MSG(prefix, fmtString,...) printf("%s" fmtString "\n", prefix, ##__VA_ARGS__);
#define LOG(fmtString,...) LOG_MSG(szLogPrefixes[logPrefixIdx], fmtString, ##__VA_ARGS__); fflush(stdout)

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
#define NUM_BUFS (16*1024)
#define SCAN_IPC_PIPE_NAME "DAW1pipc"
void deleteApp() {

}

std::shared_ptr<AppCtrl> makeApp() {
	return nullptr;
}


struct pipe_msg_hdr {
	uint32_t cmd;
};
enum vst_metadata_flags_e : int32_t {
	VST_FLAGS_NONE = 0,
	VST_FLAGS_LOADED_PLUGIN = 1,
	VST_FLAGS_IS_SHELL_PLUGIN = 2
};
struct response_type_t  {
	char szPath[1024]{0};
	char szName[256]{0};
};
struct request_type_vst24_t : response_type_t  {
	uint32_t uniqueID{0};
};
struct response_type_vst24_t : response_type_t  {
	vst_metadata_flags_e flags{VST_FLAGS_NONE};
	uint32_t uniqueID{0};
	uint32_t version{0};
	uint32_t vstVersion{0};
	uint32_t pluginCategory{0};
	bool isSynth{0};
	char szVendorName[256]{0};
	char szShellPluginName[256]{0};
};
struct response_type_vst24_plugin_t : response_type_vst24_t {
};
struct response_type_vst24_shell_plugin_t : response_type_t {
	int numPlugins;
};
struct recvbuf_t {
	char buf[NUM_BUFS];
	char* pos = nullptr;
	char* end = nullptr;
};
template<typename T>
bool writeToBuffer(recvbuf_t& buf, T& hdr) {
	dbgassert(buf.pos);
	if (static_cast<size_t>(buf.pos-buf.buf)+sizeof(hdr) <= NUM_BUFS) {
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
	if (buf.pos+sizeof(hdr) <= buf.end) {
	    memcpy(&hdr, buf.pos, sizeof(T));
	    buf.pos += sizeof(hdr);
	    return true;
	}
    return false;
}
template<typename IPC>
bool IPCrecvBuffer(IPC& conn, recvbuf_t& bufferRecv) {
	bufferRecv.pos = bufferRecv.buf;
	auto lenRcvd = conn.readData(bufferRecv.pos, NUM_BUFS);
	bufferRecv.end = bufferRecv.pos+lenRcvd;
	return NUM_BUFS == lenRcvd;
}
template<typename IPC>
bool IPCsendBuffer(IPC& conn, recvbuf_t& bufferRecv) {
	bufferRecv.pos = bufferRecv.buf;
	auto lenRcvd = conn.sendData(bufferRecv.pos, NUM_BUFS);
	bufferRecv.end = bufferRecv.pos+lenRcvd;
	return NUM_BUFS == lenRcvd;
}
/** uses function scope static buffer: NOT THREADSAFE */
template<typename IPC, typename T>
int readFromIPC(IPC& ipcConnection, T& hdr) {
	static recvbuf_t recvBuf;
	recvBuf.pos = recvBuf.buf; recvBuf.end = nullptr;
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
	sendBuffer.pos = sendBuffer.buf; sendBuffer.end = nullptr;
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

void getPluginData(vstplugin* plugin, response_type_vst24_t* _out) {
	AEffect* aeffect = plugin->handle->aeffect;
	_out->uniqueID = aeffect->uniqueID;
	_out->version = aeffect->version;
	_out->vstVersion = plugin->vstVersion;
	_out->pluginCategory = plugin->pluginCategory;
	strncpy(_out->szName, StringAsCStr(plugin->sName), plugin->sName.length());
	if (!plugin->dispatch(effGetVendorString, 0, 0, (void*)_out->szVendorName)) {
		_out->szVendorName[0] = 0;
	}
	_out->isSynth = plugin->isSynth;
}
void createTables(SQLite::Database& db);
bool quit = false;
bool inConnectNamedPipe = false;
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    switch(dwType) {
    case CTRL_C_EVENT:
		LOG("CTRL_C");
    	quit = true;
    	if (inConnectNamedPipe) {
    		exit(0);
    		break;
    	}
        break;
    }
    return TRUE;
}
#endif

bool vstplugin__getNameString(handles_t* handles, char* szBuf) {
	szBuf[0] = 0;
	if (handles->aeffect->dispatcher(handles->aeffect, effGetProductString, 0, 0, (void*)szBuf, 0) && szBuf[0] != 0) {
		return true;
	}
	szBuf[0] = 0;
	if (handles->aeffect->dispatcher(handles->aeffect, effGetEffectName, 0, 0, (void*)szBuf, 0) && szBuf[0] != 0) {
		return true;
	}
	return false;
}
int main(int argc, char* argv[]) {
	LOG("ARGC %d", argc);
	for (int i = 0; i < argc; i++) {
		LOG("argv[%d] %s", i, argv[i]);
	}
#ifdef _WIN32
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)) {
        fprintf(stderr, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
#endif
    appsettings settings = loadSettings();
	String vstPlugPath = settings.pluginPath;
	LOG("pluginPath '%s'", StringAsCStr(vstPlugPath));
    if (vstPlugPath.empty()) {
        fprintf(stderr, "Error: pluginPath not configured\n");
        return EXIT_FAILURE;
    }
    bool lastRecvState = false;
//	std::set_terminate(terminate_fn);
	if (argc > 1 && !strcmp("-server", argv[1])) {
		bool launchProcess = true;
		bool dryRun = false;
		String updatePattern = "";
		bool fullRescan = false;
		bool checkDiskTimestamp = true;
	    for (int i = 2; i < argc; i++) {
	    	if (argv[i] && strlen(argv[i]) > 2 && argv[i][0] == '-') {
	    		if (!strcmp(argv[i], "-wait")) {
	    			launchProcess = false;
	    		}
	    		if (!strcmp(argv[i], "-dry")) {
	    			dryRun = true;
	    		}
	    		if (!strcmp(argv[i], "-update") && i+1 < argc) {
	    			updatePattern = argv[i+1];
	    			checkDiskTimestamp = false;
	    		}
	    		if (!strcmp(argv[i], "-rescan")) {
	    			fullRescan = true;
	    		}
	    	}
	    }
	    if (updatePattern != "") {
	    	LOG("Update *%s*", StringAsCStr(updatePattern));
	    }
	    try {
			SQLite::Database    db("data/plugins.db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";
			createTables(db);

			threadSleep(1000);

			std::vector<FileFound> files;
//			String vstPlugPath = "C:/PluginManager/configs/default/hosts/Ableton/categories/";
			findFilesWithExt(vstPlugPath, PLATFORM_PLUGIN_EXT, true, files);
			LOG("Found %u files", (uint32_t )files.size());
			if (files.empty()) {
				return 1;
			}
#ifdef _WIN32
			TCHAR szFileName[MAX_PATH + 1];
			GetModuleFileName(NULL, szFileName, MAX_PATH + 1);
			String exeName = szFileName;
#endif
#ifdef __APPLE__
			String exeName = APPLE_getExecutablePath();
#endif
#ifdef __linux__
			String exeName = "plugin_scan";
		    char buff[4096];
		    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
		    if (len != -1) {
		      buff[len] = '\0';
		      exeName = buff;
		    }
#endif

			recvbuf_t bufferRecv;
			ipc_server server;
			int ipc_status = server.server_open(SCAN_IPC_PIPE_NAME);
			if (ipc_status) {
				LOG("Failed opening ipc_server: %d", ipc_status);
				return 1;
			}
			std::unique_ptr<ProcessThread> thread;
			int a = 0;
			bool pipeConnected = false;
//			db.exec("delete from plugins where 1");
			SQLite::Statement   queryPlugin(db, "SELECT id, moddate, forcedisable, requestState, uid, shellplugin FROM plugins where path == ?");
			SQLite::Statement   queryInsertPlugin(db, "INSERT INTO "
					"plugins(isSynth, uid, version, vstVersion, category, moddate, state, path, name, vendorName, requestState, forcedisable, shellplugin) "
					"VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
			SQLite::Statement   queryDelete(db, "DELETE from plugins where id == ? or path == ?");
			if (!dryRun) {
				SQLite::Statement   queryAll(db, "SELECT id, path from plugins");
				while (queryAll.executeStep())
				{
					String path = queryAll.getColumn(1).getString();
					try {
						size_t size = GetFileSizeSafe(path);
						if (size > 0) {
							continue;
						}
					} catch (std::exception& e) {
						LOG("REMOVE %s\n", StringAsCStr(path));
					}
					queryDelete.reset();
					queryDelete.bind(1, queryAll.getColumn(0).getInt());
					queryDelete.bind(2, path);
					queryDelete.exec();
				}
			}
			for (FileFound& file : files) {
				if (quit) {
					break;
				}
//				if (file.path.find("Absynth") == String::npos)
//					continue;
				a++;
//				if (a < 420)
//					continue;
				FileTimeGetter filetime(file.path);
				int64_t timeDisk = filetime.getWriteTimeI64();
				int id = -1;
				int32_t uid = -1;
				bool isShellPlugin = false;
				bool needScan = true;
				bool forcedisable = false;
				queryPlugin.reset();
				queryPlugin.bind(1, file.path);
				if (queryPlugin.executeStep())
				{
					id = queryPlugin.getColumn(0).getInt();
					uid = queryPlugin.getColumn(4).getInt();
					isShellPlugin = queryPlugin.getColumn(5).getInt()>0;
					forcedisable = queryPlugin.getColumn(2).getInt()>0;
					if (checkDiskTimestamp) {

						int64_t timeDB = queryPlugin.getColumn(1).getInt64();
	//					LOG("id %d timeDisk %016llX %016llX", id, timeDisk, timeDB);
						if (timeDisk == timeDB) {
							needScan = false;
						}
					} else {

						needScan = false;
					}
				}
				if (!needScan && fullRescan) {
					needScan = true;
				}
				if (updatePattern.length()) {
					needScan = file.name.find(updatePattern) != String::npos;
				}
				if (!needScan) {
					if (updatePattern.empty())
						LOG("%s is up to date", StringAsCStr(file.name));
					continue;
				}
				LOG("%s needs update", StringAsCStr(file.name));
				if (dryRun) {
					continue;
				}
				if (launchProcess&& (!thread || !thread->isRunning())) {
					if (thread)
						thread->joinProcess();
					if (pipeConnected) {
						server.server_disconnect();
					}
					threadSleep(1200);
					pipeConnected = false;
					thread = std::make_unique<ProcessThread>();
					LOG("!thread.isRunning(), last recv state: %s", lastRecvState ?"GOOD":"BAD");
					String arg1 = "-client"; 
					String lastCmd = StringFormat("%s %s", StringAsCStr(exeName), StringAsCStr(arg1));
                                        printf("binary at %s\n", StringAsCStr(exeName));
					thread->startProcess(exeName, "-client", "");
					threadSleep(200);
					if (!thread->isRunning()) {
						thread->checkException();
						LOG("Failed starting client");
						break;
					}
					LOG("Thread is up");
				}
				if (!pipeConnected && (!launchProcess || (thread && thread->isRunning()))) {
					LOG("ConnectNamedPipe()");
					inConnectNamedPipe = true;
					int ipcstatus_connect = server.server_accept();
					inConnectNamedPipe = false;
					if (ipcstatus_connect) {
						LOG("ipc_server::server_accept() failed: %d", ipcstatus_connect);
						server.server_close();
						break;
					}
					pipeConnected = true;
				}
				int ret;
				pipe_msg_hdr hdr = {CMD_PLUGIN_LOAD_REQUEST};
				ret = writeToIPC(server, hdr);
				dbgassert(ret != E_WRITE_ERR_BUF_SIZE);
				request_type_vst24_t req;
				strncpy(req.szPath, StringAsCStr(file.path), file.path.length());
				ret = writeToIPC(server, req);
				dbgassert(ret != E_WRITE_ERR_BUF_SIZE);
				if (ret != E_WRITE_OK) {
					LOG("error writeToIPC request_type_vst24_t");
					break;
				}
				if (dryRun)
					continue;
				if (id > 0) {
					queryDelete.reset();
					queryDelete.bind(1, id);
					queryDelete.bind(2, req.szPath);
					queryDelete.exec();
				}

				if (ret == E_WRITE_OK) {
					bool finished = false;
					while (!finished) {
						int responseType = 0;
						if (E_READ_OK != readFromIPC(server, responseType)) {
							LOG("failed reading responseType");
							finished = true;
							break;
						}
						switch (responseType) {
						case CMD_PLUGIN_LOAD_SUCCESS_PLUGIN:
						{
							response_type_vst24_plugin_t respShellPlugin;
							LOG("READ response_type_vst24_plugin_t");
							ret = readFromIPC(server, respShellPlugin);
							if (E_READ_OK != ret) {
								LOG("failed reading response_type_vst24_plugin_t");
								finished = true;
							} else {

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
									queryInsertPlugin.bind(bndIdx++, (long long int)timeDisk);
									queryInsertPlugin.bind(bndIdx++, 1);
									queryInsertPlugin.bind(bndIdx++, file.path);
									queryInsertPlugin.bind(bndIdx++, data.szName);
									queryInsertPlugin.bind(bndIdx++, data.szVendorName);
									queryInsertPlugin.bind(bndIdx++, 0);
									queryInsertPlugin.bind(bndIdx++, forcedisable?1:0);
									queryInsertPlugin.bind(bndIdx++, 0);
									/*int insertRowsAffected = */queryInsertPlugin.exec();
				//					LOG("insertRowsAffected %d",insertRowsAffected);
								} catch (SQLite::Exception& e) {
									std::cout << "queryInsertPlugin exception: " << e.getErrorStr() << std::endl;
								}
							}
							break;
						}
						case CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_SHELL:
						{
							LOG("READ response_type_vst24_shell_plugin_t");
							response_type_vst24_shell_plugin_t respShellPlugin;
							ret = readFromIPC(server, respShellPlugin);
							if (E_READ_OK != ret) {
								LOG("failed reading response_type_vst24_shell_plugin_t");
								finished = true;
							}
							break;
						}
							break;
						case CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_PLUGIN:
						{
							LOG("READ response_type_vst24_t");
							response_type_vst24_t respShellPluginEntry;
							ret = readFromIPC(server, respShellPluginEntry);
							if (E_READ_OK != ret) {
								LOG("failed reading response_type_vst24_t");
								finished = true;
							} else {


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
									queryInsertPlugin.bind(bndIdx++, (long long int)timeDisk);
									queryInsertPlugin.bind(bndIdx++, 1);
									queryInsertPlugin.bind(bndIdx++, file.path);
									queryInsertPlugin.bind(bndIdx++, data.szName);
									queryInsertPlugin.bind(bndIdx++, data.szVendorName);
									queryInsertPlugin.bind(bndIdx++, 0);
									queryInsertPlugin.bind(bndIdx++, forcedisable?1:0);
									queryInsertPlugin.bind(bndIdx++, 1);
									/*int insertRowsAffected = */queryInsertPlugin.exec();
				//					LOG("insertRowsAffected %d",insertRowsAffected);
								} catch (SQLite::Exception& e) {
									std::cout << "queryInsertPlugin exception: " << e.getErrorStr() << std::endl;
								}
							}
							break;
						}
							break;
						case CMD_PLUGIN_LOAD_ERROR:
							finished = true;
							break;
						case CMD_PLUGIN_END_SUCCESS:
							finished = true;
							break;
						}
					}
				}
				if (ret == E_WRITE_ERR_PIPE) {
					LOG("SERVER E_WRITE_ERR_PIPE");
					server.server_disconnect();
					pipeConnected = false;
					continue;
				}
			}
			if (thread && thread->isRunning() && pipeConnected) {

				pipe_msg_hdr hdr = {CMD_PLUGIN_THREAD_QUIT};
				writeToIPC(server, hdr);
				server.server_disconnect();
				thread->joinProcess();
			}
			thread.reset();
	    } catch (SQLite::Exception& e) {
			std::cout << "SQLite exception: " << e.getErrorStr() << std::endl;
			std::cout <<  e.what() << std::endl;
		} catch (std::exception& e) {
			std::cout << "exception: " << e.what() << std::endl;
		} catch (...) {
			std::cout << "Unhandled exception" << std::endl;
		}

		LOG("Done.");
		threadSleep(500);
	} else if (argc > 0 && !strcmp("-test", argv[argc-1])) {
		setExceptionHandler();
		threadSleep(120);
    	auto vsthostInstance = std::make_unique<vsthost>();
    	vsthost::assignMasterCallback(vsthostInstance.get());
		vsthostInstance->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32});

    	daw_tls::tlsinstance& tls = daw_tls::getTls();
    	tls.host = vsthostInstance.get();
		LOG("START");
		request_type_vst24_t type;
		static const char* testPath = "C:\\PluginManager\\configs\\default\\hosts\\Ableton\\categories\\WaveShell-VST 9.6_x64.dll";
		strncpy(type.szPath, testPath, math::min<size_t>(1023U, strlen(testPath)+1));

		LOG("loadPlugin: %s", type.szPath);

//		response_type_vst24_t
//		tryLoadPlugin(vsthostInstance.get(), type);

		threadSleep(500);
		vsthost::getInstance()->destroy();
	} else if (argc > 0 && !strcmp("-client", argv[argc-1])) {
		logPrefixIdx = 1;
		setExceptionHandler();
		threadSleep(120);

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
		vsthostInstance->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32});


    	daw_tls::tlsinstance& tls = daw_tls::getTls();
    	// tls.mainCtrl = nullptr;

    	tls.host = vsthostInstance.get();

		pipe_msg_hdr hdr;
		recvbuf_t bufferRecv;
		LOG("listening...");
		while (!quit) {
			int retRead = readFromIPC(client, hdr);
			dbgassert(E_READ_ERR_BUF_SIZE != retRead);
			if (E_READ_OK != retRead) {
				LOG("hdr readFromIPC E_READ_ERR_PIPE");
				break;
			}
			if (hdr.cmd == CMD_PLUGIN_THREAD_QUIT) {
				LOG("CMD_PLUGIN_THREAD_QUIT");
				break;
			}
			if (hdr.cmd == CMD_PLUGIN_LOAD_REQUEST) {
				LOG("CMD_PLUGIN_LOAD_REQUEST");
				request_type_vst24_t req;
				if (E_READ_OK != readFromIPC(client, req)) {
					LOG("req readFromIPC failed");
					break;
				}

				LOG("tryLoadPlugin %s", req.szPath);
				try {

					vstpluginloadres res = vsthostInstance->loadPlugin(req.szPath, 0);
					LOG("result: %d", res.result);
					if (res.result < 0) {
						int response = CMD_PLUGIN_LOAD_ERROR;
						writeToIPC(client, response);
					} else {
						if (res.result == 1) {
							handles_t* handles = res.shellPluginHandle;
							String nameShellPlugin = res.name;
							log_printf("loading shell plugin: %s\n", StringAsCStr(nameShellPlugin));

							char tempName[64] = {0};
							VstInt32 plugUniqueID = 0;
							struct shell_plugin_entry_t {
								String name;
								int32_t pluginUID;
							};

							std::vector<shell_plugin_entry_t> entries;

							response_type_vst24_shell_plugin_t respShellPlugin;
							strncpy(respShellPlugin.szName, StringAsCStr(res.name), math::min<size_t>(255, res.name.length()+1));
							respShellPlugin.szName[255] = 0;
							// loop over all shell plugin entries
							while ((plugUniqueID = handles->aeffect->dispatcher (handles->aeffect, effShellGetNextPlugin, 0, 0, tempName, 0)) != 0)
							{
								// subplug needs a name
								if (tempName[0] != 0) {
									log_printf("plugUniqueID %d\t%s\n", plugUniqueID, tempName);
									entries.push_back(shell_plugin_entry_t{tempName, plugUniqueID});
								} else {
									log_printf("plugUniqueID %d\tNAME == NULL!\n", plugUniqueID);
								}
								tempName[0] = 0;
							}
							int response = CMD_PLUGIN_LOAD_SUCCESS_PLUGINSHELL_SHELL;
							writeToIPC(client, response);
							respShellPlugin.numPlugins = entries.size();
							writeToIPC(client, respShellPlugin);
							LOG("-- begin of shell plugin list --");
							for (auto& entry: entries) {
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
									strncpy(respShellPluginEntry.szName, StringAsCStr(entry.name), math::min<size_t>(255U, entry.name.length()+1));
									writeToIPC(client, respShellPluginEntry);
									LOG("unload shell entry: %08X", entry.pluginUID);
									vsthostInstance->unloadPlugin(resShellPluginEntry.plugin);

								}
							}
							LOG("-- end of shell plugin list --");
#ifdef _WIN32
							FreeLibrary((HMODULE)handles->hmodule);
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
							vsthostInstance->unloadPlugin(res.plugin);
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
//			LOG("client sendData()");
//			if (!sendData(&client, &hdr, &data)) {
//				LOG("sendData failed");
//				break;
//			}
			threadSleep(50);
		}
		LOG("client_close()");
		client.client_close();
		threadSleep(500);
		vsthost::getInstance()->destroy();
	} else {
	}
	return 0;
}
