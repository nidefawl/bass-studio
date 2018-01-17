#include "str_util.h"
#include <stdarg.h>
#include <vector>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#endif
#if __linux__
#include <stdio.h>
#define _snprintf_s(a,b,c,...) snprintf(a,b,__VA_ARGS__)
#endif

#if __linux__
int _vscprintf (const char * format, va_list pargs) {
	int retval;
	va_list argcopy;
	va_copy(argcopy, pargs);
	retval = vsnprintf(NULL, 0, format, argcopy);
	va_end(argcopy);
	return retval;
}
#endif

String StringFormat(const char *fmt, ...)
{
	char *strp = NULL;
	String str;
	int r;
	va_list ap;
	va_start(ap, fmt);
	r = vasprintf(&strp, fmt, ap);
	va_end(ap);
	if (r > 0) {
		str = strp;
	}
	if (strp) {
		insane_free(strp);
	}
	return str;
}
int asprintf(char **strp, const char *fmt, ...)
{
	int r;
	va_list ap;
	va_start(ap, fmt);
	r = vasprintf(strp, fmt, ap);
	va_end(ap);
	return(r);
}

int vasprintf(char **strp, const char *fmt, va_list ap)
{
	int r = -1, size = _vscprintf(fmt, ap);

	if ((size >= 0) && (size < INT_MAX))
	{
		*strp = (char *)malloc(size + 1); //+1 for null
		if (*strp)
		{
			r = vsnprintf(*strp, size + 1, fmt, ap);  //+1 for null
			if ((r < 0) || (r > size))
			{
				insane_free(*strp);
				r = -1;
			}
		}
	}
	else { *strp = 0; }

	return(r);
}

String FormatTempo(float tempo) {
	return StringFormat("%.2f", tempo);
}
String StringLimit(String s, int limit) {
	if (s.length() > (size_t)limit) {
		return s.substr(0, limit);
	}
	return s;
}
void replaceString(String& s, String f, String r) {
	size_t index;
	size_t offset = 0;
	while ((index = s.find(f, offset)) != String::npos) {
		s.replace(index, f.length(), r);
		offset = index + r.length();
	}
}

static const char* const noteNames[12] {
	"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* noteName(int note) { //NOT THREAD SAFE; DONT KEEP REFERENCE
	static const size_t buf_size = 32;
	static char* const buf = (char*) malloc(buf_size);
	_snprintf_s(buf, buf_size, _TRUNCATE, "%s%d", noteNames[note%12], (note/12)-2);
	return buf;
}
#ifdef _WIN32
String wcharToSring(const LPWSTR text) {
#ifdef USE_WSTRING
	String s = text;
	return s;
#else
	std::vector<char> buffer;
	int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	if (size > 0) {
		buffer.resize(size);
		WideCharToMultiByte(CP_UTF8, 0, text, -1, &buffer[0], buffer.size(),
				NULL, NULL);
		return String(buffer.begin(), buffer.end()-1);
	}
	return "";
#endif
}
#endif
