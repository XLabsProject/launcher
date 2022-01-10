#pragma once

#include "progress_listener.hpp"

namespace updater
{
	class file_updater
	{
	public:
		file_updater(progress_listener& listener, std::string base, std::string process_file);

		void run() const;

		std::vector<file_info> get_outdated_files(const std::vector<file_info>& files) const;

		void update_host_binary(const std::vector<file_info>& outdated_files) const;

		void update_iw4x_if_necessary(std::filesystem::path iw4x_basegame_directory) const;
		void update_files(const std::vector<file_info>& outdated_files) const;

	private:
		progress_listener& listener_;

		std::string base_;
		std::string process_file_;
		std::string dead_process_file_;

		void update_file(const file_info& file) const;

		bool is_outdated_file(const file_info& file) const;
		std::string get_drive_filename(const file_info& file) const;

		void move_current_process_file() const;
		void restore_current_process_file() const;
		void delete_old_process_file() const;

		// IW4X-specific
		void create_iw4x_version_file(std::filesystem::path iw4x_basegame_directory, std::string rawfile_version, std::string iw4x_version) const;
		std::optional<std::string> get_release_tag(std::string release_url) const;
		bool does_iw4x_require_update(std::filesystem::path iw4x_basegame_directory, bool& out_requires_rawfile_update, bool& out_requires_iw4x_update) const;

		void cleanup_directories(const std::vector<file_info>& files) const;
		void cleanup_root_directory() const;
		void cleanup_data_directory(const std::vector<file_info>& files) const;
	};
}
