#include "katagoProcess.hpp"

#include <iostream>

constexpr char* KATAGO_EXE         = KATAGO_PATH "/katago";
constexpr char* KATAGO_MODEL       = KATAGO_PATH "/g170-b30c320x2-s4824661760-d1229536699.bin.gz";
constexpr char* KATAGO_MODEL_HUMAN = KATAGO_PATH "/b18c384nbt-humanv0.bin.gz";
constexpr char* KATAGO_CONFIG      = KATAGO_PATH "/gtp_human5k_example.cfg";

int main(int argc, char** argv) {
	std::cout << "Using configuration:\n"
	          << "Executable:    " << KATAGO_EXE
	          << "Model:         " << KATAGO_MODEL
	          << "Human Model:   " << KATAGO_MODEL_HUMAN
	          << "Configuration: " << KATAGO_CONFIG
	          << std::endl;

	KatagoProcess process;
	process.start(LaunchConfig{
	        .executable = KATAGO_EXE,
	        .model      = KATAGO_MODEL,
	        .modelHuman = KATAGO_MODEL_HUMAN,
	        .config     = KATAGO_CONFIG});

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
