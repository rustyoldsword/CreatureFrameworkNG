# Creature Framework NG — design

Native SKSE backend for Creature Framework 4.0 (MadMansGun build). Goal: same behaviour for
MoreNastyCritters and the MCM, with the Papyrus hot paths removed and the original bugs fixed.
Every rule below is traced to the file and line it was derived from.

## What stays

- `CreatureFramework.esm` untouched: quests `CFQuestFramework` (0xCF01), `CFQuestConfiguration`
  (0xCF02), `CFQuestDetectCreature` (0xCF03), keywords `CFIgnoreThisCreature` (0x68F4),
  `CFArmorNormal` (0x68F5), `CFArmorAroused` (0x68F6), globals `CFCloakRange` (0x86F2),
  `CFCloakCreatures` (0x86F3), armor `CFFakeSkin` (0x86F1), spell `CFTargetPuppetSpell`.
- Script names and every public function signature of `CreatureFramework.psc`, so saves,
  the MCM (`CreatureFrameworkConfig.psc`, unchanged) and third-party callers still bind.
- `Data/creatures.d/*.json` as the registration source.

## What moves native

| Papyrus today | Native |
|---|---|
| JContainers maps `mods/races/skins/creatures` | `Registry` (C++ maps, rebuilt from JSON each load) |
| `CFQuestDetectCreature` Start/Stop every `PrfTimeout` | `Scanner`: walks `ProcessLists` high actors on the main thread |
| `OnUpdate` loop with `Utility.Wait(0.1)` per actor | `Scanner` tick, no waits |
| `ChangeArousal` / `RemoveArmors` / `AddAndEquipArmor` | `ArmorSwapper` via `ActorEquipManager`, queued with `SKSE::GetTaskInterface()` |
| `StripA` / `EquipA` + `JFormDB .CFForm.Stripped*` | `StripStore`, persisted in the SKSE cosave |
| `AnimationStart/End`, `ActorChangeStart/End` mod events | `SKSE::ModCallbackEvent` sink |
| `GetFactionRank(slaArousal)` | `OSLAroused.dll!GetArousalExt` when present, faction rank otherwise |

Papyrus keeps only thin wrappers (`native` globals on `CreatureFrameworkNative`) plus the few
things that need Papyrus: firing `CFArousalChange_<modId>` mod events with the original
argument layout, and the MCM.

## Registration rules (must match JContainers exactly)

- Directory `Data/creatures.d`, non recursive, extension compared **case-sensitively** to
  `.json` (`JContainers tes_object.h:268-270`). Unparseable files are skipped.
- Files are processed in **case-insensitive filename order** — `ReadFromDirectory` returns a
  JMap, whose `std::map` uses `_stricmp` (`collections.h:363-370`). Order decides which mod
  becomes `activeMod` of a new creature (first registrant wins, `CreatureFramework.psc:332-333`).
- All object keys are **case-insensitive** (MNC writes `ModName`/`RaceName`; CF reads
  `modName`/`raceName` and still works).
- String equality for mod ids is case-insensitive (`item.h:268-270`, used by `JArray.FindStr`).
- Form strings `__formData|<plugin>|<id>`: id parsed with base auto-detect (`stoul(...,0)`);
  low 24 bits for ESP/ESM, low 12 bits for ESL (`form_handling.h:130-178`).
- A file needs non-empty `modID`, `modName` and a non-empty `creatures` array
  (`CreatureFramework.psc:667`); a creature needs `raceForm` resolving to a Race and non-empty
  `raceName` (`:677`). Missing `skinForm` → `CFFakeSkin` (`:306-310`).
- Defaults: `stripArmor` = false when absent (`JMap.GetInt` default 0, `:683`); `stripWeapons` =
  true when absent (`:684`). JSON registrations are always type 1, armor swap (`:688`).
- A creature already registered to the same mod is not re-registered (`:679`).

Data files seen in this setup: 19 MNC files, 312 creature entries, keys `modID`, `ModName`,
`creatures`, `skinName`, `RaceName`, `raceForm`, `skinForm`, `normalArmor`, `arousedArmor`,
`stripArmor` (153, all false), `restrictedSlots` (2, empty).

## Creature lookup (`GetActorMapA`, `:806-830`)

Race = leveled base race. Skin = `GetActorBase().GetSkin()` → leveled base skin → `GetRace().GetSkin()`.
Use `(race, skin)` if registered, otherwise `(race, CFFakeSkin)`. Active only when that entry
has a non-empty `activeMod`.

## Detection (replaces the quest aliases)

Alias conditions decoded from the ESM with xEdit's function table:
`IsActor == 1`, `HasLoaded3D == 1`, `HasKeyword(ActorTypeNPC) == 0`,
`HasKeyword(CFIgnoreThisCreature) == 0`, `WornHasKeyword(CFIgnoreThisCreature) == 0`,
`WornHasKeyword(CFArmorAroused) == 0`, `GetDistance(Player) <= CFCloakRange`.
Script-side extras: not flying (state 1-4), not in a scene, not in a kill move
(`IsValidForUpdate`, `:1423-1428`); cap `CFCloakCreatures` new detections per pass; 32 tracked
slots.

