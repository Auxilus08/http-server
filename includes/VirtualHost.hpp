#pragma once

#include <string>
#include <cstddef>
#include "Types.hpp"
#include "Location.hpp"

class VirtualHost {
	private:
		std::string		name_;
		StringMap		error_pages_;
		LocationMap		locations_;
		size_t			client_max_body_size_;

		static StringMap	defaultErrorPages();

	public:
		VirtualHost(
			const std::string& name,
			const std::string& max_size,
			const StringMap& errors,
			const LocationMap& locations
		);
		~VirtualHost();

		size_t						getMaxBodySize() const;
		std::map<std::string, Location>&	getLocations();
		std::string					getErrorPage(const std::string& error) const;
		std::string					getName() const;
		std::string					ToString() const;
};
