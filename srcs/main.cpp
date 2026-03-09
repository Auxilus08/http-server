#include "Logger.hpp"

int main() {
	Logger::logInfo("Server starting on port ", 8080);
	Logger::logError("Failed to bind socket: ", "Address already in use");
	Logger::logDebug("Connection pool size: ", 16, " | Timeout: ", 30, "s");

	Logger::logInfo("Single argument test");
	Logger::logError("Error code: ", 404, " Not Found");
	Logger::logDebug("This line only appears with -DDEBUG");

	return 0;
}
