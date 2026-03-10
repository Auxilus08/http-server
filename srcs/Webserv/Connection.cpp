#include "Connection.hpp"

Connection::Connection(int fd, size_t timeout)
	: fd_(fd)
	, last_active_(std::chrono::steady_clock::now())
	, timeout_(timeout) {
}

Connection::~Connection() {
}

bool Connection::HasTimedOut() const {
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<sec>(now - last_active_);
	return elapsed >= sec(timeout_);
}

void Connection::UpdateLastActive() {
	last_active_ = std::chrono::steady_clock::now();
}

int Connection::getFd() const {
	return fd_;
}
