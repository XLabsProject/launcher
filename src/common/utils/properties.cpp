#include "properties.hpp"

#include <gsl/gsl>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "io.hpp"
#include "com.hpp"
#include "string.hpp"

namespace utils::properties
{
	namespace
	{
		std::filesystem::path get_appdata_path()
		{
			PWSTR path;
			if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
			{
				throw std::runtime_error("Failed to read APPDATA path!");
			}

			auto _ = gsl::finally([&path]()
			{
				CoTaskMemFree(path);
			});

			return std::filesystem::path(path) / "xlabs";
		}

		const std::filesystem::path& get_properties_file()
		{
			static const auto props = get_appdata_path() / "user/properties.json";
			return props;
		}

		rapidjson::Document load_properties()
		{
			rapidjson::Document default_doc{};
			default_doc.SetObject();

			std::string data{};
			const auto& props = get_properties_file();
			if (!io::read_file(props, &data))
			{
				return default_doc;
			}

			rapidjson::Document doc{};
			const rapidjson::ParseResult result = doc.Parse(data);
			if (!result || !doc.IsObject())
			{
				return default_doc;
			}

			return doc;
		}

		void store_properties(const rapidjson::Document& doc)
		{
			rapidjson::StringBuffer buffer{};
			rapidjson::Writer<rapidjson::StringBuffer, rapidjson::Document::EncodingType, rapidjson::ASCII<>>
				writer(buffer);
			doc.Accept(writer);

			const std::string json(buffer.GetString(), buffer.GetLength());

			const auto& props = get_properties_file();
			io::write_file(props, json);
		}
	}

	std::unique_lock<named_mutex> lock()
	{
		static named_mutex mutex{"xlabs-properties-lock"};
		std::unique_lock<named_mutex> lock{mutex};
		return lock;
	}

	std::optional<std::string> load(const std::string& name)
	{
		const auto _ = lock();
		const auto doc = load_properties();

		if (!doc.HasMember(name))
		{
			return {};
		}

		const auto& value = doc[name];
		if (!value.IsString())
		{
			return {};
		}

		return {std::string{value.GetString(), value.GetStringLength()}};
	}

	void store(const std::string& name, const std::string& value)
	{
		const auto _ = lock();
		auto doc = load_properties();

		while (doc.HasMember(name))
		{
			doc.RemoveMember(name);
		}

		rapidjson::Value key{};
		key.SetString(name, doc.GetAllocator());

		rapidjson::Value member{};
		member.SetString(value, doc.GetAllocator());

		doc.AddMember(key, member, doc.GetAllocator());

		store_properties(doc);
	}
}
