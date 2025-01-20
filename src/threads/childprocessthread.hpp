#pragma once
#include <vector>
#include "str_util.hpp"

class ProcessThread {
    struct ProcessEnvVar {
        String name;
        String value;
    };

public:
    using Env = std::vector<ProcessEnvVar>;

private:
    class Impl;
    Env m_env;
    bool m_pipedOutput = false;

public:
    ProcessThread();
    ~ProcessThread();

    //pre startProcess
    void startProcess(const String& binary, const String& params, const String &workingDir);
    void setPipedOutput(bool hasPipedOutput) {
        this->m_pipedOutput = hasPipedOutput;
    }
    void addEnvPath(const String& path);
    void addEnv(const String& envKey, const String& envVal);

    //post startProcess
    void joinProcess();
    bool isRunning();
    void killProcess();
    bool checkException();
    int32_t getExitCode();
    size_t readLines(std::vector<String>& out);

    ProcessThread(const ProcessThread&) = delete;
    ProcessThread& operator=(const ProcessThread&) = delete;

private:
    Impl* m_threadImpl;
};
