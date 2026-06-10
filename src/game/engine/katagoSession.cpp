#include "engine/katagoSession.hpp"

#include "engine/gtp.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tengen::engine {
namespace {

bool hasValue(const std::string& text) {
	return !text.empty();
}

std::string trimTrailingWhitespace(std::string text) {
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) {
		text.pop_back();
	}
	return text;
}

std::vector<std::string> buildDifficultyCommands(const GameConfig& config) {
	std::vector<std::string> commands{};

	auto addParam = [&commands](const std::string& name, const std::string& value) {
		commands.push_back(gtp::buildCommand("kata-set-param", {name, value}));
	};

	switch (config.difficulty) {
	case Difficulty::Easy:
		addParam("maxVisits", "64");
		break;
	case Difficulty::Medium:
		addParam("maxVisits", "256");
		break;
	case Difficulty::Hard:
		addParam("maxVisits", "1024");
		break;
	case Difficulty::Custom:
		break;
	default:
		assert(false);
		break;
	}

	if (config.limits.maxVisits) {
		addParam("maxVisits", std::to_string(*config.limits.maxVisits));
	}
	if (config.limits.maxTimeSeconds) {
		std::ostringstream out;
		out << *config.limits.maxTimeSeconds;
		addParam("maxTime", out.str());
	}

	return commands;
}

bool moveNeedsCoordinate(const Move& move) {
	return move.kind == MoveKind::Place;
}

#if defined(__unix__) || defined(__APPLE__)

class KataGoProcess {
public:
	KataGoProcess() = default;
	~KataGoProcess() {
		stop();
	}

	KataGoProcess(const KataGoProcess&)            = delete;
	KataGoProcess& operator=(const KataGoProcess&) = delete;
	KataGoProcess(KataGoProcess&&)                 = delete;
	KataGoProcess& operator=(KataGoProcess&&)      = delete;

	bool start(const KataGoLaunchConfig& launchConfig, std::string& error) {
		stop();

		int stdinPipe[2]{-1, -1};
		int stdoutPipe[2]{-1, -1};
		int stderrPipe[2]{-1, -1};

		if (::pipe(stdinPipe) != 0 || ::pipe(stdoutPipe) != 0 || ::pipe(stderrPipe) != 0) {
			error = "Could not create pipes for KataGo child process.";
			closePipe(stdinPipe);
			closePipe(stdoutPipe);
			closePipe(stderrPipe);
			return false;
		}

		std::vector<std::string> args{
		        launchConfig.executablePath,
		        "gtp",
		        "-config",
		        launchConfig.configPath,
		        "-model",
		        launchConfig.modelPath,
		};
		if (launchConfig.humanModelPath) {
			args.push_back("-human-model");
			args.push_back(*launchConfig.humanModelPath);
		}
		args.insert(args.end(), launchConfig.extraArgs.begin(), launchConfig.extraArgs.end());

		std::vector<char*> argv{};
		argv.reserve(args.size() + 1u);
		for (auto& arg: args) {
			argv.push_back(arg.data());
		}
		argv.push_back(nullptr);

		const pid_t pid = ::fork();
		if (pid < 0) {
			error = "Could not fork KataGo child process.";
			closePipe(stdinPipe);
			closePipe(stdoutPipe);
			closePipe(stderrPipe);
			return false;
		}

		if (pid == 0) {
			::dup2(stdinPipe[0], STDIN_FILENO);
			::dup2(stdoutPipe[1], STDOUT_FILENO);
			::dup2(stderrPipe[1], STDERR_FILENO);

			closePipe(stdinPipe);
			closePipe(stdoutPipe);
			closePipe(stderrPipe);

			::execv(argv[0], argv.data());
			::_exit(127);
		}

		::close(stdinPipe[0]);
		::close(stdoutPipe[1]);
		::close(stderrPipe[1]);

		m_pid    = pid;
		m_stdin  = ::fdopen(stdinPipe[1], "w");
		m_stdout = ::fdopen(stdoutPipe[0], "r");
		m_stderr = ::fdopen(stderrPipe[0], "r");

		if (!m_stdin || !m_stdout || !m_stderr) {
			error = "Could not wrap KataGo child process pipes in file handles.";
			stop();
			return false;
		}

		m_stderrThread = std::thread([this] { stderrLoop(); });
		return true;
	}

