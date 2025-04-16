#pragma once

#include <array>
#include <string_view>
#include <string>

constexpr int MAX_LOGS{256};
constexpr int MAX_LOG_LENGTH{256};

enum struct log_severity {
	Info,
	Warning,
	Error,
	Fatal
};

/**
 * @brief Error storage that is a circular buffer to hold all errors from the past
 * and overwrites old errors upon too many errors
 */
template<log_severity min_log_level = log_severity::Info>
struct log_storage {
	static log_storage& Default() {
		static log_storage<> storage{};
		return storage;
	}

	log_storage() = default;

	struct entry {
		log_severity severity;
		std::array<char, MAX_LOG_LENGTH> message;
	};
	std::array<entry, MAX_LOGS> logs{};
	int write_idx{};
	
	constexpr void push(log_severity severity, std::string_view message) noexcept {
		if (severity < min_log_level)
			return;
		if (message.size() >= MAX_LOG_LENGTH)
			message = message.substr(0, MAX_LOG_LENGTH - 2);
		logs[write_idx].severity = severity;
		std::copy(message.begin(), message.end(), logs[write_idx].message.data());
		logs[write_idx].message[message.size()] = '\0';
		write_idx = (write_idx + 1) % MAX_LOGS;
	}
	std::string print_errors() const noexcept {
		std::string res;
		int i{};
		for (const auto &[sev, message]: logs) {
			if (i++ == write_idx) 
				res += "next_write: ";
			switch(sev) {
			case log_severity::Info:
				res += "[Info   ]: ";
				break;
			case log_severity::Warning:
				res += "[Warning]: ";
				break;
			case log_severity::Error:
				res += "[Error  ]: ";
				break;
			case log_severity::Fatal:
				res += "[Fatal  ]: ";
				break;
			}
			res += message;
			res += '\n';
		}
		return res;
	}
};

constexpr void LogInfo(std::string_view message) { log_storage<>::Default().push(log_severity::Info, message); }
constexpr void LogWarning(std::string_view message) { log_storage<>::Default().push(log_severity::Warning, message); }
constexpr void LogError(std::string_view message) { log_storage<>::Default().push(log_severity::Error, message); }
constexpr void LogFatal(std::string_view message) { log_storage<>::Default().push(log_severity::Fatal, message); }

