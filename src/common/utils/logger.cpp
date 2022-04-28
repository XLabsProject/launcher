#include "logger.hpp"

namespace utils
{
	const std::string logger::log_file_name = "xlabs.log";
	std::mutex logger::logger_mutex;
	std::ofstream logger::log_file_stream;

#ifdef _DEBUG
	void logger::log_format(const std::source_location& location, std::string_view fmt, std::format_args&& args)
#else
	void logger::log_format(std::string_view fmt, std::format_args&& args)
#endif
	{
#ifdef _DEBUG
		const auto loc_info = std::format("{}::{} ", location.file_name(), location.function_name());
		const auto line = loc_info + std::vformat(fmt, args);
#else
		const auto line = std::vformat(fmt, args);
#endif

		write_to_log(line);
	}

	void logger::write_to_log(const std::string& line)
	{
		std::unique_lock<std::mutex> _(logger_mutex);

		try
		{
			if (ensure_is_initialized())
			{
				log_file_stream << line << std::endl;
			}
		}
		catch (const std::exception&)
		{
		}
	}

	bool logger::ensure_is_initialized()
	{
		if (log_file_stream.is_open())
		{
			return true;
		}

		log_file_stream = std::ofstream(log_file_name, std::ios_base::out | std::ios_base::trunc);
		return log_file_stream.is_open();
	}
}
