/*
 *  Minimal duk command line C++ example.
 *  Adapted from tests/duk_cmdline.c
 */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || \
    defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
/* Suppress warnings about plain fopen() etc. */
#define _CRT_SECURE_NO_WARNINGS
#if defined(_MSC_VER) && (_MSC_VER < 1900)
/* Workaround for snprintf() missing in older MSVC versions.
 * Note that _snprintf() may not NUL terminate the string, but
 * this difference does not matter here as a NUL terminator is
 * always explicitly added.
 */
#define snprintf _snprintf
#endif
#endif

#if defined(DUK_CMDLINE_PTHREAD_STACK_CHECK)
#define _GNU_SOURCE
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
extern "C" {
#include "duktape.h"
}

#include <memory>
#include <vector>
#include <string>
#include "js/interface/duk_daw_interface.h"
#include "commandline_rep.h"
#include "host/mainctrl.h"
#include "threads.h"
#include "platform.h"
#include <iostream>
#include <windows.h>
#include <conio.h>
#include "net/network.h"
#include "net/packet.h"
#include "net/stream/audiostream.h"

#define  MEM_LIMIT_NORMAL   (128*1024*1024)   /* 128 MB */
#define  MEM_LIMIT_HIGH     (2047*1024*1024)  /* ~2 GB */
#define  LINEBUF_SIZE       65536
#define ALLOW_BYTECODE_INPUT true

/* Print error to stderr and pop error. */
static void print_pop_error(duk_context *ctx, FILE *f) {
	fprintf(f, "%s\n", duk_safe_to_stacktrace(ctx, -1));
	fflush(f);
	duk_pop(ctx);
}
struct call_context_t {
	int returnVal = -1;
	std::string strOut;
};

static duk_ret_t wrapped_compile_execute(duk_context *ctx, void *udata) {
	const char *src_data;
	duk_size_t src_len;
	duk_uint_t comp_flags;

	call_context_t* ctxt = (call_context_t*) udata;

	/* XXX: Here it'd be nice to get some stats for the compilation result
	 * when a suitable command line is given (e.g. code size, constant
	 * count, function count.  These are available internally but not through
	 * the public API.
	 */

	/* Use duk_compile_lstring_filename() variant which avoids interning
	 * the source code.  This only really matters for low memory environments.
	 */

	/* [ ... bytecode_filename src_data src_len filename ] */

	src_data = (const char *) duk_require_pointer(ctx, -3);
	src_len = (duk_size_t) duk_require_uint(ctx, -2);

	if (src_data != NULL && src_len >= 1 && src_data[0] == (char) 0xbf) {
		/* Bytecode. */
		if (ALLOW_BYTECODE_INPUT) {
			void *buf;
			buf = duk_push_fixed_buffer(ctx, src_len);
			memcpy(buf, (const void *) src_data, src_len);
			duk_load_function(ctx);
		} else {
			(void) duk_type_error(ctx, "bytecode input rejected (use -b to allow bytecode inputs)");
		}
	} else {
		/* Source code. */
		comp_flags = DUK_COMPILE_SHEBANG;
		duk_compile_lstring_filename(ctx, comp_flags, src_data, src_len);
	}

	/* [ ... bytecode_filename src_data src_len function ] */

	/* Optional bytecode dump. */
	if (duk_is_string(ctx, -4)) {
		FILE *f;
		void *bc_ptr;
		duk_size_t bc_len;
		size_t wrote;
		char fnbuf[256];
		const char *filename;

		duk_dup_top(ctx);
		duk_dump_function(ctx);
		bc_ptr = duk_require_buffer_data(ctx, -1, &bc_len);
		filename = duk_require_string(ctx, -5);
		snprintf(fnbuf, sizeof(fnbuf), "%s", filename);
		fnbuf[sizeof(fnbuf) - 1] = (char) 0;

		f = fopen(fnbuf, "wb");
		if (!f) {
			(void) duk_generic_error(ctx, "failed to open bytecode output file");
		}
		wrote = fwrite(bc_ptr, 1, (size_t) bc_len, f);  /* XXX: handle partial writes */
		(void) fclose(f);
		if (wrote != bc_len) {
			(void) duk_generic_error(ctx, "failed to write all bytecode");
		}

		return 0;  /* duk_safe_call() cleans up */
	}

#if 0
	/* Manual test for bytecode dump/load cycle: dump and load before
	 * execution.  Enable manually, then run "make ecmatest" for a
	 * reasonably good coverage of different functions and programs.
	 */
	duk_dump_function(ctx);
	duk_load_function(ctx);
#endif

	duk_push_global_object(ctx);  /* 'this' binding */
	duk_call_method(ctx, 0);


	/*
	 *  In interactive mode, write to stdout so output won't
	 *  interleave as easily.
	 *
	 *  NOTE: the ToString() coercion may fail in some cases;
	 *  for instance, if you evaluate:
	 *
	 *    ( {valueOf: function() {return {}},
	 *       toString: function() {return {}}});
	 *
	 *  The error is:
	 *
	 *    TypeError: coercion to primitive failed
	 *            duk_api.c:1420
	 *
	 *  These are handled now by the caller which also has stack
	 *  trace printing support.  User code can print out errors
	 *  safely using duk_safe_to_string().
	 */

	duk_push_global_stash(ctx);
	duk_get_prop_string(ctx, -1, "dukFormat");
	duk_dup(ctx, -3);
	duk_call(ctx, 1);  /* -> [ ... res stash formatted ] */
	const char* szStr = duk_to_string(ctx, -1);
	ctxt->returnVal = 0;
	ctxt->strOut = szStr;


	return 0;  /* duk_safe_call() cleans up */
}

