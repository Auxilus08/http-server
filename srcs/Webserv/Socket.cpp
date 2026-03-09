#include "Socket.hpp"
#include <sstream>

Socket::Socket(std::string& listen, std::string& name, std::string& max_size,
			   StringMap& errors, LocationMap& locations) {
	size_t colon = listen.find(':');
	address_ = listen.substr(0, colon);
	port_ = listen.substr(colon + 1);
	v_hosts_.emplace(name, VirtualHost(name, max_size, errors, locations));
}

Socket::~Socket() {
}

void Socket::AddVirtualHost(std::string& name, std::string& max_size,
							StringMap& errors, LocationMap& locations) {
	v_hosts_.emplace(name, VirtualHost(name, max_size, errors, locations));
}

std::string Socket::getSocket() const {
	return address_ + ":" + port_;
}

std::string Socket::ToString() const {
	std::ostringstream oss;
	oss << "Socket [" << address_ << ":" << port_ << "]\n";
	for (const auto& [name, vhost] : v_hosts_)
		oss << "  " << vhost.ToString() << "\n";
	return oss.str();
}
