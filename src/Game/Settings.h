#pragma once

namespace CFNG::Game
{
	// Values the original scripts read from CreatureFrameworkConfig and its globals
	struct McmSettings
	{
		bool         genSexLab{ true };
		bool         genAroused{ true };
		std::int32_t arousalThreshold{ 70 };  // matches the NG default in CreatureFrameworkConfig.psc
		bool         debugLog{ false };
		float        cloakRange{ 2000.0f };
		std::int32_t cloakCreatures{ 6 };
	};

	// Native-only options from Data/SKSE/Plugins/CreatureFrameworkNG.ini
	struct NativeSettings
	{
		float tickSeconds{ 1.0f };
		bool  verboseLog{ false };

		static NativeSettings& Get();
		void                   Load();
	};

	// Main thread only: reads the config script's auto properties through the VM
	McmSettings ReadMcmSettings();
}
