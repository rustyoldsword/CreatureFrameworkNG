#include "Core/Registry.h"

#include <algorithm>
#include <mutex>

namespace CFNG
{
	namespace
	{
		unsigned char Lower(char a_ch) noexcept
		{
			const auto ch = static_cast<unsigned char>(a_ch);
			return (ch >= 'A' && ch <= 'Z') ? static_cast<unsigned char>(ch + ('a' - 'A')) : ch;
		}

		bool ContainsI(const std::vector<std::string>& a_values, std::string_view a_value)
		{
			return std::ranges::any_of(a_values, [&](const std::string& a_entry) { return IEquals(a_entry, a_value); });
		}

		void EraseFirstI(std::vector<std::string>& a_values, std::string_view a_value)
		{
			const auto it = std::ranges::find_if(a_values, [&](const std::string& a_entry) { return IEquals(a_entry, a_value); });
			if (it != a_values.end()) {
				a_values.erase(it);
			}
		}

		bool Contains(const std::vector<FormID>& a_values, FormID a_value)
		{
			return std::ranges::find(a_values, a_value) != a_values.end();
		}
	}

	int ICompare(std::string_view a_lhs, std::string_view a_rhs) noexcept
	{
		const auto count = std::min(a_lhs.size(), a_rhs.size());
		for (std::size_t i = 0; i < count; ++i) {
			const auto lhs = Lower(a_lhs[i]);
			const auto rhs = Lower(a_rhs[i]);
			if (lhs != rhs) {
				return lhs < rhs ? -1 : 1;
			}
		}
		if (a_lhs.size() == a_rhs.size()) {
			return 0;
		}
		return a_lhs.size() < a_rhs.size() ? -1 : 1;
	}

	bool IEquals(std::string_view a_lhs, std::string_view a_rhs) noexcept
	{
		return a_lhs.size() == a_rhs.size() && ICompare(a_lhs, a_rhs) == 0;
	}

	void Registry::SetFakeSkin(FormID a_fakeSkin)
	{
		std::unique_lock lock(_lock);
		_fakeSkin = a_fakeSkin;
	}

	FormID Registry::FakeSkin() const
	{
		std::shared_lock lock(_lock);
		return _fakeSkin;
	}

	// --- mutation --------------------------------------------------------------

	void Registry::RegisterMod(std::string_view a_modId, std::string_view a_modName)
	{
		std::unique_lock lock(_lock);
		// JMap.SetObj replaces the whole mod map, dropping its race/skin lists
		_mods.insert_or_assign(std::string(a_modId), ModInfo{ std::string(a_modName), {}, {} });
	}

	std::vector<std::pair<FormID, FormID>> Registry::UnregisterMod(std::string_view a_modId)
	{
		auto changed = UnregisterAllCreaturesFromMod(a_modId);
		std::unique_lock lock(_lock);
		if (const auto it = _mods.find(a_modId); it != _mods.end()) {
			_mods.erase(it);
		}
		return changed;
	}

	void Registry::UnregisterAllMods()
	{
		std::unique_lock lock(_lock);
		_mods.clear();
		_creatures.clear();
		_races.clear();
		_skins.clear();
	}

	bool Registry::RegisterCreature(std::string_view a_modId, FormID a_race, FormID a_skin, std::string_view a_raceName, std::string_view a_skinName, const CreatureModReg& a_reg)
	{
		std::unique_lock lock(_lock);

		const FormID realSkin = SkinOrFake(a_skin);
		const auto   modIt = _mods.find(a_modId);
		if (modIt == _mods.end() || a_race == 0) {
			return false;
		}
		if (a_reg.type < kEvents || a_reg.type > kBoth) {
			return false;
		}

		if (!_races.contains(a_race)) {
			_races[a_race] = RaceInfo{ std::string(a_raceName), {}, {} };
			_creatures[a_race] = {};
		}

		auto& raceCreatures = _creatures[a_race];
		if (!raceCreatures.contains(realSkin)) {
			raceCreatures[realSkin] = CreatureEntry{ std::string(a_modId), {} };
			_races[a_race].skins.push_back(realSkin);
			// JFormMap.SetObj replaces an existing skin map too (same armor under another race)
			_skins[realSkin] = SkinInfo{ realSkin == _fakeSkin ? std::string() : std::string(a_skinName), {} };
		}

		auto& mod = modIt->second;
		if (!Contains(mod.races, a_race)) {
			mod.races.push_back(a_race);
			auto& raceMods = _races[a_race].mods;
			if (!ContainsI(raceMods, a_modId)) {
				raceMods.emplace_back(a_modId);
			}
		}
		if (!Contains(mod.skins, realSkin)) {
			mod.skins.push_back(realSkin);
			if (const auto skinIt = _skins.find(realSkin); skinIt != _skins.end()) {
				// the script appends unconditionally, then its FindStr guard never fires
				skinIt->second.mods.emplace_back(a_modId);
			}
		}

		CreatureModReg stored = a_reg;
		if (stored.type != kArmors && stored.type != kBoth) {
			stored.normalArmor = 0;
			stored.arousedArmor = 0;
		}
		auto& creatureMods = raceCreatures[realSkin].mods;
		if (const auto it = creatureMods.find(a_modId); it != creatureMods.end()) {
			it->second = std::move(stored);
		} else {
			creatureMods.emplace(std::string(a_modId), std::move(stored));
		}
		return true;
	}

