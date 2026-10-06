#include "Game/Events.h"

#include "Game/Tracker.h"

namespace CFNG::Game
{
	void SceneEventSink::Register()
	{
		static SceneEventSink sink;
		if (auto* source = SKSE::GetModCallbackEventSource()) {
			source->AddEventSink(std::addressof(sink));
			logger::info("Listening for SexLab scene events");
		} else {
			logger::error("ModCallbackEvent source unavailable; scenes are picked up by the regular pass only");
		}
	}

	RE::BSEventNotifyControl SceneEventSink::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const std::string_view name(a_event->eventName.c_str());
		if (name == "AnimationStart"sv || name == "ActorChangeEnd"sv) {
			// SexLab's thread quest sends these through Form.SendModEvent, so it is the sender
			Tracker::Get().OnSceneStart(a_event->sender ? a_event->sender->GetFormID() : 0);
		} else if (name == "AnimationEnd"sv || name == "ActorChangeStart"sv) {
			Tracker::Get().OnSceneEnd();
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
