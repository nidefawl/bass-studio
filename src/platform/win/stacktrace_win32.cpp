#include <windows.h>
#include <DbgHelp.h>

#include <stdio.h>
#include <stdlib.h>
#include "logging.h"
extern "C" {
	static const char* shortName(const char* input, int maxPathSegs=1) {
		if (input) {
			size_t inLen = strlen(input);
			const char* pos = input + inLen;
			while (pos >= input) {
				if (*pos == '\\' || *pos == '/') {
					if (--maxPathSegs <= 0) {
						return pos+1;
					}
				}
				pos--;
			}
		}
		return input;
	}
	void logStackTrace() {

		HANDLE process = GetCurrentProcess();
		HANDLE thread = GetCurrentThread();

		CONTEXT context;
		memset(&context, 0, sizeof(CONTEXT));
		context.ContextFlags = CONTEXT_FULL;
		RtlCaptureContext(&context);

		SymInitialize(process, NULL, TRUE);

		DWORD image;
		STACKFRAME64 frame;
		ZeroMemory(&frame, sizeof(STACKFRAME64));

	#ifdef _M_IX86
		image = IMAGE_FILE_MACHINE_I386;
		frame.AddrPC.Offset = context.Eip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Ebp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Esp;
		frame.AddrStack.Mode = AddrModeFlat;
	#elif _M_X64
		image = IMAGE_FILE_MACHINE_AMD64;
		frame.AddrPC.Offset = context.Rip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rsp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp;
		frame.AddrStack.Mode = AddrModeFlat;
	#elif _M_IA64
		image = IMAGE_FILE_MACHINE_IA64;
		frame.AddrPC.Offset = context.StIIP;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.IntSp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrBStore.Offset = context.RsBSP;
		frame.AddrBStore.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.IntSp;
		frame.AddrStack.Mode = AddrModeFlat;
	#endif

		int stackPos = 0;
		char bufPrefix[256];
		char* symbolBuffer = (char*) calloc(1, sizeof(IMAGEHLP_SYMBOL) + (MAX_SYM_NAME+1) * sizeof(TCHAR));
		while (StackWalk64(image, process, thread,
			&frame, &context,
			NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {

			const char* unknownFrame = "???";
			const char* moduleName = unknownFrame;
			const char* functioName = unknownFrame;

			DWORD64 moduleBase = SymGetModuleBase(process, frame.AddrPC.Offset);
			char moduleBuff[MAX_PATH];
			if (moduleBase && GetModuleFileNameA((HINSTANCE)moduleBase, moduleBuff, MAX_PATH))
			{
				moduleName = moduleBuff;
			}

			PSYMBOL_INFO symbol = (PSYMBOL_INFO )symbolBuffer;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;

			DWORD64 displacement = 0;
			if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
			{
				functioName = symbol->Name;
			}

			DWORD  offset = 0;
			IMAGEHLP_LINE imgline;
			imgline.SizeOfStruct = sizeof(IMAGEHLP_LINE);
			int prefOffset = snprintf(bufPrefix, 128, "%08X ", frame.AddrPC.Offset);
			if (prefOffset < 0) prefOffset = 0;
			if (SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset, &imgline)) {
				snprintf(bufPrefix+prefOffset, 128, "%s:%d:%d", shortName(imgline.FileName, 2), (int)imgline.LineNumber, offset);
			} else {
				snprintf(bufPrefix+prefOffset, 128, "%s:%d", shortName(moduleName, 0), (int)displacement);
			}
			log_out("%-32s %s\n", bufPrefix, functioName);


			stackPos++;

		}
		free(symbolBuffer);
		SymCleanup(process);
	}
}