	std::vector<std::pair<FormID, FormID>> Registry::UnregisterAllCreaturesFromMod(std::string_view a_modId)
	{
		auto changed = ClearActiveForMod(a_modId);

		std::unique_lock lock(_lock);
		if (const auto it = _mods.find(a_modId); it != _mods.end()) {
			it->second.races.clear();
			it->second.skins.clear();
		}
		for (auto& [race, bySkin] : _creatures) {
			for (auto& [skin, entry] : bySkin) {
				if (const auto it = entry.mods.find(a_modId); it != entry.mods.end()) {
					entry.mods.erase(it);
				}
			}
		}
		// the script erases index FindStr(...) even when it is -1, which JArray treats as "last";
		// that deletes an unrelated mod, so only a real match is erased here
		for (auto& [race, info] : _races) {
			EraseFirstI(info.mods, a_modId);
		}
		for (auto& [skin, info] : _skins) {
			EraseFirstI(info.mods, a_modId);
		}
		return changed;
	}

	bool Registry::SetActiveMod(FormID a_race, FormID a_skin, std::string_view a_modId)
	{
		std::unique_lock lock(_lock);
		const bool applied = SetActiveModNoLock(a_race, a_skin, a_modId);
		if (applied) {
			_overrides[{ a_race, SkinOrFake(a_skin) }] = std::string(a_modId);
		}
		return applied;
	}

	std::vector<std::pair<FormID, FormID>> Registry::ClearActiveForMod(std::string_view a_modId)
	{
		std::unique_lock lock(_lock);
		std::vector<std::pair<FormID, FormID>> changed;
		for (const auto& [race, bySkin] : _creatures) {
			const auto raceIt = _races.find(race);
			if (raceIt == _races.end()) {
				continue;
			}
			for (const FormID skin : raceIt->second.skins) {
				if (IEquals(GetActiveModNoLock(race, skin), a_modId) && SetActiveModNoLock(race, skin, std::string_view{})) {
					changed.emplace_back(race, SkinOrFake(skin));
				}
			}
		}
		return changed;
	}

	// --- private helpers ---------------------------------------------------------

	bool Registry::IsModRegisteredNoLock(std::string_view a_modId) const
	{
		return _mods.find(a_modId) != _mods.end();
	}

	const CreatureEntry* Registry::FindEntryNoLock(FormID a_race, FormID a_exactSkin) const
	{
		const auto raceIt = _creatures.find(a_race);
		if (raceIt == _creatures.end()) {
			return nullptr;
		}
		const auto skinIt = raceIt->second.find(a_exactSkin);
		return skinIt != raceIt->second.end() ? std::addressof(skinIt->second) : nullptr;
	}

	CreatureEntry* Registry::FindEntryNoLock(FormID a_race, FormID a_exactSkin)
	{
		return const_cast<CreatureEntry*>(std::as_const(*this).FindEntryNoLock(a_race, a_exactSkin));
	}

	bool Registry::IsCreatureRegisteredToModNoLock(std::string_view a_modId, FormID a_race, FormID a_skin) const
	{
		const auto* entry = FindEntryNoLock(a_race, SkinOrFake(a_skin));
		return entry && entry->mods.find(a_modId) != entry->mods.end();
	}

