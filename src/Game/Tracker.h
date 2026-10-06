#pragma once

#include "Core/Registry.h"
#include "Game/Settings.h"

namespace CFNG::Game
{
	// Replaces CreatureFramework.psc's ACreatures/AFlags/ACreaturesMaps arrays, OnUpdate,
	// CFQuestDetectCreature and ChangeArousal. All game mutation happens on the main thread.
	class Tracker
	{
	public:
		static constexpr std::size_t kSlotCount = 32;

		static Tracker& Get();

		void Start();              // timer thread, once at kDataLoaded
		void SetGameReady(bool a_ready);
		void Reset();              // cosave revert: forget tracked actors

		// thread-safe entry points (queue work onto the main thread)
		void RequestUpdate();
		void TriggerUpdate(FormID a_race, FormID a_skin);
		void TriggerUpdateForActor(RE::ActorHandle a_actor);
		void ClearCreatures();
		void OnSceneStart(FormID a_threadQuest);
		void OnSceneEnd();

		bool IsTracked(const RE::Actor* a_actor) const;
		std::size_t TrackedCount() const;

		// predicates shared with the Papyrus natives
		static bool IsCreature(RE::Actor* a_actor);
		static bool IsInScene(RE::Actor* a_actor);
		static bool IsAroused(RE::Actor* a_actor, const McmSettings& a_settings);

	private:
		struct Slot
		{
			RE::ActorHandle handle;
			FormID          race{ 0 };
			FormID          skin{ 0 };  // registry entry key, may be the fake skin
			std::int32_t    flag{ 0 };  // AFlags: 0 not aroused, 1 aroused
			bool            inScene{ false };

			[[nodiscard]] bool Empty() const noexcept { return !handle; }
		};

		void Tick();
		void UpdateTracked(const McmSettings& a_settings);
		void ScanForCreatures(const McmSettings& a_settings);
		bool ActivateActor(RE::Actor* a_actor, bool a_sexLabActive, const McmSettings& a_settings);
		void ChangeArousal(Slot& a_slot, RE::Actor* a_actor, std::int32_t a_sexLabState, bool a_force, const McmSettings& a_settings);
		void SyncScene(Slot& a_slot, RE::Actor* a_actor, bool a_inScene, const McmSettings& a_settings);
		void ReleaseSlot(Slot& a_slot, RE::Actor* a_actor, bool a_cleanup);
		void ScheduleTask(std::chrono::milliseconds a_delay, std::function<void()> a_task);

		Slot* FindSlot(const RE::Actor* a_actor);
		Slot* FreeSlot();

		mutable std::mutex              _lock;  // guards _slots for readers off the main thread
		std::array<Slot, kSlotCount>    _slots{};
		std::atomic_bool                _ready{ false };
		std::atomic_bool                _tickQueued{ false };
		std::atomic_bool                _started{ false };

		std::mutex                                                                        _delayedLock;
		std::vector<std::pair<std::chrono::steady_clock::time_point, std::function<void()>>> _delayed;
	};
}
