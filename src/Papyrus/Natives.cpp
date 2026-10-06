#include "Papyrus/Natives.h"

#include "Game/Arousal.h"
#include "Game/Forms.h"
#include "Game/Runtime.h"
#include "Game/Tracker.h"

#include <fstream>

namespace CFNG::Papyrus
{
	namespace
	{
		using Tag = RE::StaticFunctionTag;
		using json = nlohmann::ordered_json;

		FormID IdOf(const RE::TESForm* a_form) { return a_form ? a_form->GetFormID() : 0; }

		std::vector<RE::TESForm*> ToForms(const std::vector<FormID>& a_ids)
		{
			std::vector<RE::TESForm*> result;
			result.reserve(a_ids.size());
			for (const auto id : a_ids) {
				result.push_back(id ? RE::TESForm::LookupByID(id) : nullptr);
			}
			return result;
		}

		std::vector<FormID> ToIds(const std::vector<RE::TESForm*>& a_forms)
		{
			std::vector<FormID> result;
			result.reserve(a_forms.size());
			for (const auto* form : a_forms) {
				result.push_back(IdOf(form));
			}
			return result;
		}

		Registry& Reg() { return Game::GetRegistry(); }

		void TriggerChanged(const std::vector<std::pair<FormID, FormID>>& a_changed)
		{
			for (const auto& [race, skin] : a_changed) {
				Game::Tracker::Get().TriggerUpdate(race, skin);
			}
		}

		// --- registration -------------------------------------------------------------

		void ReloadRegistrations(Tag*) { Game::ReloadRegistrations(); }
		void RegisterMod(Tag*, std::string a_modId, std::string a_modName) { Reg().RegisterMod(a_modId, a_modName); }
		void UnregisterMod(Tag*, std::string a_modId) { TriggerChanged(Reg().UnregisterMod(a_modId)); }
		void UnregisterAllMods(Tag*) { Reg().UnregisterAllMods(); }

		bool RegisterCreatureToMod(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin, std::string a_raceName, std::string a_skinName,
			std::int32_t a_type, RE::TESObjectARMO* a_normal, RE::TESObjectARMO* a_aroused, bool a_stripArmor, bool a_stripWeapons,
			std::vector<RE::TESForm*> a_formBlacklist, std::vector<std::int32_t> a_slotBlacklist, std::vector<std::int32_t> a_restrictedSlots)
		{
			if (!a_race) {
				return false;
			}
			if (a_raceName.empty()) {
				a_raceName = a_race->GetName();
			}
			CreatureModReg reg;
			reg.type = a_type;
			reg.normalArmor = IdOf(a_normal);
			reg.arousedArmor = IdOf(a_aroused);
			reg.stripArmor = a_stripArmor;
			reg.stripWeapons = a_stripWeapons;
			reg.stripFormBlacklist = ToIds(a_formBlacklist);
			reg.stripSlotBlacklist = std::move(a_slotBlacklist);
			reg.restrictedSlots = std::move(a_restrictedSlots);
			return Reg().RegisterCreature(a_modId, a_race->GetFormID(), IdOf(a_skin), a_raceName, a_skinName, reg);
		}

		// --- queries --------------------------------------------------------------------

		bool                      IsModRegistered(Tag*, std::string a_modId) { return Reg().IsModRegistered(a_modId); }
		std::string               GetModName(Tag*, std::string a_modId) { return Reg().GetModName(a_modId); }
		std::vector<std::string>  GetRegisteredMods(Tag*) { return Reg().GetRegisteredMods(); }

		bool IsCreatureRegistered(Tag*, RE::TESRace* a_race, RE::TESObjectARMO* a_skin, bool a_checkFake) { return Reg().IsCreatureRegistered(IdOf(a_race), IdOf(a_skin), a_checkFake); }
		bool IsExactCreatureRegistered(Tag*, RE::TESRace* a_race, RE::TESObjectARMO* a_skin) { return Reg().IsExactCreatureRegistered(IdOf(a_race), IdOf(a_skin)); }
		bool IsCreatureRegisteredToMod(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin) { return Reg().IsCreatureRegisteredToMod(a_modId, IdOf(a_race), IdOf(a_skin)); }

