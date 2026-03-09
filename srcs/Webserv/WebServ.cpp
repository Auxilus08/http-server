#include "WebServ.hpp"
#include "Logger.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

// ─── Constructor / Destructor ────────────────────────────────────────────────

WebServ::WebServ(const char* conf)
	: conf_(conf ? conf : "conf/default.conf") {
}

WebServ::~WebServ() {
	CloseAllConnections();
}

// ─── Init ────────────────────────────────────────────────────────────────────

int WebServ::Init() {
	// Verify config file ends with .conf
	if (conf_.size() < 5 || conf_.substr(conf_.size() - 5) != ".conf") {
		Logger::logError("Configuration file must have .conf extension: ", conf_);
		return 1;
	}

	// Parse configuration
	ConfigParser parser(conf_);
	if (parser.ParseConfig(sockets_) != 0) {
		Logger::logError("Failed to parse configuration: ", conf_);
		return 1;
	}

	Logger::logInfo("Parsed ", sockets_.size(), " socket(s) from ", conf_);

	// Initialize listening sockets
	for (auto& socket : sockets_) {
		if (socket.InitServer(pollFDs_) != 0) {
			Logger::logError("Failed to initialize socket: ", socket.getSocket());
			return 1;
		}
	}

	Logger::logInfo("Server initialized — ", pollFDs_.size(),
					" listening fd(s) ready");
	return 0;
}

// ─── Run (main event loop) ───────────────────────────────────────────────────

void WebServ::Run() {
	Logger::logInfo("Entering event loop...");

	while (true) {
		int ready = poll(pollFDs_.data(), pollFDs_.size(), 500);

		if (ready < 0) {
			Logger::logError("poll: ", strerror(errno));
			break;
		}

		if (ready > 0)
			PollAvailableFDs();
	}

	CloseAllConnections();
}

// ─── PollAvailableFDs ────────────────────────────────────────────────────────

void WebServ::PollAvailableFDs() {
	// Iterate backwards to safely erase elements by index
	for (int i = static_cast<int>(pollFDs_.size()) - 1; i >= 0; --i) {
		if (pollFDs_[i].revents == 0)
			continue;

		int fd = pollFDs_[i].fd;
		short revents = pollFDs_[i].revents;

		if (static_cast<size_t>(i) < sockets_.size()) {
			// This is a listening socket
			CheckForNewConnection(fd, revents, i);
		} else {
			// This is a client connection
			if (revents & POLLERR) {
				Logger::logError("POLLERR on fd ", fd, " — closing connection");
				CloseConnection(fd, i);
			} else if (revents & POLLIN) {
				Logger::logDebug("POLLIN on fd ", fd, " (client data ready)");
				// TODO: read and parse request
			} else if (revents & POLLOUT) {
				Logger::logDebug("POLLOUT on fd ", fd, " (client writable)");
				// TODO: send response
			}
		}
	}
}

// ─── CheckForNewConnection ───────────────────────────────────────────────────

void WebServ::CheckForNewConnection(int fd, short revents, int i) {
	(void)i;

	if (!(revents & POLLIN))
		return;

	if (connections_.size() > 500) {
		Logger::logError("Connection limit reached (500) — rejecting new connection");
		return;
	}

	int client_fd = accept(fd, nullptr, nullptr);
	if (client_fd < 0) {
		Logger::logError("accept: ", strerror(errno));
		return;
	}

	// Set non-blocking
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
		Logger::logError("fcntl: ", strerror(errno));
		close(client_fd);
		return;
	}

	// Add to pollFDs with POLLIN
	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	pollFDs_.push_back(pfd);

	// Create stub Connection
	connections_[client_fd] = std::make_unique<Connection>(client_fd);

	Logger::logInfo("New connection accepted on fd ", client_fd);
}

// ─── CloseConnection ─────────────────────────────────────────────────────────

void WebServ::CloseConnection(int fd, int i) {
	close(fd);
	connections_.erase(fd);

	if (i >= 0 && static_cast<size_t>(i) < pollFDs_.size())
		pollFDs_.erase(pollFDs_.begin() + i);

	Logger::logDebug("Closed connection fd ", fd);
}

// ─── CloseAllConnections ─────────────────────────────────────────────────────

void WebServ::CloseAllConnections() {
	// Close client connections
	for (auto& [fd, conn] : connections_)
		close(fd);
	connections_.clear();

	// Close listening sockets
	for (auto& pfd : pollFDs_) {
		close(pfd.fd);
	}
	pollFDs_.clear();

	Logger::logInfo("All connections closed");
}

// ─── Stubs (declared for future use) ─────────────────────────────────────────

void WebServ::AddNewConnection(int fd) {
	(void)fd;
	// TODO: will create a full Connection object with request/response state
}

void WebServ::SwitchCgiToReceive(int fd) {
	(void)fd;
	// TODO: switch CGI pipe fd from POLLOUT to POLLIN
}

void WebServ::SwitchClientToSend(int fd) {
	(void)fd;
	// TODO: switch client fd from POLLIN to POLLOUT when response is ready
}
