#pragma once

#include <chrono>
#include <map>
#include <string>
#include <poll.h>

using timepoint = std::chrono::steady_clock::time_point;
using sec = std::chrono::seconds;

class Connection {
	protected:
		int										fd_;
		timepoint								last_active_;
		size_t									timeout_;
		std::map<std::string, std::string>		additional_headers_;

	public:
		Connection(int fd, size_t timeout);
		virtual ~Connection();

		virtual int ReceiveData(struct pollfd& poll) = 0;
		virtual int SendData(struct pollfd& poll) = 0;

		bool HasTimedOut() const;
		void UpdateLastActive();
		int getFd() const;
};
