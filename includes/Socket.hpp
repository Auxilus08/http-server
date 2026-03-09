#pragma once

#include <string>
#include <map>
#include <deque>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include "Types.hpp"
#include "VirtualHost.hpp"

class Socket {
	private:
		std::string							address_;
		std::string							port_;
		std::map<std::string, VirtualHost>	v_hosts_;
		VirtualHost*						first_vhost_;
		struct pollfd						listening_;

	public:
		Socket(std::string& listen, std::string& name, std::string& max_size,
			   StringMap& errors, LocationMap& locations);
		~Socket();

		void AddVirtualHost(std::string& name, std::string& max_size,
							StringMap& errors, LocationMap& locations);
		int InitServer(std::vector<struct pollfd>& pollFDs);
		VirtualHost* FindVhost(const std::string& host);
		std::string getSocket() const;
		std::string ToString() const;
};
