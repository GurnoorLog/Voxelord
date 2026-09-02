#include <cstdlib>
#include <filesystem>
#include <string>

#include "Network/Protocol.h"
#include "Server/GameServer.h"
#include "Util/Logger.h"

// Dedicated headless server. Usage: voxlord-server [port]
int main(int argc, char* argv[]) {
	std::filesystem::create_directories("Data/Logs");
	LOG.setFileOutputLevel(Level::INFO);
	LOG.setOutputFile("Data/Logs/server.log");

	uint16_t port = Protocol::DEFAULT_PORT;
	if (argc > 1)
		port = static_cast<uint16_t>(std::atoi(argv[1]));

	LOG(Level::INFO) << "VoxLord dedicated server starting on port " << port << std::endl;

	GameServer server(port);
	if (!server.start()) {
		LOG(Level::ERROR) << "Could not bind port " << port << " (is it already in use?)" << std::endl;
		return 1;
	}

	LOG(Level::INFO) << "Server is running. Press Ctrl+C in a terminal or close it to stop." << std::endl;
	server.run();

	LOG(Level::INFO) << "Server stopped." << std::endl;
	return 0;
}