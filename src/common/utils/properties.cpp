#include "properties.hpp"

#include <gsl/gsl>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "io.hpp"
#include "com.hpp"
#include "string.hpp"

namespace utils
{
	namespace
	{
		rapidjson::Document load_properties(const std::string& filePath)
		{
			rapidjson::Document default_doc{};
			default_doc.SetObject();

			std::string data{};
			if (!io::read_file(filePath, &data))
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

		void store_properties(const std::string& filePath, const rapidjson::Document& doc)
		{
			rapidjson::StringBuffer buffer{};
			rapidjson::Writer<rapidjson::StringBuffer, rapidjson::Document::EncodingType, rapidjson::ASCII<>>
				writer(buffer);
			doc.Accept(writer);

			const std::string json(buffer.GetString(), buffer.GetLength());
			
			io::write_file(filePath, json);
		}
	}

    properties::properties()
        : file_path("properties.json")
    {
    }

	properties::properties(std::string filePath)
        : file_path(std::move(filePath))
    {
    }

	std::unique_lock<named_mutex> properties::lock()
	{
		static named_mutex mutex{"xlabs-properties-lock"};
		std::unique_lock<named_mutex> lock{mutex};
		return lock;
	}

	std::optional<std::string> properties::load(const std::string& name) const
    {
		const auto _ = lock();
		const auto doc = load_properties(file_path);

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

	void properties::store(const std::string& name, const std::string& value) const
    {
		const auto _ = lock();
		auto doc = load_properties(file_path);

		while (doc.HasMember(name))
		{
			doc.RemoveMember(name);
		}

		rapidjson::Value key{};
		key.SetString(name, doc.GetAllocator());

		rapidjson::Value member{};
		member.SetString(value, doc.GetAllocator());

		doc.AddMember(key, member, doc.GetAllocator());

		store_properties(file_path, doc);
	}
}
