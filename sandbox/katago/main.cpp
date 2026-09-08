#include "katagoProcess.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

constexpr char* KATAGO_EXE         = KATAGO_PATH "/katago";
constexpr char* KATAGO_MODEL       = KATAGO_PATH "/g170-b30c320x2-s4824661760-d1229536699.bin.gz";
constexpr char* KATAGO_MODEL_HUMAN = KATAGO_PATH "/b18c384nbt-humanv0.bin.gz";
constexpr char* KATAGO_CONFIG      = KATAGO_PATH "/gtp_human5k_example.cfg";


class BotSession {
	enum class Status {
		Idle,
		// Active, // Active is implied by !Idle && !Finished
		BotMove,  //!< Bot's turn. The move has not been requested yet.
		Thinking, //!< Move requested. The engine answers through onBotMove().
		PlayerMove,
		Finished
	};

public:
	//! Notified once the bot picked a move. Runs on the engine thread.
	using MoveCallback = std::function<void(const std::string& move)>;

	~BotSession() {
		joinEngineThread();
	}

	void setMoveCallback(MoveCallback callback) {
		m_moveCallback = std::move(callback);
	}

	bool start() {
		if (m_status != Status::Idle && m_status != Status::Finished) {
			return false;
		}

		if (!m_engine.start(LaunchConfig{
		            .executable = KATAGO_EXE,
		            .model      = KATAGO_MODEL,
		            .modelHuman = KATAGO_MODEL_HUMAN,
		            .config     = KATAGO_CONFIG})) {
			return false;
		}

		m_status = Status::PlayerMove; // Player plays black and moves first.
		return true;
	}

	//! Register the player move. The move is already validated by the game rules.
	bool registerPlayerMove(const std::string& move) {
		if (m_status != Status::PlayerMove) {
			return false;
		}

		std::string response;
		if (!m_engine.sendCommand("play b " + move, response)) {
			return false;
		}

		m_status = Status::BotMove;
		return true;
	}

	//! Ask the engine for its move. Returns immediately while the bot thinks; onBotMove() delivers the result.
	bool requestBotMove() {
		if (m_status != Status::BotMove) {
			return false;
		}

		joinEngineThread(); // Retire the previous request. Only one is ever in flight.

		m_status       = Status::Thinking;
		m_engineThread = std::thread([this] {
			std::string move;
			const bool success = m_engine.sendCommand("genmove w", move);
			onBotMove(success, move);
		});
		return true;
	}

	bool isActive() {
		return m_status != Status::Idle && m_status != Status::Finished;
	}

private:
	//! Runs on the engine thread once the bot answered.
	void onBotMove(const bool success, const std::string& move) {
		if (!success) {
			m_status = Status::Finished;
			return;
		}

		m_status = Status::PlayerMove;
		if (m_moveCallback) {
			m_moveCallback(move);
		}
	}

	//! Wait for the in flight request to finish.
	void joinEngineThread() {
		if (m_engineThread.joinable()) {
			m_engineThread.join();
		}
	}

private:
	std::atomic<Status> m_status{Status::Idle}; //!< Also written from the engine thread.
	MoveCallback m_moveCallback;                //!< Attached by the owner of the session.
	KatagoProcess m_engine;                     //!< The engine process. Only touched by one thread at a time.
	std::thread m_engineThread;                 //!< Runs the in flight move request.
};

int main(int argc, char** argv) {
	std::cout << "Using configuration:\n"
	          << "Executable:    " << KATAGO_EXE
	          << "Model:         " << KATAGO_MODEL
	          << "Human Model:   " << KATAGO_MODEL_HUMAN
	          << "Configuration: " << KATAGO_CONFIG
	          << std::endl;

	BotSession session;
	if (!session.start()) {
		std::cerr << "Failed to start the session\n";
		return 1;
	}

	std::mutex mutex;
	std::condition_variable condition;
	bool done = false;

	session.setMoveCallback([&](const std::string& move) {
		std::cout << "Bot plays: " << move << '\n';
		{
			std::lock_guard<std::mutex> lock(mutex);
			done = true;
		}
		condition.notify_one();
	});

	if (!session.registerPlayerMove("q4") || !session.requestBotMove()) {
		std::cerr << "Failed to play the move\n";
		return 1;
	}

	// The request returned while the bot is still thinking. A real owner (the game loop, the GUI)
	// keeps running here and reacts when the callback fires.
	std::cout << "Player played q4, bot is thinking...\n";

	std::unique_lock<std::mutex> lock(mutex);
	condition.wait(lock, [&] { return done; });

	return 0;
}
