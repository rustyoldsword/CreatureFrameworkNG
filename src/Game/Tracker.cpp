#include "Game/Tracker.h"

#include "Game/Arousal.h"
#include "Game/Equipment.h"
#include "Game/Forms.h"
#include "Game/Runtime.h"

#include <thread>

namespace CFNG::Game
{
	namespace
	{
		std::vector<FormID> SkinCandidates(RE::Actor* a_actor)
		{
			// GetActorBase().GetSkin() -> GetLeveledActorBase().GetSkin() -> GetRace().GetSkin()
			auto*             leveled = a_actor->GetActorBase();
			RE::TESActorBase* base = leveled;
			if (const auto* extra = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>(); extra && extra->originalBase) {
				base = extra->originalBase;
			}
			const auto skinOf = [](const RE::BGSSkinForm* a_form) -> FormID {
				return a_form && a_form->skin ? a_form->skin->GetFormID() : 0;
			};
			const auto* race = a_actor->GetRace();
			return { skinOf(base), skinOf(leveled), skinOf(race) };
		}

		FormID RaceOf(RE::Actor* a_actor)
		{
			// GetLeveledActorBase().GetRace()
			auto*       npc = a_actor->GetActorBase();
			const auto* race = npc ? npc->GetRace() : nullptr;
			return race ? race->GetFormID() : 0;
		}

		bool IsValidForUpdate(RE::Actor* a_actor)
		{
			const auto flyState = static_cast<std::uint32_t>(a_actor->AsActorState()->GetFlyState());
			if (flyState > 0 && flyState < 5) {
				return false;
			}
			return !a_actor->GetCurrentScene() && !a_actor->IsInKillMove();
		}

		void FireArousalEvent(std::string_view a_modId, RE::Actor* a_actor, bool a_aroused, bool a_fromSex)
		{
			// CFArousalChange_<modId> keeps its Papyrus argument layout, so the script fires it
			auto*       vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			const auto* quest = Forms::Get().frameworkQuest;
			if (!vm || !quest) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(RE::FormType::Quest, quest);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			auto* args = RE::MakeFunctionArguments(RE::BSFixedString(a_modId), std::move(a_actor), std::move(a_aroused), std::move(a_fromSex), !a_fromSex);
			vm->DispatchMethodCall(handle, "CreatureFramework", "NGFireEvent", args, callback);
		}

		bool Verbose(const McmSettings& a_settings)
		{
			return a_settings.debugLog || NativeSettings::Get().verboseLog;
		}

		std::string NameOf(RE::Actor* a_actor)
		{
			const char* name = a_actor ? a_actor->GetName() : nullptr;
			return std::format("{} [{:08X}]", name && *name ? name : "Unknown", a_actor ? a_actor->GetFormID() : 0);
		}

		// arousal from the active source, the SLA faction rank the original read, and the SexLab scene rank
		std::string StateOf(RE::Actor* a_actor)
		{
			const auto& forms = Forms::Get();
			const auto  slaRank = forms.slaArousal ? a_actor->GetFactionRank(forms.slaArousal, a_actor->IsPlayerRef()) : -99;
			const auto  sceneRank = forms.sexLabAnimating ? a_actor->GetFactionRank(forms.sexLabAnimating, a_actor->IsPlayerRef()) : -99;
			return std::format("arousal {}, sla rank {}, scene rank {}", Arousal::Get(a_actor), slaRank, sceneRank);
		}
	}

	Tracker& Tracker::Get()
	{
		static Tracker singleton;
		return singleton;
	}

	// --- predicates ------------------------------------------------------------------

	bool Tracker::IsCreature(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		auto*       npc = a_actor->GetActorBase();
		const auto* race = npc ? npc->GetRace() : nullptr;
		const auto& forms = Forms::Get();
		return race && !race->HasKeyword(forms.actorTypeNPC) && !race->HasKeyword(forms.ignoreCreature);
	}

	bool Tracker::IsInScene(RE::Actor* a_actor)
	{
		// SexLab marks a finished scene with rank -1 instead of removing the faction, and
		// Papyrus IsInFaction (what SexLab.IsActorActive uses) is rank >= 0
		auto* faction = Forms::Get().sexLabAnimating;
		return a_actor && faction && a_actor->GetFactionRank(faction, a_actor->IsPlayerRef()) >= 0;
	}

	bool Tracker::IsAroused(RE::Actor* a_actor, const McmSettings& a_settings)
	{
		return a_settings.genAroused && Arousal::Get(a_actor) >= a_settings.arousalThreshold;
	}

	// --- scheduling -------------------------------------------------------------------

