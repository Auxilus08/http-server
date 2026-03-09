#pragma once

#include <string>
#include <deque>
#include <vector>
#include <map>
#include <memory>
#include <poll.h>
#include "Socket.hpp"
#include "Connection.hpp"
#include "ConfigParser.hpp"

class WebServ {
	private:
		std::string								conf_;
		std::deque<Socket>						sockets_;
		std::vector<struct pollfd>				pollFDs_;
		std::map<int, std::unique_ptr<Connection>>	connections_;

		void PollAvailableFDs();
		void CheckForNewConnection(int fd, short revents, int i);
		void CloseConnection(int fd, int i);
		void CloseAllConnections();

		// Declared for future use — minimal stubs for now
		void AddNewConnection(int fd);
		void SwitchCgiToReceive(int fd);
		void SwitchClientToSend(int fd);

	public:
		WebServ(const char* conf);
		~WebServ();

		int Init();
		void Run();
};
