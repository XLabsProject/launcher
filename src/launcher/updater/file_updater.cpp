#include "std_include.hpp"

#include "updater.hpp"
#include "updater_ui.hpp"
#include "file_updater.hpp"

#include <utils/cryptography.hpp>
#include <utils/http.hpp>
#include <utils/io.hpp>
#include <utils/winzip.hpp>
#include <utils/logger.hpp>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <iostream>

#define UPDATE_SERVER "https://master.xlabs.dev/"

#define UPDATE_FILE_MAIN UPDATE_SERVER "files.json"
#define UPDATE_FOLDER_MAIN UPDATE_SERVER "data/"

#define UPDATE_FILE_DEV UPDATE_SERVER "files-dev.json"
#define UPDATE_FOLDER_DEV UPDATE_SERVER "data-dev/"

#define UPDATE_HOST_BINARY "xlabs.exe"

#define IW4X_VERSION_FILE ".version.json"
#define IW4X_RAWFILES_UPDATE_FILE "release.zip"
#define IW4X_RAWFILES_UPDATE_URL "https://github.com/XLabsProject/iw4x-rawfiles/releases/latest/download/" IW4X_RAWFILES_UPDATE_FILE

namespace updater
{
	namespace
	{
		std::string get_update_file()
		{
			return is_main_channel() ? UPDATE_FILE_MAIN : UPDATE_FILE_DEV;
		}

		std::string get_update_folder()
		{
			return is_main_channel() ? UPDATE_FOLDER_MAIN : UPDATE_FOLDER_DEV;
		}

		std::vector<file_info> parse_file_infos(const std::string& json)
		{
			rapidjson::Document doc{};
			doc.Parse(json.data(), json.size());

			if (!doc.IsArray())
			{
				return {};
			}

			std::vector<file_info> files{};

			for (const auto& element : doc.GetArray())
			{
				if (!element.IsArray())
				{
					continue;
				}

				auto array = element.GetArray();

				file_info info{};
				info.name.assign(array[0].GetString(), array[0].GetStringLength());
				info.size = array[1].GetInt64();
				info.hash.assign(array[2].GetString(), array[2].GetStringLength());

				files.emplace_back(std::move(info));
			}

			return files;
		}

		std::vector<file_info> get_file_infos()
		{
			const auto json = utils::http::get_data(get_update_file());
			if (!json)
			{
				return {};
			}

			return parse_file_infos(*json);
		}

		std::string get_hash(const std::string& data)
		{
			return utils::cryptography::sha1::compute(data, true);
		}

		const file_info* find_host_file_info(const std::vector<file_info>& outdated_files)
		{
			for (const auto& file : outdated_files)
			{
				if (file.name == UPDATE_HOST_BINARY)
				{
					return &file;
				}
			}

			return nullptr;
		}

		size_t get_optimal_concurrent_download_count(const size_t file_count)
		{
			size_t cores = std::thread::hardware_concurrency();
			cores = (cores * 2) / 3;
			return std::max(1ull, std::min(cores, file_count));
		}

		bool is_inside_folder(const std::filesystem::path& file, const std::filesystem::path& folder)
		{
			const auto relative = std::filesystem::relative(file, folder);
			const auto start = relative.begin();
			return start != relative.end() && start->string() != "..";
		}
	}

	file_updater::file_updater(progress_listener& listener, const std::filesystem::path base, std::filesystem::path process_file)
		: listener_(listener)
		, base_(std::move(base))
		, process_file_(std::move(process_file))
	{
		this->dead_process_file_ = std::filesystem::path(this->process_file_.generic_wstring() + L".old");
		this->delete_old_process_file();
	}

	void file_updater::run() const
	{
		const auto files = get_file_infos();
		if (!files.empty())
		{
			this->cleanup_directories(files);
		}

		const auto outdated_files = this->get_outdated_files(files);
		if (outdated_files.empty())
		{
			return;
		}

		this->update_host_binary(outdated_files);
		this->update_files(outdated_files);
	}

	void file_updater::update_file(const file_info& file, bool iw4x_file) const
	{
		auto url = get_update_folder() + file.name;
		utils::logger::write("Updating file {}", url);

		if (iw4x_file)
		{
			url = file.name;
			utils::logger::write("This is an iw4x file, the url has been changed to {} instead", url);
		}

		const auto data = utils::http::get_data(url, {}, [&](const size_t progress)
		{
			this->listener_.file_progress(file, progress);
		});

		// IW4x files have invalid hash and size for now
		if (!data || (!iw4x_file && (data->size() != file.size || get_hash(*data) != file.hash)))
		{
			throw std::runtime_error("Failed to download: " + url);
		}

		auto out_file = this->get_drive_filename(file);

		// IW4x hack to fetch release from github
		if (iw4x_file)
		{
			out_file = this->base_ / std::filesystem::path(file.name).filename().string();
		}

		utils::logger::write("Writing file to {}", out_file.string());

		if (!utils::io::write_file(out_file, *data, false))
		{
			throw std::runtime_error("Failed to write: " + file.name);
		}

		utils::logger::write("Done updating file {}", file.name);
	}