	void Tracker::Start()
	{
		if (_started.exchange(true)) {
			return;
		}
		std::thread([this]() {
			auto nextTick = std::chrono::steady_clock::now();
			for (;;) {
				std::this_thread::sleep_for(50ms);
				const auto now = std::chrono::steady_clock::now();

				std::vector<std::function<void()>> due;
				{
					std::scoped_lock lock(_delayedLock);
					for (auto it = _delayed.begin(); it != _delayed.end();) {
						if (it->first <= now) {
							due.push_back(std::move(it->second));
							it = _delayed.erase(it);
						} else {
							++it;
						}
					}
				}
				for (auto& task : due) {
					SKSE::GetTaskInterface()->AddTask(std::move(task));
				}

				if (now >= nextTick) {
					nextTick = now + std::chrono::milliseconds(static_cast<std::int64_t>(NativeSettings::Get().tickSeconds * 1000.0f));
					RequestUpdate();
				}
			}
		}).detach();
	}

	void Tracker::SetGameReady(bool a_ready)
	{
		_ready = a_ready;
	}

	void Tracker::Reset()
	{
		std::scoped_lock lock(_lock);
		_slots.fill(Slot{});
	}

	void Tracker::ScheduleTask(std::chrono::milliseconds a_delay, std::function<void()> a_task)
	{
		std::scoped_lock lock(_delayedLock);
		_delayed.emplace_back(std::chrono::steady_clock::now() + a_delay, std::move(a_task));
	}

	void Tracker::RequestUpdate()
	{
		if (!_ready || _tickQueued.exchange(true)) {
			return;
		}
		SKSE::GetTaskInterface()->AddTask([this]() {
			_tickQueued = false;
			Tick();
		});
	}

	// --- main thread ------------------------------------------------------------------

