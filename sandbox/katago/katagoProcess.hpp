#pragma once

#include <string>

struct LaunchConfig {
	std::string executable;
	std::string model;
	std::string modelHuman;
	std::string config;
};

class KatagoProcess {
public:
	KatagoProcess()                                = default;
	KatagoProcess(const KatagoProcess&)            = delete;
	KatagoProcess& operator=(const KatagoProcess&) = delete;
	~KatagoProcess();

	bool start(const LaunchConfig& config);                              //!< Start katago with the given configuration.
	void stop();                                                         //!< Stop the subprocess.
	bool sendCommand(const std::string& command, std::string& response); //!< Send a command and wait for the response.

private:
	pid_t m_pid{-1};
	int m_inPipe[2]{-1, -1};  //!< Pipe: parent -> child
	int m_outPipe[2]{-1, -1}; //!< Pipe: child  -> parent
};
