#include "str_util.h"
#include "../vst_sdk_2.4/aeffectx.h"
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
#include "settings.h"
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#endif


#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)

#define CMD_PLUGIN_LOAD_ERROR 3
#define CMD_PLUGIN_LOAD_SUCCESS 2
#define CMD_PLUGIN_LOAD_REQUEST 1
#define CMD_PLUGIN_THREAD_QUIT 4
#define BUF_SIZE 2048

struct pipe_msg_hdr {
	uint32_t cmd;
};
struct vst_metadata {
	uint32_t id;
	uint32_t version;
	uint32_t vstVersion;
	uint32_t pluginCategory;
	bool isSynth;
	char szPath[1024];
	char szName[256];
	char szVendorName[256];
};
char buf[BUF_SIZE];
bool sendData(ipc_connection* conn, pipe_msg_hdr* hdr, vst_metadata* data) {
    memset(buf, 0, BUF_SIZE);
    char* bufPos = buf;
    memcpy(bufPos, hdr, sizeof(pipe_msg_hdr));
    bufPos += sizeof(pipe_msg_hdr);
    memcpy(bufPos, data, sizeof(vst_metadata));
    bufPos += sizeof(vst_metadata);
    int len = conn->sendData(buf, BUF_SIZE);
    if (len == BUF_SIZE) {
    	return true;
    }
    return false;
}
bool recvData(ipc_connection* conn, pipe_msg_hdr* hdr, vst_metadata* data) {
    memset(buf, 0, BUF_SIZE);
    int len = conn->readData(buf, BUF_SIZE);
    if (len == BUF_SIZE) {
//    	printf("Read %d bytes, expected %d\n", (int32_t)bytesSent, (int32_t)BUF_SIZE);
		char* bufPos = buf;
		memcpy(hdr, bufPos, sizeof(pipe_msg_hdr));
		bufPos += sizeof(pipe_msg_hdr);
		if (data) {
			memcpy(data, bufPos, sizeof(vst_metadata));
			bufPos += sizeof(vst_metadata);
		}
    	return true;
    }
    return false;
}