	Tracker::Slot* Tracker::FindSlot(const RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}
		// GetHandle() is not const in CommonLibSSE-NG, although it only reads
		const auto native = const_cast<RE::Actor*>(a_actor)->GetHandle().native_handle();
		for (auto& slot : _slots) {
			if (!slot.Empty() && slot.handle.native_handle() == native) {
				return std::addressof(slot);
			}
		}
		return nullptr;
	}

	Tracker::Slot* Tracker::FreeSlot()
	{
		for (auto& slot : _slots) {
			if (slot.Empty()) {
				return std::addressof(slot);
			}
		}
		return nullptr;
	}

	bool Tracker::IsTracked(const RE::Actor* a_actor) const
	{
		if (!a_actor) {
			return false;
		}
		std::scoped_lock lock(_lock);
		const auto       native = const_cast<RE::Actor*>(a_actor)->GetHandle().native_handle();
		return std::ranges::any_of(_slots, [native](const Slot& a_slot) { return !a_slot.Empty() && a_slot.handle.native_handle() == native; });
	}

	std::size_t Tracker::TrackedCount() const
	{
		std::scoped_lock lock(_lock);
		return static_cast<std::size_t>(std::ranges::count_if(_slots, [](const Slot& a_slot) { return !a_slot.Empty(); }));
	}

	void Tracker::Tick()
	{
		if (!_ready) {
			return;
		}
		auto* ui = RE::UI::GetSingleton();
		if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Is3DLoaded()) {
			return;
		}

		const auto       settings = ReadMcmSettings();
		std::scoped_lock lock(_lock);
		UpdateTracked(settings);
		ScanForCreatures(settings);
	}

	void Tracker::ReleaseSlot(Slot& a_slot, RE::Actor* a_actor, bool a_cleanup)
	{
		if (a_cleanup && a_actor) {
			Equipment::RemoveCFArmors(a_actor, IsInScene(a_actor));
			auto gear = StripStore::Get().Take(a_actor->GetFormID());
			if (!gear.Empty()) {
				Equipment::Restore(a_actor, true, true, gear);
			}
		}
		a_slot = Slot{};
	}

	void Tracker::SyncScene(Slot& a_slot, RE::Actor* a_actor, bool a_inScene, const McmSettings& a_settings)
	{
		if (a_inScene == a_slot.inScene) {
			return;
		}
		a_slot.inScene = a_inScene;

		auto&      registry = GetRegistry();
		const auto activeMod = registry.GetActiveMod(a_slot.race, a_slot.skin);
		const auto reg = activeMod.empty() ? std::nullopt : registry.GetCreatureModReg(activeMod, a_slot.race, a_slot.skin);
		const auto actorID = a_actor->GetFormID();

		if (a_inScene) {
			// the original only stripped while switching to aroused, so a creature that was
			// already aroused kept its weapons for the whole scene
			if (!reg || !IsCreature(a_actor) || (!reg->stripArmor && !reg->stripWeapons)) {
				return;
			}
			auto       gear = StripStore::Get().Peek(actorID);
			const auto stripped = Equipment::Strip(a_actor, *reg, gear);
			StripStore::Get().Put(actorID, gear);
			if (Verbose(a_settings)) {
				logger::info("{} entered a scene: stripped {} item(s) (left {:08X}, right {:08X})", NameOf(a_actor), stripped, gear.leftHand, gear.rightHand);
			}
			// UnequipSomeMore: creatures re-draw weapons right after the unequip
			const auto handle = a_actor->GetHandle();
			const bool armor = reg->stripArmor;
			const bool weapons = reg->stripWeapons;
			const bool verbose = Verbose(a_settings);
			for (const auto delay : { 750ms, 2000ms }) {
				ScheduleTask(delay, [handle, armor, weapons, verbose, delay]() {
					auto actor = handle.get();
					if (!actor || !IsInScene(actor.get())) {
						return;
					}
					if (verbose) {
						// what the engine reports as still in hand once the queued unequip had time to run
						const auto* left = actor->GetEquippedObject(true);
						const auto* right = actor->GetEquippedObject(false);
						logger::info("{} +{}ms in scene: left hand {:08X}, right hand {:08X}", NameOf(actor.get()), delay.count(), left ? left->GetFormID() : 0, right ? right->GetFormID() : 0);
					}
					Equipment::UnequipAgain(actor.get(), armor, weapons, StripStore::Get().Peek(actor->GetFormID()));
				});
			}
		} else {
			auto       gear = StripStore::Get().Take(actorID);
			const auto restored = gear.Empty() ? 0 : Equipment::Restore(a_actor, true, true, gear);
			if (Verbose(a_settings)) {
				// whether the creature stays aroused after the scene is decided by this arousal value
				logger::info("{} left the scene: restored {} item(s); {}, threshold {}, stays aroused {}", NameOf(a_actor), restored, StateOf(a_actor), a_settings.arousalThreshold, IsAroused(a_actor, a_settings));
			}
		}
	}

	void Tracker::UpdateTracked(const McmSettings& a_settings)
	{
		auto*       player = RE::PlayerCharacter::GetSingleton();
		const auto& forms = Forms::Get();

		for (auto& slot : _slots) {
			if (slot.Empty()) {
				continue;
			}
			const auto actorPtr = slot.handle.get();
			auto*      actor = actorPtr.get();
			if (!actor) {
				slot = Slot{};
				continue;
			}
			if (!IsValidForUpdate(actor)) {
				continue;
			}
			if (!actor->Is3DLoaded()) {
				slot = Slot{};
				continue;
			}
			if (actor->IsDisabled() || !IsCreature(actor) || actor->GetDistance(player) > a_settings.cloakRange + 1000.0f) {
				if (Verbose(a_settings)) {
					logger::info("Releasing {} (out of range, disabled or no longer a creature)", NameOf(actor));
				}
				ReleaseSlot(slot, actor, true);
				continue;
			}

			const bool inScene = IsInScene(actor);
			SyncScene(slot, actor, inScene, a_settings);

			const auto activeMod = GetRegistry().GetActiveMod(slot.race, slot.skin);
			const auto reg = GetRegistry().GetCreatureModReg(activeMod, slot.race, slot.skin);
			const bool eventsOnly = reg && reg->type == kEvents;

			if (inScene || IsAroused(actor, a_settings)) {
				const bool needsChange = eventsOnly ? slot.flag != 1 : !Equipment::WornHasKeyword(actor, forms.armorAroused);
				if (needsChange) {
					ChangeArousal(slot, actor, inScene ? 1 : 0, true, a_settings);
				}
			} else if (Equipment::WornHasKeyword(actor, forms.armorAroused)) {
				ChangeArousal(slot, actor, 0, true, a_settings);
			} else if (!Equipment::WornHasKeyword(actor, forms.armorNormal)) {
				ChangeArousal(slot, actor, 0, false, a_settings);
			}
		}
	}

	void Tracker::ScanForCreatures(const McmSettings& a_settings)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* processLists = RE::ProcessLists::GetSingleton();
		if (!processLists || a_settings.cloakCreatures <= 0) {
			return;
		}
		const auto& forms = Forms::Get();

		std::vector<std::pair<float, RE::Actor*>> candidates;
		processLists->ForEachHighActor([&](RE::Actor* a_actor) {
			if (!a_actor || a_actor == player || !a_actor->Is3DLoaded() || a_actor->IsDisabled()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			if (FindSlot(a_actor) || !IsCreature(a_actor) || a_actor->HasKeyword(forms.ignoreCreature)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const float distance = a_actor->GetDistance(player);
			if (distance > a_settings.cloakRange || !IsValidForUpdate(a_actor)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			// CFQuestDetectCreature also skipped creatures already wearing CFArmorAroused; tracking them
			// instead lets a creature left aroused by an old save or a crash get cleaned up
			if (Equipment::WornHasKeyword(a_actor, forms.ignoreCreature)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			candidates.emplace_back(distance, a_actor);
			return RE::BSContainer::ForEachResult::kContinue;
		});

		std::ranges::sort(candidates, {}, &std::pair<float, RE::Actor*>::first);
		const auto limit = std::min<std::size_t>(candidates.size(), static_cast<std::size_t>(a_settings.cloakCreatures));
		for (std::size_t i = 0; i < limit; ++i) {
			ActivateActor(candidates[i].second, false, a_settings);
		}
	}

	bool Tracker::ActivateActor(RE::Actor* a_actor, bool a_sexLabActive, const McmSettings& a_settings)
	{
		const auto& forms = Forms::Get();
		const bool  inScene = a_sexLabActive || IsInScene(a_actor);

		if (auto* slot = FindSlot(a_actor)) {
			SyncScene(*slot, a_actor, inScene, a_settings);
			if (inScene || IsAroused(a_actor, a_settings)) {
				if (!Equipment::WornHasKeyword(a_actor, forms.armorAroused)) {
					ChangeArousal(*slot, a_actor, inScene ? 1 : 0, true, a_settings);
				}
			} else if (Equipment::WornHasKeyword(a_actor, forms.armorAroused)) {
				ChangeArousal(*slot, a_actor, 0, true, a_settings);
			} else if (!Equipment::WornHasKeyword(a_actor, forms.armorNormal)) {
				ChangeArousal(*slot, a_actor, 0, false, a_settings);
			}
			return true;
		}

		auto* slot = FreeSlot();
		if (!slot) {
			return false;
		}
		const auto resolved = GetRegistry().Resolve(RaceOf(a_actor), SkinCandidates(a_actor));
		if (!resolved || resolved->activeMod.empty()) {
			return false;
		}

		*slot = Slot{ a_actor->GetHandle(), resolved->race, resolved->skin, 0, false };
		if (Verbose(a_settings)) {
			logger::info("Tracking {} as {:08X}/{:08X} ({}; {})", NameOf(a_actor), resolved->race, resolved->skin, resolved->activeMod, StateOf(a_actor));
		}
		SyncScene(*slot, a_actor, inScene, a_settings);
		ChangeArousal(*slot, a_actor, inScene ? 1 : 0, true, a_settings);
		return true;
	}

	void Tracker::ChangeArousal(Slot& a_slot, RE::Actor* a_actor, std::int32_t a_sexLabState, bool a_force, const McmSettings& a_settings)
	{
		std::int32_t newFlag = 0;
		if (a_sexLabState == 1) {
			newFlag = 1;
		} else {
			a_sexLabState = 0;
			newFlag = IsAroused(a_actor, a_settings) ? 1 : 0;
		}

		const std::int32_t oldFlag = a_slot.flag;
		if (oldFlag == newFlag && !a_force) {
			return;
		}

		auto&      registry = GetRegistry();
		const auto activeMod = registry.GetActiveMod(a_slot.race, a_slot.skin);
		const auto reg = activeMod.empty() ? std::nullopt : registry.GetCreatureModReg(activeMod, a_slot.race, a_slot.skin);
		const bool inSceneFaction = IsInScene(a_actor);

		if (reg) {
			if (!(a_sexLabState && reg->stripArmor) && Equipment::HasAnyEquippedSlot(a_actor, reg->restrictedSlots)) {
				a_slot.flag = newFlag;
				return;
			}

			// the original swaps only for type 1, although type 2 is documented as swap and events
			if (reg->type == kArmors || reg->type == kBoth) {
				auto* normalArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(reg->normalArmor);
				auto* arousedArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(reg->arousedArmor);
				if (!arousedArmor) {
					arousedArmor = normalArmor;
				}
				if (normalArmor != arousedArmor || (a_force && arousedArmor)) {
					Equipment::RemoveCFArmors(a_actor, inSceneFaction);
					if (newFlag) {
						Equipment::AddAndEquipArmor(a_actor, arousedArmor);
					} else if (normalArmor) {
						Equipment::AddAndEquipArmor(a_actor, normalArmor);
					}
				}
			}

			if (reg->type == kEvents || reg->type == kBoth) {
				FireArousalEvent(activeMod, a_actor, newFlag != 0, a_sexLabState != 0);
			}

			if (Verbose(a_settings)) {
				logger::info("{} -> {} (scene {}, forced {}, mod {}; {})", NameOf(a_actor), newFlag ? "aroused" : "normal", a_sexLabState, a_force, activeMod, StateOf(a_actor));
			}
		} else {
			Equipment::RemoveCFArmors(a_actor, inSceneFaction);
			if (oldFlag != 1) {
				auto gear = StripStore::Get().Take(a_actor->GetFormID());
				Equipment::Restore(a_actor, true, true, gear);
			}
		}

		a_slot.flag = newFlag;
	}

	// --- queued entry points ----------------------------------------------------------

	void Tracker::TriggerUpdate(FormID a_race, FormID a_skin)
	{
		SKSE::GetTaskInterface()->AddTask([this, a_race, a_skin]() {
			if (!_ready) {
				return;
			}
			const auto       settings = ReadMcmSettings();
			auto*            player = RE::PlayerCharacter::GetSingleton();
			std::scoped_lock lock(_lock);
			for (auto& slot : _slots) {
				if (slot.Empty() || slot.race != a_race || slot.skin != a_skin) {
					continue;
				}
				const auto actorPtr = slot.handle.get();
				auto*      actor = actorPtr.get();
				if (!actor || !actor->Is3DLoaded()) {
					continue;
				}
				if (actor->IsDisabled() || actor->GetDistance(player) > settings.cloakRange + 1000.0f) {
					ReleaseSlot(slot, actor, true);
				} else {
					const bool inScene = IsInScene(actor);
					SyncScene(slot, actor, inScene, settings);
					ChangeArousal(slot, actor, inScene ? 1 : 0, true, settings);
				}
			}
		});
	}

	void Tracker::TriggerUpdateForActor(RE::ActorHandle a_handle)
	{
		SKSE::GetTaskInterface()->AddTask([this, a_handle]() {
			if (!_ready) {
				return;
			}
			const auto actorPtr = a_handle.get();
			auto*      actor = actorPtr.get();
			if (!actor || !actor->Is3DLoaded()) {
				return;
			}
			const auto       settings = ReadMcmSettings();
			auto*            player = RE::PlayerCharacter::GetSingleton();
			std::scoped_lock lock(_lock);
			auto*            slot = FindSlot(actor);
			if (actor->IsDisabled() || actor->GetDistance(player) > settings.cloakRange + 500.0f) {
				if (slot) {
					ReleaseSlot(*slot, actor, true);
				} else {
					Equipment::RemoveCFArmors(actor, IsInScene(actor));
				}
			} else if (!slot) {
				ActivateActor(actor, false, settings);
			} else {
				const bool inScene = IsInScene(actor);
				SyncScene(*slot, actor, inScene, settings);
				ChangeArousal(*slot, actor, inScene ? 1 : 0, true, settings);
			}
		});
	}

	void Tracker::ClearCreatures()
	{
		SKSE::GetTaskInterface()->AddTask([this]() {
			std::scoped_lock lock(_lock);
			for (auto& slot : _slots) {
				if (slot.Empty()) {
					continue;
				}
				const auto actorPtr = slot.handle.get();
				ReleaseSlot(slot, actorPtr.get(), true);
			}
		});
	}

	void Tracker::OnSceneStart(FormID a_threadQuest)
	{
		SKSE::GetTaskInterface()->AddTask([this, a_threadQuest]() {
			if (!_ready) {
				return;
			}
			const auto settings = ReadMcmSettings();
			if (!settings.genSexLab) {
				return;
			}
			auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(a_threadQuest);
			if (!quest) {
				RequestUpdate();
				return;
			}

			std::vector<RE::Actor*> positions;
			for (auto* alias : quest->aliases) {
				auto* refAlias = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
				if (auto* actor = refAlias ? refAlias->GetActorReference() : nullptr) {
					positions.push_back(actor);
				}
			}

			std::scoped_lock lock(_lock);
			for (auto* actor : positions) {
				if (!IsCreature(actor)) {
					continue;
				}
				auto* cell = actor->GetParentCell();
				if (cell && cell->IsAttached() && ActivateActor(actor, true, settings)) {
					actor->NotifyAnimationGraph("SOSFastErect");
				}
			}
		});
	}

	void Tracker::OnSceneEnd()
	{
		// the SexLab faction rank drops to -1 shortly after AnimationEnd; the regular pass then restores gear
		ScheduleTask(1000ms, [this]() { RequestUpdate(); });
	}
}
