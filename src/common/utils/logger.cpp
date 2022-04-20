#include "logger.hpp"

const std::string utils::logger::LOG_FILE_NAME = "xlabs.log";
std::ofstream utils::logger::log_file_stream;
std::mutex utils::logger::lock;

void utils::logger::write(std::string msg)
{
	lock.lock();

	if (ensure_is_initialized())
	{
		log_file_stream << msg << "\n";
	}

	lock.unlock();
}

bool utils::logger::ensure_is_initialized()
{
	if (log_file_stream.is_open())
	{
		return true;
	}

	log_file_stream = std::ofstream(LOG_FILE_NAME, std::ios_base::out | std::ios_base::trunc);
	return log_file_stream.is_open();
}
