#include "Game/Runtime.h"

#include "Core/JsonLoader.h"
#include "Game/Forms.h"
#include "Game/Tracker.h"

namespace CFNG::Game
{
	namespace
	{
		constexpr std::uint32_t kUniqueID = 'CFNG';
		constexpr std::uint32_t kStripRecord = 'STRP';
		constexpr std::uint32_t kActiveModRecord = 'AMOD';
		constexpr std::uint32_t kRecordVersion = 1;

		FormID Resolve(SKSE::SerializationInterface* a_intfc, FormID a_old)
		{
			if (a_old == 0) {
				return 0;
			}
			FormID resolved = 0;
			return a_intfc->ResolveFormID(a_old, resolved) ? resolved : 0;
		}

		void SaveActiveMods(SKSE::SerializationInterface* a_intfc)
		{
			const auto overrides = GetRegistry().GetActiveModOverrides();
			if (!a_intfc->OpenRecord(kActiveModRecord, kRecordVersion)) {
				logger::error("Could not open active mod record");
				return;
			}
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(overrides.size()));
			for (const auto& [key, modId] : overrides) {
				a_intfc->WriteRecordData(key.first);
				a_intfc->WriteRecordData(key.second);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(modId.size()));
				if (!modId.empty()) {
					a_intfc->WriteRecordData(modId.data(), static_cast<std::uint32_t>(modId.size()));
				}
			}
		}

		void LoadActiveMods(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t count = 0;
			a_intfc->ReadRecordData(count);
			std::map<std::pair<FormID, FormID>, std::string> overrides;
			for (std::uint32_t i = 0; i < count; ++i) {
				FormID        race = 0;
				FormID        skin = 0;
				std::uint32_t length = 0;
				a_intfc->ReadRecordData(race);
				a_intfc->ReadRecordData(skin);
				a_intfc->ReadRecordData(length);
				std::string modId(length, '\0');
				if (length) {
					a_intfc->ReadRecordData(modId.data(), length);
				}
				race = Resolve(a_intfc, race);
				skin = Resolve(a_intfc, skin);
				if (race && skin) {
					overrides[{ race, skin }] = std::move(modId);
				}
			}
			auto& registry = GetRegistry();
			registry.SetActiveModOverrides(std::move(overrides));
			registry.ApplyActiveModOverrides();
		}

		void OnSave(SKSE::SerializationInterface* a_intfc)
		{
			StripStore::Get().Save(a_intfc);
			SaveActiveMods(a_intfc);
		}

		void OnLoad(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t type = 0;
			std::uint32_t version = 0;
			std::uint32_t length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (version != kRecordVersion) {
					logger::warn("Skipping cosave record {:08X} with version {}", type, version);
					continue;
				}
				switch (type) {
				case kStripRecord:
					StripStore::Get().Load(a_intfc, version, length);
					break;
				case kActiveModRecord:
					LoadActiveMods(a_intfc);
					break;
				default:
					logger::warn("Unknown cosave record {:08X}", type);
					break;
				}
			}
		}

		void OnRevert(SKSE::SerializationInterface*)
		{
			Tracker::Get().Reset();
			StripStore::Get().Clear();
			GetRegistry().SetActiveModOverrides({});
			ReloadRegistrations();
		}
	}

	Registry& GetRegistry()
	{
		static Registry registry;
		return registry;
	}

	void ReloadRegistrations()
	{
		auto& registry = GetRegistry();
		const auto& forms = Forms::Get();
		registry.UnregisterAllMods();
		registry.SetFakeSkin(forms.fakeSkin ? forms.fakeSkin->GetFormID() : 0);

		const auto sources = ReadJsonDirectory("Data/creatures.d");
		const auto report = LoadRegistrations(registry, sources, ResolveFormString);
		for (const auto& message : report.messages) {
			logger::warn("{}", message);
		}
		registry.ApplyActiveModOverrides();
		logger::info("creatures.d: {} files seen, {} loaded, {} creature registrations ({} entries)", report.filesSeen, report.filesLoaded, report.creaturesRegistered, registry.CreatureCount());
	}

	StripStore& StripStore::Get()
	{
		static StripStore singleton;
		return singleton;
	}

	Equipment::StrippedGear StripStore::Take(FormID a_actor)
	{
		std::scoped_lock lock(_lock);
		const auto       it = _gear.find(a_actor);
		if (it == _gear.end()) {
			return {};
		}
		auto gear = it->second;
		_gear.erase(it);
		return gear;
	}

	Equipment::StrippedGear StripStore::Peek(FormID a_actor) const
	{
		std::scoped_lock lock(_lock);
		const auto       it = _gear.find(a_actor);
		return it != _gear.end() ? it->second : Equipment::StrippedGear{};
	}

	void StripStore::Put(FormID a_actor, const Equipment::StrippedGear& a_gear)
	{
		std::scoped_lock lock(_lock);
		if (a_gear.Empty()) {
			_gear.erase(a_actor);
		} else {
			_gear[a_actor] = a_gear;
		}
	}

	void StripStore::Clear()
	{
		std::scoped_lock lock(_lock);
		_gear.clear();
	}

	void StripStore::Save(SKSE::SerializationInterface* a_intfc) const
	{
		std::scoped_lock lock(_lock);
		if (!a_intfc->OpenRecord(kStripRecord, kRecordVersion)) {
			logger::error("Could not open stripped gear record");
			return;
		}
		a_intfc->WriteRecordData(static_cast<std::uint32_t>(_gear.size()));
		for (const auto& [actor, gear] : _gear) {
			a_intfc->WriteRecordData(actor);
			for (const auto id : gear.armor) {
				a_intfc->WriteRecordData(id);
			}
			a_intfc->WriteRecordData(gear.leftHand);
			a_intfc->WriteRecordData(gear.rightHand);
		}
	}

	void StripStore::Load(SKSE::SerializationInterface* a_intfc, std::uint32_t, std::uint32_t)
	{
		std::scoped_lock lock(_lock);
		_gear.clear();
		std::uint32_t count = 0;
		a_intfc->ReadRecordData(count);
		for (std::uint32_t i = 0; i < count; ++i) {
			FormID                  actor = 0;
			Equipment::StrippedGear gear;
			a_intfc->ReadRecordData(actor);
			for (auto& id : gear.armor) {
				a_intfc->ReadRecordData(id);
				id = Resolve(a_intfc, id);
			}
			a_intfc->ReadRecordData(gear.leftHand);
			a_intfc->ReadRecordData(gear.rightHand);
			gear.leftHand = Resolve(a_intfc, gear.leftHand);
			gear.rightHand = Resolve(a_intfc, gear.rightHand);
			actor = Resolve(a_intfc, actor);
			if (actor && !gear.Empty()) {
				_gear[actor] = gear;
			}
		}
	}

	void InstallSerialization()
	{
		auto* serialization = SKSE::GetSerializationInterface();
		serialization->SetUniqueID(kUniqueID);
		serialization->SetSaveCallback(OnSave);
		serialization->SetLoadCallback(OnLoad);
		serialization->SetRevertCallback(OnRevert);
	}
}
