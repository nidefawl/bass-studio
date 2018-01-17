#pragma once
#include "config.h"
#if USE_LOGGING
#include <typeinfo>
#include "str_util.h"
String demangleName(String toDemangle);
template <class T>
String typeName(const T& t) {
    return demangleName(typeid(t).name());
}
void _my_printf(const char *file, int line, const char *func, const char *fmt, ...);
#define OBJ(x) typeid(x).name()
#define my_printf(fmt, ...) _my_printf(__FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#else
#define my_printf(fmt, ...) 
#define OBJ(x)
#endif
