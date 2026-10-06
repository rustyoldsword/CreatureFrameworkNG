#include "Game/Settings.h"

#include "Game/Forms.h"

#include <fstream>

namespace CFNG::Game
{
	namespace
	{
		std::string Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r\n");
			return std::string(a_text.substr(first, last - first + 1));
		}
	}

	NativeSettings& NativeSettings::Get()
	{
		static NativeSettings singleton;
		return singleton;
	}

	void NativeSettings::Load()
	{
		std::ifstream file("Data/SKSE/Plugins/CreatureFrameworkNG.ini");
		std::string   line;
		while (file && std::getline(file, line)) {
			const auto text = Trim(line);
			if (text.empty() || text.front() == ';' || text.front() == '#' || text.front() == '[') {
				continue;
			}
			const auto eq = text.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = Trim(std::string_view(text).substr(0, eq));
			const auto value = Trim(std::string_view(text).substr(eq + 1));
			try {
				if (IEquals(key, "fTickSeconds")) {
					tickSeconds = std::clamp(std::stof(value), 0.25f, 15.0f);
				} else if (IEquals(key, "bVerboseLog")) {
					verboseLog = value == "1" || IEquals(value, "true");
				}
			} catch (...) {
				logger::warn("Ignoring invalid ini value {}={}", key, value);
			}
		}
		logger::info("Tick every {:.2f}s, verbose log {}", tickSeconds, verboseLog);
	}

	McmSettings ReadMcmSettings()
	{
		McmSettings  settings;
		const auto&  forms = Forms::Get();

		if (forms.cloakRange) {
			settings.cloakRange = forms.cloakRange->value;
		}
		if (forms.cloakCreatures) {
			settings.cloakCreatures = static_cast<std::int32_t>(forms.cloakCreatures->value);
		}

		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm || !forms.configQuest) {
			return settings;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return settings;
		}
		const auto handle = policy->GetHandleForObject(RE::FormType::Quest, forms.configQuest);
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		if (!vm->FindBoundObject(handle, "CreatureFrameworkConfig", object) || !object) {
			return settings;
		}

		if (const auto* var = object->GetProperty("GenSexLab"); var && var->IsBool()) {
			settings.genSexLab = var->GetBool();
		}
		if (const auto* var = object->GetProperty("GenAroused"); var && var->IsBool()) {
			settings.genAroused = var->GetBool();
		}
		if (const auto* var = object->GetProperty("GenArousalThreshold"); var && var->IsInt()) {
			settings.arousalThreshold = var->GetSInt();
		}
		if (const auto* var = object->GetProperty("DbgOutputLog"); var && var->IsBool()) {
			settings.debugLog = var->GetBool();
		}
		return settings;
	}
}
