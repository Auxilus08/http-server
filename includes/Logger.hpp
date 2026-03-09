#pragma once

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Logger {

namespace detail {

inline std::string getTimestamp() {
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf;
	localtime_r(&time, &tm_buf);
	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
	return std::string(buf);
}

// ANSI color codes
constexpr const char* RESET  = "\033[0m";
constexpr const char* RED    = "\033[31m";
constexpr const char* BLUE   = "\033[34m";

} // namespace detail

template <typename... Args>
void logInfo(Args&&... args) {
	std::cout << "[" << detail::getTimestamp() << "] [INFO] ";
	(std::cout << ... << std::forward<Args>(args));
	std::cout << detail::RESET << "\n";
}

template <typename... Args>
void logError(Args&&... args) {
	std::cerr << detail::RED << "[" << detail::getTimestamp() << "] [ERROR] ";
	(std::cerr << ... << std::forward<Args>(args));
	std::cerr << detail::RESET << "\n";
}

template <typename... Args>
void logDebug([[maybe_unused]] Args&&... args) {
#ifdef DEBUG
	std::cout << detail::BLUE << "[" << detail::getTimestamp() << "] [DEBUG] ";
	(std::cout << ... << std::forward<Args>(args));
	std::cout << detail::RESET << "\n";
#else
	(void)sizeof...(args);
#endif
}

} // namespace Logger
