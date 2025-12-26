#include "Logging.hpp"

#include "Logger/LogConfig.hpp"
#include "Logger/LogOutputConsole.hpp"
#include "Logger/LogOutputFile.hpp"

#include <iostream>

namespace go::ui {

static Logging::LogConfig config;

//! Enable logging of any entries to an output file + console(for debug builds).
static void InitializeLogger() {
	config.SetLogEnabled(true);
	config.SetMinLogLevel(Logging::LogLevel::Any);

    // TODO: Better path
	auto logFile = std::make_shared<Logging::LogOutputFile>("Logfile.txt");
	config.AddLogOutput(logFile);

#ifndef NDEBUG
	config.AddLogOutput(std::make_shared<Logging::LogOutputConsole>());
#endif
}

Logging::Logger Logger() {
	static bool initialized = false;
	if (!initialized) {
		InitializeLogger();
		initialized = true;
	}

	return Logging::Logger(config);
}

}