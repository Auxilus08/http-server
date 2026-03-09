#include "VirtualHost.hpp"
#include <sstream>
#include <unistd.h>

// ─── Static default error pages ──────────────────────────────────────────────

StringMap VirtualHost::defaultErrorPages() {
	StringMap defaults;
	const std::string codes[] = {
		"400", "403", "404", "405", "411",
		"413", "415", "431", "500", "501",
		"502", "504", "505"
	};
	for (const auto& code : codes)
		defaults[code] = "www/error_pages/" + code + ".html";
	return defaults;
}

// ─── Constructor ─────────────────────────────────────────────────────────────

VirtualHost::VirtualHost(
	const std::string& name,
	const std::string& max_size,
	const StringMap& errors,
	const LocationMap& locations)
	: name_(name)
	, error_pages_(defaultErrorPages())
	, locations_(locations)
	, client_max_body_size_(1048576) {

	// Parse max body size (e.g., "150M", "500K", "0")
	if (!max_size.empty()) {
		char suffix = max_size.back();
		if (suffix == 'M' || suffix == 'm') {
			std::string num = max_size.substr(0, max_size.size() - 1);
			client_max_body_size_ = static_cast<size_t>(std::stoull(num)) * 1048576;
		} else if (suffix == 'K' || suffix == 'k') {
			std::string num = max_size.substr(0, max_size.size() - 1);
			client_max_body_size_ = static_cast<size_t>(std::stoull(num)) * 1024;
		} else {
			client_max_body_size_ = static_cast<size_t>(std::stoull(max_size));
		}
	}

	// Override defaults with custom error pages (only if file is readable)
	for (const auto& [code, path] : errors) {
		if (access(path.c_str(), R_OK) == 0)
			error_pages_[code] = path;
	}
}

VirtualHost::~VirtualHost() {
}

// ─── Getters ─────────────────────────────────────────────────────────────────

size_t VirtualHost::getMaxBodySize() const {
	return client_max_body_size_;
}

std::map<std::string, Location>& VirtualHost::getLocations() {
	return locations_;
}

std::string VirtualHost::getErrorPage(const std::string& error) const {
	auto it = error_pages_.find(error);
	if (it != error_pages_.end())
		return it->second;
	return "";
}

std::string VirtualHost::getName() const {
	return name_;
}

// ─── ToString ────────────────────────────────────────────────────────────────

std::string VirtualHost::ToString() const {
	std::ostringstream oss;

	oss << "VirtualHost: " << name_ << "\n"
		<< "  client_max_body_size: " << client_max_body_size_ << " bytes\n"
		<< "  error_pages:\n";

	for (const auto& [code, path] : error_pages_)
		oss << "    " << code << " -> " << path << "\n";

	oss << "  locations:\n";
	for (const auto& [path, loc] : locations_)
		oss << "    " << path << ":\n" << loc.ToString() << "\n";

	return oss.str();
}
