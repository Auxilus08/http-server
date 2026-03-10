#include "Location.hpp"
#include <sstream>

Location::Location()
	: methods_("")
	, redirection_("", "")
	, root_("")
	, autoindex_(false)
	, index_("")
	, upload_("") {
}

Location::Location(
	const std::string& methods,
	const StringPair& redirection,
	const std::string& root,
	const std::string& autoindex,
	const std::string& index,
	const std::string& upload)
	: methods_(methods)
	, redirection_(redirection)
	, root_(root)
	, autoindex_(autoindex == "on")
	, index_(index)
	, upload_(upload) {
}

Location::~Location() {
}

std::string Location::ToString() const {
	std::ostringstream oss;
	std::string indent(31, ' ');

	oss << indent << "methods:      " << methods_ << "\n"
		<< indent << "redirection:  " << redirection_.first << " " << redirection_.second << "\n"
		<< indent << "root:         " << root_ << "\n"
		<< indent << "autoindex:    " << (autoindex_ ? "on" : "off") << "\n"
		<< indent << "index:        " << index_ << "\n"
		<< indent << "upload:       " << upload_;

	return oss.str();
}

const std::string& Location::getMethods() const { return methods_; }
const StringPair& Location::getRedirection() const { return redirection_; }
const std::string& Location::getRoot() const { return root_; }
bool Location::getAutoindex() const { return autoindex_; }
const std::string& Location::getIndex() const { return index_; }
const std::string& Location::getUpload() const { return upload_; }
