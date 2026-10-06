#pragma once

#include "Core/JsonLoader.h"

namespace CFNG::Game
{
	// Forms from CreatureFramework.esm and the soft dependencies, looked up once at kDataLoaded
	struct Forms
	{
		RE::TESQuest*      frameworkQuest{ nullptr };   // CreatureFramework.esm 0xCF01
		RE::TESQuest*      configQuest{ nullptr };      // 0xCF02
		RE::TESObjectARMO* fakeSkin{ nullptr };         // 0x86F1
		RE::BGSKeyword*    ignoreCreature{ nullptr };   // 0x68F4
		RE::BGSKeyword*    armorNormal{ nullptr };      // 0x68F5
		RE::BGSKeyword*    armorAroused{ nullptr };     // 0x68F6
		RE::TESGlobal*     cloakRange{ nullptr };       // 0x86F2
		RE::TESGlobal*     cloakCreatures{ nullptr };   // 0x86F3
		RE::BGSKeyword*    actorTypeNPC{ nullptr };     // Skyrim.esm 0x13794
		RE::TESFaction*    sexLabAnimating{ nullptr };  // SexLab.esm 0xE50F, optional
		RE::TESFaction*    slaArousal{ nullptr };       // SexLabAroused.esm 0x3FC36, optional

		static Forms& Get();

		// false when CreatureFramework.esm or one of its required records is missing
		bool Load();

	private:
		void ApplyIgnoreKeywords();
	};

	// JContainers "__formData|<plugin>|<id>" -> loaded FormID of the requested kind, 0 otherwise
	FormID ResolveFormString(std::string_view a_formString, FormKind a_kind);

	// FormID -> "__formData|<plugin>|0x<local id>" (the form JContainers writes)
	std::string FormToString(FormID a_formID);
}