	bool send(std::string_view command, std::string& error) {
		if (!m_stdin) {
			error = "KataGo stdin is not available.";
			return false;
		}

		if (std::fwrite(command.data(), sizeof(char), command.size(), m_stdin) != command.size()) {
			error = "Could not write command to KataGo stdin.";
			return false;
		}
		if (std::fflush(m_stdin) != 0) {
			error = "Could not flush KataGo stdin.";
			return false;
		}
		return true;
	}

	std::optional<std::string> readResponseBlock(std::string& error) {
		if (!m_stdout) {
			error = "KataGo stdout is not available.";
			return std::nullopt;
		}

		std::string block{};
		bool sawHeader = false;

		while (true) {
			const auto line = readLine(m_stdout);
			if (!line) {
				error = "KataGo stdout closed before a full GTP response was received.";
				return std::nullopt;
			}

			if (!sawHeader) {
				if (*line == "\n" || *line == "\r\n") {
					continue;
				}
				sawHeader = true;
			}

			block += *line;
			if (*line == "\n" || *line == "\r\n") {
				if (block.size() >= line->size() && (line->empty() || *line == "\n" || *line == "\r\n")) {
					break;
				}
			}
		}

		return block;
	}

	std::string stderrTail() const {
		std::lock_guard<std::mutex> lock(m_stderrMutex);
		return m_stderrTail;
	}

	void stop() {
		if (m_stdin) {
			std::fputs("quit\n", m_stdin);
			std::fflush(m_stdin);
			std::fclose(m_stdin);
			m_stdin = nullptr;
		}

		waitForExit();

		if (m_stdout) {
			std::fclose(m_stdout);
			m_stdout = nullptr;
		}
		if (m_stderrThread.joinable()) {
			m_stderrThread.join();
		}
		if (m_stderr) {
			std::fclose(m_stderr);
			m_stderr = nullptr;
		}

		m_pid = -1;
	}

private:
	static void closePipe(int (&pipeFds)[2]) {
		if (pipeFds[0] >= 0) {
			::close(pipeFds[0]);
			pipeFds[0] = -1;
		}
		if (pipeFds[1] >= 0) {
			::close(pipeFds[1]);
			pipeFds[1] = -1;
		}
	}

	static std::optional<std::string> readLine(std::FILE* stream) {
		char* buffer = nullptr;
		size_t size  = 0u;
		const auto read = ::getline(&buffer, &size, stream);
		if (read < 0) {
			std::free(buffer);
			return std::nullopt;
		}

		std::string line{buffer, static_cast<std::size_t>(read)};
		std::free(buffer);
		return line;
	}

