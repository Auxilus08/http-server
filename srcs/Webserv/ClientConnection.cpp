#include "ClientConnection.hpp"
#include "Logger.hpp"
#include <sys/socket.h>
#include <cstring>
#include <cerrno>

ClientConnection::ClientConnection(int fd, Socket& sock, WebServ& webserv)
	: Connection(fd, 10)
	, stage_(Stage::kHeader)
	, status_("200")
	, sock_(sock)
	, webserv_(webserv)
	, parser_(*this)
	, response_(*this)
	, vhost_(nullptr)
	, drain_incoming_(false)
	, drained_bytes_(0) {
}

ClientConnection::~ClientConnection() {
	if (file_.is_open())
		file_.close();
	if (fd_ >= 0) {
		close(fd_);
		fd_ = -1;
	}
}

int ClientConnection::ReceiveData(struct pollfd& poll) {
	char buf[8192];
	ssize_t bytes = recv(fd_, buf, sizeof(buf), 0);

	if (bytes < 0) {
		Logger::logError("recv on fd ", fd_, ": ", strerror(errno));
		return 1;
	}

	if (bytes == 0) {
		Logger::logInfo("Client on fd ", fd_, " closed connection (EOF)");
		return 2;
	}

	UpdateLastActive();

	// Drain mode: count bytes and close if >= 1MB
	if (stage_ == Stage::kDrain) {
		drained_bytes_ += static_cast<size_t>(bytes);
		if (drained_bytes_ >= 1048576) {
			Logger::logInfo("Drained 1MB on fd ", fd_, " — closing");
			return 1;
		}
		return 0;
	}

	// Header stage: accumulate and parse
	if (stage_ == Stage::kHeader) {
		recv_buffer_.append(buf, static_cast<size_t>(bytes));

		// Wait for complete headers
		size_t header_end = recv_buffer_.find("\r\n\r\n");
		if (header_end == std::string::npos)
			return 0;

		// Parse request headers
		bool parse_ok = parser_.ParseHeader(recv_buffer_);

		// Resolve vhost (even on parse failure, for error pages)
		vhost_ = sock_.FindVhost(parser_.getHost());

		// Handle the request
		bool handle_ok = parse_ok && parser_.HandleRequest();

		if (!handle_ok) {
			// Open error page file
			std::string error_path = vhost_->getErrorPage(status_);
			if (!error_path.empty()) {
				file_.open(error_path, std::ios::in | std::ios::binary);
				if (file_.is_open())
					Logger::logDebug("Opened error page: ", error_path);
			}
			stage_ = Stage::kResponse;
		}

		// Log results
		Logger::logInfo("fd ", fd_, ": ", parser_.getMethod(), " ",
						parser_.getTarget(),
						parser_.getQueryString().empty() ? "" : "?",
						parser_.getQueryString(),
						" → ", status_);

		recv_buffer_.clear();
	}

	// Switch to POLLOUT when response is ready
	if (stage_ == Stage::kResponse) {
		poll.events = POLLOUT;
	}

	return 0;
}

int ClientConnection::SendData(struct pollfd& poll) {
	// Prepare response header on first call
	if (stage_ == Stage::kResponse) {
		response_.PrepareResponse();
		stage_ = Stage::kSending;
	}

	// Send response (one send() per call)
	int result = response_.SendResponse(poll);
	if (result != 0) {
		if (file_.is_open())
			file_.close();
		return 1;
	}

	// Check if response is fully sent (poll switched back to POLLIN)
	if (poll.events == POLLIN) {
		if (file_.is_open())
			file_.close();

		if (status_ != "200") {
			// Error responses: close connection or drain
			Logger::logDebug("Response sent (", status_, ") — closing fd ", fd_);
			return 1;
		}

		// HTTP keep-alive: reset for next request
		Logger::logDebug("Response sent (200) — keep-alive on fd ", fd_);
		ResetClientConnection();
	}

	return 0;
}

void ClientConnection::ResetClientConnection() {
	parser_.ResetParser();
	response_.ResetResponse();
	status_ = "200";
	stage_ = Stage::kHeader;
	vhost_ = nullptr;
	drain_incoming_ = false;
	drained_bytes_ = 0;
	recv_buffer_.clear();
	if (file_.is_open())
		file_.close();
}