static void cmdline_fatal_handler(void *udata, const char *msg) {
	(void) udata;
	fprintf(stderr, "*** FATAL ERROR: %s\n", msg ? msg : "no message");
	fprintf(stderr, "Causing intentional segfault...\n");
	fflush(stderr);
	*((volatile unsigned int *) 0) = (unsigned int) 0xdeadbeefUL;
	abort();
}
/*
 *  String.fromBufferRaw()
 */

static duk_ret_t string_frombufferraw(duk_context *ctx) {
	duk_buffer_to_string(ctx, 0);
	return 1;
}
namespace {

class tcp_handler_cli_client : public inetwork_handler {
public:
	std::shared_ptr<network_conn_t> conn;
	std::vector<uint8_t> buf;
	bool connected = false;
	tcp_handler_cli_client() = delete;
	tcp_handler_cli_client(std::shared_ptr<network_conn_t> _conn) : conn(_conn) {

	}
	void onError(int errorType, String msg) override {
		my_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		buf.insert(buf.end(), reinterpret_cast<uint8_t*>(data), reinterpret_cast<uint8_t*>(data)+size);
		return true;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
		conn->setSocketOpt(IPPROTO_TCP, TCP_NODELAY, 1);
		conn->parent->setSelectTimeout(0.0001);
		my_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		my_printf("disconnected\n", 0);
		connected = false;
	}

