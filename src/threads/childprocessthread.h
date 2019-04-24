#pragma once
#include <vector>
#include "str_util.h"
class ProcessThread
{
	struct ProcessEnvVar {
		String name;
		String value;
	};
public:
	using Env = std::vector<ProcessEnvVar>;
private:
	class Impl;
	Env env;
	bool hasPipedOutput = false;
public:
	ProcessThread();
	~ProcessThread();

	//pre startProcess
    void startProcess(String binary, String params, String workingDir);
	void setPipedOutput(bool hasPipedOutput) {
		this->hasPipedOutput = hasPipedOutput;
	}
	void addEnvPath(const String& path);
	void addEnv(const String& envKey, const String& envVal);

	//post startProcess
	void joinProcess();
	bool isRunning();
	bool checkException();
	int32_t getExitCode();
	int32_t readLines(std::vector<String>& out);

	ProcessThread(const ProcessThread&) = delete;
	ProcessThread& operator=(const ProcessThread&) = delete;
private:
	Impl* _M_impl = NULL;
};

