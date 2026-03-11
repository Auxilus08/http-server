#pragma once

#include "Connection.hpp"
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "Socket.hpp"
#include <string>
#include <fstream>
#include <unistd.h>

class WebServ; // Forward declaration

class ClientConnection : public Connection {
	public:
		enum class Stage { kHeader, kBody, kCgi, kResponse, kSending, kDrain };

		Stage			stage_;
		std::string		status_;
		Socket&			sock_;
		WebServ&		webserv_;
		HttpParser		parser_;
		HttpResponse	response_;
		std::fstream	file_;
		VirtualHost*	vhost_;
		bool			drain_incoming_;
		size_t			drained_bytes_;

		ClientConnection(int fd, Socket& sock, WebServ& webserv);
		~ClientConnection() override;

		int ReceiveData(struct pollfd& poll) override;
		int SendData(struct pollfd& poll) override;
		void ResetClientConnection();

	private:
		std::string		recv_buffer_;
};
