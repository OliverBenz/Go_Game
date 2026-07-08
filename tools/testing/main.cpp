// simple_gtp_client.cpp
//
// Minimal, single-threaded example of talking to a GTP engine (e.g. KataGo)
// as a child process. YAGNI version: no mutex, no background stderr thread,
// no session state machine — just fork/exec, write one command, read one
// response block, print it, repeat.
//
// Build:  g++ -std=c++17 -O2 simple_gtp_client.cpp -o gtp_client
// Run:    ./gtp_client /path/to/katago gtp -config gtp.cfg -model model.bin.gz

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Wraps a single child process connected via stdin/stdout pipes.
// stderr is left connected to the parent's stderr -- good enough for
// seeing engine errors on the console, no extra thread required.
class ChildProcess {
public:
	bool start(const std::vector<std::string>& args) {
		int inPipe[2];
		int outPipe[2];
		if (::pipe(inPipe) != 0 || ::pipe(outPipe) != 0) {
			std::perror("pipe");
			return false;
		}

		const pid_t pid = ::fork();
		if (pid < 0) {
			std::perror("fork");
			return false;
		}

		if (pid == 0) {
			// Child: wire up stdin/stdout, inherit parent's stderr as-is.
			::dup2(inPipe[0], STDIN_FILENO);
			::dup2(outPipe[1], STDOUT_FILENO);
			::close(inPipe[0]);
			::close(inPipe[1]);
			::close(outPipe[0]);
			::close(outPipe[1]);

			std::vector<char*> argv;
			argv.reserve(args.size() + 1);
			for (const auto& arg: args) {
				argv.push_back(const_cast<char*>(arg.c_str()));
			}
			argv.push_back(nullptr);

			::execvp(argv[0], argv.data());
			std::perror("execvp"); // only reached if exec fails
			::_exit(127);
		}

		// Parent: keep our ends of the pipes, close the child's ends.
		::close(inPipe[0]);
		::close(outPipe[1]);

		m_pid = pid;
		m_in  = ::fdopen(inPipe[1], "w");
		m_out = ::fdopen(outPipe[0], "r");
		return m_in != nullptr && m_out != nullptr;
	}

	// Sends one line of text (a GTP command) to the child.
	bool send(const std::string& line) {
		if (std::fputs(line.c_str(), m_in) == EOF)
			return false;
		if (std::fputc('\n', m_in) == EOF)
			return false;
		return std::fflush(m_in) == 0;
	}

	// GTP responses are terminated by a blank line. Read lines until we
	// see one, skipping any leading blank lines first.
	std::string readResponse() {
		std::string block;
		char* buf     = nullptr;
		std::size_t n = 0;

		while (true) {
			const auto read = ::getline(&buf, &n, m_out);
			if (read < 0)
				break; // EOF: engine closed the pipe

			std::string line(buf, static_cast<std::size_t>(read));
			if (line == "\n" || line == "\r\n") {
				if (!block.empty())
					break; // blank line ends the response
				continue;  // leading blank line: skip it
			}
			block += line;
		}

		std::free(buf);
		return block;
	}

	void stop() {
		if (m_in) {
			send("quit");
			std::fclose(m_in);
			m_in = nullptr;
		}
		if (m_pid > 0) {
			int status = 0;
			::waitpid(m_pid, &status, 0);
			m_pid = -1;
		}
		if (m_out) {
			std::fclose(m_out);
			m_out = nullptr;
		}
	}

	~ChildProcess() {
		stop();
	}

private:
	pid_t m_pid      = -1;
	std::FILE* m_in  = nullptr;
	std::FILE* m_out = nullptr;
};

} // namespace

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <engine-path> [engine-args...]\n";
		return 1;
	}

	const std::vector<std::string> args(argv + 1, argv + argc);

	ChildProcess engine;
	if (!engine.start(args)) {
		std::cerr << "Failed to start engine process.\n";
		return 1;
	}

	std::cout << "Enter a coordinate (e.g. a2), or 'quit' to exit.\n";

	std::string input;
	while (std::cout << "> " && std::getline(std::cin, input)) {
		if (input.empty())
			continue;
		if (input == "quit")
			break;

		std::ostringstream command;
		command << "play b " << input;

		if (!engine.send(command.str())) {
			std::cerr << "Failed to write to engine stdin.\n";
			break;
		}

		const std::string response = engine.readResponse();
		if (response.empty()) {
			std::cerr << "Engine closed the connection.\n";
			break;
		}

		std::cout << response;
	}

	engine.stop();
	return 0;
}
