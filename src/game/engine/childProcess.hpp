#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tengen {

//! General purpose wrapper class for child process handling. Ensures proper RAII and windows/posix compatibility.
class ChildProcess {
public:
	enum class ReadResult {
		Success,
		EndOfFile,
		Error,
		NotStarted
	};
	enum class WaitResult {
		Exited,
		Error,
		NotStarted
	};

	struct Options {
		std::filesystem::path executable;   //!< Path to the executable.
		std::vector<std::string> arguments; //!< Commandline arguments to pass.

		// Capture for the pipe
		bool captureStdout = true; //!< For protocol replies.
		bool captureStderr = true; //!< For logs and errors.
	};

	explicit ChildProcess(Options options) noexcept;
	~ChildProcess() noexcept;

	ChildProcess(const ChildProcess&)            = delete;
	ChildProcess& operator=(const ChildProcess&) = delete;

	ChildProcess(ChildProcess&& other) noexcept;
	ChildProcess& operator=(ChildProcess&& other) noexcept;

public:
	bool start() noexcept;                 //!< Start the child process.
	bool write(std::string_view data);     //!< Write raw bytes to the stdin of the child. Returns true on success.
	bool writeLine(std::string_view line); //!< Convenience helper for text protocols like GTP. Write with '\n' appended. Returns true on success.

	ReadResult readStdoutLine(std::string& result); //!< Reads one line from the stdout of the child into 'result'.
	ReadResult readStderrLine(std::string& result); //!< Reads one line from the stderr of the child into 'result'.

	void closeStdin() noexcept;      //!< Close child's stdin (Useful when you want to signal EOF).
	bool isRunning() const noexcept; //!< Returns true if the process still appears to be running.

	void terminate(std::chrono::milliseconds gracePeriod = std::chrono::milliseconds{500}) noexcept; //!< Ask the process to stop and force stop after a grace period.
	WaitResult wait() noexcept;                                                                      //!< Wait until the process exits. Returns the process exit code when available.

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl;
};

} // namespace tengen