		std::vector<RE::TESForm*> GetRegisteredRaces(Tag*) { return ToForms(Reg().GetRegisteredRaces()); }
		bool                      IsRaceRegistered(Tag*, RE::TESRace* a_race) { return Reg().IsRaceRegistered(IdOf(a_race)); }
		std::vector<RE::TESForm*> GetRacesRegisteredToMod(Tag*, std::string a_modId) { return ToForms(Reg().GetRacesRegisteredToMod(a_modId)); }
		bool                      IsRaceRegisteredToMod(Tag*, std::string a_modId, RE::TESRace* a_race) { return Reg().IsRaceRegisteredToMod(a_modId, IdOf(a_race)); }

		std::vector<RE::TESForm*> GetRegisteredSkins(Tag*) { return ToForms(Reg().GetRegisteredSkins()); }
		bool                      IsSkinRegistered(Tag*, RE::TESObjectARMO* a_skin) { return Reg().IsSkinRegistered(IdOf(a_skin)); }
		std::vector<RE::TESForm*> GetSkinsRegisteredToMod(Tag*, std::string a_modId) { return ToForms(Reg().GetSkinsRegisteredToMod(a_modId)); }
		bool                      IsSkinRegisteredToMod(Tag*, std::string a_modId, RE::TESObjectARMO* a_skin) { return Reg().IsSkinRegisteredToMod(a_modId, IdOf(a_skin)); }
		std::vector<RE::TESForm*> GetSkinsRegisteredToRace(Tag*, RE::TESRace* a_race) { return ToForms(Reg().GetSkinsRegisteredToRace(IdOf(a_race))); }

		std::vector<std::string> GetModsRegisteredWithRace(Tag*, RE::TESRace* a_race) { return Reg().GetModsRegisteredWithRace(IdOf(a_race)); }
		std::vector<std::string> GetModsRegisteredWithSkin(Tag*, RE::TESObjectARMO* a_skin) { return Reg().GetModsRegisteredWithSkin(IdOf(a_skin)); }
		std::vector<std::string> GetModsRegisteredWithCreature(Tag*, RE::TESRace* a_race, RE::TESObjectARMO* a_skin) { return Reg().GetModsRegisteredWithCreature(IdOf(a_race), IdOf(a_skin)); }

		std::optional<CreatureModReg> RegOf(const std::string& a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			return Reg().GetCreatureModReg(a_modId, IdOf(a_race), IdOf(a_skin));
		}

		// missing registrations answer like JMap getters on a 0 handle: 0 / None / false / empty
		std::int32_t GetModCreatureType(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg ? reg->type : 0;
		}
		RE::TESObjectARMO* GetModCreatureNormalArmor(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg ? RE::TESForm::LookupByID<RE::TESObjectARMO>(reg->normalArmor) : nullptr;
		}
		RE::TESObjectARMO* GetModCreatureArousedArmor(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg ? RE::TESForm::LookupByID<RE::TESObjectARMO>(reg->arousedArmor) : nullptr;
		}
		bool GetModCreatureStripArmor(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg && reg->stripArmor;
		}
		bool GetModCreatureStripWeapons(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg && reg->stripWeapons;
		}
		std::vector<RE::TESForm*> GetModCreatureStripFormBlacklist(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg ? ToForms(reg->stripFormBlacklist) : std::vector<RE::TESForm*>{};
		}
		std::vector<std::int32_t> GetModCreatureStripSlotBlacklist(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg ? reg->stripSlotBlacklist : std::vector<std::int32_t>{};
		}
		std::vector<std::int32_t> GetModCreatureRestrictedSlots(Tag*, std::string a_modId, RE::TESRace* a_race, RE::TESObjectARMO* a_skin)
		{
			const auto reg = RegOf(a_modId, a_race, a_skin);
			return reg ? reg->restrictedSlots : std::vector<std::int32_t>{};
		}

		std::string  GetRaceName(Tag*, RE::TESRace* a_race) { return Reg().GetRaceName(IdOf(a_race)); }
		std::string  GetSkinName(Tag*, RE::TESObjectARMO* a_skin) { return Reg().GetSkinName(IdOf(a_skin)); }
		std::string  GetActiveMod(Tag*, RE::TESRace* a_race, RE::TESObjectARMO* a_skin) { return Reg().GetActiveMod(IdOf(a_race), IdOf(a_skin)); }
		std::int32_t GetActiveModIndex(Tag*, RE::TESRace* a_race, RE::TESObjectARMO* a_skin) { return Reg().GetActiveModIndex(IdOf(a_race), IdOf(a_skin)); }

