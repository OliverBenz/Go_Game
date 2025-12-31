#include "Logging.hpp"

#include "Logger/LogConfig.hpp"
#include "Logger/LogOutputConsole.hpp"
#include "Logger/LogOutputFile.hpp"

#include <filesystem>
#include <format>
#include <iostream>
#include <mutex>

namespace go::network {

static Logging::LogConfig config;

//! Enable logging of any entries to an output file + console(for debug builds).
static void InitializeLogger() {
	config.SetLogEnabled(true);
	config.SetMinLogLevel(Logging::LogLevel::Any);

	// Get and create default logging dir
	const auto logPath = Logging::GetDefaultLogDir("GoGame/Networking");

	std::error_code ec{};
	std::filesystem::create_directories(logPath, ec);
	if (!ec) {
		config.AddLogOutput(std::make_shared<Logging::LogOutputFile>(logPath / "log.txt"));
	} else {
		std::cerr << std::format("[Logger] Could not create directory: {}\nApplication will not log to file.", logPath.string());
	}

#ifndef NDEBUG
	config.AddLogOutput(std::make_shared<Logging::LogOutputConsole>());
#endif
}

Logging::Logger Logger() {
	static std::once_flag logInitFlag;
	std::call_once(logInitFlag, InitializeLogger);

	return Logging::Logger(config);
}

} // namespace go::network
