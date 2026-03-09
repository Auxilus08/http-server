#pragma once

#include <string>
#include <map>
#include <deque>
#include "Types.hpp"
#include "VirtualHost.hpp"

class Socket {
	private:
		std::string							address_;
		std::string							port_;
		std::map<std::string, VirtualHost>	v_hosts_;

	public:
		Socket(std::string& listen, std::string& name, std::string& max_size,
			   StringMap& errors, LocationMap& locations);
		~Socket();

		void AddVirtualHost(std::string& name, std::string& max_size,
							StringMap& errors, LocationMap& locations);
		std::string getSocket() const;
		std::string ToString() const;
};