	std::vector<file_info> file_updater::get_outdated_files(const std::vector<file_info>& files) const
	{
		std::vector<file_info> outdated_files{};

		for (const auto& info : files)
		{
			if (this->is_outdated_file(info))
			{
				outdated_files.emplace_back(info);
			}
		}

		return outdated_files;
	}

	void file_updater::update_host_binary(const std::vector<file_info>& outdated_files) const
	{
		const auto* host_file = find_host_file_info(outdated_files);
		if (!host_file)
		{
			return;
		}

		try
		{
			this->move_current_process_file();
			this->update_files({*host_file});
		}
		catch (...)
		{
			this->restore_current_process_file();
			throw;
		}

		utils::nt::relaunch_self();
		throw update_cancelled();
	}

	bool file_updater::does_iw4x_require_update(iw4x_update_state& update_state) const
	{
		std::filesystem::path iw4x_basegame_directory(this->base_);
		std::filesystem::path revision_file_path = iw4x_basegame_directory / IW4X_VERSION_FILE;

		auto every_update_required = false;
		std::string data{};
		rapidjson::Document doc{};

		if (utils::io::read_file(revision_file_path, &data))
		{
			const rapidjson::ParseResult result = doc.Parse(data);
			if (!result || !doc.IsObject())
			{
				every_update_required = true;
				utils::logger::write("Could not load the JSON revision file for iw4x, considering all updates to be necessary");
			}
		}
		else
		{
			utils::logger::write("No revision file found for iw4x, considering all updates to be necessary");
			every_update_required = true;
		}

		if (every_update_required || doc.HasMember("rawfile_version"))
		{
			utils::logger::write("Fetching iw4x-rawfiles tag from github...");

			std::optional<std::string> rawfiles_tag = get_release_tag("https://api.github.com/repos/XLabsProject/iw4x-rawfiles/releases/latest");
			if (rawfiles_tag.has_value())
			{
				update_state.rawfile_requires_update = every_update_required || doc["rawfile_version"].GetString() != rawfiles_tag.value();
				update_state.rawfile_latest_tag = rawfiles_tag.value();

				utils::logger::write("Got rawfiles tag {}, Requires updating: {}", rawfiles_tag.value(),
					update_state.rawfile_requires_update ? "Yes" : "No");
			}
		}

		return every_update_required || update_state.rawfile_requires_update;
	}

	std::optional<std::string> file_updater::get_release_tag(const std::string& release_url) const
	{
		std::optional<std::string> iw4x_release_info = utils::http::get_data(release_url);
		if (iw4x_release_info.has_value())
		{
			rapidjson::Document release_json{};
			release_json.SetObject();
			release_json.Parse(iw4x_release_info.value());

			if (release_json.HasMember("tag_name"))
			{
				auto tag_name = release_json["tag_name"].GetString();
				return tag_name;
			}
		}

		return {};
	}

	void file_updater::create_iw4x_version_file(const std::string& rawfile_version) const
	{
		utils::logger::write("Creating iw4x version file in {}: rawfiles are {}", this->base_.string(), rawfile_version);
		const auto iw4x_base_game_directory = this->base_;

		rapidjson::Document doc{};

		rapidjson::StringBuffer buffer{};
		rapidjson::Writer<rapidjson::StringBuffer, rapidjson::Document::EncodingType, rapidjson::ASCII<>>
			writer(buffer);

		doc.SetObject();

		doc.AddMember("rawfile_version", rawfile_version, doc.GetAllocator());

		doc.Accept(writer);

		const std::string json(buffer.GetString());

		const auto revision_file_path = iw4x_base_game_directory / IW4X_VERSION_FILE;

		if (utils::io::write_file(revision_file_path, json))
		{
			utils::logger::write("File {} written successfully!", revision_file_path.string());
		}
		else
		{
			utils::logger::write("Error while writing file! {}", std::system_category().message(static_cast<int>(::GetLastError())));
		}
	}

	void file_updater::update_iw4x_if_necessary() const
	{
		iw4x_update_state update_state;

		if (does_iw4x_require_update(update_state))
		{
			std::vector<file_info> files_to_update{};

			if (update_state.rawfile_requires_update)
			{
				files_to_update.emplace_back(IW4X_RAWFILES_UPDATE_URL);
			}

			utils::logger::write("Updating iw4x files");
			update_files(files_to_update, /*iw4x_file=*/true);

			if (update_state.rawfile_requires_update)
			{
				utils::logger::write("Deploying iw4x rawfiles");
				deploy_iw4x_rawfiles();
			}

			// Do this last to make sure we don't ever create a versionfile when something failed
			create_iw4x_version_file(update_state.rawfile_latest_tag);
		}
	}

