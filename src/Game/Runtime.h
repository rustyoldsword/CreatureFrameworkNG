#pragma once

#include "Core/Registry.h"
#include "Game/Equipment.h"

namespace CFNG::Game
{
	Registry& GetRegistry();

	// Rebuilds the registry from Data/creatures.d and re-applies saved MCM choices
	void ReloadRegistrations();

	// Stripped gear per actor, persisted in the cosave (replaces JFormDB .CFForm.Stripped*)
	class StripStore
	{
	public:
		static StripStore& Get();

		Equipment::StrippedGear Take(FormID a_actor);
		Equipment::StrippedGear Peek(FormID a_actor) const;
		void                    Put(FormID a_actor, const Equipment::StrippedGear& a_gear);
		void                    Clear();

		void Save(SKSE::SerializationInterface* a_intfc) const;
		void Load(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);

	private:
		mutable std::mutex                                   _lock;
		std::unordered_map<FormID, Equipment::StrippedGear>  _gear;
	};

	void InstallSerialization();
}
