#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include "Socket.hpp"

class ConfigParser {
	private:
		std::ifstream	_file;

		std::string		ParseListen(std::stringstream& ss);
		std::string		ParseServerName(std::stringstream& ss);
		std::string		ParseMaxBodySize(std::stringstream& ss);
		std::pair<std::string, std::string>	ParseErrorPage(std::stringstream& ss);
		std::pair<std::string, Location>	ParseLocation(std::stringstream& ss);
		void			ParseServer(std::stringstream& ss, std::deque<Socket>& sockets);

	public:
		ConfigParser(const std::string& conf);
		~ConfigParser();

		int ParseConfig(std::deque<Socket>& sockets);
};
