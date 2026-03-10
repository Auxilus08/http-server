#include "HttpParser.hpp"
#include "ClientConnection.hpp"
#include "Logger.hpp"
#include <filesystem>

// ─── Constructor / Destructor ────────────────────────────────────────────────

HttpParser::HttpParser(ClientConnection& client)
	: client_(client)
	, content_length_(0)
	, is_chunked_(false) {
}

HttpParser::~HttpParser() {
}

// ─── Main Parse Entry ────────────────────────────────────────────────────────

bool HttpParser::ParseHeader(const std::string& request) {
	std::istringstream stream(request);

	if (!ParseStartLine(stream))
		return false;

	if (!ParseHeaderFields(stream))
		return false;

	if (method_ == "POST") {
		if (!CheckPostHeaders(stream))
			return false;
	} else {
		// GET/DELETE: reject any body data
		std::string remaining;
		remaining.assign(std::istreambuf_iterator<char>(stream),
						 std::istreambuf_iterator<char>());
		if (!remaining.empty()) {
			client_.status_ = "400";
			return false;
		}
	}

	return true;
}

// ─── ParseStartLine ─────────────────────────────────────────────────────────

bool HttpParser::ParseStartLine(std::istringstream& stream) {
	std::string line;
	if (!std::getline(stream, line)) {
		client_.status_ = "400";
		return false;
	}

	if (!line.empty() && line.back() == '\r')
		line.pop_back();

	std::istringstream linestream(line);
	std::string method, target, version;
	linestream >> method >> target >> version;

	if (method != "GET" && method != "POST" && method != "DELETE") {
		client_.status_ = "501";
		return false;
	}
	method_ = method;

	if (target.empty() || target[0] != '/') {
		client_.status_ = "400";
		return false;
	}

	if (version != "HTTP/1.1") {
		client_.status_ = "505";
		return false;
	}

	size_t qpos = target.find('?');
	if (qpos != std::string::npos) {
		query_string_ = target.substr(qpos + 1);
		target = target.substr(0, qpos);
	}

	request_target_ = DecodeUrlPath(target);
	if (client_.status_ != "200")
		return false;

	return true;
}

// ─── URL Decoding ────────────────────────────────────────────────────────────

int HttpParser::HexToInt(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

std::string HttpParser::DecodeUrlPath(const std::string& path) {
	std::string decoded;
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '%' && i + 2 < path.size()) {
			int hi = HexToInt(path[i + 1]);
			int lo = HexToInt(path[i + 2]);
			if (hi < 0 || lo < 0) {
				client_.status_ = "400";
				return "";
			}
			char c = static_cast<char>(hi * 16 + lo);
			if (c == '\0' || c == '\r' || c == '\n') {
				client_.status_ = "400";
				return "";
			}
			decoded += c;
			i += 2;
		} else {
			decoded += path[i];
		}
	}
	return decoded;
}

// ─── ParseHeaderFields ──────────────────────────────────────────────────────

bool HttpParser::ParseHeaderFields(std::istringstream& stream) {
	std::string line;
	bool found_empty = false;
	bool bad_request = false;

	while (std::getline(stream, line)) {
		if (line == "\r" || line.empty()) {
			found_empty = true;
			break;
		}

		if (line.back() != '\r') {
			bad_request = true;
			continue;
		}
		line.pop_back();

		size_t colon = line.find(':');
		if (colon == std::string::npos) {
			bad_request = true;
			continue;
		}

		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		size_t start = value.find_first_not_of(" \t");
		if (start != std::string::npos)
			value = value.substr(start);
		else
			value = "";

		headers_[key] = value;
	}

	if (!found_empty) {
		client_.status_ = "431";
		return false;
	}

	if (bad_request) {
		client_.status_ = "400";
		return false;
	}

	if (headers_.find("Host") == headers_.end()) {
		client_.status_ = "400";
		return false;
	}

	auto cookie_it = headers_.find("Cookie");
	if (cookie_it != headers_.end()) {
		std::istringstream cookie_stream(cookie_it->second);
		std::string pair;
		while (std::getline(cookie_stream, pair, ';')) {
			size_t start = pair.find_first_not_of(" \t");
			if (start == std::string::npos)
				continue;
			pair = pair.substr(start);

			size_t eq = pair.find('=');
			if (eq != std::string::npos) {
				std::string key = pair.substr(0, eq);
				std::string val = pair.substr(eq + 1);
				size_t ks = key.find_first_not_of(" \t");
				size_t ke = key.find_last_not_of(" \t");
				if (ks != std::string::npos)
					key = key.substr(ks, ke - ks + 1);
				size_t vs = val.find_first_not_of(" \t");
				size_t ve = val.find_last_not_of(" \t");
				if (vs != std::string::npos)
					val = val.substr(vs, ve - vs + 1);
				session_store_[key] = val;
			}
		}
	}

	return true;
}

// ─── CheckPostHeaders ───────────────────────────────────────────────────────

