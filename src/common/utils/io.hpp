#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace utils::io
{
	bool remove_file(const std::filesystem::path& file);
	bool move_file(const std::filesystem::path& src, const std::filesystem::path& target);
	bool file_exists(const std::wstring& file);
	bool write_file(const std::wstring& file, const std::string& data, bool append = false);
	bool read_file(const std::wstring& file, std::string* data);
	std::string read_file(const std::wstring& file);
	size_t file_size(const std::wstring& file);
	bool create_directory(const std::filesystem::path& directory);
	bool directory_exists(const std::filesystem::path& directory);
	bool directory_is_empty(const std::filesystem::path& directory);
	std::vector<std::wstring> list_files(const std::filesystem::path& directory, bool recursive = false);
	void copy_folder(const std::filesystem::path& src, const std::filesystem::path& target);
}