		void SetActiveMod(Tag*, RE::TESRace* a_race, RE::TESObjectARMO* a_skin, std::string a_modId)
		{
			if (!a_race) {
				return;
			}
			// the MCM's OnOptionDefault passes -1, which the original treated as an unknown mod id
			if (a_modId == "-1") {
				a_modId.clear();
			}
			const FormID race = a_race->GetFormID();
			const FormID skin = a_skin ? a_skin->GetFormID() : Reg().FakeSkin();
			if (Reg().SetActiveMod(race, skin, a_modId)) {
				Game::Tracker::Get().TriggerUpdate(race, skin);
			}
		}

		// --- actors ---------------------------------------------------------------------

		bool         IsActorActive(Tag*, RE::Actor* a_actor) { return Game::Tracker::Get().IsTracked(a_actor); }
		bool         IsCreature(Tag*, RE::Actor* a_actor) { return Game::Tracker::IsCreature(a_actor); }
		bool         IsInScene(Tag*, RE::Actor* a_actor) { return Game::Tracker::IsInScene(a_actor); }
		std::int32_t GetArousal(Tag*, RE::Actor* a_actor) { return Game::Arousal::Get(a_actor); }
		bool         IsArousalAvailable(Tag*) { return Game::Arousal::IsAvailable(); }

		void TriggerUpdateForActor(Tag*, RE::Actor* a_actor)
		{
			if (a_actor) {
				Game::Tracker::Get().TriggerUpdateForActor(a_actor->GetHandle());
			}
		}
		void RequestUpdate(Tag*) { Game::Tracker::Get().RequestUpdate(); }
		void ClearCreatures(Tag*) { Game::Tracker::Get().ClearCreatures(); }

		void MigrateStrippedGear(Tag*, RE::Actor* a_actor, std::vector<RE::TESForm*> a_armor, RE::TESForm* a_left, RE::TESForm* a_right)
		{
			if (!a_actor) {
				return;
			}
			auto gear = Game::StripStore::Get().Peek(a_actor->GetFormID());
			for (std::size_t i = 0; i < gear.armor.size() && i < a_armor.size(); ++i) {
				if (a_armor[i]) {
					gear.armor[i] = a_armor[i]->GetFormID();
				}
			}
			if (a_left) {
				gear.leftHand = a_left->GetFormID();
			}
			if (a_right) {
				gear.rightHand = a_right->GetFormID();
			}
			Game::StripStore::Get().Put(a_actor->GetFormID(), gear);
		}

		std::string GetVersionString(Tag*)
		{
			return std::string(SKSE::PluginDeclaration::GetSingleton()->GetVersion().string());
		}

		// --- dump (parity oracle, mirrors JValue.WriteToFile of the old JDB map) -------

		json FormList(const std::vector<FormID>& a_ids)
		{
			json list = json::array();
			for (const auto id : a_ids) {
				list.push_back(Game::FormToString(id));
			}
			return list;
		}

		bool Dump(Tag*, std::string a_path)
		{
			json root = json::object();
			Reg().Visit([&](const auto& a_mods, const auto& a_races, const auto& a_skins, const auto& a_creatures) {
				json& mods = root["mods"] = json::object();
				for (const auto& [id, info] : a_mods) {
					mods[id] = { { "name", info.name }, { "races", FormList(info.races) }, { "skins", FormList(info.skins) } };
				}
				json& races = root["races"] = json::object();
				for (const auto& [race, info] : a_races) {
					races[Game::FormToString(race)] = { { "name", info.name }, { "mods", info.mods }, { "skins", FormList(info.skins) } };
				}
				json& skins = root["skins"] = json::object();
				for (const auto& [skin, info] : a_skins) {
					skins[Game::FormToString(skin)] = { { "name", info.name }, { "mods", info.mods } };
				}
				json& creatures = root["creatures"] = json::object();
				for (const auto& [race, bySkin] : a_creatures) {
					json& raceNode = creatures[Game::FormToString(race)] = json::object();
					for (const auto& [skin, entry] : bySkin) {
						json regs = json::object();
						for (const auto& [modId, reg] : entry.mods) {
							regs[modId] = {
								{ "type", reg.type },
								{ "normalArmor", Game::FormToString(reg.normalArmor) },
								{ "arousedArmor", Game::FormToString(reg.arousedArmor) },
								{ "stripArmor", reg.stripArmor ? 1 : 0 },
								{ "stripWeapons", reg.stripWeapons ? 1 : 0 },
								{ "stripFormBlacklist", FormList(reg.stripFormBlacklist) },
								{ "stripSlotBlacklist", reg.stripSlotBlacklist },
								{ "restrictedSlots", reg.restrictedSlots },
							};
						}
						raceNode[Game::FormToString(skin)] = { { "activeMod", entry.activeMod }, { "mods", std::move(regs) } };
					}
				}
			});

			std::ofstream file(a_path.empty() ? "CreatureFrameworkNG.json" : a_path, std::ios::binary | std::ios::trunc);
			if (!file) {
				logger::error("Dump: cannot write {}", a_path);
				return false;
			}
			file << root.dump(2);
			logger::info("Dumped registrations to {}", a_path);
			return true;
		}
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		// pure registry reads are guarded by the registry lock, so they skip the per-frame sync
		const auto fast = [&](std::string_view a_name, auto a_fn) { a_vm->RegisterFunction(a_name, kClass, a_fn, true); };
		const auto main = [&](std::string_view a_name, auto a_fn) { a_vm->RegisterFunction(a_name, kClass, a_fn); };

