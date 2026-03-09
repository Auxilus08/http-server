#include "ConfigParser.hpp"
#include "Logger.hpp"
#include <regex>

// ─── Constructor / Destructor ────────────────────────────────────────────────

ConfigParser::ConfigParser(const std::string& conf) : _file(conf) {
}

ConfigParser::~ConfigParser() {
	if (_file.is_open())
		_file.close();
}

// ─── Main Parse Entry Point ──────────────────────────────────────────────────

int ConfigParser::ParseConfig(std::deque<Socket>& sockets) {
	if (!_file.is_open()) {
		Logger::logError("Failed to open configuration file");
		return 1;
	}

	// Read entire file, stripping # comments
	std::stringstream ss;
	std::string line;
	while (std::getline(_file, line)) {
		size_t hash = line.find('#');
		if (hash != std::string::npos)
			line = line.substr(0, hash);
		ss << line << "\n";
	}

	std::string token;
	int server_count = 0;
	while (ss >> token) {
		if (token == "server") {
			try {
				ParseServer(ss, sockets);
				server_count++;
			} catch (std::string& e) {
				Logger::logError("Invalid token: ", e);
				return 1;
			} catch (const char* e) {
				Logger::logError("Parse error: ", e);
				return 1;
			}
		} else {
			Logger::logError("Unexpected token outside server block: ", token);
			return 1;
		}
	}

	if (server_count == 0) {
		Logger::logError("No server blocks found in configuration");
		return 1;
	}

	return 0;
}

// ─── ParseServer ─────────────────────────────────────────────────────────────

void ConfigParser::ParseServer(std::stringstream& ss, std::deque<Socket>& sockets) {
	std::string token;
	if (!(ss >> token) || token != "{")
		throw "Expected '{' after 'server'";

	std::string listen;
	std::string server_name;
	std::string max_body_size;
	StringMap error_pages;
	LocationMap locations;

	while (ss >> token) {
		if (token == "}") {
			break;
		} else if (token == "listen") {
			listen = ParseListen(ss);
		} else if (token == "server_name") {
			server_name = ParseServerName(ss);
		} else if (token == "client_max_body_size") {
			max_body_size = ParseMaxBodySize(ss);
		} else if (token == "error_page") {
			auto [code, path] = ParseErrorPage(ss);
			error_pages[code] = path;
		} else if (token == "location") {
			auto [path, loc] = ParseLocation(ss);
			locations[path] = loc;
		} else {
			throw token;
		}
	}

	if (listen.empty())
		throw "Server block missing 'listen' directive";
	if (locations.empty())
		throw "Server block missing location blocks";

	// Check if a socket with this listen address already exists
	for (auto& socket : sockets) {
		if (socket.getSocket() == listen) {
			socket.AddVirtualHost(server_name, max_body_size, error_pages, locations);
			return;
		}
	}

	sockets.emplace_back(listen, server_name, max_body_size, error_pages, locations);
}

// ─── Directive Parsers ───────────────────────────────────────────────────────

std::string ConfigParser::ParseListen(std::stringstream& ss) {
	std::string token;
	if (!(ss >> token))
		throw "Expected value after 'listen'";

	static const std::regex re(
		"((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\.){3}"
		"(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9]):"
		"(6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|"
		"[1-5][0-9]{4}|[1-9][0-9]{0,3}|0);");

	if (!std::regex_match(token, re))
		throw "Invalid listen value";

	token.pop_back();
	return token;
}

std::string ConfigParser::ParseServerName(std::stringstream& ss) {
	std::string token;
	if (!(ss >> token))
		throw "Expected value after 'server_name'";

	static const std::regex re("([a-z0-9-]{1,63}\\.){1,124}com;");

	if (!std::regex_match(token, re))
		throw "Invalid server_name value";

	token.pop_back();
	if (token.length() > 253)
		throw "server_name exceeds 253 characters";

	return token;
}