Release rules: unloaded → drop; disabled or farther than `CloakRange + 1000` → remove CF armors,
restore stripped gear, drop (`:1546-1552`).

## Arousal state (`ChangeArousal`, `:1640-1812`)

Aroused when in a SexLab scene (`SexLabAnimatingFaction` 0x00E50F in SexLab.esm, which is what
`IsActorActive` checks) or arousal ≥ `GenArousalThreshold` with `GenAroused` on.
Type 1: remove both CF armor kinds, then equip `arousedArmor` (falls back to `normalArmor`) or
`normalArmor`. Skip when a restricted slot is occupied and not stripping for sex.
Scene start strips armor slots head/body/hands/feet/ring/amulet and both hands unless
blacklisted, and sends `SOSFastErect` (`:1286`). Scene end restores.

## Fixed original bugs

1. `GenericEquip` equips the actor onto itself instead of the stored weapon, so weapons are
   never given back (`CreatureFrameworkUtility.psc:144`).
2. `OnModifyExposure` tests `!IsCreature`, so it never handles creatures (`:1323`).
3. `rcGuard` drops a state change after 0.3 s instead of queueing it (`:1649-1657`).
4. `AddAndEquipArmor` loops `while !IsEquipped` with no bound (`Utility.psc:134-136`).
5. `Log()` fetches the config form on every call even with logging off (`Utility.psc:225-233`).
   Hot paths are native now, so the remaining calls are rare.
6. Merely aroused creatures are stripped: OnUpdate calls `ChangeArousal(i, 1, true)` for
   "in scene **or** aroused", and state 1 means "playing animation" (`:1553-1558`). NG strips only
   while the actor is in `SexLabAnimatingFaction`.
7. Stripped gear only comes back when the creature leaves range (`EquipA` is reached from the
   release paths only). NG restores it when the scene ends.
8. The MCM's `OnOptionDefault` passes `-1` as the mod id, so "default" never disabled anything
   (`Config.psc:638`). NG treats `-1` as disable.
9. Type 2 ("armor swap and events") never swapped armor — `ChangeArousal` checks `type == 1`
   only (`:1697`). NG swaps for types 1 and 2.
10. `UnregisterAllCreaturesFromMod` erases `FindStr(...)` even when it is -1, and JArray reads -1
    as "last element", deleting an unrelated mod from races and skins (`:425`, `:434`).

## Plugin changes (tools/rebuild_esm.py)

- Every record header, TES4 included, is form 44; 4.0 shipped form 43.
- CreatureFramework.esm 4.0 carried Skyrim LE copies of five vanilla records at form 40:
  `MudcrabRace` (0BA545), `MQ101Alduin` (032B94), `MQ206AncientAlduin` (04377F), `MQ101Horse`
  (0654E5), `MQ101CartHorse` (0B9E1D). Compared with Skyrim.esm SE the only intended change is the
  added `CFIgnoreThisCreature` keyword; the rest reverted SE data (keyword lists, ACBS flags,
  spell/faction order, the horse's VMAD). They cannot simply be re-copied from SE because
  Skyrim.esm is localized and CreatureFramework.esm is not. NG drops the overrides and
  `Forms::ApplyIgnoreKeywords` adds the keyword to the same forms at kDataLoaded.
- CF-owned records are byte-identical to 4.0; HEDR record count 24 -> 17.

## Deliberate differences

- Registrations are rebuilt from `creatures.d` on every data load instead of living in JDB, so
  edited or removed JSON takes effect; MCM active-mod choices persist in the cosave.
- Creatures already wearing `CFArmorAroused` are tracked too (the alias excluded them), so a
  creature left aroused by an old save or a crash is cleaned up.
- Native pass every `fTickSeconds` (default 1 s) instead of every `PrfTimeout` (15 s); the MCM
  cooldown slider no longer drives it.
- Pure registry queries are registered as callable from tasklets, so Papyrus callers do not wait
  for a frame.

## Save migration

On the first load with NG, the Papyrus wrapper walks the old JDB `.CreatureFramework.creatures`
map once and hands every non-default `activeMod` choice to native, and moves
`.CFForm.Stripped*` entries into the cosave. Afterwards JDB is not touched.

## Verification gates

1. Registry parity: CF's own `Dump()` JSON from the original scripts vs `CFNG dump` output,
   compared per race/skin/mod/activeMod by `tools/compare_dump.py`.
2. Unit tests for the JSON rules above (ordering, case, form strings, defaults).
3. In-game log markers for every equip/unequip, plus a timing baseline before/after on the same
   scene.
