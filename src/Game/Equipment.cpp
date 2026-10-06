#include "Game/Equipment.h"

#include "Game/Forms.h"

namespace CFNG::Game::Equipment
{
	namespace
	{
		bool IsArmorFilter(RE::TESBoundObject& a_object)
		{
			return a_object.IsArmor();
		}

		bool HasCFKeyword(const RE::TESObjectARMO* a_armor)
		{
			const auto& forms = Forms::Get();
			return a_armor->HasKeyword(forms.armorNormal) || a_armor->HasKeyword(forms.armorAroused);
		}

		std::int32_t ItemCount(RE::Actor* a_actor, RE::TESBoundObject* a_object)
		{
			const auto counts = a_actor->GetInventoryCounts([a_object](RE::TESBoundObject& a_candidate) { return std::addressof(a_candidate) == a_object; });
			const auto it = counts.find(a_object);
			return it != counts.end() ? it->second : 0;
		}

		// Gear taken off for a scene goes through the same Papyrus natives the original used
		// (Actor.UnequipItem / EquipItem / UnequipSpell / EquipSpell). ActorEquipManager left
		// weapons in the creature's hands in game, and these calls are rare enough not to matter.
		template <class... Args>
		void DispatchActorCall(RE::Actor* a_actor, std::string_view a_function, Args... a_args)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm || !a_actor) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(RE::FormType::ActorCharacter, a_actor);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			auto* args = RE::MakeFunctionArguments(std::move(a_args)...);
			vm->DispatchMethodCall(handle, "Actor", RE::BSFixedString(a_function), args, callback);
		}

		void UnequipHand(RE::Actor* a_actor, RE::TESForm* a_object, bool a_leftHand)
		{
			if (auto* spell = a_object->As<RE::SpellItem>()) {
				DispatchActorCall(a_actor, "UnequipSpell", spell, a_leftHand ? 0 : 1);
			} else {
				DispatchActorCall(a_actor, "UnequipItem", a_object, true, true);
			}
		}
	}

	bool WornHasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword)
	{
		if (!a_actor || !a_keyword) {
			return false;
		}
		for (const auto& [object, data] : a_actor->GetInventory(IsArmorFilter)) {
			const auto& entry = data.second;
			if (data.first > 0 && entry && entry->IsWorn()) {
				if (const auto* armor = object->As<RE::TESObjectARMO>(); armor && armor->HasKeyword(a_keyword)) {
					return true;
				}
			}
		}
		return false;
	}

	bool HasAnyEquippedSlot(RE::Actor* a_actor, const std::vector<std::int32_t>& a_slotMasks)
	{
		for (const auto mask : a_slotMasks) {
			if (a_actor->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(mask))) {
				return true;
			}
		}
		return false;
	}

	void RemoveCFArmors(RE::Actor* a_actor, bool a_inScene)
	{
		if (!a_actor) {
			return;
		}
		const auto& forms = Forms::Get();

		std::vector<std::pair<RE::TESObjectARMO*, std::int32_t>> toRemove;
		for (const auto& [object, data] : a_actor->GetInventory(IsArmorFilter)) {
			auto* armor = object->As<RE::TESObjectARMO>();
			if (!armor || data.first <= 0) {
				continue;
			}
			if (armor->HasKeyword(forms.armorNormal) || (armor->HasKeyword(forms.armorAroused) && !a_inScene)) {
				toRemove.emplace_back(armor, data.first);
			}
		}

		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		for (const auto& [armor, count] : toRemove) {
			equipManager->UnequipObject(a_actor, armor, nullptr, 1, nullptr, true, false, false);
			a_actor->RemoveItem(armor, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		}
	}

	void AddAndEquipArmor(RE::Actor* a_actor, RE::TESObjectARMO* a_armor)
	{
		if (!a_actor || !a_armor) {
			return;
		}
		if (ItemCount(a_actor, a_armor) <= 0) {
			a_actor->AddObjectToContainer(a_armor, nullptr, 1, nullptr);
		}
		// no retry loop: the next tick re-equips if the engine dropped the request
		RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, a_armor, nullptr, 1, nullptr, true, false, false);
	}

	std::size_t Strip(RE::Actor* a_actor, const CreatureModReg& a_reg, StrippedGear& a_gear)
	{
		std::size_t stripped = 0;

		if (a_reg.stripArmor) {
			for (std::size_t s = 0; s < kArmorSlots.size(); ++s) {
				auto* armor = a_actor->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(kArmorSlots[s]));
				if (!armor || HasCFKeyword(armor)) {
					continue;
				}
				const bool formBlacklisted = std::ranges::find(a_reg.stripFormBlacklist, armor->GetFormID()) != a_reg.stripFormBlacklist.end();
				const bool slotBlacklisted = std::ranges::find(a_reg.stripSlotBlacklist, static_cast<std::int32_t>(kArmorSlots[s])) != a_reg.stripSlotBlacklist.end();
				if (formBlacklisted || slotBlacklisted) {
					continue;
				}
				DispatchActorCall(a_actor, "UnequipItem", static_cast<RE::TESForm*>(armor), true, true);
				a_gear.armor[s] = armor->GetFormID();
				++stripped;
			}
		}

		if (a_reg.stripWeapons) {
			if (auto* left = a_actor->GetEquippedObject(true)) {
				a_gear.leftHand = left->GetFormID();
				UnequipHand(a_actor, left, true);
				++stripped;
			}
			if (auto* right = a_actor->GetEquippedObject(false)) {
				a_gear.rightHand = right->GetFormID();
				UnequipHand(a_actor, right, false);
				++stripped;
			}
		}
		return stripped;
	}

	std::size_t Restore(RE::Actor* a_actor, bool a_armor, bool a_weapons, StrippedGear& a_gear)
	{
		std::size_t restored = 0;

		if (a_armor) {
			for (auto& id : a_gear.armor) {
				// EquipItem would conjure a copy of an item the actor no longer has; skip those instead
				if (auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(id); armor && ItemCount(a_actor, armor) > 0) {
					DispatchActorCall(a_actor, "EquipItem", static_cast<RE::TESForm*>(armor), false, true);
					++restored;
				}
				id = 0;
			}
		}

		if (a_weapons) {
			// GenericEquip in the original passes the actor itself to EquipItem, so weapons never came back
			for (const bool leftHand : { true, false }) {
				auto& id = leftHand ? a_gear.leftHand : a_gear.rightHand;
				if (auto* form = RE::TESForm::LookupByID(id)) {
					if (auto* spell = form->As<RE::SpellItem>()) {
						DispatchActorCall(a_actor, "EquipSpell", spell, leftHand ? 0 : 1);
						++restored;
					} else if (auto* bound = form->As<RE::TESBoundObject>(); bound && ItemCount(a_actor, bound) > 0) {
						DispatchActorCall(a_actor, "EquipItem", form, false, true);
						++restored;
					}
				}
				id = 0;
			}
		}
		return restored;
	}

	void UnequipAgain(RE::Actor* a_actor, bool a_armor, bool a_weapons, const StrippedGear& a_gear)
	{
		if (a_armor) {
			for (const auto id : a_gear.armor) {
				if (auto* armor = RE::TESForm::LookupByID(id)) {
					DispatchActorCall(a_actor, "UnequipItem", armor, true, true);
				}
			}
		}
		if (a_weapons) {
			if (auto* left = a_actor->GetEquippedObject(true)) {
				UnequipHand(a_actor, left, true);
			}
			if (auto* right = a_actor->GetEquippedObject(false)) {
				UnequipHand(a_actor, right, false);
			}
		}
	}
}
