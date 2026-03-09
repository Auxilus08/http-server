#include "Socket.hpp"
#include "Logger.hpp"
#include <sstream>
#include <cstring>

Socket::Socket(std::string& listen, std::string& name, std::string& max_size,
			   StringMap& errors, LocationMap& locations)
	: first_vhost_(nullptr) {
	size_t colon = listen.find(':');
	address_ = listen.substr(0, colon);
	port_ = listen.substr(colon + 1);
	v_hosts_.emplace(name, VirtualHost(name, max_size, errors, locations));
	first_vhost_ = &v_hosts_.begin()->second;
	listening_.fd = -1;
	listening_.events = 0;
	listening_.revents = 0;
}

Socket::~Socket() {
	if (listening_.fd >= 0)
		close(listening_.fd);
}

void Socket::AddVirtualHost(std::string& name, std::string& max_size,
							StringMap& errors, LocationMap& locations) {
	v_hosts_.emplace(name, VirtualHost(name, max_size, errors, locations));
}

// ─── InitServer ──────────────────────────────────────────────────────────────

int Socket::InitServer(std::vector<struct pollfd>& pollFDs) {
	struct addrinfo hints, *servinfo;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	// 1. Resolve address
	int rv = getaddrinfo(address_.c_str(), port_.c_str(), &hints, &servinfo);
	if (rv != 0) {
		Logger::logError("getaddrinfo: ", gai_strerror(rv));
		return 1;
	}

	// 2. Create socket
	listening_.fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (listening_.fd < 0) {
		Logger::logError("socket: ", strerror(errno));
		freeaddrinfo(servinfo);
		return 2;
	}

	// 3. Set SO_REUSEADDR
	int yes = 1;
	if (setsockopt(listening_.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
		Logger::logError("setsockopt: ", strerror(errno));
		close(listening_.fd);
		listening_.fd = -1;
		freeaddrinfo(servinfo);
		return 3;
	}

	// 4. Bind
	if (bind(listening_.fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
		Logger::logError("bind: ", strerror(errno));
		close(listening_.fd);
		listening_.fd = -1;
		freeaddrinfo(servinfo);
		return 4;
	}

	// 5. Free addrinfo
	freeaddrinfo(servinfo);

	// 6. Listen with backlog of 10
	if (listen(listening_.fd, 10) < 0) {
		Logger::logError("listen: ", strerror(errno));
		close(listening_.fd);
		listening_.fd = -1;
		return 5;
	}

	// 7. Set POLLIN and push to pollFDs
	listening_.events = POLLIN;
	pollFDs.push_back(listening_);

	Logger::logInfo("Listening on ", address_, ":", port_, " (fd ", listening_.fd, ")");
	return 0;
}

// ─── FindVhost ───────────────────────────────────────────────────────────────

VirtualHost* Socket::FindVhost(const std::string& host) {
	if (host.empty()) {
		return first_vhost_;
	}
	auto it = v_hosts_.find(host);
	if (it == v_hosts_.end())
		return first_vhost_;
	return &it->second;
}

// ─── Getters / ToString ─────────────────────────────────────────────────────

std::string Socket::getSocket() const {
	return address_ + ":" + port_;
}

std::string Socket::ToString() const {
	std::ostringstream oss;
	oss << "Socket [" << address_ << ":" << port_
		<< "] (fd " << listening_.fd << ")\n";
	for (const auto& [name, vhost] : v_hosts_)
		oss << "  " << vhost.ToString() << "\n";
	return oss.str();
}
