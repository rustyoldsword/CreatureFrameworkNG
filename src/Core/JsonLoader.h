#pragma once

// Reads Data/creatures.d the way CreatureFramework.psc LoadJSONRegistrations does through
// JContainers, and feeds the Registry. Game access is injected via FormResolver.

#include "Core/Registry.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace CFNG
{
	enum class FormKind
	{
		kAny,
		kRace,
		kArmor
	};

	// "__formData|<plugin>|<id>" -> FormID of the requested kind, 0 when unresolved or of another type
	using FormResolver = std::function<FormID(std::string_view a_formString, FormKind a_kind)>;

	struct JsonSource
	{
		std::string fileName;
		std::string content;
	};

	struct LoadReport
	{
		std::size_t              filesSeen{ 0 };
		std::size_t              filesLoaded{ 0 };
		std::size_t              creaturesRegistered{ 0 };
		std::vector<std::string> messages;
	};

	// non-recursive; extension must be exactly ".json" (case-sensitive, like JContainers)
	std::vector<JsonSource> ReadJsonDirectory(const std::filesystem::path& a_directory);

	// processes files in case-insensitive name order; later duplicates of a name replace earlier content
	LoadReport LoadRegistrations(Registry& a_registry, const std::vector<JsonSource>& a_sources, const FormResolver& a_resolver);
}
