#pragma once

// Game-independent model of Creature Framework's registration maps.
// Forms are stored as FormIDs so this compiles and tests without Skyrim.
// Container order and string comparison follow JContainers (see docs/DESIGN.md).

#include <cstdint>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CFNG
{
	using FormID = std::uint32_t;

	// _stricmp ordering: ASCII lower-cased, unsigned byte compare
	int  ICompare(std::string_view a_lhs, std::string_view a_rhs) noexcept;
	bool IEquals(std::string_view a_lhs, std::string_view a_rhs) noexcept;

	struct ILess
	{
		using is_transparent = void;
		bool operator()(std::string_view a_lhs, std::string_view a_rhs) const noexcept { return ICompare(a_lhs, a_rhs) < 0; }
	};

	enum RegistrationType : std::int32_t
	{
		kEvents = 0,
		kArmors = 1,
		kBoth = 2
	};

	struct CreatureModReg
	{
		std::int32_t              type{ kArmors };
		FormID                    normalArmor{ 0 };
		FormID                    arousedArmor{ 0 };
		bool                      stripArmor{ false };
		bool                      stripWeapons{ true };
		std::vector<FormID>       stripFormBlacklist;
		std::vector<std::int32_t> stripSlotBlacklist;
		std::vector<std::int32_t> restrictedSlots;
	};

	struct CreatureEntry
	{
		std::string                                  activeMod;
		std::map<std::string, CreatureModReg, ILess> mods;
	};

	struct RaceInfo
	{
		std::string              name;
		std::vector<std::string> mods;
		std::vector<FormID>      skins;
	};

	struct SkinInfo
	{
		std::string              name;
		std::vector<std::string> mods;
	};

	struct ModInfo
	{
		std::string         name;
		std::vector<FormID> races;
		std::vector<FormID> skins;
	};

	// What an actor resolves to (CreatureFramework.psc GetActorMapA)
	struct ResolvedCreature
	{
		FormID                        race{ 0 };
		FormID                        skin{ 0 };  // registered skin actually used, may be the fake skin
		std::string                   activeMod;
		std::optional<CreatureModReg> reg;        // registration of activeMod, if any
	};

	class Registry
	{
	public:
		void   SetFakeSkin(FormID a_fakeSkin);
		FormID FakeSkin() const;

		// --- mutation ---------------------------------------------------------
		void RegisterMod(std::string_view a_modId, std::string_view a_modName);
		// returns the (race, skin) entries whose activeMod changed
		std::vector<std::pair<FormID, FormID>> UnregisterMod(std::string_view a_modId);
		void UnregisterAllMods();
		// a_raceName must already hold the race's own name when the caller passed "" (Papyrus falls back to Race.GetName)
		bool RegisterCreature(std::string_view a_modId, FormID a_race, FormID a_skin, std::string_view a_raceName, std::string_view a_skinName, const CreatureModReg& a_reg);
		// returns the (race, skin) entries whose activeMod changed
		std::vector<std::pair<FormID, FormID>> UnregisterAllCreaturesFromMod(std::string_view a_modId);
		bool                                   SetActiveMod(FormID a_race, FormID a_skin, std::string_view a_modId);
		std::vector<std::pair<FormID, FormID>> ClearActiveForMod(std::string_view a_modId);

		// --- queries (names mirror CreatureFramework.psc) ----------------------
		bool                     IsModRegistered(std::string_view a_modId) const;
		std::string              GetModName(std::string_view a_modId) const;
		std::vector<std::string> GetRegisteredMods() const;

		bool                IsCreatureRegistered(FormID a_race, FormID a_skin, bool a_checkFake) const;
		bool                IsExactCreatureRegistered(FormID a_race, FormID a_skin) const;
		bool                IsCreatureRegisteredToMod(std::string_view a_modId, FormID a_race, FormID a_skin) const;
		std::vector<FormID> GetRegisteredRaces() const;  // keys of the creatures map
		bool                IsRaceRegistered(FormID a_race) const;
		std::vector<FormID> GetRacesRegisteredToMod(std::string_view a_modId) const;
		bool                IsRaceRegisteredToMod(std::string_view a_modId, FormID a_race) const;
		std::vector<FormID> GetRegisteredSkins() const;
		bool                IsSkinRegistered(FormID a_skin) const;
		std::vector<FormID> GetSkinsRegisteredToMod(std::string_view a_modId) const;
		bool                IsSkinRegisteredToMod(std::string_view a_modId, FormID a_skin) const;
		std::vector<FormID> GetSkinsRegisteredToRace(FormID a_race) const;

		std::vector<std::string>      GetModsRegisteredWithRace(FormID a_race) const;
		std::vector<std::string>      GetModsRegisteredWithSkin(FormID a_skin) const;
		std::vector<std::string>      GetModsRegisteredWithCreature(FormID a_race, FormID a_skin) const;
		std::optional<CreatureModReg> GetCreatureModReg(std::string_view a_modId, FormID a_race, FormID a_skin) const;

		std::string GetRaceName(FormID a_race) const;
		std::string GetSkinName(FormID a_skin) const;
		std::string GetActiveMod(FormID a_race, FormID a_skin) const;
		std::int32_t GetActiveModIndex(FormID a_race, FormID a_skin) const;

		// a_skinCandidates in lookup order (base skin, leveled base skin, race skin); zeros are skipped
		std::optional<ResolvedCreature> Resolve(FormID a_race, const std::vector<FormID>& a_skinCandidates) const;

		// user choices made through the MCM, persisted in the cosave and re-applied after each load
		std::map<std::pair<FormID, FormID>, std::string> GetActiveModOverrides() const;
		void SetActiveModOverrides(std::map<std::pair<FormID, FormID>, std::string> a_overrides);
		void ApplyActiveModOverrides();

		std::size_t CreatureCount() const;

		template <class F>
		void Visit(F&& a_visitor) const
		{
			std::shared_lock lock(_lock);
			a_visitor(_mods, _races, _skins, _creatures);
		}

	private:
		using CreatureMap = std::map<FormID, std::map<FormID, CreatureEntry>>;

		FormID SkinOrFake(FormID a_skin) const noexcept { return a_skin ? a_skin : _fakeSkin; }

		bool IsModRegisteredNoLock(std::string_view a_modId) const;
		bool IsCreatureRegisteredToModNoLock(std::string_view a_modId, FormID a_race, FormID a_skin) const;
		const CreatureEntry* FindEntryNoLock(FormID a_race, FormID a_exactSkin) const;
		CreatureEntry*       FindEntryNoLock(FormID a_race, FormID a_exactSkin);
		bool                 SetActiveModNoLock(FormID a_race, FormID a_skin, std::string_view a_modId);
		std::string          GetActiveModNoLock(FormID a_race, FormID a_skin) const;

		mutable std::shared_mutex                         _lock;
		FormID                                            _fakeSkin{ 0 };
		std::map<std::string, ModInfo, ILess>             _mods;
		std::map<FormID, RaceInfo>                        _races;
		std::map<FormID, SkinInfo>                        _skins;
		CreatureMap                                       _creatures;
		std::map<std::pair<FormID, FormID>, std::string>  _overrides;
	};
}
