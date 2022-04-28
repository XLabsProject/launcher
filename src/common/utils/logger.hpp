#pragma once

#include <string>
#include <string_view>
#include <format>
#include <source_location>
#include <fstream>
#include <mutex>

namespace utils
{
	class logger
	{
	public:
		static inline void write(std::string_view fmt)
		{
			log(fmt, std::make_format_args(0));
		}

		template <typename... Args>
		static inline void write(std::string_view fmt, Args&&... args)
		{
			log(fmt, std::make_format_args(args...));
		}

		static inline void log(std::string_view fmt, std::format_args&& args)
		{
#ifdef _DEBUG
			log_format(std::source_location::current(), fmt, std::move(args));
#else
			log_format(fmt, std::move(args));
#endif
		}

	private:
		static const std::string log_file_name;

#ifdef _DEBUG
		static void log_format(const std::source_location& location, std::string_view fmt, std::format_args&& args);
#else
		static void log_format(std::string_view fmt, std::format_args&& args);
#endif

		static void write_to_log(const std::string& line);

		static bool ensure_is_initialized();

		static std::mutex logger_mutex;
		static std::ofstream log_file_stream;
	};
}
