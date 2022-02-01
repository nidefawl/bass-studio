#include "childprocessthread.h"

#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <atomic>
#include "str_util.h"
#include "thread.h"
#include "exceptions.h"
#include "assert_dbg.h"

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <spawn.h>
#include <csignal>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <cstdio>

extern "C" {
extern char** environ;
}

class ProcessRunScope {
    pid_t pid = 0;

public:
    int exitCode        = 0;
    int procSpawnStatus = 0;
    ProcessRunScope(const String& binary, const String& params, const String& workingDir, const ProcessThread::Env& env, bool pipeOutput) {

        if (env.size()) {
            dbgassert(0 && "Custom environment not yet implemented on this platform");
        }
        //char *argv[] = { "ls", (char *) 0 };
        std::vector<String> strings;
        std::istringstream f(params);
        String s;
        while (getline(f, s, ' ')) {
            strings.push_back(s);
        }
        const char** argv = (const char**) alloca(sizeof(char*) * (strings.size() + 1));
        for (unsigned i = 0; i < strings.size(); i++) {
            argv[i] = StringAsCStr(strings[i]);
        }
        argv[strings.size()] = 0;
        const char* bin      = StringAsCStr(binary);
        procSpawnStatus      = posix_spawn(&pid, bin, NULL, NULL, (char* const*) argv, environ);
        if (procSpawnStatus != 0) {
            String errmsg = StringFormat("posix_spawn(%s, %s) failed",
                                         StringAsCStr(binary), StringAsCStr(params));
            throw SystemException(procSpawnStatus, errmsg);
        }
    }
    void waitForever() {
        dbgassert(0 == procSpawnStatus);
        int procStatus = 0;
        exitCode       = -1;
        int waitRet    = waitpid(pid, &procStatus, 0);
        if (waitRet == -1 || !(WIFEXITED(procStatus))) {
            throw SystemException(errno, "Process did not exit normally");
        }
        exitCode = WEXITSTATUS(procStatus);
    }
    void killProcess() {
        kill(pid, SIGKILL);
    }
    ~ProcessRunScope() {
    }
};
#elif defined _WIN32
#include <windows.h>
class ProcessRunScope {
public:
    DWORD exitCode{};
    PROCESS_INFORMATION processInformation{};
    STARTUPINFO startupInfo{};
    HANDLE handleStdOutRead{};
    HANDLE handleStdOutWrite{};
    ProcessRunScope(const String& binary, const String& params, const String& workingDir, const ProcessThread::Env& env, bool pipeOutput) {
#ifdef UNICODE
#error "not implemented"
#endif
        startupInfo.cb = sizeof(startupInfo);
        std::vector<char> bufEnv;
        if (!env.empty()) {

            size_t totalEnvLen = 0;
            for (const auto& entry : env) {
                totalEnvLen += entry.name.length();
                totalEnvLen += 1;// =
                totalEnvLen += entry.value.length();
                totalEnvLen += 1;// \0
            }
            totalEnvLen += 1;// \0
            bufEnv.resize(totalEnvLen);
            char* dstOffset = bufEnv.data();
            char* dstEnd    = bufEnv.data() + totalEnvLen;
            for (const auto& entry : env) {
                //my_printf("ENV[\"%s\"]\t=\t%s\n", StringAsCStr(entry.name), StringAsCStr(entry.value));
                if (strcpy_s(dstOffset, (dstEnd - dstOffset), StringAsCStr(entry.name)))
                    throw appexception("Failed processing env key");
                dstOffset += entry.name.length();
                dbgassert((dstEnd - dstOffset) > 0);
                *dstOffset++ = '=';
                if (strcpy_s(dstOffset, (dstEnd - dstOffset), StringAsCStr(entry.value)))
                    throw appexception("Failed processing env val");
                dstOffset += entry.value.length();
                dbgassert((dstEnd - dstOffset) > 0);
                *dstOffset++ = '\0';
            }
            dbgassert((dstEnd - dstOffset) == 1);
            *dstOffset++ = '\0';
            dbgassert((dstOffset - bufEnv.data()) == totalEnvLen);
            bufEnv.resize(totalEnvLen);
        } else {

            dbgassert(bufEnv.empty());
        }
        std::vector<char> bufWorkingDir;
        if (workingDir.length()) {
            bufWorkingDir.resize(workingDir.length() + 1);
            if (strcpy_s(bufWorkingDir.data(), bufWorkingDir.size(), StringAsCStr(workingDir)))
                throw appexception("Failed processing working dir");
        } else {
            dbgassert(bufWorkingDir.empty());
        }
        bool inheritHandles = pipeOutput;
        // Create a pipe for the child process's STDOUT.
        if (pipeOutput) {

            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
            saAttr.bInheritHandle       = TRUE;
            saAttr.lpSecurityDescriptor = nullptr;

            if (!CreatePipe(&handleStdOutRead, &handleStdOutWrite, &saAttr, 0))
                throw SystemException(GetLastError(), "CreateProcess CreatePipe failed");

            // Ensure the read handle to the pipe for STDOUT is not inherited.

            if (!SetHandleInformation(handleStdOutRead, HANDLE_FLAG_INHERIT, 0))
                throw SystemException(GetLastError(), "CreateProcess SetHandleInformation failed");

            startupInfo.hStdError  = this->handleStdOutWrite;
            startupInfo.hStdOutput = this->handleStdOutWrite;
            startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        }
        if (!CreateProcess((LPSTR) StringAsCStr(binary),
                           (LPSTR) StringAsCStr(binary + " " + params),
                           nullptr,
                           nullptr,
                           inheritHandles,
                           NORMAL_PRIORITY_CLASS /*|CREATE_NEW_CONSOLE | CREATE_NO_WINDOW*/,
                           !bufEnv.empty() ? bufEnv.data() : nullptr,
                           !bufWorkingDir.empty() ? bufWorkingDir.data() : nullptr,
                           &startupInfo,
                           &processInformation)) {
            String errmsg = StringFormat("CreateProcess(%s, %s) failed", StringAsCStr(binary), StringAsCStr(params));
            throw SystemException(GetLastError(), errmsg);
        }
        if (pipeOutput) {
            CloseHandle(handleStdOutWrite);
            CloseHandle(processInformation.hThread);
        }
    }
    void waitForever() {
        WaitForSingleObject(processInformation.hProcess, INFINITE);
        if (!GetExitCodeProcess(processInformation.hProcess, &exitCode)) {
            throw SystemException(GetLastError(), "Process did not exit normally");
        }
    }
    bool waitTimeOut(int32_t timeoutMsec) {
        dbgassert(timeoutMsec > 0);
        auto ret = WaitForSingleObject(processInformation.hProcess, timeoutMsec);
        if (ret == WAIT_TIMEOUT) {
            return false;
        }
        if (ret != WAIT_OBJECT_0) {
            TerminateProcess(processInformation.hProcess, 1);
            throw SystemException(GetLastError(), "WaitForSingleObject returned unexpected state");
        }
        if (!GetExitCodeProcess(processInformation.hProcess, &exitCode)) {
            throw SystemException(GetLastError(), "Process did not exit normally");
        }
        return true;
    }
    void killProcess() {
        TerminateProcess(processInformation.hProcess, 1);
    }
    ~ProcessRunScope() {
        CloseHandle(processInformation.hProcess);
        CloseHandle(handleStdOutRead);
    }
};
#endif
class ProcessThread::Impl {
    std::thread m_t;
    std::recursive_mutex m_mutex;
    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_readFailed{false};
    std::vector<String> m_buffer;
    bool m_started = false;
#ifdef _WIN32
    HANDLE m_processHandle = nullptr;
#endif
public:
    volatile int32_t m_processExitCode = 0;
    std::exception_ptr m_eptr = nullptr;
    String m_lastCmd;

