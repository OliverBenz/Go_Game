#include "MainWindowPresenter.hpp"

#include "GamePresenter.hpp"
#include "Logging.hpp"
#include "engine/katagoSession.hpp"
#include "tengen/botSession.hpp"
#include "tengen/networkSession.hpp"
#include "tengen/openSession.hpp"

#include <QMessageBox>
#include <QObject>
#include <filesystem>
#include <memory>

namespace tengen {
namespace {

constexpr const char* KATAGO_EXECUTABLE = TENGEN_KATAGO_EXECUTABLE;
constexpr const char* KATAGO_CONFIG     = TENGEN_KATAGO_CONFIG;
constexpr const char* KATAGO_MODEL      = TENGEN_KATAGO_MODEL;

engine::Difficulty toDifficulty(const unsigned difficultyIndex) {
	switch (difficultyIndex) {
	case 0u:
		return engine::Difficulty::Easy;
	case 1u:
		return engine::Difficulty::Medium;
	case 2u:
		return engine::Difficulty::Hard;
	default:
		return engine::Difficulty::Medium;
	}
}

bool katagoPathsAvailable() {
	return std::filesystem::exists(KATAGO_EXECUTABLE) && std::filesystem::exists(KATAGO_CONFIG) && std::filesystem::exists(KATAGO_MODEL);
}

engine::KataGoLaunchConfig launchConfig() {
	return engine::KataGoLaunchConfig{
	        .executablePath = KATAGO_EXECUTABLE,
	        .configPath     = KATAGO_CONFIG,
	        .modelPath      = KATAGO_MODEL,
	};
}

engine::GameConfig toEngineGameConfig(const unsigned boardSize, const unsigned difficultyIndex) {
	return engine::GameConfig{
	        .boardSize  = boardSize,
	        .komi       = 6.5,
	        .rules      = "chinese",
	        .difficulty = toDifficulty(difficultyIndex),
	        .limits     = engine::SearchLimits{},
	};
}

} // namespace

MainWindowPresenter::MainWindowPresenter(gui::MainWindow& mainWindow) : QObject(nullptr), m_mainWindow(mainWindow) {
	QObject::connect(&m_mainWindow, &gui::MainWindow::newLocalGameRequested, this, &MainWindowPresenter::onNewLocalGameRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::botGameRequested, this, &MainWindowPresenter::onBotGameRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::connectRequested, this, &MainWindowPresenter::onConnectRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::hostRequested, this, &MainWindowPresenter::onHostRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::shutdownRequested, this, &MainWindowPresenter::onShutdownRequested);

	startOpenPlay();
}

MainWindowPresenter::~MainWindowPresenter() = default;

void MainWindowPresenter::startOpenPlay() {
	m_game          = std::make_unique<app::OpenSession>(9u);
	m_gamePresenter = std::make_unique<GamePresenter>(*m_game, m_mainWindow.gameWidget());
}

void MainWindowPresenter::onNewLocalGameRequested() {
	onShutdownRequested();
	startOpenPlay();
}

void MainWindowPresenter::onBotGameRequested(const unsigned boardSize, const unsigned difficultyIndex, const bool humanPlaysBlack) {
	onShutdownRequested();

	if (!katagoPathsAvailable()) {
		const auto message = QStringLiteral("Could not find the configured KataGo executable, config, or model under the local ../KataGo/cpp tree.");
		gui::Logger().Log(Logging::LogLevel::Error, message.toStdString());
		QMessageBox::critical(&m_mainWindow, QStringLiteral("KataGo Unavailable"), message);
		startOpenPlay();
		return;
	}

	auto engine = std::make_unique<engine::KataGoSession>(launchConfig());
	const auto config = toEngineGameConfig(boardSize, difficultyIndex);
	if (!engine->newGame(config)) {
		const auto message = QString::fromStdString(engine->lastError());
		gui::Logger().Log(Logging::LogLevel::Error, message.toStdString());
		QMessageBox::critical(&m_mainWindow, QStringLiteral("Could Not Start KataGo"), message);
		startOpenPlay();
		return;
	}

	m_game = std::make_unique<app::BotSession>(
	        app::BotSessionConfig{
	                .boardSize   = boardSize,
	                .komi        = 6.5,
	                .rules       = "chinese",
	                .humanPlayer = humanPlaysBlack ? Player::Black : Player::White,
	                .difficulty  = toDifficulty(difficultyIndex),
	                .limits      = engine::SearchLimits{},
	        },
	        std::move(engine));
	m_gamePresenter = std::make_unique<GamePresenter>(*m_game, m_mainWindow.gameWidget());
}

void MainWindowPresenter::onConnectRequested(const QString& hostIp) {
	onShutdownRequested();

	auto session = std::make_unique<app::NetworkSession>();
	session->connect(hostIp.toStdString());

	auto& game      = static_cast<app::IGameSession&>(*session);
	auto& chat      = static_cast<app::IChatSession&>(*session);
	m_gamePresenter = std::make_unique<GamePresenter>(game, m_mainWindow.gameWidget());
	m_gamePresenter->addChatWindow(chat);
	m_game = std::move(session);
}

void MainWindowPresenter::onHostRequested(const unsigned boardSize) {
	onShutdownRequested();

	auto session = std::make_unique<app::NetworkSession>();
	session->host(boardSize);

	auto& game      = static_cast<app::IGameSession&>(*session);
	auto& chat      = static_cast<app::IChatSession&>(*session);
	m_gamePresenter = std::make_unique<GamePresenter>(game, m_mainWindow.gameWidget());
	m_gamePresenter->addChatWindow(chat);
	m_game = std::move(session);
}

void MainWindowPresenter::onShutdownRequested() {
	if (m_game) {
		m_gamePresenter = nullptr; // Destroy before m_game
		m_game->shutdown();
		m_game = nullptr;
	}
}

} // namespace tengen
