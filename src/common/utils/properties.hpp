#pragma once

#include <mutex>
#include <optional>

#include "named_mutex.hpp"

namespace utils
{
	class properties
	{
	public:
		properties();
		explicit properties(std::string filePath);

		static std::unique_lock<named_mutex> lock();

		std::optional<std::string> load(const std::string& name) const;
		void store(const std::string& name, const std::string& value) const;

	private:
		std::string file_path;
	};

}