std::string ConfigParser::ParseMaxBodySize(std::stringstream& ss) {
	std::string token;
	if (!(ss >> token))
		throw "Expected value after 'client_max_body_size'";

	static const std::regex re(
		"0;|((0|[1-9][0-9]{0,5}|1000000)K;)|((0|[1-9][0-9]{0,2}|1000)M;)");

	if (!std::regex_match(token, re))
		throw "Invalid client_max_body_size value";

	token.pop_back();
	return token;
}

std::pair<std::string, std::string> ConfigParser::ParseErrorPage(std::stringstream& ss) {
	std::string code, path;
	if (!(ss >> code))
		throw "Expected error code after 'error_page'";
	if (!(ss >> path))
		throw "Expected file path after error code";

	static const std::regex code_re(
		"(400|403|404|405|411|413|415|431|500|501|502|504|505)");
	static const std::regex path_re("/[a-zA-Z0-9_/.-]+\\.html;");

	if (!std::regex_match(code, code_re))
		throw "Invalid error_page status code";
	if (!std::regex_match(path, path_re))
		throw "Invalid error_page file path";

	path.pop_back();
	return {code, path};
}

std::pair<std::string, Location> ConfigParser::ParseLocation(std::stringstream& ss) {
	std::string path;
	if (!(ss >> path))
		throw "Expected path after 'location'";

	static const std::regex path_re("/[a-zA-Z0-9_-]*");
	if (!std::regex_match(path, path_re))
		throw "Invalid location path";

	std::string token;
	if (!(ss >> token) || token != "{")
		throw "Expected '{' after location path";

	std::string methods;
	StringPair redirection("", "");
	std::string root;
	std::string autoindex = "off";
	std::string index;
	std::string upload;

	bool reprocess = false;
	while (reprocess || (ss >> token)) {
		reprocess = false;

		if (token == "}") {
			break;
		} else if (token == "limit_except") {
			static const std::regex method_re("(GET|POST|DELETE|PUT|HEAD)");
			while (ss >> token) {
				if (std::regex_match(token, method_re)) {
					methods += " " + token;
				} else {
					reprocess = true;
					break;
				}
			}
			if (!methods.empty())
				methods += " ";
		} else if (token == "return") {
			std::string code, url;
			if (!(ss >> code))
				throw "Expected status code after 'return'";
			if (!(ss >> url))
				throw "Expected URL after return status code";

			static const std::regex ret_code_re("[1-5][0-9]{2}");
			if (!std::regex_match(code, ret_code_re))
				throw "Invalid return status code";
			if (url.back() != ';')
				throw "Expected ';' after return URL";
			url.pop_back();
			redirection = {code, url};
		} else if (token == "root") {
			if (!(ss >> token))
				throw "Expected path after 'root'";
			static const std::regex root_re("/[a-zA-Z0-9_/.-]*;");
			if (!std::regex_match(token, root_re))
				throw "Invalid root path";
			token.pop_back();
			root = token;
		} else if (token == "autoindex") {
			if (!(ss >> token))
				throw "Expected value after 'autoindex'";
			static const std::regex ai_re("(on|off);");
			if (!std::regex_match(token, ai_re))
				throw "Invalid autoindex value";
			token.pop_back();
			autoindex = token;
		} else if (token == "index") {
			if (!(ss >> token))
				throw "Expected filename after 'index'";
			static const std::regex idx_re("[a-zA-Z0-9_.-]+;");
			if (!std::regex_match(token, idx_re))
				throw "Invalid index filename";
			token.pop_back();
			index = token;
		} else if (token == "upload") {
			if (!(ss >> token))
				throw "Expected path after 'upload'";
			static const std::regex upl_re("/[a-zA-Z0-9_/.-]*;");
			if (!std::regex_match(token, upl_re))
				throw "Invalid upload path";
			token.pop_back();
			upload = token;
		} else {
			throw token;
		}
	}

	if (root.empty())
		throw "Location block missing required 'root' directive";

	return {path, Location(methods, redirection, root, autoindex, index, upload)};
}
