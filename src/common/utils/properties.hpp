#pragma once

#include <mutex>
#include <optional>

#include "named_mutex.hpp"

namespace utils
{
	class Properties
	{
	public:
		Properties();
		explicit Properties(std::string filePath);

		static std::unique_lock<named_mutex> Lock();

		std::optional<std::string> Load(const std::string& name) const;
		void Store(const std::string& name, const std::string& value) const;

	private:
		std::string file_path;
	};

}
