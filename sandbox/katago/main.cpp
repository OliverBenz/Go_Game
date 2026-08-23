#include <cassert>
#include <chrono>
#include <fcntl.h>
#include <format>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

constexpr char* KATAGO_EXE         = KATAGO_PATH "/katago";
constexpr char* KATAGO_MODEL       = KATAGO_PATH "/g170-b30c320x2-s4824661760-d1229536699.bin.gz";
constexpr char* KATAGO_MODEL_HUMAN = KATAGO_PATH "/b18c384nbt-humanv0.bin.gz";
constexpr char* KATAGO_CONFIG      = KATAGO_PATH "/gtp_human5k_example.cfg";

struct LaunchConfig {
	std::string executable{KATAGO_EXE};
	std::string model{KATAGO_MODEL};
	std::string config{KATAGO_CONFIG};
	std::string modelHuman{KATAGO_MODEL_HUMAN};
};

class KatagoProcess {
public:
	KatagoProcess() = default;

	KatagoProcess(const KatagoProcess&)            = delete;
	KatagoProcess& operator=(const KatagoProcess&) = delete;
	~KatagoProcess() {
		stop();
	}

	bool start(const LaunchConfig& config) {
		if (m_pid >= 0) {
			assert(false);
			return false; // Already running
		}

		// Pipes
		if (pipe(m_inPipe) == -1 || pipe(m_outPipe) == -1) {
			return false;
		}

		// Subprocess
		pid_t pid = fork();
		if (pid < 0) {
			return false;
		}

		if (pid == 0) {
			// Child process

			// Redirect stderr to log file.
			int logFile = open("katago.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (logFile == -1 || dup2(logFile, STDERR_FILENO) == -1) {
				// TODO: Handle stderr redirect. Child process will still function properly
			}
			close(logFile);

			// Redirect the pipe ends to stdin/stdout (copy)
			if (dup2(m_inPipe[0], STDIN_FILENO) == -1 || dup2(m_outPipe[1], STDOUT_FILENO) == -1) {
				perror("dup2 failed");
				_exit(EXIT_FAILURE);
			}

			// These are now redundant -> close them
			close(m_inPipe[0]);
			close(m_inPipe[1]);
			close(m_outPipe[0]);
			close(m_outPipe[1]);

			// Replace current process with katago
			const std::vector<char*> argv = {const_cast<char*>(config.executable.c_str()),
			                                 const_cast<char*>("gtp"),
			                                 const_cast<char*>("-model"),
			                                 const_cast<char*>(config.model.c_str()),
			                                 const_cast<char*>("-human-model"),
			                                 const_cast<char*>(config.modelHuman.c_str()),
			                                 const_cast<char*>("-config"),
			                                 const_cast<char*>(config.config.c_str()),
			                                 nullptr};
			execvp(config.executable.c_str(), argv.data());

			// If  execvp command failed
			perror("execvp failed");
			_exit(EXIT_FAILURE); // Exit the subprocess with failure.
		} else {
			// Parent process
			m_pid = pid;

			// Close and reset unused pipes
			close(m_inPipe[0]);
			close(m_outPipe[1]);
			m_inPipe[0]  = -1;
			m_outPipe[1] = -1;
		}

		// TODO: Catch if the execvp failed?

		return true;
	}

	void stop() {
		// Request stop from KataGo
		if (m_pid >= 0 && m_inPipe[1] >= 0) {
			const std::string quit = "quit\n";
			write(m_inPipe[1], quit.c_str(), quit.size());
		}

		// Cleanup pipes
		if (m_inPipe[0] >= 0) {
			close(m_inPipe[0]);
			m_inPipe[0] = -1;
		}
		if (m_inPipe[1] >= 0) {
			close(m_inPipe[1]);
			m_inPipe[1] = -1;
		}
		if (m_outPipe[0] >= 0) {
			close(m_outPipe[0]);
			m_outPipe[0] = -1;
		}
		if (m_outPipe[1] >= 0) {
			close(m_outPipe[1]);
			m_outPipe[1] = -1;
		}

		// Cleanup the child process.
		if (m_pid < 0) {
			return;
		}

		// Wait for process to shut down cleanly. Force kill after a timeout.
		constexpr auto shutdownTimeout = std::chrono::milliseconds(2000);
		constexpr auto pollInterval    = std::chrono::milliseconds(20);
		const auto deadline            = std::chrono::steady_clock::now() + shutdownTimeout;
		int status{0};
		pid_t result;
		while ((result = waitpid(m_pid, &status, WNOHANG)) == 0) {
			if (std::chrono::steady_clock::now() >= deadline) {
				kill(m_pid, SIGKILL);
				waitpid(m_pid, &status, 0); // SIGKILL can't be caught/blocked, returns promptly
				break;
			}
			std::this_thread::sleep_for(pollInterval);
		}
		m_pid = -1;
	}

	//! Send a command and wait for the response.
	bool sendCommand(const std::string& command, std::string& response) {
		response = "";

		const std::string line = command + '\n';
		if (write(m_inPipe[1], line.c_str(), line.size()) == -1) {
			return false;
		}

		// Receive result
		char buffer[16];
		while (response.find("\n\n") == std::string::npos) {
			ssize_t n = read(m_outPipe[0], buffer, std::size(buffer));
			if (n <= 0) {
				break; // pipe closed or error
			}
			response.append(buffer, n);
		}

		//! Trim the KataGo response and evaluate if the command succeeded.
		const auto trimEval = [](std::string& response) -> bool {
			bool success = response[0] == '='; //!< Success if the response starts with '='.
			if (response.size() < 2) {
				return success;
			}

			response = response.substr(2); // Drop leading "= " or "? "

			// The response ends with \n\n
			const auto end = response.find_last_not_of(" \t\r\n");
			response       = (end == std::string::npos) ? std::string{} : response.substr(0, end + 1);
			return success;
		};

		return trimEval(response);
	}

private:
	pid_t m_pid{-1};
	int m_inPipe[2]{-1, -1};  //!< Pipe: parent -> child
	int m_outPipe[2]{-1, -1}; //!< Pipe: child  -> parent
};

int main(int argc, char** argv) {
	std::cout << std::format("Using configuration:\n\tExecutable:    {}\n\tModel:         {}\n\tHuman Model:   {}\n\tConfiguration: {}\n", KATAGO_EXE,
	                         KATAGO_MODEL, KATAGO_MODEL_HUMAN, KATAGO_CONFIG);

	KatagoProcess process;
	process.start(LaunchConfig{});

	std::string response;
	if (process.sendCommand("version", response)) {
		std::cout << response << '\n';
	}
	if (process.sendCommand("genmove b", response)) {
		std::cout << response << '\n';
	}
	if (process.sendCommand("quit", response)) {
		std::cout << response << '\n';
	}

	return 0;
}
