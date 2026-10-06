Scriptname CreatureFrameworkNative Hidden
{Native backend of Creature Framework NG (CreatureFrameworkNG.dll). Signatures must match src/Papyrus/Natives.cpp.}

; Registration
Function ReloadRegistrations() global native
Function RegisterMod(string modId, string modName) global native
Function UnregisterMod(string modId) global native
Function UnregisterAllMods() global native
bool Function RegisterCreatureToMod(string modId, Race raceForm, Armor skinForm, string raceName, string skinName, int regType, Armor normalArmor, Armor arousedArmor, bool stripArmor, bool stripWeapons, Form[] stripFormBlacklist, int[] stripSlotBlacklist, int[] restrictedSlots) global native

; Queries
bool Function IsModRegistered(string modId) global native
string Function GetModName(string modId) global native
string[] Function GetRegisteredMods() global native

bool Function IsCreatureRegistered(Race raceForm, Armor skinForm, bool checkFake) global native
bool Function IsExactCreatureRegistered(Race raceForm, Armor skinForm) global native
bool Function IsCreatureRegisteredToMod(string modId, Race raceForm, Armor skinForm) global native

Form[] Function GetRegisteredRaces() global native
bool Function IsRaceRegistered(Race raceForm) global native
Form[] Function GetRacesRegisteredToMod(string modId) global native
bool Function IsRaceRegisteredToMod(string modId, Race raceForm) global native

Form[] Function GetRegisteredSkins() global native
bool Function IsSkinRegistered(Armor skinForm) global native
Form[] Function GetSkinsRegisteredToMod(string modId) global native
bool Function IsSkinRegisteredToMod(string modId, Armor skinForm) global native
Form[] Function GetSkinsRegisteredToRace(Race raceForm) global native

string[] Function GetModsRegisteredWithRace(Race raceForm) global native
string[] Function GetModsRegisteredWithSkin(Armor skinForm) global native
string[] Function GetModsRegisteredWithCreature(Race raceForm, Armor skinForm) global native

int Function GetModCreatureType(string modId, Race raceForm, Armor skinForm) global native
Armor Function GetModCreatureNormalArmor(string modId, Race raceForm, Armor skinForm) global native
Armor Function GetModCreatureArousedArmor(string modId, Race raceForm, Armor skinForm) global native
bool Function GetModCreatureStripArmor(string modId, Race raceForm, Armor skinForm) global native
bool Function GetModCreatureStripWeapons(string modId, Race raceForm, Armor skinForm) global native
Form[] Function GetModCreatureStripFormBlacklist(string modId, Race raceForm, Armor skinForm) global native
int[] Function GetModCreatureStripSlotBlacklist(string modId, Race raceForm, Armor skinForm) global native
int[] Function GetModCreatureRestrictedSlots(string modId, Race raceForm, Armor skinForm) global native

string Function GetRaceName(Race raceForm) global native
string Function GetSkinName(Armor skinForm) global native
string Function GetActiveMod(Race raceForm, Armor skinForm) global native
int Function GetActiveModIndex(Race raceForm, Armor skinForm) global native
Function SetActiveMod(Race raceForm, Armor skinForm, string modId) global native

; Actors
bool Function IsActorActive(Actor actorForm) global native
bool Function IsCreature(Actor actorForm) global native
bool Function IsInScene(Actor actorForm) global native
int Function GetArousal(Actor actorForm) global native
bool Function IsArousalAvailable() global native
Function TriggerUpdateForActor(Actor actorForm) global native
Function RequestUpdate() global native
Function ClearCreatures() global native
Function MigrateStrippedGear(Actor actorForm, Form[] armor, Form leftHand, Form rightHand) global native

; Misc
string Function GetVersionString() global native
bool Function Dump(string path) global native
