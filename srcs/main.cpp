#include "Logger.hpp"
#include "ConfigParser.hpp"
#include <deque>

int main(int argc, char** argv) {
	std::string config_path = "conf/default.conf";
	if (argc > 1)
		config_path = argv[1];

	Logger::logInfo("Parsing configuration: ", config_path);

	ConfigParser parser(config_path);
	std::deque<Socket> sockets;

	if (parser.ParseConfig(sockets) != 0) {
		Logger::logError("Failed to parse configuration");
		return 1;
	}

	Logger::logInfo("Successfully parsed ", sockets.size(), " socket(s)");
	std::cout << "\n";

	for (const auto& socket : sockets)
		std::cout << socket.ToString() << std::endl;

	return 0;
}
