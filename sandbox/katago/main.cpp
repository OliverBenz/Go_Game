#include <fcntl.h>
#include <format>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

constexpr char* KATAGO_EXE         = KATAGO_PATH "/katago";
constexpr char* KATAGO_MODEL       = KATAGO_PATH "/g170-b30c320x2-s4824661760-d1229536699.bin.gz";
constexpr char* KATAGO_MODEL_HUMAN = KATAGO_PATH "/b18c384nbt-humanv0.bin.gz";
constexpr char* KATAGO_CONFIG      = KATAGO_PATH "/gtp_human5k_example.cfg";

int main(int argc, char** argv) {
	std::cout << std::format("Using configuration:\n\tExecutable:    {}\n\tModel:         {}\n\tHuman Model:   {}\n\tConfiguration: {}\n", KATAGO_EXE,
	                         KATAGO_MODEL, KATAGO_MODEL_HUMAN, KATAGO_CONFIG);

	// Pipes
	int inPipe[2];  // parent -> child
	int outPipe[2]; // child  -> parent
	if (pipe(inPipe) == -1 || pipe(outPipe) == -1) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	// Subprocess
	pid_t pid = fork();
	if (pid < 0) {
		return EXIT_FAILURE;
	}

	if (pid == 0) {
		// Child process

		// Redirect stderr to log file.
		int logFile = open("katago.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		dup2(logFile, STDERR_FILENO);
		close(logFile);

		// Redirect the pipe ends to stdin/stdout (copy)
		dup2(inPipe[0], STDIN_FILENO);   // Redirect stdin  to our childs inPipe[0]
		dup2(outPipe[1], STDOUT_FILENO); // Redirect stdout to our childs outPipe[1]

		// These are now redundant -> close them
		close(inPipe[0]);
		close(inPipe[1]);
		close(outPipe[0]);
		close(outPipe[1]);

		std::vector<char*> argv = {KATAGO_EXE, "gtp", "-model", KATAGO_MODEL, "-human-model", KATAGO_MODEL_HUMAN, "-config", KATAGO_CONFIG, nullptr};

		// Replace current process with katago
		execvp(KATAGO_EXE, argv.data());

		// If  execvp command failed
		perror("execvp failed");
		_exit(EXIT_FAILURE);
	} else {
		// Parent process

		close(inPipe[0]);
		close(outPipe[1]);

		const auto sendCommand = [&](const std::string& command) {
			// Send command
			const std::string line = command + '\n';
			write(inPipe[1], line.c_str(), line.size());

			// Receive result
			std::string response;
			char buffer[4096];
			while (response.find("\n\n") == std::string::npos) {
				ssize_t n = read(outPipe[0], buffer, sizeof(buffer));
				if (n <= 0)
					break; // pipe closed or error
				response.append(buffer, n);
			}
			return response;
		};

		// Write test command
		auto response = sendCommand("version");
		std::cout << response << "\n";

		response = sendCommand("genmove b");
		std::cout << response << "\n";
	}

	return 0;
}
