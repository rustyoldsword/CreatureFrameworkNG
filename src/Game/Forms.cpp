#include "Game/Forms.h"

namespace CFNG::Game
{
	namespace
	{
		constexpr auto kPlugin = "CreatureFramework.esm"sv;

		template <class T>
		T* Lookup(RE::FormID a_localID, std::string_view a_plugin, bool a_required)
		{
			auto* form = RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_localID, a_plugin);
			if (!form && a_required) {
				logger::error("Missing {:06X} in {}", a_localID, a_plugin);
			}
			return form;
		}
	}

	Forms& Forms::Get()
	{
		static Forms singleton;
		return singleton;
	}

	bool Forms::Load()
	{
		frameworkQuest = Lookup<RE::TESQuest>(0xCF01, kPlugin, true);
		configQuest = Lookup<RE::TESQuest>(0xCF02, kPlugin, true);
		fakeSkin = Lookup<RE::TESObjectARMO>(0x86F1, kPlugin, true);
		ignoreCreature = Lookup<RE::BGSKeyword>(0x68F4, kPlugin, true);
		armorNormal = Lookup<RE::BGSKeyword>(0x68F5, kPlugin, true);
		armorAroused = Lookup<RE::BGSKeyword>(0x68F6, kPlugin, true);
		cloakRange = Lookup<RE::TESGlobal>(0x86F2, kPlugin, true);
		cloakCreatures = Lookup<RE::TESGlobal>(0x86F3, kPlugin, true);
		actorTypeNPC = Lookup<RE::BGSKeyword>(0x13794, "Skyrim.esm"sv, true);
		sexLabAnimating = Lookup<RE::TESFaction>(0xE50F, "SexLab.esm"sv, false);
		slaArousal = Lookup<RE::TESFaction>(0x3FC36, "SexLabAroused.esm"sv, false);

		logger::info("SexLab faction {}, SLA arousal faction {}", sexLabAnimating ? "found" : "absent", slaArousal ? "found" : "absent");

		const bool required = frameworkQuest && configQuest && fakeSkin && ignoreCreature && armorNormal && armorAroused && cloakRange && cloakCreatures && actorTypeNPC;
		if (required) {
			ApplyIgnoreKeywords();
		}
		return required;
	}

	void Forms::ApplyIgnoreKeywords()
	{
		// CreatureFramework.esm 4.0 carried Skyrim LE copies of these records only to add
		// CFIgnoreThisCreature; NG's plugin drops those overrides and adds the keyword here, so
		// vanilla SE data and localized names stay intact
		constexpr std::array<std::pair<RE::FormID, std::string_view>, 5> targets{ {
			{ 0x0BA545, "MudcrabRace" },
			{ 0x032B94, "MQ101Alduin" },
			{ 0x04377F, "MQ206AncientAlduin" },
			{ 0x0654E5, "MQ101Horse" },
			{ 0x0B9E1D, "MQ101CartHorse" },
		} };

		for (const auto& [localID, editorID] : targets) {
			auto* form = RE::TESDataHandler::GetSingleton()->LookupForm(localID, "Skyrim.esm"sv);
			auto* keywords = form ? form->As<RE::BGSKeywordForm>() : nullptr;
			if (!keywords) {
				logger::warn("Cannot add CFIgnoreThisCreature to {} ({:06X})", editorID, localID);
				continue;
			}
			if (!keywords->HasKeyword(ignoreCreature)) {
				keywords->AddKeyword(ignoreCreature);
			}
		}
		logger::info("CFIgnoreThisCreature applied to {} vanilla forms", targets.size());
	}

	FormID ResolveFormString(std::string_view a_formString, FormKind a_kind)
	{
		constexpr auto prefix = "__formData|"sv;
		if (!a_formString.starts_with(prefix)) {
			return 0;
		}
		a_formString.remove_prefix(prefix.size());

		const auto separator = a_formString.find('|');
		if (separator == std::string_view::npos) {
			return 0;
		}
		const auto plugin = a_formString.substr(0, separator);
		const auto idText = std::string(a_formString.substr(separator + 1));

		std::uint32_t id = 0;
		try {
			id = static_cast<std::uint32_t>(std::stoul(idText, nullptr, 0));
		} catch (...) {
			return 0;
		}

		RE::TESForm* form = nullptr;
		if (plugin.empty()) {
			form = RE::TESForm::LookupByID(id);
		} else {
			auto*       dataHandler = RE::TESDataHandler::GetSingleton();
			const auto* file = dataHandler->LookupModByName(plugin);
			if (!file) {
				return 0;
			}
			// JContainers keeps 12 bits for light plugins and 24 for the rest
			const auto localID = file->IsLight() ? (id & 0xFFF) : (id & 0xFFFFFF);
			form = dataHandler->LookupForm(localID, plugin);
		}
		if (!form) {
			return 0;
		}

		switch (a_kind) {
		case FormKind::kRace:
			return form->Is(RE::FormType::Race) ? form->GetFormID() : 0;
		case FormKind::kArmor:
			return form->Is(RE::FormType::Armor) ? form->GetFormID() : 0;
		default:
			return form->GetFormID();
		}
	}

	std::string FormToString(FormID a_formID)
	{
		const auto* form = a_formID ? RE::TESForm::LookupByID(a_formID) : nullptr;
		if (!form) {
			return {};
		}
		const auto* file = form->GetFile(0);
		if (!file) {
			return std::format("__formData||0x{:X}", a_formID);
		}
		const auto localID = file->IsLight() ? (a_formID & 0xFFF) : (a_formID & 0xFFFFFF);
		return std::format("__formData|{}|0x{:X}", file->GetFilename(), localID);
	}
}
