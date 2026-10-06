#include "Game/Arousal.h"
#include "Game/Events.h"
#include "Game/Forms.h"
#include "Game/Runtime.h"
#include "Game/Settings.h"
#include "Game/Tracker.h"
#include "Papyrus/Natives.h"

namespace
{
	void InitializeLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= "CreatureFrameworkNG.log";

		auto log = std::make_shared<spdlog::logger>(
			"Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		using namespace CFNG::Game;

		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			NativeSettings::Get().Load();
			if (!Forms::Get().Load()) {
				logger::critical("CreatureFramework.esm is missing or incomplete; the native backend stays idle");
				return;
			}
			Arousal::Init();
			ReloadRegistrations();
			SceneEventSink::Register();
			Tracker::Get().Start();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			Tracker::Get().SetGameReady(false);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Tracker::Get().SetGameReady(true);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLogging();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} is loading", plugin->GetName(), plugin->GetVersion().string());

	SKSE::Init(a_skse);

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::stl::report_and_fail("Unable to register the SKSE message listener.");
	}
	CFNG::Game::InstallSerialization();
	SKSE::GetPapyrusInterface()->Register(CFNG::Papyrus::Register);

	logger::info("{} loaded", plugin->GetName());
	return true;
}