void getPluginData(vstplugin* plugin, vst_metadata* _out) {
	AEffect* aeffect = plugin->handle->aeffect;
	_out->id = aeffect->uniqueID;
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
    appsettings settings;
    loadSettings(settings);
    bool lastRecvState = false;
//	std::set_terminate(terminate_fn);
	if (argc > 1 && !strcmp("-server", argv[1])) {
		bool launchProcess = true;
		bool dryRun = false;
		String rescanPattern = "";
	    for (int i = 2; i < argc; i++) {
	    	if (argv[i] && strlen(argv[i]) > 2 && argv[i][0] == '-') {
	    		if (!strcmp(argv[i], "-wait")) {
	    			launchProcess = false;
	    		}
	    		if (!strcmp(argv[i], "-dry")) {
	    			dryRun = true;
	    		}
	    		if (!strcmp(argv[i], "-rescan") && i+1 < argc) {
	    			rescanPattern = argv[i+1];
	    		}
	    	}
	    }
	    if (rescanPattern != "") {
	    	LOG("RESCAN *%s*", StringAsCStr(rescanPattern));
	    }
	    try {
			SQLite::Database    db("data/plugins.db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";
			createTables(db);

			threadSleep(1000);

			std::vector<FileFound> files;
//			String vstPlugPath = "C:/PluginManager/configs/default/hosts/Ableton/categories/";
			String vstPlugPath = settings.pluginPath;
			LOG("pluginPath %s", StringAsCStr(vstPlugPath));
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
#ifdef __linux__
			String exeName = "plugin_scan";
		    char buff[4096];
		    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
		    if (len != -1) {
		      buff[len] = '\0';
		      exeName = buff;
		    }
#endif

			ipc_server server;
			int ipc_status = server.server_open("vst_scanner_pipe");
			if (ipc_status) {
				LOG("Failed opening ipc_server: %d", ipc_status);
				return 1;
			}
			pipe_msg_hdr hdr;
			vst_metadata data;
			std::unique_ptr<ProcessThread> thread;
			int a = 0;
			bool pipeConnected = false;
//			db.exec("delete from plugins where 1");
			SQLite::Statement   queryPlugin(db, "SELECT id, moddate FROM plugins where path == ?");
			SQLite::Statement   queryInsertPlugin(db, "INSERT INTO "
					"plugins(isSynth, uid, version, vstVersion, category, moddate, ok, path, name, vendorName) "
					"VALUES(?,?,?,?,?,?,?,?,?,?)");
			SQLite::Statement   queryDelete(db, "DELETE from plugins where id = ?");
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
				bool needScan = true;
				queryPlugin.reset();
				queryPlugin.bind(1, file.path);
				if (queryPlugin.executeStep())
				{
					id = queryPlugin.getColumn(0).getInt();
					int64_t timeDB = queryPlugin.getColumn(1).getInt64();
//					LOG("id %d timeDisk %016llX %016llX", id, timeDisk, timeDB);
					if (timeDisk == timeDB) {
						needScan = false;
					}
				}
				if (rescanPattern.length()) {
					needScan = false;
					needScan = file.name.find(rescanPattern) != String::npos;
					if (!needScan) {
						continue;
					}
				} else {
					if (!needScan && !dryRun) {
						continue;
					}
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

					thread->startProcess(exeName, "-client");
					threadSleep(200);
					if (!thread->isRunning()) {
						thread->checkExcepetion();
						LOG("Failed starting client");
						break;
					}
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
				memset(&data, 0, sizeof(data));
				memset(&hdr, 0, sizeof(hdr));
				hdr.cmd = CMD_PLUGIN_LOAD_REQUEST;
				strncpy(data.szPath, StringAsCStr(file.path), file.path.length());
				//			LOG("SCAN %s   %s", StringAsCStr(file.path), data.szPath);
				//			LOG("server sendData()");
				bool ok = sendData(&server, &hdr, &data);
				if (!ok) {
					LOG("sendData failed");
					server.server_disconnect();
					pipeConnected = false;
					continue;
				}
				threadSleep(50);
				ok = recvData(&server, &hdr, &data);
				lastRecvState = ok;
				bool status = false;
				if (!ok) {
					LOG("recvData failed");
					server.server_disconnect();
					pipeConnected = false;
				} else {
					status = hdr.cmd == CMD_PLUGIN_LOAD_SUCCESS;
				}
				printf("%s %s\n", data.szPath, status ?"GOOD":"BAD");
				if (dryRun)
					continue;
				if (id > 0) {
					queryDelete.reset();
					queryDelete.bind(1, id);
					queryDelete.exec();
				}
				try {
					queryInsertPlugin.reset();
					int bndIdx = 1;
					queryInsertPlugin.bind(bndIdx++, data.isSynth);
					queryInsertPlugin.bind(bndIdx++, data.id);
					queryInsertPlugin.bind(bndIdx++, data.version);
					queryInsertPlugin.bind(bndIdx++, data.vstVersion);
					queryInsertPlugin.bind(bndIdx++, data.pluginCategory);
					queryInsertPlugin.bind(bndIdx++, (long long int)timeDisk);
					queryInsertPlugin.bind(bndIdx++, status);
					queryInsertPlugin.bind(bndIdx++, file.path);
					queryInsertPlugin.bind(bndIdx++, data.szName);
					queryInsertPlugin.bind(bndIdx++, data.szVendorName);
					int insertRowsAffected = queryInsertPlugin.exec();
//					LOG("insertRowsAffected %d",insertRowsAffected);
				} catch (SQLite::Exception& e) {
					std::cout << "queryInsertPlugin exception: " << e.getErrorStr() << std::endl;
				}
				threadSleep(200);
			}
			if (thread && thread->isRunning() && pipeConnected) {

				memset(&data, 0, sizeof(data));
				memset(&hdr, 0, sizeof(hdr));
				hdr.cmd = CMD_PLUGIN_THREAD_QUIT;
				sendData(&server, &hdr, &data);
				server.server_disconnect();
				thread->joinProcess();
			}
			thread.reset();
	    } catch (SQLite::Exception& e) {
			std::cout << "SQLite exception: " << e.getErrorStr() << std::endl;
		} catch (std::exception& e) {
			std::cout << "exception: " << e.what() << std::endl;
		} catch (...) {
			std::cout << "Unhandled exception" << std::endl;
		}

		LOG("Done.");
		threadSleep(500);
	} else if (argc > 0 && !strcmp("-client", argv[argc-1])) {
		setExceptionHandler();
		threadSleep(120);
	    // Open the named pipe
	    // Most of these parameters aren't very relevant for pipes.
		ipc_client client;
		int ipcstatus = client.client_connect("vst_scanner_pipe");
		if (ipcstatus) {
			LOG("Failed opening ipc_client: %d", ipcstatus);
			return 1;
		}
	    vsthost::setInstance(std::make_unique<vsthost>());
	    auto audiohost = vsthost::getInstance();
		LOG("START");
		pipe_msg_hdr hdr;
		vst_metadata data;
		while (!quit) {
//			LOG("client recvData()");
			if (!recvData(&client, &hdr, &data)) {
				LOG("recvData failed");
				break;
			}
			if (hdr.cmd == CMD_PLUGIN_THREAD_QUIT) {
				break;
			}
			if (hdr.cmd == CMD_PLUGIN_LOAD_REQUEST) {
				hdr.cmd = CMD_PLUGIN_LOAD_ERROR;
				LOG("loadPlugin: %s", data.szPath);
				try {

					vstpluginloadres res = audiohost->loadPlugin(data.szPath);
					if (res.result == 0) {
						vstplugin* plugin = res.plugin;
//						printf("%d params in %d categories\n", plugin->params.size(), plugin->paramsCategories.size());
//
//						for (vst_param& cat : plugin->params) {
//							printf("param[%d] (%s) flags: ", cat.idx, StringAsCStr(cat.label));
//							if (cat.flags & ParamIsSwitch) {
//								printf("ParamIsSwitch ");
//							}
//							if (cat.flags & ParamUsesIntegerMinMax) {
//								printf("ParamUsesIntegerMinMax ");
//							}
//							if (cat.flags & ParamUsesFloatStep) {
//								printf("ParamUsesFloatStep ");
//							}
//							if (cat.flags & ParamUsesIntStep) {
//								printf("ParamUsesIntStep ");
//							}
//							if (cat.flags & ParamSupportsDisplayIndex) {
//								printf("ParamSupportsDisplayIndex ");
//							}
//							if (cat.flags & ParamSupportsDisplayCategory) {
//								printf("ParamSupportsDisplayCategory ");
//							}
//							if (cat.flags & ParamSupportsDisplayIndex) {
//								printf("ParamSupportsDisplayIndex ");
//							}
//							if (cat.flags & ParamCanRamp) {
//								printf("ParamCanRamp ");
//							}
//							if (cat.flags & ParamIsAdvanced) {
//								printf("ParamIsAdvanced ");
//							}
//							if (!cat.flags) {
//								printf("0");
//							}
//							printf("\n");
//
//						}
//						for (vst_param_category& cat : plugin->paramsCategories) {
//							printf("categories[%d] = %s (%d params)\n", cat.idx, StringAsCStr(cat.label), cat.numParametersInCategory);
//						}
						getPluginData(res.plugin, &data);
						audiohost->unloadPlugin(plugin);
						hdr.cmd = CMD_PLUGIN_LOAD_SUCCESS;
					}
				} catch (...) {
					LOG("exception while loading %s", data.szPath);
				}
			}
//			LOG("client sendData()");
			if (!sendData(&client, &hdr, &data)) {
				LOG("sendData failed");
				break;
			}
			threadSleep(50);
		}
		client.client_close();
		threadSleep(500);
		vsthost::getInstance()->destroy();
	} else {
	}
	return 0;
}