	void file_updater::deploy_iw4x_rawfiles() const
	{
		const auto rawfiles_zip = base_ / IW4X_RAWFILES_UPDATE_FILE;

		if (!utils::io::file_exists(rawfiles_zip))
		{
			// The zip was absent when it was expected, should we throw for this?
			throw std::runtime_error("I'm supposed to deploy rawfiles from " + rawfiles_zip.generic_string() + ", but where is it?\nCould not find the downloaded update file.");
		}

		// the goal is now to unzip rawfiles_zip into base_
		utils::zip::unzip(rawfiles_zip, base_);

		const auto has_removed_file = utils::io::remove_file(rawfiles_zip);

		if (has_removed_file)
		{
			utils::logger::write("Successfully removed file {}", rawfiles_zip.string());
		}
		else
		{
			throw std::runtime_error(
				"Failed to remove " + rawfiles_zip.string() + ". Error code " + std::to_string(::GetLastError()));
		}
	}

	void file_updater::update_files(const std::vector<file_info>& outdated_files, bool iw4x_files) const
	{
		this->listener_.update_files(outdated_files);

		const auto thread_count = get_optimal_concurrent_download_count(outdated_files.size());

		std::vector<std::thread> threads{};
		std::atomic<size_t> current_index{0};

		utils::concurrency::container<std::exception_ptr> exception{};

		for (size_t i = 0; i < thread_count; ++i)
		{
			threads.emplace_back([&]()
			{
				while (!exception.access<bool>([](const std::exception_ptr& ptr)
				{
					return static_cast<bool>(ptr);
				}))
				{
					const auto index = current_index++;
					if (index >= outdated_files.size())
					{
						break;
					}

					try
					{
						const auto& file = outdated_files[index];
						this->listener_.begin_file(file);
						this->update_file(file, iw4x_files);
						this->listener_.end_file(file);
					}
					catch (...)
					{
						exception.access([](std::exception_ptr& ptr)
						{
							ptr = std::current_exception();
						});

						return;
					}
				}
			});
		}

		for (auto& thread : threads)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}

		exception.access([](const std::exception_ptr& ptr)
		{
			if (ptr)
			{
				std::rethrow_exception(ptr);
			}
		});

		this->listener_.done_update();
	}

	bool file_updater::is_outdated_file(const file_info& file) const
	{
#ifndef CI_BUILD
		if (file.name == UPDATE_HOST_BINARY)
		{
			return false;
		}
#endif

		std::string data{};
		const auto drive_name = this->get_drive_filename(file);
		if (!utils::io::read_file(drive_name, &data))
		{
			return true;
		}

		if (data.size() != file.size)
		{
			return true;
		}

		const auto hash = get_hash(data);
		return hash != file.hash;
	}

	std::filesystem::path file_updater::get_drive_filename(const file_info& file) const
	{
		if (file.name == UPDATE_HOST_BINARY)
		{
			return this->process_file_;
		}

		return this->base_ / "data" / file.name;
	}

	void file_updater::move_current_process_file() const
	{
		utils::io::move_file(this->process_file_, this->dead_process_file_);
	}

	void file_updater::restore_current_process_file() const
	{
		utils::io::move_file(this->dead_process_file_, this->process_file_);
	}

	void file_updater::delete_old_process_file() const
	{
		// Wait for other process to die
		for (auto i = 0; i < 4; ++i)
		{
			utils::io::remove_file(this->dead_process_file_);
			if (!utils::io::file_exists(this->dead_process_file_))
			{
				break;
			}

			std::this_thread::sleep_for(2s);
		}
	}

	void file_updater::cleanup_directories(const std::vector<file_info>& files) const
	{
		if (!utils::io::directory_exists(this->base_))
		{
			return;
		}

		this->cleanup_root_directory();
		this->cleanup_data_directory(files);
	}

	void file_updater::cleanup_root_directory() const
	{
		const auto existing_files = utils::io::list_files(this->base_);
		for (const auto& file : existing_files)
		{
			const auto entry = std::filesystem::relative(file, this->base_);
			if ((entry.string() == "user" || entry.string() == "data") && utils::io::directory_exists(file))
			{
				continue;
			}

			std::error_code code{};
			std::filesystem::remove_all(file, code);
		}
	}

	void file_updater::cleanup_data_directory(const std::vector<file_info>& files) const
	{
		const auto base = std::filesystem::path(this->base_) / "data";
		if (!utils::io::directory_exists(base.string()))
		{
			return;
		}

		std::vector<std::filesystem::path> legal_files{};
		legal_files.reserve(files.size());
		for (const auto& file : files)
		{
			if (file.name != UPDATE_HOST_BINARY)
			{
				legal_files.emplace_back(std::filesystem::absolute(base / file.name));
			}
		}

		const auto existing_files = utils::io::list_files(base.string(), true);
		for (auto& file : existing_files)
		{
			const auto is_file = std::filesystem::is_regular_file(file);
			const auto is_folder = std::filesystem::is_directory(file);

			if (is_file || is_folder)
			{
				bool is_legal = false;

				for (const auto& legal_file : legal_files)
				{
					if ((is_folder && is_inside_folder(legal_file, file)) ||
						(is_file && legal_file == file))
					{
						is_legal = true;
						break;
					}
				}

				if (is_legal)
				{
					continue;
				}
			}

			std::error_code code{};
			std::filesystem::remove_all(file, code);
		}
	}
}