bool HttpParser::CheckPostHeaders(std::istringstream& stream) {
	auto ct_it = headers_.find("Content-Type");
	if (ct_it == headers_.end()) {
		client_.status_ = "400";
		return false;
	}
	content_type_ = ct_it->second;

	auto te_it = headers_.find("Transfer-Encoding");
	if (te_it != headers_.end()
		&& te_it->second.find("chunked") != std::string::npos) {
		is_chunked_ = true;
	}

	if (!is_chunked_) {
		auto cl_it = headers_.find("Content-Length");
		if (cl_it == headers_.end()) {
			client_.status_ = "411";
			return false;
		}
		try {
			content_length_ = std::stoull(cl_it->second);
		} catch (const std::invalid_argument&) {
			client_.status_ = "400";
			return false;
		} catch (const std::out_of_range&) {
			client_.status_ = "413";
			return false;
		}
	}

	std::string remaining;
	remaining.assign(std::istreambuf_iterator<char>(stream),
					 std::istreambuf_iterator<char>());
	request_body_.assign(remaining.begin(), remaining.end());

	return true;
}

// ─── HandleRequest ──────────────────────────────────────────────────────────

bool HttpParser::HandleRequest() {
	auto& locations = client_.vhost_->getLocations();

	// FindLocation: longest prefix match
	std::string best_match;
	Location* best_loc = nullptr;
	for (auto& [path, loc] : locations) {
		if (request_target_.compare(0, path.size(), path) == 0) {
			if (path.size() > best_match.size()) {
				best_match = path;
				best_loc = &loc;
			}
		}
	}

	if (!best_loc) {
		client_.status_ = "404";
		return false;
	}

	// Check method is allowed (empty methods_ = all allowed)
	const std::string& methods = best_loc->getMethods();
	if (!methods.empty()
		&& methods.find(" " + method_ + " ") == std::string::npos) {
		client_.status_ = "405";
		return false;
	}

	// Check for redirect
	const auto& redir = best_loc->getRedirection();
	if (!redir.first.empty()) {
		client_.status_ = redir.first;
		headers_["Location"] = redir.second;
		client_.stage_ = ClientConnection::Stage::kResponse;
		return true;
	}

	// Rewrite path: root + suffix after location match
	std::string suffix = request_target_.substr(best_match.size());
	std::string root = best_loc->getRoot();
	if (!root.empty() && root.back() != '/'
		&& !suffix.empty() && suffix.front() != '/')
		request_target_ = root + "/" + suffix;
	else
		request_target_ = root + suffix;

	// Store location settings for later use
	index_ = best_loc->getIndex();
	uploads_ = best_loc->getUpload();

	if (method_ == "GET")
		return HandleGet(best_loc->getAutoindex());

	// POST / DELETE handling in future steps
	return true;
}

// ─── HandleGet ──────────────────────────────────────────────────────────────

bool HttpParser::HandleGet(bool autoIndex) {
	namespace fs = std::filesystem;

	if (!fs::exists(request_target_)) {
		client_.status_ = "404";
		return false;
	}

	if (fs::is_directory(request_target_)) {
		// Ensure trailing slash
		std::string target = request_target_;
		if (target.back() != '/')
			target += '/';

		// Try index file
		if (!index_.empty()) {
			std::string index_path = target + index_;
			if (fs::exists(index_path) && fs::is_regular_file(index_path)) {
				request_target_ = index_path;
			} else if (autoIndex) {
				// Directory listing — will implement later
				Logger::logInfo("Autoindex for: ", request_target_);
				client_.stage_ = ClientConnection::Stage::kResponse;
				return true;
			} else {
				client_.status_ = "404";
				return false;
			}
		} else if (autoIndex) {
			Logger::logInfo("Autoindex for: ", request_target_);
			client_.stage_ = ClientConnection::Stage::kResponse;
			return true;
		} else {
			client_.status_ = "404";
			return false;
		}
	}

	// Open the file
	client_.file_.open(request_target_, std::ios::in | std::ios::binary);
	if (!client_.file_.is_open()) {
		client_.status_ = "500";
		return false;
	}

	Logger::logInfo("Opened file: ", request_target_);
	client_.stage_ = ClientConnection::Stage::kResponse;
	return true;
}

// ─── ResetParser ─────────────────────────────────────────────────────────────

void HttpParser::ResetParser() {
	method_.clear();
	request_target_.clear();
	query_string_.clear();
	headers_.clear();
	request_body_.clear();
	content_length_ = 0;
	is_chunked_ = false;
	content_type_.clear();
	index_.clear();
	uploads_.clear();
	file_list_.clear();
	session_id_.clear();
	session_store_.clear();
	client_.status_ = "200";
}

// ─── Getters ─────────────────────────────────────────────────────────────────

const std::string& HttpParser::getMethod() const { return method_; }
const std::string& HttpParser::getTarget() const { return request_target_; }
const std::string& HttpParser::getQueryString() const { return query_string_; }
const std::map<std::string, std::string>& HttpParser::getHeaders() const { return headers_; }
const std::vector<char>& HttpParser::getBody() const { return request_body_; }
size_t HttpParser::getContentLength() const { return content_length_; }
const std::string& HttpParser::getContentType() const { return content_type_; }
const std::map<std::string, std::string>& HttpParser::getSessionStore() const { return session_store_; }

std::string HttpParser::getHost() const {
	auto it = headers_.find("Host");
	if (it != headers_.end())
		return it->second;
	return "";
}
