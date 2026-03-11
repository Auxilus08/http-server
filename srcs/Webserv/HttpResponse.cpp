#include "HttpResponse.hpp"
#include "ClientConnection.hpp"
#include "Logger.hpp"
#include <sys/socket.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cerrno>

// ─── Static tables ───────────────────────────────────────────────────────────

const std::map<std::string, std::string>& HttpResponse::MimeTypes() {
	static const std::map<std::string, std::string> types = {
		{".html",  "text/html"},
		{".htm",   "text/html"},
		{".css",   "text/css"},
		{".js",    "application/javascript"},
		{".mjs",   "application/javascript"},
		{".json",  "application/json"},
		{".xml",   "application/xml"},
		{".txt",   "text/plain"},
		{".csv",   "text/csv"},
		{".jpg",   "image/jpeg"},
		{".jpeg",  "image/jpeg"},
		{".png",   "image/png"},
		{".gif",   "image/gif"},
		{".bmp",   "image/bmp"},
		{".ico",   "image/x-icon"},
		{".svg",   "image/svg+xml"},
		{".webp",  "image/webp"},
		{".avif",  "image/avif"},
		{".mp3",   "audio/mpeg"},
		{".wav",   "audio/wav"},
		{".ogg",   "audio/ogg"},
		{".mp4",   "video/mp4"},
		{".webm",  "video/webm"},
		{".avi",   "video/x-msvideo"},
		{".pdf",   "application/pdf"},
		{".zip",   "application/zip"},
		{".gz",    "application/gzip"},
		{".tar",   "application/x-tar"},
		{".woff",  "font/woff"},
		{".woff2", "font/woff2"},
		{".ttf",   "font/ttf"},
		{".otf",   "font/otf"},
		{".wasm",  "application/wasm"},
		{".md",    "text/markdown"},
		{".yaml",  "text/yaml"},
		{".yml",   "text/yaml"},
	};
	return types;
}

const std::map<std::string, std::string>& HttpResponse::StatusMessages() {
	static const std::map<std::string, std::string> msgs = {
		{"200", "200 OK"},
		{"201", "201 Created"},
		{"204", "204 No Content"},
		{"301", "301 Moved Permanently"},
		{"302", "302 Found"},
		{"303", "303 See Other"},
		{"304", "304 Not Modified"},
		{"307", "307 Temporary Redirect"},
		{"308", "308 Permanent Redirect"},
		{"400", "400 Bad Request"},
		{"403", "403 Forbidden"},
		{"404", "404 Not Found"},
		{"405", "405 Method Not Allowed"},
		{"411", "411 Length Required"},
		{"413", "413 Payload Too Large"},
		{"415", "415 Unsupported Media Type"},
		{"431", "431 Request Header Fields Too Large"},
		{"500", "500 Internal Server Error"},
		{"501", "501 Not Implemented"},
		{"502", "502 Bad Gateway"},
		{"504", "504 Gateway Timeout"},
		{"505", "505 HTTP Version Not Supported"},
	};
	return msgs;
}

// ─── Constructor / Destructor ────────────────────────────────────────────────

HttpResponse::HttpResponse(ClientConnection& client)
	: client_(client)
	, header_sent_(false)
	, body_sent_(false)
	, buffer_length_(0) {
	std::memset(buffer_, 0, sizeof(buffer_));
}

HttpResponse::~HttpResponse() {
}

// ─── AssignContType ──────────────────────────────────────────────────────────

void HttpResponse::AssignContType() {
	const std::string& target = client_.parser_.getTarget();
	size_t dot = target.rfind('.');
	if (dot != std::string::npos) {
		std::string ext = target.substr(dot);
		auto& types = MimeTypes();
		auto it = types.find(ext);
		if (it != types.end()) {
			client_.parser_.getHeaders(); // just for reference
			// Store Content-Type via additional_headers isn't available,
			// so we'll set it in ComposeHeader via the header string directly
			// Actually, let's store it as a member and use it in ComposeHeader
			return; // handled in ComposeHeader
		}
	}
}

// ─── PrepareResponse ─────────────────────────────────────────────────────────

void HttpResponse::PrepareResponse() {
	// Error responses (4xx / 5xx) — no special override needed;
	// if no error page file is open, GetOneChunk() sends a hardcoded body

	// Look up status message
	auto& msgs = StatusMessages();
	auto it = msgs.find(client_.status_);
	if (it != msgs.end())
		status_message_ = it->second;
	else
		status_message_ = client_.status_ + " Unknown";

	// 3xx redirects: no body needed
	if (!client_.status_.empty() && client_.status_[0] == '3') {
		body_sent_ = true;
	}

	ComposeHeader();
}

// ─── ComposeHeader ───────────────────────────────────────────────────────────