	void waitForExit() {
		if (m_pid <= 0) {
			return;
		}

		int status = 0;
		for (unsigned i = 0; i != 50u; ++i) {
			const auto result = ::waitpid(m_pid, &status, WNOHANG);
			if (result == m_pid) {
				return;
			}
			if (result < 0) {
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		::kill(m_pid, SIGTERM);
		for (unsigned i = 0; i != 50u; ++i) {
			const auto result = ::waitpid(m_pid, &status, WNOHANG);
			if (result == m_pid) {
				return;
			}
			if (result < 0) {
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		::kill(m_pid, SIGKILL);
		::waitpid(m_pid, &status, 0);
	}

	void stderrLoop() {
		while (m_stderr) {
			const auto line = readLine(m_stderr);
			if (!line) {
				break;
			}

			std::lock_guard<std::mutex> lock(m_stderrMutex);
			m_stderrTail += *line;
			if (m_stderrTail.size() > 8192u) {
				m_stderrTail.erase(0u, m_stderrTail.size() - 8192u);
			}
		}
	}

private:
	pid_t m_pid{-1};
	std::FILE* m_stdin{nullptr};
	std::FILE* m_stdout{nullptr};
	std::FILE* m_stderr{nullptr};
	std::thread m_stderrThread{};

	mutable std::mutex m_stderrMutex;
	std::string m_stderrTail{};
};

#endif

} // namespace

class KataGoSession::Implementation {
public:
	explicit Implementation(KataGoLaunchConfig launchConfig) : m_launchConfig(std::move(launchConfig)) {
	}

	bool newGame(const GameConfig& config) {
		std::lock_guard<std::mutex> lock(m_mutex);

		clearError();
		stopProcess();

		if (!validateLaunchConfig()) {
			return false;
		}
		if (config.boardSize == 0u) {
			setError("KataGo game config requires a non-zero board size.");
			return false;
		}

#if !defined(__unix__) && !defined(__APPLE__)
		setError("KataGoSession transport is only implemented for POSIX platforms.");
		return false;
#else
		m_gameConfig    = config;
		m_setupCommands = buildSetupCommands(config);
		m_recordedMoves.clear();
		m_lastCommand.clear();

		if (!m_process.start(m_launchConfig, m_lastError)) {
			m_state = SessionState::Error;
			return false;
		}

		for (const auto& command: m_setupCommands) {
			if (!sendCommandExpectSuccess(command)) {
				stopProcess();
				return false;
			}
		}

		clearError();
		m_state = SessionState::Ready;
		return true;
#endif
	}

	bool recordMove(const Move& move) {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_state != SessionState::Ready) {
			setError("Cannot record move before KataGo session is ready.");
			return false;
		}
		if (!m_gameConfig) {
			setError("Cannot record move without an active KataGo game config.");
			return false;
		}
		if (moveNeedsCoordinate(move) && !move.coord) {
			setError("Place move missing coordinate for KataGo mirror state.");
			return false;
		}

		if (move.kind != MoveKind::Resign) {
			const auto command = buildPlayCommand(move, m_gameConfig->boardSize);
			if (!sendCommandExpectSuccess(command)) {
				return false;
			}
		}

		m_recordedMoves.push_back(move);
		clearError();
		m_state = SessionState::Ready;
		return true;
	}

	std::optional<Decision> requestMove(const Player player) {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_state != SessionState::Ready) {
			setError("Cannot request move while KataGo session is not ready.");
			return std::nullopt;
		}
		if (!m_gameConfig) {
			setError("Cannot request KataGo move without an active game config.");
			return std::nullopt;
		}

		m_state = SessionState::Thinking;

		const auto command  = buildSearchCommand(player);
		const auto response = sendCommandExpectSuccess(command);
		if (!response) {
			return std::nullopt;
		}

		const auto move = gtp::parseMoveResponse(*response, player, m_gameConfig->boardSize);
		if (!move) {
			setError("Could not parse KataGo move response: " + response->payload);
			return std::nullopt;
		}

		clearError();
		m_state = SessionState::Ready;
		return Decision{
		        .move       = *move,
		        .visits     = std::nullopt,
		        .winrate    = std::nullopt,
		        .rawPayload = response->payload,
		};
	}

	SessionState state() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_state;
	}

	std::string lastError() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_lastError;
	}

	void shutdown() {
		std::lock_guard<std::mutex> lock(m_mutex);

		stopProcess();
		m_recordedMoves.clear();
		m_setupCommands.clear();
		m_lastCommand.clear();
		m_gameConfig.reset();
		clearError();
		m_state = SessionState::Closed;
	}

