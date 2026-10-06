#pragma once

namespace CFNG::Game::Arousal
{
	// binds OSLAroused.dll!GetArousalExt when that plugin is loaded
	void Init();

	bool IsAvailable();

	// arousal rating comparable with GenArousalThreshold; -1 when no arousal framework is present
	std::int32_t Get(RE::Actor* a_actor);
}
