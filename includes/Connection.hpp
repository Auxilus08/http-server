#pragma once

// Stub Connection class — will be fully implemented later.
// For now, just enough to be stored in WebServ's connections_ map.

class Connection {
	private:
		int fd_;

	public:
		explicit Connection(int fd) : fd_(fd) {}
		~Connection() = default;

		int getFd() const { return fd_; }
};
