#pragma once
#include "str_util.h"
class ProcessThread
{
public:

private:
	class Impl;
public:
	ProcessThread();
	~ProcessThread();
    void startProcess(String binary, String params);
	void joinProcess();
	bool isRunning();
	bool checkExcepetion();
	int32_t getExitCode();
	ProcessThread(const ProcessThread&) = delete;
	ProcessThread& operator=(const ProcessThread&) = delete;
private:
	Impl* _M_impl = NULL;
};

