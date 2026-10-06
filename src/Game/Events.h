#pragma once

namespace CFNG::Game
{
	// SexLab thread hooks: AnimationStart / ActorChangeEnd start, AnimationEnd / ActorChangeStart end
	class SceneEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
	public:
		static void Register();

		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;
	};
}
