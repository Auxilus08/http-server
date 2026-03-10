#pragma once

#include <string>
#include "Types.hpp"

class Location {
	private:
		std::string		methods_;
		StringPair		redirection_;
		std::string		root_;
		bool			autoindex_;
		std::string		index_;
		std::string		upload_;

	public:
		Location();
		Location(
			const std::string& methods,
			const StringPair& redirection,
			const std::string& root,
			const std::string& autoindex,
			const std::string& index,
			const std::string& upload
		);
		~Location();

		std::string ToString() const;

		const std::string&	getMethods() const;
		const StringPair&	getRedirection() const;
		const std::string&	getRoot() const;
		bool				getAutoindex() const;
		const std::string&	getIndex() const;
		const std::string&	getUpload() const;
};
