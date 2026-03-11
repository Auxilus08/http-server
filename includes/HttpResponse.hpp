#pragma once

#include <string>
#include <map>
#include <poll.h>

class ClientConnection; // Forward declaration

class HttpResponse {
	private:
		ClientConnection&	client_;
		std::string			header_;
		std::string			status_message_;
		bool				header_sent_;
		bool				body_sent_;
		char				buffer_[8192];
		int					buffer_length_;

		void	AssignContType();
		void	ComposeHeader();
		int		SendBuffer();
		int		SendHeader();
		int		SendBody();
		std::string	GetOneChunk();

		static const std::map<std::string, std::string>&	MimeTypes();
		static const std::map<std::string, std::string>&	StatusMessages();

	public:
		HttpResponse(ClientConnection& client);
		~HttpResponse();

		void	PrepareResponse();
		int		SendResponse(struct pollfd& poll);
		void	ResetResponse();
};