void HttpResponse::ComposeHeader() {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << status_message_ << "\r\n";
	oss << "Server: miniserv\r\n";

	// Determine Content-Type
	std::string content_type;
	const std::string& status = client_.status_;

	if (!status.empty() && (status[0] == '4' || status[0] == '5')) {
		content_type = "text/html";
	} else {
		// Look up by file extension
		const std::string& target = client_.parser_.getTarget();
		size_t dot = target.rfind('.');
		if (dot != std::string::npos) {
			std::string ext = target.substr(dot);
			auto& types = MimeTypes();
			auto mit = types.find(ext);
			if (mit != types.end())
				content_type = mit->second;
		}
		if (content_type.empty())
			content_type = "text/html";
	}
	oss << "Content-Type: " << content_type << "\r\n";

	// Add Location header for redirects
	auto& headers = client_.parser_.getHeaders();
	auto loc_it = headers.find("Location");
	if (loc_it != headers.end()) {
		oss << "Location: " << loc_it->second << "\r\n";
	}

	// If error with no file, set Content-Length for hardcoded body
	if (!status.empty() && (status[0] == '4' || status[0] == '5')
		&& !client_.file_.is_open()) {
		std::string fallback = "<h1>" + status_message_ + "</h1>";
		oss << "Content-Length: " << fallback.size() << "\r\n";
	} else if (!body_sent_) {
		// Use chunked transfer for file bodies
		oss << "Transfer-Encoding: chunked\r\n";
	}

	oss << "\r\n";
	header_ = oss.str();
}

// ─── SendResponse (called per poll cycle) ────────────────────────────────────

int HttpResponse::SendResponse(struct pollfd& poll) {
	// Phase 1: flush remaining buffer
	if (buffer_length_ > 0) {
		if (SendBuffer() < 0)
			return 1;
		return 0;
	}

	// Phase 2: send header
	if (!header_sent_) {
		if (SendHeader() < 0)
			return 1;
		return 0;
	}

	// Phase 3: send body
	if (!body_sent_) {
		if (SendBody() < 0)
			return 1;
		return 0;
	}

	// All done — switch back to POLLIN
	if (buffer_length_ == 0 && header_sent_ && body_sent_) {
		poll.events = POLLIN;
	}

	return 0;
}

// ─── SendBuffer ──────────────────────────────────────────────────────────────

int HttpResponse::SendBuffer() {
	ssize_t sent = send(client_.getFd(), buffer_, buffer_length_, MSG_NOSIGNAL);
	if (sent < 0) {
		Logger::logError("send on fd ", client_.getFd(), ": ", strerror(errno));
		return -1;
	}

	if (sent < buffer_length_) {
		// Partial send — shift remaining bytes
		int remaining = buffer_length_ - static_cast<int>(sent);
		std::memmove(buffer_, buffer_ + sent, remaining);
		buffer_length_ = remaining;
	} else {
		buffer_length_ = 0;
	}

	return 0;
}

// ─── SendHeader ──────────────────────────────────────────────────────────────

int HttpResponse::SendHeader() {
	if (header_.empty()) {
		header_sent_ = true;
		return 0;
	}

	size_t to_send = std::min(header_.size(), sizeof(buffer_) - 1);
	ssize_t sent = send(client_.getFd(), header_.c_str(), to_send, MSG_NOSIGNAL);
	if (sent < 0) {
		Logger::logError("send header on fd ", client_.getFd(), ": ", strerror(errno));
		return -1;
	}

	header_.erase(0, static_cast<size_t>(sent));

	if (!header_.empty()) {
		// Partial: buffer the remainder
		size_t rem = std::min(header_.size(), sizeof(buffer_) - 1);
		std::memcpy(buffer_, header_.c_str(), rem);
		buffer_length_ = static_cast<int>(rem);
		header_.erase(0, rem);
	}

	if (header_.empty())
		header_sent_ = true;

	return 0;
}

// ─── SendBody ────────────────────────────────────────────────────────────────

int HttpResponse::SendBody() {
	std::string chunk = GetOneChunk();
	if (chunk.empty())
		return 0;

	ssize_t sent = send(client_.getFd(), chunk.c_str(), chunk.size(), MSG_NOSIGNAL);
	if (sent < 0) {
		Logger::logError("send body on fd ", client_.getFd(), ": ", strerror(errno));
		return -1;
	}

	if (static_cast<size_t>(sent) < chunk.size()) {
		// Buffer the unsent remainder
		size_t rem = chunk.size() - static_cast<size_t>(sent);
		size_t copy_len = std::min(rem, sizeof(buffer_) - 1);
		std::memcpy(buffer_, chunk.c_str() + sent, copy_len);
		buffer_length_ = static_cast<int>(copy_len);
	}

	return 0;
}

// ─── GetOneChunk ─────────────────────────────────────────────────────────────

std::string HttpResponse::GetOneChunk() {
	const std::string& status = client_.status_;

	// Special case: 500 with no file — send hardcoded body (non-chunked)
	if ((status[0] == '4' || status[0] == '5') && !client_.file_.is_open()) {
		body_sent_ = true;
		return "<h1>" + status_message_ + "</h1>";
	}

	// Read from file
	char data[8000];
	client_.file_.read(data, sizeof(data));
	std::streamsize bytes_read = client_.file_.gcount();

	if (bytes_read == 0) {
		// Terminal chunk
		body_sent_ = true;
		return "0\r\n\r\n";
	}

	// Format as chunked: hex_size\r\ndata\r\n
	std::ostringstream oss;
	oss << std::hex << bytes_read << "\r\n";
	oss.write(data, bytes_read);
	oss << "\r\n";

	// Check if this was the last read
	if (client_.file_.eof()) {
		// Next call will produce the terminal chunk
	}

	return oss.str();
}

// ─── ResetResponse ───────────────────────────────────────────────────────────

void HttpResponse::ResetResponse() {
	header_.clear();
	status_message_.clear();
	header_sent_ = false;
	body_sent_ = false;
	std::memset(buffer_, 0, sizeof(buffer_));
	buffer_length_ = 0;
}
