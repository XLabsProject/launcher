#pragma once
#include <filesystem>


namespace utils::zip
{
	bool unzip(const std::filesystem::path& file, const std::filesystem::path& into);
}
