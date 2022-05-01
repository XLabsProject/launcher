#pragma once

#include <string_view>
#include <format>
#include <source_location>

namespace utils::logger
{
#ifdef _DEBUG
	void log_format(const std::source_location& location, std::string_view fmt, std::format_args&& args);
#else
	void log_format(std::string_view fmt, std::format_args&& args);
#endif

	static inline void log(std::string_view fmt, std::format_args&& args)
	{
#ifdef _DEBUG
		log_format(std::source_location::current(), fmt, std::move(args));
#else
		log_format(fmt, std::move(args));
#endif
	}

	static inline void write(std::string_view fmt)
	{
		log(fmt, std::make_format_args(0));
	}

	template <typename... Args>
	static inline void write(std::string_view fmt, Args&&... args)
	{
		log(fmt, std::make_format_args(args...));
	}
}
