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

		void update_iw4x_if_necessary() const;
		void update_files(const std::vector<file_info>& outdated_files, bool iw4x_files = false) const;

	private:

		struct iw4x_update_state {
			bool rawfile_requires_update = false;
			bool library_requires_update = false;
			std::string rawfile_latest_tag;
			std::string library_latest_tag;
		};

		progress_listener& listener_;

		std::string base_;
		std::string process_file_;
		std::string dead_process_file_;

		void update_file(const file_info& file, bool iw4x_files = false) const;

		bool is_outdated_file(const file_info& file) const;
		std::string get_drive_filename(const file_info& file) const;

		void move_current_process_file() const;
		void restore_current_process_file() const;
		void delete_old_process_file() const;

		// IW4X-specific
		void create_iw4x_version_file(std::string rawfile_version, std::string iw4x_version) const;
		std::optional<std::string> get_release_tag(std::string release_url) const;
		bool does_iw4x_require_update(iw4x_update_state& update_state) const;
		void deploy_iw4x_rawfiles() const;

		void cleanup_directories(const std::vector<file_info>& files) const;
		void cleanup_root_directory() const;
		void cleanup_data_directory(const std::vector<file_info>& files) const;
	};
}
