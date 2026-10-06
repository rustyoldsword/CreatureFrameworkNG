#include "Core/JsonLoader.h"

#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

#include <nlohmann/json.hpp>

namespace CFNG
{
	namespace
	{
		using json = nlohmann::ordered_json;

		// JMap keys are case-insensitive and a later equal key replaces an earlier one
		const json* FindKey(const json& a_object, std::string_view a_key)
		{
			if (!a_object.is_object()) {
				return nullptr;
			}
			const json* found = nullptr;
			for (auto it = a_object.begin(); it != a_object.end(); ++it) {
				if (IEquals(it.key(), a_key)) {
					found = std::addressof(it.value());
				}
			}
			return found;
		}

		std::string GetStr(const json& a_object, std::string_view a_key)
		{
			const auto* value = FindKey(a_object, a_key);
			return value && value->is_string() ? value->get<std::string>() : std::string();
		}

		std::int32_t ToInt(const json& a_value)
		{
			if (a_value.is_boolean()) {
				return a_value.get<bool>() ? 1 : 0;
			}
			if (a_value.is_number_integer()) {
				return static_cast<std::int32_t>(a_value.get<std::int64_t>());
			}
			if (a_value.is_number_float()) {
				return static_cast<std::int32_t>(a_value.get<double>());
			}
			return 0;
		}

		std::int32_t GetInt(const json& a_object, std::string_view a_key)
		{
			const auto* value = FindKey(a_object, a_key);
			return value ? ToInt(*value) : 0;
		}

		FormID ToForm(const json& a_value, FormKind a_kind, const FormResolver& a_resolver)
		{
			return a_value.is_string() ? a_resolver(a_value.get_ref<const std::string&>(), a_kind) : 0;
		}

		FormID GetForm(const json& a_object, std::string_view a_key, FormKind a_kind, const FormResolver& a_resolver)
		{
			const auto* value = FindKey(a_object, a_key);
			return value ? ToForm(*value, a_kind, a_resolver) : 0;
		}

		std::vector<std::int32_t> GetIntArray(const json& a_object, std::string_view a_key)
		{
			std::vector<std::int32_t> result;
			if (const auto* value = FindKey(a_object, a_key); value && value->is_array()) {
				for (const auto& element : *value) {
					result.push_back(ToInt(element));
				}
			}
			return result;
		}

		std::vector<FormID> GetFormArray(const json& a_object, std::string_view a_key, const FormResolver& a_resolver)
		{
			std::vector<FormID> result;
			if (const auto* value = FindKey(a_object, a_key); value && value->is_array()) {
				for (const auto& element : *value) {
					result.push_back(ToForm(element, FormKind::kAny, a_resolver));
				}
			}
			return result;
		}
	}

	std::vector<JsonSource> ReadJsonDirectory(const std::filesystem::path& a_directory)
	{
		std::vector<JsonSource> result;
		std::error_code         ec;
		for (std::filesystem::directory_iterator it(a_directory, ec), end; !ec && it != end; it.increment(ec)) {
			if (!it->is_regular_file(ec) || it->path().extension().generic_string() != ".json") {
				continue;
			}
			std::ifstream file(it->path(), std::ios::binary);
			if (!file) {
				continue;
			}
			std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			result.push_back({ it->path().filename().generic_string(), std::move(content) });
		}
		return result;
	}

	LoadReport LoadRegistrations(Registry& a_registry, const std::vector<JsonSource>& a_sources, const FormResolver& a_resolver)
	{
		LoadReport report;

		std::map<std::string, const JsonSource*, ILess> ordered;
		for (const auto& source : a_sources) {
			if (const auto it = ordered.find(source.fileName); it != ordered.end()) {
				it->second = std::addressof(source);
			} else {
				ordered.emplace(source.fileName, std::addressof(source));
			}
		}

		for (const auto& [fileName, source] : ordered) {
			++report.filesSeen;

			const json root = json::parse(source->content, nullptr, false);
			if (root.is_discarded() || !root.is_object() || root.empty()) {
				report.messages.push_back("File " + fileName + " is invalid or empty");
				continue;
			}

			const std::string modId = GetStr(root, "modID");
			const std::string modName = GetStr(root, "modName");
			const json*       creatures = FindKey(root, "creatures");
			if (modId.empty() || modName.empty() || !creatures || !creatures->is_array() || creatures->empty()) {
				report.messages.push_back("File " + fileName + " is missing a mod ID or name, or has no creatures");
				continue;
			}

			if (!a_registry.IsModRegistered(modId)) {
				a_registry.RegisterMod(modId, modName);
			}

			std::size_t index = 0;
			for (const auto& creature : *creatures) {
				const FormID      race = GetForm(creature, "raceForm", FormKind::kRace, a_resolver);
				const std::string raceName = GetStr(creature, "raceName");
				if (race == 0 || raceName.empty()) {
					report.messages.push_back("File " + fileName + " creature " + std::to_string(index) + " is missing its race or race name");
					++index;
					continue;
				}

				const FormID skin = GetForm(creature, "skinForm", FormKind::kArmor, a_resolver);
				if (!a_registry.IsCreatureRegisteredToMod(modId, race, skin)) {
					CreatureModReg reg;
					reg.type = kArmors;
					reg.normalArmor = GetForm(creature, "normalArmor", FormKind::kArmor, a_resolver);
					reg.arousedArmor = GetForm(creature, "arousedArmor", FormKind::kArmor, a_resolver);
					reg.stripArmor = GetInt(creature, "stripArmor") != 0;
					reg.stripWeapons = !FindKey(creature, "stripWeapons") || GetInt(creature, "stripWeapons") != 0;
					reg.stripFormBlacklist = GetFormArray(creature, "stripFormBlacklist", a_resolver);
					reg.stripSlotBlacklist = GetIntArray(creature, "stripSlotBlacklist");
					reg.restrictedSlots = GetIntArray(creature, "restrictedSlots");

					if (a_registry.RegisterCreature(modId, race, skin, raceName, GetStr(creature, "skinName"), reg)) {
						++report.creaturesRegistered;
					}
				}
				++index;
			}

			++report.filesLoaded;
		}

		return report;
	}
}
