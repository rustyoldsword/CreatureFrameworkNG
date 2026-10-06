// Game-free tests for Core/Registry and Core/JsonLoader.
// Every expectation is derived from CreatureFramework.psc or JContainers source (docs/DESIGN.md).

#include "Core/JsonLoader.h"
#include "Core/Registry.h"

#include <cstdio>
#include <map>
#include <string>

using namespace CFNG;

namespace
{
	int g_failures = 0;
	int g_checks = 0;

#define CHECK(expr)                                                                     \
	do {                                                                                \
		++g_checks;                                                                     \
		if (!(expr)) {                                                                  \
			++g_failures;                                                               \
			std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);                 \
		}                                                                               \
	} while (false)

	constexpr FormID kFakeSkin = 0x020086F1;
	constexpr FormID kDraugrRace = 0x00000D53;
	constexpr FormID kDogRace = 0x000131EE;
	constexpr FormID kSkinDraugr0 = 0x00016EE3;
	constexpr FormID kSkinDraugr1 = 0x0001FE7E;
	constexpr FormID kArmorA = 0x050012CD;
	constexpr FormID kArmorB = 0x05002300;

	// "__formData|<plugin>|<id>" resolved against a small fake load order
	FormResolver MakeResolver()
	{
		static const std::map<std::string, std::pair<FormID, FormKind>, ILess> table{
			{ "__formData|Skyrim.esm|0xd53", { kDraugrRace, FormKind::kRace } },
			{ "__formData|Skyrim.esm|0x131ee", { kDogRace, FormKind::kRace } },
			{ "__formData|Skyrim.esm|0x16ee3", { kSkinDraugr0, FormKind::kArmor } },
			{ "__formData|Skyrim.esm|0x1fe7e", { kSkinDraugr1, FormKind::kArmor } },
			{ "__formData|MoreNastyCritters.esp|0x12cd", { kArmorA, FormKind::kArmor } },
			{ "__formData|MoreNastyCritters.esp|0x2300", { kArmorB, FormKind::kArmor } },
		};
		return [](std::string_view a_str, FormKind a_kind) -> FormID {
			const auto it = table.find(a_str);
			if (it == table.end()) {
				return 0;
			}
			return (a_kind == FormKind::kAny || a_kind == it->second.second) ? it->second.first : 0;
		};
	}

	// Registry owns a shared_mutex, so it is built in place rather than returned
	void Init(Registry& a_registry)
	{
		a_registry.SetFakeSkin(kFakeSkin);
	}

	void TestStringCompare()
	{
		CHECK(IEquals("ModName", "modname"));
		CHECK(!IEquals("mod", "mods"));
		// _stricmp lower-cases first, so '_' (0x5F) sorts before letters
		CHECK(ICompare("a_b", "aab") < 0);
		CHECK(ICompare("MNC.json", "MNCDragon1.json") < 0);
		CHECK(ICompare("b", "A") > 0);
	}

	void TestFileOrderDecidesActiveMod()
	{
		Registry registry;
		Init(registry);
		const std::vector<JsonSource> sources{
			{ "Zeta.json", R"({"modID":"Z","modName":"Zeta","creatures":[{"raceForm":"__formData|Skyrim.esm|0xd53","raceName":"Draugr","skinForm":"__formData|Skyrim.esm|0x16ee3","arousedArmor":"__formData|MoreNastyCritters.esp|0x2300"}]})" },
			{ "alpha.json", R"({"modID":"A","modName":"Alpha","creatures":[{"raceForm":"__formData|Skyrim.esm|0xd53","raceName":"Draugr","skinForm":"__formData|Skyrim.esm|0x16ee3","arousedArmor":"__formData|MoreNastyCritters.esp|0x12cd"}]})" },
		};
		const auto report = LoadRegistrations(registry, sources, MakeResolver());
		CHECK(report.filesLoaded == 2);
		CHECK(report.creaturesRegistered == 2);
		// alpha.json sorts before Zeta.json case-insensitively, so A registered first
		CHECK(registry.GetActiveMod(kDraugrRace, kSkinDraugr0) == "A");
		const auto mods = registry.GetModsRegisteredWithCreature(kDraugrRace, kSkinDraugr0);
		CHECK(mods.size() == 2 && mods[0] == "A" && mods[1] == "Z");
		CHECK(registry.GetActiveModIndex(kDraugrRace, kSkinDraugr0) == 0);
	}

	void TestCaseInsensitiveKeysAndDefaults()
	{
		Registry registry;
		Init(registry);
		// the exact key spelling MNC ships: ModName / RaceName
		const std::vector<JsonSource> sources{
			{ "MNC.json", R"({"modID":"MNC","ModName":"More Nasty Critters","creatures":[
				{"skinName":"Draugr 0","normalArmor":null,"arousedArmor":"__formData|MoreNastyCritters.esp|0x12cd","RaceName":"Draugr","raceForm":"__formData|Skyrim.esm|0xd53","skinForm":"__formData|Skyrim.esm|0x16ee3"},
				{"skinName":"Draugr 1","normalArmor":null,"arousedArmor":"__formData|MoreNastyCritters.esp|0x2300","RaceName":"draugr","raceForm":"__formData|Skyrim.esm|0xd53","skinForm":"__formData|Skyrim.esm|0x1fe7e","stripArmor":false,"restrictedSlots":[]}
			]})" },
		};
		const auto report = LoadRegistrations(registry, sources, MakeResolver());
		CHECK(report.filesLoaded == 1);
		CHECK(report.creaturesRegistered == 2);
		CHECK(registry.GetModName("mnc") == "More Nasty Critters");
		CHECK(registry.GetRaceName(kDraugrRace) == "Draugr");  // first registrant names the race
		CHECK(registry.GetSkinName(kSkinDraugr1) == "Draugr 1");

		const auto reg = registry.GetCreatureModReg("MNC", kDraugrRace, kSkinDraugr0);
		CHECK(reg.has_value());
		CHECK(reg && reg->type == kArmors);
		CHECK(reg && reg->normalArmor == 0);
		CHECK(reg && reg->arousedArmor == kArmorA);
		CHECK(reg && !reg->stripArmor);   // absent -> JMap.GetInt default 0
		CHECK(reg && reg->stripWeapons);  // absent -> true

		CHECK(registry.GetRegisteredRaces() == std::vector<FormID>{ kDraugrRace });
		CHECK((registry.GetSkinsRegisteredToRace(kDraugrRace) == std::vector<FormID>{ kSkinDraugr0, kSkinDraugr1 }));
	}

	void TestRejectedFilesAndCreatures()
	{
		Registry registry;
		Init(registry);
		const std::vector<JsonSource> sources{
			{ "a.json", "not json" },
			{ "b.json", "[]" },
			{ "c.json", R"({"modID":"C","creatures":[{"raceForm":"__formData|Skyrim.esm|0xd53","raceName":"Draugr"}]})" },
			{ "d.json", R"({"modID":"D","modName":"D","creatures":[]})" },
			{ "e.json", R"({"modID":"E","modName":"E","creatures":[{"raceForm":"__formData|Skyrim.esm|0xd53"},{"raceForm":"__formData|Missing.esp|0x1","raceName":"X"},{"raceForm":"__formData|Skyrim.esm|0x16ee3","raceName":"wrong type"}]})" },
		};
		const auto report = LoadRegistrations(registry, sources, MakeResolver());
		CHECK(report.filesSeen == 5);
		CHECK(report.filesLoaded == 1);  // e.json has a valid header even though every creature is rejected
		CHECK(report.creaturesRegistered == 0);
		CHECK(registry.IsModRegistered("E"));
		CHECK(!registry.IsModRegistered("C"));
		CHECK(registry.CreatureCount() == 0);
	}

	void TestFakeSkinAndResolve()
	{
		Registry registry;
		Init(registry);
		const std::vector<JsonSource> sources{
			{ "dog.json", R"({"modID":"Dogs","modName":"Dogs","creatures":[{"raceForm":"__formData|Skyrim.esm|0x131ee","raceName":"Dog","skinName":"ignored","arousedArmor":"__formData|MoreNastyCritters.esp|0x12cd"}]})" },
		};
		LoadRegistrations(registry, sources, MakeResolver());
		CHECK(registry.IsExactCreatureRegistered(kDogRace, 0));
		CHECK(registry.IsCreatureRegistered(kDogRace, kSkinDraugr0, true));
		CHECK(!registry.IsCreatureRegistered(kDogRace, kSkinDraugr0, false));
		CHECK(registry.GetSkinName(kFakeSkin).empty());

		// an unregistered skin falls back to the fake-skin entry
		const auto resolved = registry.Resolve(kDogRace, { 0, kSkinDraugr0, 0 });
		CHECK(resolved.has_value());
		CHECK(resolved && resolved->skin == kFakeSkin);
		CHECK(resolved && resolved->activeMod == "Dogs");
		CHECK(resolved && resolved->reg && resolved->reg->arousedArmor == kArmorA);
		CHECK(!registry.Resolve(kDraugrRace, { kSkinDraugr0 }).has_value());
	}

	void TestSetActiveModAndOverrides()
	{
		Registry registry;
		Init(registry);
		registry.RegisterMod("A", "Alpha");
		registry.RegisterMod("B", "Beta");
		CreatureModReg reg;
		CHECK(registry.RegisterCreature("A", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg));
		CHECK(registry.RegisterCreature("B", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg));
		CHECK(!registry.RegisterCreature("Nope", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg));

		CHECK(!registry.SetActiveMod(kDraugrRace, kSkinDraugr0, "Unknown"));
		CHECK(registry.GetActiveMod(kDraugrRace, kSkinDraugr0) == "A");
		CHECK(registry.SetActiveMod(kDraugrRace, kSkinDraugr0, "b"));
		CHECK(registry.GetActiveMod(kDraugrRace, kSkinDraugr0) == "b");
		CHECK(registry.GetActiveModIndex(kDraugrRace, kSkinDraugr0) == 1);

		const auto overrides = registry.GetActiveModOverrides();
		CHECK(overrides.size() == 1);

		// rebuild from scratch and re-apply the saved choice
		Registry rebuilt;
		Init(rebuilt);
		rebuilt.RegisterMod("A", "Alpha");
		rebuilt.RegisterMod("B", "Beta");
		rebuilt.RegisterCreature("A", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg);
		rebuilt.RegisterCreature("B", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg);
		rebuilt.SetActiveModOverrides(overrides);
		rebuilt.ApplyActiveModOverrides();
		CHECK(rebuilt.GetActiveMod(kDraugrRace, kSkinDraugr0) == "b");

		CHECK(registry.SetActiveMod(kDraugrRace, kSkinDraugr0, ""));
		CHECK(registry.GetActiveMod(kDraugrRace, kSkinDraugr0).empty());
		CHECK(registry.GetActiveModIndex(kDraugrRace, kSkinDraugr0) == -1);
	}

	void TestUnregisterMod()
	{
		Registry registry;
		Init(registry);
		registry.RegisterMod("A", "Alpha");
		registry.RegisterMod("B", "Beta");
		CreatureModReg reg;
		registry.RegisterCreature("A", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg);
		registry.RegisterCreature("B", kDraugrRace, kSkinDraugr0, "Draugr", "D0", reg);
		registry.RegisterCreature("B", kDogRace, 0, "Dog", "", reg);

		const auto changed = registry.UnregisterMod("a");
		CHECK(changed.size() == 1);
		CHECK(!registry.IsModRegistered("A"));
		CHECK(registry.GetActiveMod(kDraugrRace, kSkinDraugr0).empty());
		CHECK((registry.GetModsRegisteredWithCreature(kDraugrRace, kSkinDraugr0) == std::vector<std::string>{ "B" }));
		CHECK((registry.GetModsRegisteredWithRace(kDraugrRace) == std::vector<std::string>{ "B" }));
		// Dog never had A; the script's EraseIndex(-1) would have dropped B here
		CHECK((registry.GetModsRegisteredWithRace(kDogRace) == std::vector<std::string>{ "B" }));
	}

	void TestSkinMapReplacedAcrossRaces()
	{
		Registry registry;
		Init(registry);
		registry.RegisterMod("A", "Alpha");
		CreatureModReg reg;
		registry.RegisterCreature("A", kDraugrRace, kSkinDraugr0, "Draugr", "first", reg);
		registry.RegisterCreature("A", kDogRace, kSkinDraugr0, "Dog", "second", reg);
		// JFormMap.SetObj(jSkinsMap, skin, new map) replaced the name and the mod list
		CHECK(registry.GetSkinName(kSkinDraugr0) == "second");
		CHECK(registry.GetModsRegisteredWithSkin(kSkinDraugr0).empty());
		CHECK(registry.IsSkinRegisteredToMod("A", kSkinDraugr0));
	}
}

int main()
{
	TestStringCompare();
	TestFileOrderDecidesActiveMod();
	TestCaseInsensitiveKeysAndDefaults();
	TestRejectedFilesAndCreatures();
	TestFakeSkinAndResolve();
	TestSetActiveModAndOverrides();
	TestUnregisterMod();
	TestSkinMapReplacedAcrossRaces();

	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