	bool Registry::SetActiveModNoLock(FormID a_race, FormID a_skin, std::string_view a_modId)
	{
		const FormID realSkin = SkinOrFake(a_skin);
		if (!a_modId.empty()) {
			if (!IsCreatureRegisteredToModNoLock(a_modId, a_race, realSkin)) {
				return false;
			}
			FindEntryNoLock(a_race, realSkin)->activeMod = std::string(a_modId);
			return true;
		}
		if (auto* entry = FindEntryNoLock(a_race, realSkin)) {
			entry->activeMod.clear();
			return true;
		}
		return false;
	}

	std::string Registry::GetActiveModNoLock(FormID a_race, FormID a_skin) const
	{
		const CreatureEntry* entry = a_skin ? FindEntryNoLock(a_race, a_skin) : nullptr;
		if (!entry) {
			entry = FindEntryNoLock(a_race, _fakeSkin);
		}
		return entry ? entry->activeMod : std::string();
	}

	// --- queries -------------------------------------------------------------------

	bool Registry::IsModRegistered(std::string_view a_modId) const
	{
		std::shared_lock lock(_lock);
		return IsModRegisteredNoLock(a_modId);
	}

	std::string Registry::GetModName(std::string_view a_modId) const
	{
		std::shared_lock lock(_lock);
		const auto it = _mods.find(a_modId);
		return it != _mods.end() ? it->second.name : std::string();
	}

	std::vector<std::string> Registry::GetRegisteredMods() const
	{
		std::shared_lock         lock(_lock);
		std::vector<std::string> result;
		result.reserve(_mods.size());
		for (const auto& [id, info] : _mods) {
			result.push_back(id);
		}
		return result;
	}

	bool Registry::IsCreatureRegistered(FormID a_race, FormID a_skin, bool a_checkFake) const
	{
		std::shared_lock lock(_lock);
		const auto       raceIt = _creatures.find(a_race);
		if (raceIt == _creatures.end()) {
			return false;
		}
		return raceIt->second.contains(SkinOrFake(a_skin)) || (a_checkFake && raceIt->second.contains(_fakeSkin));
	}

	bool Registry::IsExactCreatureRegistered(FormID a_race, FormID a_skin) const
	{
		return IsCreatureRegistered(a_race, a_skin, false);
	}

	bool Registry::IsCreatureRegisteredToMod(std::string_view a_modId, FormID a_race, FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		return IsCreatureRegisteredToModNoLock(a_modId, a_race, a_skin);
	}

	std::vector<FormID> Registry::GetRegisteredRaces() const
	{
		std::shared_lock    lock(_lock);
		std::vector<FormID> result;
		result.reserve(_creatures.size());
		for (const auto& [race, bySkin] : _creatures) {
			result.push_back(race);
		}
		return result;
	}

	bool Registry::IsRaceRegistered(FormID a_race) const
	{
		std::shared_lock lock(_lock);
		return _races.contains(a_race);
	}

