#pragma once

#include <format>
#include <source_location>

namespace utils::logger
{
#ifdef _DEBUG
	void log_format(const std::source_location& location, std::string_view fmt, std::format_args&& args);
#else
	void log_format(std::string_view fmt, std::format_args&& args);
#endif

	template <typename... Args>
	class write
	{
	public:
		write(std::string_view fmt, const Args&... args, [[maybe_unused]] const std::source_location& loc = std::source_location::current())
		{
#ifdef _DEBUG
			log_format(loc, fmt, std::make_format_args(args...));
#else
			log_format(fmt, std::make_format_args(args...));
#endif
		}
	};

	template <typename... Args>
	write(std::string_view fmt, const Args&... args) -> write<Args...>;
}