	void writeBuffer(void* buf, size_t size) {
		conn->write(buf, size);
	}
	void handlePackets() {
		if (buf.size())
			my_printf("process client recv buf of size %d\n", buf.size());
		buf.clear();
	}
};
class tcp_handler_cli_server : public inetwork_handler {
public:
	struct cli_server_client_conn_t {
		std::shared_ptr<network_conn_t> conn;
		std::shared_ptr<tcp_handler_cli_client> handler;
	};
	std::vector<std::shared_ptr<cli_server_client_conn_t>> conns;
	bool connected = false;
	tcp_handler_cli_server() {

	}
	void writeBufferToAll(const String& strBuf) {
		writeBufferToAll((void*)strBuf.c_str(), strBuf.length());
	}
	void writeBufferToAll(void* buf, size_t size) {
		for (auto& pc : conns) {
			pc->handler->writeBuffer(buf, size);
		}
	}
	void onError(int errorType, String msg) override {
		my_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		dbgassert(0); // not called
		return 0;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
		conn->setSocketOpt(IPPROTO_TCP, TCP_NODELAY, 1);
		conn->parent->setSelectTimeout(0.0001);
		my_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		my_printf("disconnected\n", 0);
		connected = false;
	}
	virtual bool onAccept(std::shared_ptr<network_conn_t> clientConn) {
		my_printf("onAccept %s\n", StringAsCStr(clientConn->address));
		auto handler = std::make_shared<tcp_handler_cli_client>(clientConn);
		clientConn->handler = handler.get();
		conns.push_back(std::shared_ptr<cli_server_client_conn_t>(new cli_server_client_conn_t{clientConn, handler}));
		return true;
	}
};
}
namespace NU {
namespace CONSOLE {
class CommandLineREP_TCP::CLIImpl {
	duk_context *ctx;
	std::vector<std::string> commandQueue;
	std::atomic_bool hasCommands{false};
	std::recursive_mutex mutex;
	tcp_handler_cli_server handler;
	rep_running_state& threadState;
public:
	CLIImpl(rep_running_state& _threadState) : threadState(_threadState) {
		/*
		 *  Create heap
		 */
		ctx = duk_create_heap(NULL, NULL, NULL, NULL, cmdline_fatal_handler);
		/* Register String.fromBufferRaw() which does a 1:1 buffer-to-string
		 * coercion needed by testcases.  String.fromBufferRaw() is -not- a
		 * default built-in!  For stripped builds the 'String' built-in
		 * doesn't exist and we create it here; for ROM builds it may be
		 * present but unwritable (which is ignored).
		 */
		duk_eval_string(ctx,
			"(function(v){"
			    "if (typeof String === 'undefined') { String = {}; }"
			    "Object.defineProperty(String, 'fromBufferRaw', {value:v, configurable:true});"
			"})");
		duk_push_c_function(ctx, string_frombufferraw, 1 /*nargs*/);
		(void) duk_pcall(ctx, 1);
		duk_pop(ctx);

		/* Stash a formatting function for evaluation results. */
		duk_push_global_stash(ctx);
		duk_eval_string(ctx,
			"(function (E) {"
			    "return function format(v){"
			        "try{"
			            "return E('jx',v);"
			        "}catch(e){"
			            "return ''+v;"
			        "}"
			    "};"
			"})(Duktape.enc)");
		duk_put_prop_string(ctx, -2, "dukFormat");
		duk_pop(ctx);
		NU::SCRIPTING::registerInterfaceToContext(ctx);
		fflush(stdin);
	}
	~CLIImpl() {
		duk_destroy_heap(ctx);
	}
protected:
	int printAndExecute(const std::string&& str) {
		fprintf(stdout, "%s", str.c_str());
		call_context_t ctxt;
		execute(str.c_str(), str.length(), ctxt);
		if (ctxt.returnVal == 0) {
			fprintf(stdout, " = %s\n", ctxt.strOut.c_str());
		}
		return ctxt.returnVal;
	}
	int execute(std::string&& str, call_context_t& ctxt) {
		return execute(str.c_str(), str.length(), ctxt);
	}
	int execute(const char* szScript, size_t size, call_context_t& ctxt) {
		duk_push_pointer(ctx, (void *) szScript);
		duk_push_uint(ctx, (duk_uint_t) size);
		duk_push_string(ctx, "input");
		duk_int_t rc = duk_safe_call(ctx, wrapped_compile_execute, &ctxt /*udata*/, 3 /*nargs*/, 1 /*nret*/);
		if (rc != DUK_EXEC_SUCCESS) {
			// throw!
			/* in interactive mode, write to stdout */
			print_pop_error(ctx, stdout);
			return 1;
		}
		duk_pop(ctx);
		return 0;
	}

public:
	void init() {
		network_init();
		using NU::SCRIPTING::DawInterface;
		NU::SCRIPTING::setGlobalInstance(ctx, MainCtrl::get());
	}
	int runConsole() {
		threadState.isRunning = true;
		network_io netio(&handler);
		std::shared_ptr<network_conn_t> conn;
		std::shared_ptr<network_conn_t> listenSocket;
		if (netio.listenAt(nullptr, 2123, listenSocket)) {
			netio.setSelectTimeout(0.0001);
			while (netio.hasOpenSockets() && !threadState.shouldQuit) {
				netio.update();

				threadState.isConnected = handler.conns.size() > 0;
				for (auto& pc : handler.conns) {
					if (!pc->handler->buf.size()) {
						continue;
					}
					std::string strBuf;
					strBuf.assign(pc->handler->buf.begin(), pc->handler->buf.end());
					pc->handler->buf.clear();
					std::lock_guard<std::recursive_mutex> lock(mutex);
					commandQueue.push_back(std::move(strBuf));
					hasCommands = true;
				}
			}
		}
		threadState.isRunning = false;
//		bool got_eof = false;
//		while (_kbhit()) {
//			char c = _getch();
//			if (c == EOF) {
//				got_eof = 1;
//				return 0;
//			} else if (c == '\n'||c == '\r') {
//				if (bufferIdx > 0) {
//				}
//				return 1;
//			} else if (bufferIdx >= inputBuffer.size()) {
//				log_printf("Input exceeds buffer size!\n", 0);
//				return -1;
//			} else {
//				inputBuffer[bufferIdx++] = (char) c;
//			}
//		}
		return 0;
	}
	int executeCommands() {
		if (!hasCommands)
			return 0;
		std::vector<std::string> tmpCommandQueue;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex);
			//TODO: move from
			tmpCommandQueue = commandQueue;
			commandQueue.clear();
			hasCommands= false;
		}
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		for (std::string& str : tmpCommandQueue) {
			if (str.length() == 0) {
				continue;
			}
			log_out("js input: '%s'\n", str.c_str());
			call_context_t ctxt;
			try {
				duk_push_pointer(ctx, (void *) str.c_str());
				duk_push_uint(ctx, (duk_uint_t) str.length());
				duk_push_string(ctx, "input");
				duk_int_t rc = duk_safe_call(ctx, wrapped_compile_execute, &ctxt /*udata*/, 3 /*nargs*/, 1 /*nret*/);
				if (rc != DUK_EXEC_SUCCESS) {
					// throw!
					/* in interactive mode, write to stdout */
					const char* tmpStackTrace = duk_safe_to_stacktrace(ctx, -1);
					if (tmpStackTrace) {
						auto excDesc = StringFormat("%s\r\n", tmpStackTrace);
						handler.writeBufferToAll(excDesc);
						fwrite(excDesc.c_str(), excDesc.length(), 1, stdout);
						fflush(stdout);
					}
					duk_pop(ctx);
					return 1;
				}
				duk_pop(ctx);
			} catch (std::exception& e) {
				auto excDesc = StringFormat("Exception on CLI: %s\r\n", e.what());
				handler.writeBufferToAll(excDesc);
				fwrite(excDesc.c_str(), excDesc.length(), 1, stdout);
				fflush(stdout);

			}
			if (ctxt.returnVal == 0) {
				auto jsStdOut = StringFormat("%s\r\n", ctxt.strOut.c_str());
				handler.writeBufferToAll((void*)jsStdOut.c_str(), jsStdOut.length());
				fwrite(jsStdOut.c_str(), jsStdOut.length(), 1, stdout);
				fflush(stdout);

			}
		}
		return 0;
	}

};
CommandLineREP_TCP::CommandLineREP_TCP() : m_impl(new CommandLineREP_TCP::CLIImpl{runState}) {
}
CommandLineREP_TCP::~CommandLineREP_TCP() {
	delete m_impl;
}
int CommandLineREP_TCP::runConsole() {
	return m_impl->runConsole();
}
void CommandLineREP_TCP::init() {
	return m_impl->init();
}
int CommandLineREP_TCP::executeCommands() {
	return m_impl->executeCommands();
}

}
}