		main("ReloadRegistrations", ReloadRegistrations);
		fast("RegisterMod", RegisterMod);
		main("UnregisterMod", UnregisterMod);
		fast("UnregisterAllMods", UnregisterAllMods);
		fast("RegisterCreatureToMod", RegisterCreatureToMod);

		fast("IsModRegistered", IsModRegistered);
		fast("GetModName", GetModName);
		fast("GetRegisteredMods", GetRegisteredMods);
		fast("IsCreatureRegistered", IsCreatureRegistered);
		fast("IsExactCreatureRegistered", IsExactCreatureRegistered);
		fast("IsCreatureRegisteredToMod", IsCreatureRegisteredToMod);
		fast("GetRegisteredRaces", GetRegisteredRaces);
		fast("IsRaceRegistered", IsRaceRegistered);
		fast("GetRacesRegisteredToMod", GetRacesRegisteredToMod);
		fast("IsRaceRegisteredToMod", IsRaceRegisteredToMod);
		fast("GetRegisteredSkins", GetRegisteredSkins);
		fast("IsSkinRegistered", IsSkinRegistered);
		fast("GetSkinsRegisteredToMod", GetSkinsRegisteredToMod);
		fast("IsSkinRegisteredToMod", IsSkinRegisteredToMod);
		fast("GetSkinsRegisteredToRace", GetSkinsRegisteredToRace);
		fast("GetModsRegisteredWithRace", GetModsRegisteredWithRace);
		fast("GetModsRegisteredWithSkin", GetModsRegisteredWithSkin);
		fast("GetModsRegisteredWithCreature", GetModsRegisteredWithCreature);
		fast("GetModCreatureType", GetModCreatureType);
		fast("GetModCreatureNormalArmor", GetModCreatureNormalArmor);
		fast("GetModCreatureArousedArmor", GetModCreatureArousedArmor);
		fast("GetModCreatureStripArmor", GetModCreatureStripArmor);
		fast("GetModCreatureStripWeapons", GetModCreatureStripWeapons);
		fast("GetModCreatureStripFormBlacklist", GetModCreatureStripFormBlacklist);
		fast("GetModCreatureStripSlotBlacklist", GetModCreatureStripSlotBlacklist);
		fast("GetModCreatureRestrictedSlots", GetModCreatureRestrictedSlots);
		fast("GetRaceName", GetRaceName);
		fast("GetSkinName", GetSkinName);
		fast("GetActiveMod", GetActiveMod);
		fast("GetActiveModIndex", GetActiveModIndex);
		main("SetActiveMod", SetActiveMod);

		fast("IsActorActive", IsActorActive);
		main("IsCreature", IsCreature);
		main("IsInScene", IsInScene);
		main("GetArousal", GetArousal);
		fast("IsArousalAvailable", IsArousalAvailable);
		fast("TriggerUpdateForActor", TriggerUpdateForActor);
		fast("RequestUpdate", RequestUpdate);
		fast("ClearCreatures", ClearCreatures);
		main("MigrateStrippedGear", MigrateStrippedGear);
		fast("GetVersionString", GetVersionString);
		main("Dump", Dump);

		logger::info("Registered {} natives", kClass);
		return true;
	}
}
