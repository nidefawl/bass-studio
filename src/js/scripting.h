#pragma once
#include "str_util.h"

struct call_context_t {
	int returnVal = -1;
	String strOut;
};
class JSContext {
	class Impl;
	Impl* const impl;
public:
	JSContext();
	~JSContext();
	Impl* getImpl();
	String eval(const String& js, call_context_t& calltxt);
};
