#pragma once

#include "Core/Registry.h"

namespace CFNG::Game::Equipment
{
	// CreatureFramework.psc Initialize(): head, body, hands, feet, ring, amulet
	inline constexpr std::array<std::uint32_t, 6> kArmorSlots{ 0x00000001, 0x00000004, 0x00000008, 0x00000080, 0x00000040, 0x00000020 };

	struct StrippedGear
	{
		std::array<FormID, 6> armor{};
		FormID                leftHand{ 0 };
		FormID                rightHand{ 0 };

		[[nodiscard]] bool Empty() const noexcept
		{
			return leftHand == 0 && rightHand == 0 && std::ranges::all_of(armor, [](FormID a_id) { return a_id == 0; });
		}
	};

	bool WornHasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword);
	bool HasAnyEquippedSlot(RE::Actor* a_actor, const std::vector<std::int32_t>& a_slotMasks);

	// RemoveArmors: drops CF normal armor, and CF aroused armor unless the actor is in a SexLab scene
	void RemoveCFArmors(RE::Actor* a_actor, bool a_inScene);
	void AddAndEquipArmor(RE::Actor* a_actor, RE::TESObjectARMO* a_armor);

	// StripA / EquipA / UnequipSomeMore; main thread only. Return how many items were queued.
	std::size_t Strip(RE::Actor* a_actor, const CreatureModReg& a_reg, StrippedGear& a_gear);
	std::size_t Restore(RE::Actor* a_actor, bool a_armor, bool a_weapons, StrippedGear& a_gear);
	void        UnequipAgain(RE::Actor* a_actor, bool a_armor, bool a_weapons, const StrippedGear& a_gear);
}