    Impl() {
        m_isRunning = false;
    }
    ~Impl() = default;
    void flushOutputBuffer(std::vector<char>& buf) {
        while (!buf.empty()) {
            auto it = std::find_if(buf.begin(), buf.end(), [](const char c) {
                return c == '\n' || c == '\r';
            });

            if (it == buf.begin()) {
                buf.erase(buf.begin(), buf.begin() + 1);
                continue;
            }
            if (it == buf.end()) {
                break;
            }
            m_mutex.lock();
            m_buffer.emplace_back(buf.begin(), it);
            m_mutex.unlock();
            if (!buf.empty())
                it++;
            buf.erase(buf.begin(), it);
        }
    }
    void startProcess(const String& binary, const String& params, const String& workingDir, const Env& env, bool pipedOutput) {
        this->m_lastCmd = StringFormat("%s> %s %s", StringAsCStr(workingDir), StringAsCStr(binary), StringAsCStr(params));
        m_started = true;
        m_isRunning = true;
        m_t = std::thread([this, argbinary = binary, argparams = params, argwd = workingDir, argenv = env, argpipe = pipedOutput]() {
            seqthreads::registerThread("childprocessthread");
            try {
                std::array<char, 2048> TEMP{};
                std::vector<char> buf;
                ProcessRunScope scopedProcess(argbinary, argparams, argwd, argenv, argpipe);
#ifdef _WIN32
                m_processHandle = scopedProcess.processInformation.hProcess;
#endif
                if (argpipe) {
                    while (!m_readFailed) {
#ifndef _WIN32
                        dbgassert(0 && "Not implemented on this platform");
#else
                        dbgassert(scopedProcess.handleStdOutRead);
                        DWORD dwRead = 0;
                        if (!ReadFile(scopedProcess.handleStdOutRead, TEMP.data(), TEMP.size(), &dwRead, nullptr)) {
                            m_readFailed = true;
                            if (scopedProcess.waitTimeOut(50)) {
                                break;
                            }
                        }
                        if (dwRead > 0) {
                            buf.insert(buf.end(), TEMP.begin(), TEMP.begin() + dwRead);
                            flushOutputBuffer(buf);
                        }
#endif
                        seqthreads::threadSleep(25);
                    }
                    if (!buf.empty()) {
                        flushOutputBuffer(buf);
                    }
                }
                scopedProcess.waitForever();
                m_processExitCode = (int32_t) scopedProcess.exitCode;
            } catch (...) {
                m_eptr = std::current_exception();
            }
            m_isRunning = false;
        });
    }
    void killProcess() {
#ifdef _WIN32
        if (m_processHandle) {
            // handle might be invalid
            TerminateProcess(m_processHandle, 1);
        }
#endif
    }
    bool isRunning() {
        return m_isRunning;
    }
    void joinProcess() {
        if (m_started && m_t.joinable())
            m_t.join();
    }
    size_t readLines(std::vector<String>& out) {
        size_t size;
        m_mutex.lock();
        size = m_buffer.size();
        out  = m_buffer;
        m_buffer.clear();
        m_mutex.unlock();
        return size;
    }
};
ProcessThread::ProcessThread() : m_threadImpl{ new ProcessThread::Impl{} } {
}
ProcessThread::~ProcessThread() {
    m_threadImpl->joinProcess();
    delete m_threadImpl;
}
void ProcessThread::startProcess(const String& binary, const String& params, const String& workingDir) {
    m_threadImpl->startProcess(binary, params, workingDir, this->m_env, this->m_pipedOutput);
}
void ProcessThread::joinProcess() {
    m_threadImpl->joinProcess();
}
bool ProcessThread::isRunning() {
    return m_threadImpl->isRunning();
}
void ProcessThread::killProcess() {
    if (m_threadImpl)
        m_threadImpl->killProcess();
}
bool ProcessThread::checkException() {
    if (m_threadImpl->m_eptr != nullptr) {
        try {
            std::rethrow_exception(m_threadImpl->m_eptr);
        } catch (const std::exception& ex) {
            printf("process[%s] had exception: %s\n", StringAsCStr(m_threadImpl->m_lastCmd), ex.what());
        }
        return true;
    }
    return false;
}
size_t ProcessThread::readLines(std::vector<String>& out) {
    return m_threadImpl->readLines(out);
}
int ProcessThread::getExitCode() {
    return m_threadImpl->m_processExitCode;
}

void ProcessThread::addEnvPath(const String& path) {
    auto it = std::find_if(m_env.begin(), m_env.end(), [](const auto& entry) {
        return entry.name == "Path";
    });
    if (it != m_env.end()) {
        it->value += ";" + path;
    } else {
        m_env.push_back({ "Path", path });
    }
}
void ProcessThread::addEnv(const String& envKey, const String& envVal) {
    auto it = std::find_if(m_env.begin(), m_env.end(), [&envKey](const auto& entry) {
        return entry.name == envKey;
    });
    if (it != m_env.end()) {
        it->value = envKey;
    } else {
        m_env.push_back({ envKey, envVal });
    }
}
