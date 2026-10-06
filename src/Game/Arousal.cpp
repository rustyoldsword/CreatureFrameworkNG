#include "Game/Arousal.h"

#include "Game/Forms.h"

#include <Windows.h>

namespace CFNG::Game::Arousal
{
	namespace
	{
		using GetArousalExtFn = float (*)(RE::Actor*);
		using SLAGetArousalIntFn = std::int32_t (*)(RE::Actor*);

		GetArousalExtFn    g_getArousalExt = nullptr;
		SLAGetArousalIntFn g_slaNGGetArousalInt = nullptr;
	}

	void Init()
	{
		// SexLab Aroused NG (crajjjj/SexlabArousedNG) exports SLA_GetArousalInt, already clamped 0-100. It and OSL
		// Aroused both ship SexLabAroused.esm, so only one is ever loaded; the faction rank stays the fallback.
		if (const auto module = ::GetModuleHandleW(L"SexlabArousedNG.dll")) {
			g_slaNGGetArousalInt = reinterpret_cast<SLAGetArousalIntFn>(::GetProcAddress(module, "SLA_GetArousalInt"));
		}
		if (!g_slaNGGetArousalInt) {
			if (const auto module = ::GetModuleHandleW(L"OSLAroused.dll")) {
				g_getArousalExt = reinterpret_cast<GetArousalExtFn>(::GetProcAddress(module, "GetArousalExt"));
			}
		}
		logger::info("Arousal source: {}", g_slaNGGetArousalInt ? "SexlabArousedNG.dll SLA_GetArousalInt" :
		                                   (g_getArousalExt ? "OSLAroused.dll GetArousalExt" : (Forms::Get().slaArousal ? "sla_Arousal faction rank" : "none")));
	}

	bool IsAvailable()
	{
		return g_slaNGGetArousalInt || g_getArousalExt || Forms::Get().slaArousal;
	}

	std::int32_t Get(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return -1;
		}
		// fresh value without advancing the framework's own state; the faction rank lags behind its ticker
		if (g_slaNGGetArousalInt) {
			return g_slaNGGetArousalInt(a_actor);
		}
		if (g_getArousalExt) {
			return static_cast<std::int32_t>(g_getArousalExt(a_actor));
		}
		if (auto* faction = Forms::Get().slaArousal) {
			return a_actor->GetFactionRank(faction, a_actor->IsPlayerRef());
		}
		return -1;
	}
}
