#pragma once

namespace NU {
namespace CONSOLE {
struct rep_running_state {
	std::atomic<bool> isRunning{true};
	std::atomic<bool> isConnected{true};
	std::atomic<bool> shouldQuit{false};
};
/**
 * Command Line Read Evaluate Print
 */
class CommandLineREP {
protected:
	rep_running_state runState;
public:
	virtual ~CommandLineREP() { };
	virtual int runConsole() = 0;
	virtual int executeCommands() = 0;
	virtual void init() = 0;
	rep_running_state& getRunningState() {
		return runState;
	}
};
class CommandLineREP_Console : public CommandLineREP {
	class CLIImpl;
	CLIImpl* const m_impl;
public:
	CommandLineREP_Console();
	~CommandLineREP_Console();
	int runConsole() override;
	int executeCommands() override;
	void init() override;
};
class CommandLineREP_TCP : public CommandLineREP {
	class CLIImpl;
	CLIImpl* const m_impl;
public:
	CommandLineREP_TCP();
	~CommandLineREP_TCP();
	int runConsole() override;
	int executeCommands() override;
	void init() override;
};
}
}