private:
	bool validateLaunchConfig() {
		auto requireExistingFile = [this](const std::string& path, const std::string& label) {
			if (!hasValue(path)) {
				setError("KataGo launch config missing " + label + '.');
				return false;
			}
			if (!std::filesystem::exists(path)) {
				setError("KataGo launch config " + label + " does not exist: " + path);
				return false;
			}
			return true;
		};

		if (!requireExistingFile(m_launchConfig.executablePath, "executablePath")) {
			return false;
		}
		if (!requireExistingFile(m_launchConfig.configPath, "configPath")) {
			return false;
		}
		if (!requireExistingFile(m_launchConfig.modelPath, "modelPath")) {
			return false;
		}
		if (m_launchConfig.humanModelPath && !std::filesystem::exists(*m_launchConfig.humanModelPath)) {
			setError("KataGo launch config humanModelPath does not exist: " + *m_launchConfig.humanModelPath);
			return false;
		}

		return true;
	}

	std::vector<std::string> buildSetupCommands(const GameConfig& config) const {
		std::vector<std::string> commands{};
		commands.push_back(gtp::buildCommand("boardsize", {std::to_string(config.boardSize)}));
		commands.push_back(gtp::buildCommand("clear_board"));

		std::ostringstream komi;
		komi << config.komi;
		commands.push_back(gtp::buildCommand("komi", {komi.str()}));

		if (!config.rules.empty()) {
			commands.push_back(gtp::buildCommand("kata-set-rules", {config.rules}));
		}

		auto difficultyCommands = buildDifficultyCommands(config);
		commands.insert(commands.end(), std::make_move_iterator(difficultyCommands.begin()), std::make_move_iterator(difficultyCommands.end()));

		return commands;
	}

	std::string buildPlayCommand(const Move& move, const std::size_t boardSize) const {
		switch (move.kind) {
		case MoveKind::Place:
			assert(move.coord);
			return gtp::buildCommand("play", {gtp::toGtpColor(move.player), gtp::toGtpVertex(*move.coord, boardSize)});
		case MoveKind::Pass:
			return gtp::buildCommand("play", {gtp::toGtpColor(move.player), "pass"});
		case MoveKind::Resign:
			return {};
		default:
			assert(false);
			return {};
		}
	}

	std::string buildSearchCommand(const Player player) const {
		return gtp::buildCommand("kata-search", {gtp::toGtpColor(player)});
	}

	std::optional<gtp::Response> sendCommandExpectSuccess(const std::string& command) {
#if !defined(__unix__) && !defined(__APPLE__)
		(void)command;
		setError("KataGoSession transport is only implemented for POSIX platforms.");
		return std::nullopt;
#else
		std::string processError{};

		m_lastCommand = command;
		if (!m_process.send(command, processError)) {
			setError(composeError("Could not send KataGo command", processError));
			return std::nullopt;
		}

		const auto rawBlock = m_process.readResponseBlock(processError);
		if (!rawBlock) {
			setError(composeError("Could not read KataGo response", processError));
			return std::nullopt;
		}

		const auto response = gtp::parseResponseBlock(*rawBlock);
			if (!response) {
				setError(composeError("Received malformed GTP response from KataGo", trimTrailingWhitespace(*rawBlock)));
				return std::nullopt;
			}
		if (!response->success) {
			setError(composeError("KataGo rejected GTP command", response->payload));
			return std::nullopt;
		}

		return response;
#endif
	}

	std::string composeError(const std::string& prefix, const std::string& detail) const {
		std::ostringstream out;
		out << prefix;
		if (!m_lastCommand.empty()) {
			out << " [" << trimTrailingWhitespace(m_lastCommand) << "]";
		}
		if (!detail.empty()) {
			out << ": " << trimTrailingWhitespace(detail);
		}
#if defined(__unix__) || defined(__APPLE__)
		const auto stderrTail = trimTrailingWhitespace(m_process.stderrTail());
		if (!stderrTail.empty()) {
			out << " | stderr: " << stderrTail;
		}
#endif
		return out.str();
	}

	void clearError() {
		m_lastError.clear();
	}

	void setError(std::string message) {
		m_lastError = std::move(message);
		m_state     = SessionState::Error;
	}

	void stopProcess() {
#if defined(__unix__) || defined(__APPLE__)
		m_process.stop();
#endif
	}

private:
	KataGoLaunchConfig m_launchConfig{};
	std::optional<GameConfig> m_gameConfig{};
	std::vector<Move> m_recordedMoves{};
	std::vector<std::string> m_setupCommands{};
	std::string m_lastCommand{};
	std::string m_lastError{};
	SessionState m_state{SessionState::Idle};
	mutable std::mutex m_mutex{};

#if defined(__unix__) || defined(__APPLE__)
	KataGoProcess m_process{};
#endif
};

KataGoSession::KataGoSession(KataGoLaunchConfig launchConfig) : m_pimpl(std::make_unique<Implementation>(std::move(launchConfig))) {
}

KataGoSession::~KataGoSession() = default;

bool KataGoSession::newGame(const GameConfig& config) {
	return m_pimpl->newGame(config);
}

bool KataGoSession::recordMove(const Move& move) {
	return m_pimpl->recordMove(move);
}

std::optional<Decision> KataGoSession::requestMove(const Player player) {
	return m_pimpl->requestMove(player);
}

SessionState KataGoSession::state() const {
	return m_pimpl->state();
}

std::string KataGoSession::lastError() const {
	return m_pimpl->lastError();
}

void KataGoSession::shutdown() {
	m_pimpl->shutdown();
}

} // namespace tengen::engine
