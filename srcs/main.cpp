#include "WebServ.hpp"
#include "Logger.hpp"

int main(int argc, char** argv) {
	const char* config = (argc > 1) ? argv[1] : nullptr;

	WebServ server(config);

	if (server.Init() != 0) {
		Logger::logError("Server initialization failed");
		return 1;
	}

	server.Run();

	return 0;
}
