#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>

class ClientConnection; // Forward declaration

class HttpParser {
	private:
		ClientConnection&						client_;
		std::string								method_;
		std::string								request_target_;
		std::string								query_string_;
		std::map<std::string, std::string>		headers_;
		std::vector<char>						request_body_;
		size_t									content_length_;
		bool									is_chunked_;
		std::string								content_type_;
		std::string								index_;
		std::string								uploads_;
		std::string								file_list_;
		std::string								session_id_;
		std::map<std::string, std::string>		session_store_;

		bool		ParseStartLine(std::istringstream& stream);
		bool		ParseHeaderFields(std::istringstream& stream);
		bool		CheckPostHeaders(std::istringstream& stream);
		std::string	DecodeUrlPath(const std::string& path);
		static int	HexToInt(char c);
		bool		HandleGet(bool autoIndex);

	public:
		HttpParser(ClientConnection& client);
		~HttpParser();

		bool ParseHeader(const std::string& request);
		bool HandleRequest();
		void ResetParser();

		const std::string&							getMethod() const;
		const std::string&							getTarget() const;
		const std::string&							getQueryString() const;
		const std::map<std::string, std::string>&	getHeaders() const;
		const std::vector<char>&					getBody() const;
		size_t										getContentLength() const;
		const std::string&							getContentType() const;
		const std::map<std::string, std::string>&	getSessionStore() const;
		std::string									getHost() const;
};
