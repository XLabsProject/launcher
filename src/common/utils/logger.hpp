#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>

namespace utils
{
	class logger {

	public:
		static void write(std::string msg);

	private:
		static const std::string LOG_FILE_NAME;

		static bool ensure_is_initialized();

		static std::ofstream log_file_stream;

		static std::mutex lock;
	};
}