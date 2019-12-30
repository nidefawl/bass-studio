#if defined (__APPLE__)
#include "fileio.h"
#include "str_util.h"
#include <vector>
#include "window.h"


int promptUserFilePath(window_base* w, int mode,
		std::vector<SupportedFileType> fileTypes, String& _out) {
	return 0;
}

String cwdPath = "";
String toCWDPath(String relPath) {
	return cwdPath + relPath;
}
void setCWDPath(String cwd) {
	if (cwd.length() && (!StrEndsWith(cwd, "/") && !StrEndsWith(cwd, "\\")))
		cwd += "/";
	cwdPath = cwd;
}

#endif
