#pragma once

#include <print>
#include <iostream>
#include "static_types.h"

constexpr int MAX_LOGS{64}; // with 128 the output buffer gets overfull, maybe solve by flush inbetween
constexpr int MAX_LOG_LENGTH{64};

enum struct log_severity {
	Info,
	Warning,
	Error,
	Fatal,
};

/**
 * @brief Error storage that is a circular buffer to hold all errors from the past
 * and overwrites old errors upon too many errors
 */
struct log_storage {
	static log_storage& Default();
	struct log_entry{
		log_severity severity{log_severity::Info};
		static_string<MAX_LOG_LENGTH> message{};
	};

	static_ring_buffer<log_entry, MAX_LOGS> logs{};
	log_severity cur_severity{log_severity::Info};
	
	constexpr log_entry* push(log_severity severity, std::string_view static_message = {}) noexcept {
		if (severity < cur_severity)
			return {};
		log_entry *entry = logs.push();
		if (!entry)
			return {};
		entry->severity = severity;
		if (!static_message.empty())
			entry->message.fill(static_message);
		return entry;
	}
	template<int N>
	int print_errors(static_string<N> &dst) const noexcept {
		int s{};
		for (const auto &[sev, message]: logs) {
			switch(sev) {
			case log_severity::Info:
				s += dst.append_formatted("[Info   ]: {}\n", message.sv());
				break;
			case log_severity::Warning:
				s += dst.append_formatted("[Warning]: {}\n", message.sv());
				break;
			case log_severity::Error:
				s += dst.append_formatted("[Error  ]: {}\n", message.sv());
				break;
			case log_severity::Fatal:
				s += dst.append_formatted("[Fatal  ]: {}\n", message.sv());
				break;
			}
		}
		return s;
	}
};

// ---------------------------------------------------------------------------------------
// Formatted logging
// ---------------------------------------------------------------------------------------
template<typename... Args>
inline void LogInfo(std::format_string<Args...> fmt, Args&&... args) { 
	auto *entry = log_storage::Default().push(log_severity::Info); 
	if (entry) {
		entry->message.fill_formatted(fmt, std::forward<Args>(args)...);
		//std::println("[Info   ]: {}", entry->message.view);
	}
}
template<typename... Args>
inline void LogWarning(std::format_string<Args...> fmt, Args&&... args) { 
	auto *entry = log_storage::Default().push(log_severity::Warning); 
	if (entry) {
		entry->message.fill_formatted(fmt, std::forward<Args>(args)...);
		//std::println("[Warning]: {}", entry->message.view);
	}
}
template<typename... Args>
inline void LogError(std::format_string<Args...> fmt, Args&&... args) { 
	auto *entry = log_storage::Default().push(log_severity::Error); 
	if (entry) {
		entry->message.fill_formatted(fmt, std::forward<Args>(args)...);
		//std::println("[Error  ]: {}", entry->message.view);
	}
}
template<typename... Args>
inline void LogFatal(std::format_string<Args...> fmt, Args&&... args) { 
	auto *entry = log_storage::Default().push(log_severity::Fatal); 
	if (entry) {
		entry->message.fill_formatted(fmt, std::forward<Args>(args)...);
		//std::println("[Fatal  ]: {}", entry->message.view);
	}
}

// ---------------------------------------------------------------------------------------
// Static string logging
// ---------------------------------------------------------------------------------------
inline void LogInfo(std::string_view message) { log_storage::Default().push(log_severity::Info, message) /* std::println("[Info   ]: {}", message) */;}
inline void LogWarning(std::string_view message) { log_storage::Default().push(log_severity::Warning, message) /* std::println("[Warning]: {}", message)*/;}
inline void LogError(std::string_view message) { log_storage::Default().push(log_severity::Error, message) /* std::println("[Error  ]: {}", message)*/;}
inline void LogFatal(std::string_view message) { log_storage::Default().push(log_severity::Fatal, message) /* std::println("[Fatal  ]: {}", message)*/;}

