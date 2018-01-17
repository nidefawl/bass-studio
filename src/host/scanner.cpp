#include "str_util.h"
#include "../host/vst_host.h"
#include "../host/vst_plugin.h"
#include "../vst_sdk_2.4/aeffectx.h"
#include "../host/vst_plugin_handles.h"
#include "fileio.h"
#include "exceptions.h"
#include <windows.h>
#include "../threads/childprocessthread.h"
#include "platform.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include <iostream>
#include <memory>


#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)

#define CMD_PLUGIN_LOAD_ERROR 3
#define CMD_PLUGIN_LOAD_SUCCESS 2
#define CMD_PLUGIN_LOAD_REQUEST 1
#define CMD_PLUGIN_THREAD_QUIT 4
#define BUF_SIZE 2048
#define SCANNER_PIPE_NAME _T("\\\\.\\pipe\\vst_scanner_pipe")
HANDLE pipe = NULL;

void closePipe() {
	if (pipe) {
	    CloseHandle(pipe);
		pipe = NULL;
	}
}
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
bool sendData(pipe_msg_hdr* hdr, vst_metadata* data) {
    memset(buf, 0, BUF_SIZE);
    char* bufPos = buf;
    memcpy(bufPos, hdr, sizeof(pipe_msg_hdr));
    bufPos += sizeof(pipe_msg_hdr);
    memcpy(bufPos, data, sizeof(vst_metadata));
    bufPos += sizeof(vst_metadata);
    DWORD bytesSent;
    if (WriteFile(pipe, buf, BUF_SIZE, &bytesSent, NULL)) {
//    	printf("wrote %d bytes, expected %d\n", (int32_t)bytesSent, (int32_t)BUF_SIZE);
    	return true;
    }
//    printf("Error: WriteFile operation failed: %lu\n", GetLastError());
    return false;
}
bool recvData(pipe_msg_hdr* hdr, vst_metadata* data) {
    memset(buf, 0, BUF_SIZE);
    DWORD bytesSent;
    if (ReadFile(pipe, buf, BUF_SIZE, &bytesSent, NULL)) {
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
//    printf("Error: ReadFile operation failed: %lu\n", GetLastError());
    return false;
}

//void testVSTPlugins()
//{
//    MSG msg;
//    HWND hwnd;
//    WNDCLASS wc;
//
//    wc.style         = CS_HREDRAW | CS_VREDRAW;
//    wc.cbClsExtra    = 0;
//    wc.cbWndExtra    = 0;
//    wc.lpszClassName = WINDOW_NAME;
//    wc.hInstance     = GetModuleHandle(NULL);
//    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
//    wc.lpszMenuName  = NULL;
//    wc.lpfnWndProc   = WndProc;
//    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
//    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
//
//    RegisterClass(&wc);
//    hwnd = CreateWindow(wc.lpszClassName, WINDOW_NAME,
//                WS_OVERLAPPEDWINDOW,
//                100, 100, 350, 250, NULL, NULL, wc.hInstance, NULL);
//    assert(hwnd != NULL);
//
////    ShowWindow(hwnd, SW_HIDE);
//    UpdateWindow(hwnd);
//
//    while  (GetMessage(&msg, NULL, 0, 0))
//    {
//        DispatchMessage(&msg);
//    }
//}
void getPluginData(vstplugin* plugin, vst_metadata* _out) {
	AEffect* aeffect = plugin->handle->aeffect;
	_out->id = aeffect->uniqueID;
	_out->version = aeffect->version;
	_out->vstVersion = plugin->vstVersion;
	_out->pluginCategory = plugin->pluginCategory;
	strncpy_s(_out->szName, StringAsCStr(plugin->sName), plugin->sName.length());
	if (!plugin->dispatch(effGetVendorString, 0, 0, (void*)_out->szVendorName)) {
		_out->szVendorName[0] = 0;
	}
	_out->isSynth = plugin->isSynth;
}
void printLastError(String fn) {
	DWORD err = GetLastError();
	printf("%s failed (%d): %s\n", StringAsCStr(fn), (int32_t)err, StringAsCStr(FormatErrorMessage(err)));
}
void createTables(SQLite::Database& db);
class FileTimeGetter {
public:
    FILETIME ftCreate = {0};
    FILETIME ftAccess = {0};
    FILETIME ftWrite = {0};
    HANDLE hFile = {0};
    bool ok = false;
public:
    int64_t getWriteTimeI64() {
    	if (!ok) {
    		return 0;
    	}
    	int64_t time = (uint64_t)ftWrite.dwLowDateTime;
    	time = (uint64_t)time | (uint64_t)ftWrite.dwHighDateTime << 32;
    	return time;
    }
	FileTimeGetter(String path) {
	    hFile = CreateFile(StringAsCStr(path), GENERIC_READ, FILE_SHARE_READ, NULL,
	        OPEN_EXISTING, 0, NULL);
	    if(hFile != INVALID_HANDLE_VALUE)
	    {
	    	ok = GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
	    }

	}
	~FileTimeGetter() {
	    if(hFile != INVALID_HANDLE_VALUE)
	    	CloseHandle(hFile);
	}
};
bool quit = false;
bool inConnectNamedPipe = false;
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
int main(int argc, char* argv[]) {
	LOG("ARGC %d", argc);
	for (int i = 0; i < argc; i++) {
		LOG("argv[%d] %s", i, argv[i]);
	}
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)) {
        fprintf(stderr, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }

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

			Sleep(1000);

			std::vector<FileFound> files;
			String vstPlugPath = "C:/PluginManager/configs/default/hosts/Ableton/categories/";
			findFilesWithExt(vstPlugPath, "dll", true, files);
			LOG("Found %u files", (uint32_t )files.size());
			if (files.empty()) {
				return 1;
			}
			TCHAR szFileName[MAX_PATH + 1];
			GetModuleFileName(NULL, szFileName, MAX_PATH + 1);
			String exeName = szFileName;

			// Create a pipe to send data
			if (pipe)
			DisconnectNamedPipe(pipe);
			pipe = CreateNamedPipe(
			SCANNER_PIPE_NAME, // name of the pipe
					PIPE_ACCESS_DUPLEX,
					PIPE_TYPE_BYTE | PIPE_WAIT, // send data as a byte stream
					1, // only allow 1 instance of this pipe
					0, // no outbound buffer
					0, // no inbound buffer
					3000, // use default wait time
					NULL // use default security attributes
					);
			if (!pipe || pipe == INVALID_HANDLE_VALUE) {
				LOG("INVALID_HANDLE_VALUE");
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
				if (!needScan && !dryRun) {
					if (rescanPattern.length() && file.name.find(rescanPattern) != String::npos) {
					} else {
//						LOG("skip plugin %s (%d)\n", StringAsCStr(file.path), id);

						continue;
					}
				}
				if (launchProcess&& (!thread || !thread->isRunning())) {
					if (thread)
						thread->joinProcess();
					if (pipeConnected) {
						DisconnectNamedPipe(pipe);
					}
					Sleep(1200);
					pipeConnected = false;
					thread = std::make_unique<ProcessThread>();
					LOG("!thread.isRunning(), last recv state: %s", lastRecvState ?"GOOD":"BAD");
					String arg1 = "-client";
					String lastCmd = StringFormat("%s %s", StringAsCStr(exeName), StringAsCStr(arg1));

					thread->startProcess(exeName, "-client");
					Sleep(200);
					if (!thread->isRunning()) {
						thread->checkExcepetion();
						LOG("Failed starting client");
						break;
					}
				}
				if (!pipeConnected && (!launchProcess || (thread && thread->isRunning()))) {
					LOG("ConnectNamedPipe()");
					inConnectNamedPipe = true;
					bool connectStatus = ConnectNamedPipe(pipe, NULL);
					inConnectNamedPipe = false;
					if (!connectStatus && GetLastError() != ERROR_PIPE_CONNECTED) {
						printLastError("ConnectNamedPipe");
						CloseHandle(pipe);
						break;
					}
					pipeConnected = true;
				}
				if (!pipe) {
					LOG("!pipe");
					break;
				}
				memset(&data, 0, sizeof(data));
				memset(&hdr, 0, sizeof(hdr));
				hdr.cmd = CMD_PLUGIN_LOAD_REQUEST;
				strncpy_s(data.szPath, StringAsCStr(file.path), file.path.length());
				//			LOG("SCAN %s   %s", StringAsCStr(file.path), data.szPath);
				//			LOG("server sendData()");
				bool ok = sendData(&hdr, &data);
				if (!ok) {
					printLastError("sendData");
					DisconnectNamedPipe(pipe);
					pipeConnected = false;
					continue;
				}
				Sleep(50);
				ok = recvData(&hdr, &data);
				lastRecvState = ok;
				bool status = false;
				if (!ok) {
					printLastError("recvData");
					DisconnectNamedPipe(pipe);
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
					queryInsertPlugin.bind(bndIdx++, timeDisk);
					queryInsertPlugin.bind(bndIdx++, status);
					queryInsertPlugin.bind(bndIdx++, file.path);
					queryInsertPlugin.bind(bndIdx++, data.szName);
					queryInsertPlugin.bind(bndIdx++, data.szVendorName);
					int insertRowsAffected = queryInsertPlugin.exec();
//					LOG("insertRowsAffected %d",insertRowsAffected);
				} catch (SQLite::Exception& e) {
					std::cout << "queryInsertPlugin exception: " << e.getErrorStr() << std::endl;
				}
				Sleep(200);
			}
			if (thread && thread->isRunning() && pipe) {

				memset(&data, 0, sizeof(data));
				memset(&hdr, 0, sizeof(hdr));
				hdr.cmd = CMD_PLUGIN_THREAD_QUIT;
				sendData(&hdr, &data);
				DisconnectNamedPipe(pipe);
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
		closePipe();

		LOG("Done.");
		Sleep(500);
	} else if (argc > 0 && !strcmp("-client", argv[argc-1])) {
		setExceptionHandler();
		Sleep(120);
	    // Open the named pipe
	    // Most of these parameters aren't very relevant for pipes.
		LOG("WaitNamedPipe");
		if (!WaitNamedPipe(SCANNER_PIPE_NAME, 3000)) {
			LOG("WaitNamedPipe timeout");
			closePipe();
			Sleep(5000);
			return 1;
		}
		LOG("CreateFile");

		pipe = CreateFile(
			SCANNER_PIPE_NAME,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);
	    if (!pipe || pipe == INVALID_HANDLE_VALUE) {
			LOG("INVALID_HANDLE_VALUE");
			Sleep(5000);
			return 1;
	    }
	    project_globals_t project;
	    vsthost::setInstance(std::make_unique<vsthost>(project));
	    auto audiohost = vsthost::getInstance();
		LOG("START");
		pipe_msg_hdr hdr;
		vst_metadata data;
		while (pipe && !quit) {
//			LOG("client recvData()");
			if (!recvData(&hdr, &data)) {
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
			if (!sendData(&hdr, &data)) {
				break;
			}
			Sleep(50);
		}
		closePipe();
		Sleep(500);
		vsthost::getInstance()->destroy();
	} else {
	}
	return 0;
}
