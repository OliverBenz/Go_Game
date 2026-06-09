#include "engine/katagoSession.hpp"

#include "engine/gtp.hpp"

#include <cassert>
#include <filesystem>
#include <iterator>
#include <optional>
#include <sstream>
#include <utility>

namespace tengen::engine {
namespace {

bool hasValue(const std::string& text) {
	return !text.empty();
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

} // namespace

class KataGoSession::Implementation {
public:
	explicit Implementation(KataGoLaunchConfig launchConfig) : m_launchConfig(std::move(launchConfig)) {
	}

	bool newGame(const GameConfig& config) {
		if (!validateLaunchConfig()) {
			return false;
		}
		if (config.boardSize == 0u) {
			setError("KataGo game config requires a non-zero board size.");
			return false;
		}

		m_gameConfig     = config;
		m_recordedMoves.clear();
		m_setupCommands  = buildSetupCommands(config);
		m_lastCommand.clear();
		m_lastError.clear();
		m_state = SessionState::Ready;
		return true;
	}

	bool recordMove(const Move& move) {
		if (m_state != SessionState::Ready && m_state != SessionState::Thinking) {
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

		m_lastCommand = buildPlayCommand(move, m_gameConfig->boardSize);
		m_recordedMoves.push_back(move);
		m_state = SessionState::Ready;
		return true;
	}

	std::optional<Decision> requestMove(const Player player) {
		if (m_state != SessionState::Ready) {
			setError("Cannot request move while KataGo session is not ready.");
			return std::nullopt;
		}
		if (!m_gameConfig) {
			setError("Cannot request KataGo move without an active game config.");
			return std::nullopt;
		}

		m_state       = SessionState::Thinking;
		m_lastCommand = buildSearchCommand(player);

		// TODO: Spawn KataGo as a child process, drain stderr continuously, and
		// serialize GTP request/response handling through a single transport object.
		setError("KataGo subprocess transport is not implemented yet.");
		return std::nullopt;
	}

	SessionState state() const {
		return m_state;
	}

	std::string lastError() const {
		return m_lastError;
	}

	void shutdown() {
		m_recordedMoves.clear();
		m_setupCommands.clear();
		m_lastCommand.clear();
		m_gameConfig.reset();
		m_lastError.clear();
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
			return gtp::buildCommand("play", {gtp::toGtpColor(move.player), "resign"});
		default:
			assert(false);
			return {};
		}
	}

	std::string buildSearchCommand(const Player player) const {
		return gtp::buildCommand("kata-search", {gtp::toGtpColor(player)});
	}

	void setError(std::string message) {
		m_lastError = std::move(message);
		m_state     = SessionState::Error;
	}

private:
	KataGoLaunchConfig m_launchConfig{};
	std::optional<GameConfig> m_gameConfig{};
	std::vector<Move> m_recordedMoves{};
	std::vector<std::string> m_setupCommands{};
	std::string m_lastCommand{};
	std::string m_lastError{};
	SessionState m_state{SessionState::Idle};
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