	std::vector<FormID> Registry::GetRacesRegisteredToMod(std::string_view a_modId) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _mods.find(a_modId);
		return it != _mods.end() ? it->second.races : std::vector<FormID>{};
	}

	bool Registry::IsRaceRegisteredToMod(std::string_view a_modId, FormID a_race) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _mods.find(a_modId);
		return it != _mods.end() && Contains(it->second.races, a_race);
	}

	std::vector<FormID> Registry::GetRegisteredSkins() const
	{
		std::shared_lock    lock(_lock);
		std::vector<FormID> result;
		result.reserve(_skins.size());
		for (const auto& [skin, info] : _skins) {
			result.push_back(skin);
		}
		return result;
	}

	bool Registry::IsSkinRegistered(FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		return _skins.contains(SkinOrFake(a_skin));
	}

	std::vector<FormID> Registry::GetSkinsRegisteredToMod(std::string_view a_modId) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _mods.find(a_modId);
		return it != _mods.end() ? it->second.skins : std::vector<FormID>{};
	}

	bool Registry::IsSkinRegisteredToMod(std::string_view a_modId, FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _mods.find(a_modId);
		return it != _mods.end() && Contains(it->second.skins, SkinOrFake(a_skin));
	}

	std::vector<FormID> Registry::GetSkinsRegisteredToRace(FormID a_race) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _races.find(a_race);
		return it != _races.end() ? it->second.skins : std::vector<FormID>{};
	}

	std::vector<std::string> Registry::GetModsRegisteredWithRace(FormID a_race) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _races.find(a_race);
		return it != _races.end() ? it->second.mods : std::vector<std::string>{};
	}

	std::vector<std::string> Registry::GetModsRegisteredWithSkin(FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		const auto       it = _skins.find(SkinOrFake(a_skin));
		return it != _skins.end() ? it->second.mods : std::vector<std::string>{};
	}

	std::vector<std::string> Registry::GetModsRegisteredWithCreature(FormID a_race, FormID a_skin) const
	{
		std::shared_lock         lock(_lock);
		std::vector<std::string> result;
		if (const auto* entry = FindEntryNoLock(a_race, SkinOrFake(a_skin))) {
			for (const auto& [id, reg] : entry->mods) {
				result.push_back(id);
			}
		}
		return result;
	}

	std::optional<CreatureModReg> Registry::GetCreatureModReg(std::string_view a_modId, FormID a_race, FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		if (const auto* entry = FindEntryNoLock(a_race, SkinOrFake(a_skin))) {
			if (const auto it = entry->mods.find(a_modId); it != entry->mods.end()) {
				return it->second;
			}
		}
		return std::nullopt;
	}

	std::string Registry::GetRaceName(FormID a_race) const
	{
		std::shared_lock lock(_lock);
		const auto       it = a_race ? _races.find(a_race) : _races.end();
		return it != _races.end() ? it->second.name : std::string();
	}

	std::string Registry::GetSkinName(FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		if (a_skin == 0 || a_skin == _fakeSkin) {
			return {};
		}
		const auto it = _skins.find(a_skin);
		return it != _skins.end() ? it->second.name : std::string();
	}

	std::string Registry::GetActiveMod(FormID a_race, FormID a_skin) const
	{
		std::shared_lock lock(_lock);
		return GetActiveModNoLock(a_race, a_skin);
	}

	std::int32_t Registry::GetActiveModIndex(FormID a_race, FormID a_skin) const
	{
		const auto mods = GetModsRegisteredWithCreature(a_race, a_skin);
		const auto active = GetActiveMod(a_race, a_skin);
		for (std::size_t i = 0; i < mods.size(); ++i) {
			if (IEquals(mods[i], active)) {
				return static_cast<std::int32_t>(i);
			}
		}
		return -1;
	}

	std::optional<ResolvedCreature> Registry::Resolve(FormID a_race, const std::vector<FormID>& a_skinCandidates) const
	{
		std::shared_lock lock(_lock);
		const auto       raceIt = _creatures.find(a_race);
		if (raceIt == _creatures.end()) {
			return std::nullopt;
		}

		// the script takes the first non-None skin, then that exact entry or the fake-skin entry
		FormID skin = 0;
		for (const FormID candidate : a_skinCandidates) {
			if (candidate) {
				skin = candidate;
				break;
			}
		}
		const FormID key = (skin && raceIt->second.contains(skin)) ? skin : _fakeSkin;
		const auto   entryIt = raceIt->second.find(key);
		if (entryIt == raceIt->second.end()) {
			return std::nullopt;
		}

		ResolvedCreature result{ a_race, key, entryIt->second.activeMod, std::nullopt };
		if (!result.activeMod.empty()) {
			if (const auto regIt = entryIt->second.mods.find(result.activeMod); regIt != entryIt->second.mods.end()) {
				result.reg = regIt->second;
			}
		}
		return result;
	}

	std::map<std::pair<FormID, FormID>, std::string> Registry::GetActiveModOverrides() const
	{
		std::shared_lock lock(_lock);
		return _overrides;
	}

	void Registry::SetActiveModOverrides(std::map<std::pair<FormID, FormID>, std::string> a_overrides)
	{
		std::unique_lock lock(_lock);
		_overrides = std::move(a_overrides);
	}

	void Registry::ApplyActiveModOverrides()
	{
		std::unique_lock lock(_lock);
		for (const auto& [key, modId] : _overrides) {
			SetActiveModNoLock(key.first, key.second, modId);
		}
	}

	std::size_t Registry::CreatureCount() const
	{
		std::shared_lock lock(_lock);
		std::size_t      count = 0;
		for (const auto& [race, bySkin] : _creatures) {
			count += bySkin.size();
		}
		return count;
	}
}
