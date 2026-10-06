Scriptname CreatureFramework extends Quest
{The framework script to interact with | Creature Framework NG
 Same public API as Creature Framework 4.0; the registry, detection and armour swapping run in
 CreatureFrameworkNG.dll (CreatureFrameworkNative). Functions returning JContainers handles
 build them on demand from the native data.}

; General properties (bound by CreatureFramework.esm; names and types must not change)
CreatureFrameworkConfig property Config auto hidden
Armor property CFFakeSkin auto
Keyword property ActorTypeNPC auto
Keyword property CFArmorAroused auto
Keyword property CFArmorNormal auto
Keyword property CFIgnoreThisCreature auto
SexLabFramework property SexLab auto hidden
slaUtilScr property SexLabAroused auto hidden
Faction property ArousedFaction auto hidden
Spell property CFTargetPuppetSpell auto
Quest Property CFQuestDetectCreature Auto
Actor Property PlayerRef Auto

; The currently installed version
int version = 0

; JContainers (events are still kept in JDB for FireEvent's API)
int jMainMap = -1
int jEventsMap = -1

; Registration types
string[] types

; Armour slots
int[] armorSlots
string[] arousalSources

; Counter for how many things are telling us to prevent mucking about
int muckingAboutPreventor = 0

Actor puppet

; Initialise (needs to be run on each startup)
function Initialize()
	muckingAboutPreventor += 1
	Config = CreatureFrameworkUtility.GetConfig()
	UnregisterForAllModEvents()
	; saves from the Papyrus version still hold the 15 s detection loop
	UnregisterForUpdate()

	if JContainers.APIVersion() >= 3 && JContainers.IsInstalled()
	else
		CreatureFrameworkUtility.Log("[Framework] Aborting initialisation")
		Debug.MessageBox("Creature Framework's startup initialisation has failed, please make sure JContainers is installed and running correctly.")
		muckingAboutPreventor -= 1
		return
	endIf

	if CreatureFrameworkNative.GetVersionString() == ""
		Debug.MessageBox("Creature Framework NG: CreatureFrameworkNG.dll is not loaded. Check SKSE and Address Library.")
		muckingAboutPreventor -= 1
		return
	endIf

	jMainMap = JDB.SolveObj(".CreatureFramework")
	if jMainMap == 0
		jMainMap = JMap.Object()
		JDB.SetObj("CreatureFramework", jMainMap)
	endIf
	jEventsMap = InitializeContainer("events")

	types = new string[3]
	types[0] = "events"
	types[1] = "armors"
	types[2] = "both"

	armorSlots = new int[6]
	armorSlots[0] = 0x00000001 ; Head
	armorSlots[1] = 0x00000004 ; Body
	armorSlots[2] = 0x00000008 ; Hands
	armorSlots[3] = 0x00000080 ; Feet
	armorSlots[4] = 0x00000040 ; Ring (for vampire lords)
	armorSlots[5] = 0x00000020 ; Amulet (for vampire lords)

	arousalSources = new string[3]
	arousalSources[0] = "$None"
	arousalSources[1] = "$SexLab animation"
	arousalSources[2] = "$SexLab Aroused"

	if Game.GetModByName("SexLab.esm") != 255
		SexLab = SexLabUtil.GetAPI()
	else
		SexLab = none
	endIf

	if Game.GetModByName("SexLabAroused.esm") != 255
		SexLabAroused = Game.GetFormFromFile(0x4290F, "SexLabAroused.esm") as slaUtilScr
		if SexLabAroused
			ArousedFaction = SexLabAroused.slaArousal
		else
			ArousedFaction = none
		endIf
	else
		SexLabAroused = none
		ArousedFaction = none
	endIf

	Int t = CreatureFrameworkUtility.GetVersion()
	if version == 0
		CreatureFrameworkUtility.Log("[Framework] Installed")
		Debug.Notification("$CF_InstallNotification")
	elseIf version < t
		CreatureFrameworkUtility.Log("[Framework] Upgraded from version " + version)
		Debug.Notification("$CF_UpdateNotification")
	elseIf version > t
		CreatureFrameworkUtility.Log("[Framework] Downgraded from version " + version)
		Debug.MessageBox("Creature Framework has been downgraded to " + GetVersionString() + ". Things will be broken!")
		muckingAboutPreventor -= 1
		return
	endIf
	version = t

	if JMap.GetInt(jMainMap, "ngMigrated") == 0
		MigrateFromPapyrusVersion()
		JMap.SetInt(jMainMap, "ngMigrated", 1)
	endIf

	; NG raised the default arousal threshold from 35 to 70: with OSL Aroused most creatures sit
	; above 35 and stay erect outside scenes. Only an untouched old default is moved, once.
	if JMap.GetInt(jMainMap, "ngThresholdDefault") == 0
		if Config.GenArousalThreshold == 35
			Config.GenArousalThreshold = 70
			CreatureFrameworkUtility.Log("[Framework] Raised the arousal threshold default from 35 to 70")
		endIf
		JMap.SetInt(jMainMap, "ngThresholdDefault", 1)
	endIf

	; give other mods' OnGameReload a moment to register for CFRegister, like the original did
	Utility.Wait(3)
	SendRegisterEvent()
	CreatureFrameworkNative.RequestUpdate()

	muckingAboutPreventor -= 1
	if muckingAboutPreventor < 0
		muckingAboutPreventor = 0
	endIf

	UnregisterForAllKeys()
	RegisterForKey(Config.PupTargetKey)
	CreatureFrameworkUtility.Log("[Framework] Creature Framework NG " + CreatureFrameworkNative.GetVersionString() + " is done initialising")
endFunction

; Initialize a JMap container under the main map if necessary
int function InitializeContainer(string containerKey)
	if JMap.ValueType(jMainMap, containerKey) == 5
		return JMap.GetObj(jMainMap, containerKey)
	endIf
	int obj = JMap.Object()
	JMap.SetObj(jMainMap, containerKey, obj)
	return obj
endFunction

; One-time import of the JDB data the Papyrus version kept: MCM active-mod choices and stripped gear
function MigrateFromPapyrusVersion()
	int jCreatures = JMap.GetObj(jMainMap, "creatures")
	if jCreatures
		Form raceKey = JFormMap.NextKey(jCreatures)
		while raceKey
			int jBySkin = JFormMap.GetObj(jCreatures, raceKey)
			Form skinKey = JFormMap.NextKey(jBySkin)
			while skinKey
				string oldActive = JMap.GetStr(JFormMap.GetObj(jBySkin, skinKey), "activeMod")
				if oldActive != CreatureFrameworkNative.GetActiveMod(raceKey as Race, skinKey as Armor)
					CreatureFrameworkNative.SetActiveMod(raceKey as Race, skinKey as Armor, oldActive)
				endIf
				skinKey = JFormMap.NextKey(jBySkin, skinKey)
			endWhile
			raceKey = JFormMap.NextKey(jCreatures, raceKey)
		endWhile
	endIf

	int jOldFormDB = JDB.SolveObj(".CFForm")
	if jOldFormDB
		Form actorKey = JFormMap.NextKey(jOldFormDB)
		while actorKey
			int jEntry = JFormMap.GetObj(jOldFormDB, actorKey)
			Form[] strippedArmor = new Form[6]
			int s = 0
			while s < 6
				strippedArmor[s] = JMap.GetForm(jEntry, "StrippedArmor" + s)
				s += 1
			endWhile
			CreatureFrameworkNative.MigrateStrippedGear(actorKey as Actor, strippedArmor, JMap.GetForm(jEntry, "StrippedWeaponLeft"), JMap.GetForm(jEntry, "StrippedWeaponRight"))
			actorKey = JFormMap.NextKey(jOldFormDB, actorKey)
		endWhile
		JDB.SetObj("CFForm", 0)
	endIf
	CreatureFrameworkUtility.Log("[Framework] Migrated Papyrus-version data to the native backend")
endFunction

; The puppet target key was changed
function PuppetTargetKeyChange()
	UnregisterForAllKeys()
	RegisterForKey(Config.PupTargetKey)
Endfunction

; A key has been released
event OnKeyUp(int keyCode, float holdTime)
	if keyCode == Config.PupTargetKey
		CFTargetPuppetSpell.Cast(PlayerRef)
	endIf
endEvent

; Pending single-update registrations from old saves land here harmlessly
event OnUpdate()
endEvent

Function Uninstall()
	UnregisterForUpdate()
	UnregisterForAllKeys()
	ClearCreatures()
	UnregisterAllMods()
	ClearFormDB()
Endfunction

;----------------------------------------------------------------
; Mod-related methods						|
;----------------------------------------------------------------

function RegisterMod(string modId, string modName)
	CreatureFrameworkNative.RegisterMod(modId, modName)
	CreatureFrameworkUtility.Log("[Framework] Registered mod with ID \"" + modId + "\" and name \"" + modName + "\"")
endFunction

function UnregisterMod(string modId)
	CreatureFrameworkNative.UnregisterMod(modId)
	CreatureFrameworkUtility.Log("[Framework] Unregistered mod with ID \"" + modId + "\"")
endFunction

function UnregisterAllMods()
	CreatureFrameworkNative.UnregisterAllMods()
	CreatureFrameworkUtility.Log("[Framework] Unregistered all mods")
endFunction

function ReregisterAllMods()
	if !muckingAboutPreventor
		muckingAboutPreventor += 1
		CreatureFrameworkUtility.Log("[Framework] Reregistering all mods")
		CreatureFrameworkNative.ReloadRegistrations()
		SendRegisterEvent()
		CreatureFrameworkNative.RequestUpdate()
		muckingAboutPreventor -= 1
		if muckingAboutPreventor < 0
			muckingAboutPreventor = 0
		endIf
	endIf
endFunction

int function GetRegisteredMods()
	return JArray.ObjectWithStrings(CreatureFrameworkNative.GetRegisteredMods())
endFunction

bool function IsModRegistered(string modId)
	return CreatureFrameworkNative.IsModRegistered(modId)
endFunction

string function GetModName(string modId)
	return CreatureFrameworkNative.GetModName(modId)
endFunction

function SendRegisterEvent()
	int handle = ModEvent.Create("CFRegister")
	if handle
		CreatureFrameworkUtility.Log("[Framework] Sending register event")
		ModEvent.PushForm(handle, self)
		ModEvent.Send(handle)
	else
		CreatureFrameworkUtility.Log("[Framework] Unable to send register event; invalid handle")
	endIf
endFunction

;----------------------------------------------------------------
; Creature-related methods					|
;----------------------------------------------------------------

function RegisterCreatureToMod(string modId, Race raceForm, Armor skinForm, string raceName, string skinName, int type, Armor normalArmor = none, Armor arousedArmor = none, bool stripArmor = false, bool stripWeapons = true, Form[] stripFormBlacklist = none, int[] stripSlotBlacklist = none, int[] restrictedSlots = none)
	if CreatureFrameworkNative.RegisterCreatureToMod(modId, raceForm, skinForm, raceName, skinName, type, normalArmor, arousedArmor, stripArmor, stripWeapons, stripFormBlacklist, stripSlotBlacklist, restrictedSlots)
		CreatureFrameworkUtility.Log("[Framework] Registered " + CreatureFrameworkUtility.GetDetailedFormName(raceForm) + CreatureFrameworkUtility.GetDetailedFormName(GetSkinOrFake(skinForm)) + " to mod \"" + modId + "\"")
	endIf
endFunction

string function RegisterCreatureEventsToMod(string modId, Race raceForm, Armor skinForm, string raceName, string skinName, bool stripArmor = true, bool stripWeapons = true, Form[] stripFormBlacklist = none, int[] stripSlotBlacklist = none, int[] restrictedSlots = none)
	RegisterCreatureToMod(modId, raceForm, skinForm, raceName, skinName, 0, none, none, stripArmor, stripWeapons, stripFormBlacklist, stripSlotBlacklist, restrictedSlots)
	return "CFArousalChange_" + modId
endFunction

function RegisterCreatureArmorSwapToMod(string modId, Race raceForm, Armor skinForm, string raceName, string skinName, Armor normalArmor, Armor arousedArmor = none, bool stripArmor = true, bool stripWeapons = true, Form[] stripFormBlacklist = none, int[] stripSlotBlacklist = none, int[] restrictedSlots = none)
	RegisterCreatureToMod(modId, raceForm, skinForm, raceName, skinName, 1, normalArmor, arousedArmor, stripArmor, stripWeapons, stripFormBlacklist, stripSlotBlacklist, restrictedSlots)
endFunction

string function RegisterCreatureArmorSwapAndEventsToMod(string modId, Race raceForm, Armor skinForm, string raceName, string skinName, Armor normalArmor, Armor arousedArmor = none, bool stripArmor = true, bool stripWeapons = true, Form[] stripFormBlacklist = none, int[] stripSlotBlacklist = none, int[] restrictedSlots = none)
	RegisterCreatureToMod(modId, raceForm, skinForm, raceName, skinName, 2, normalArmor, arousedArmor, stripArmor, stripWeapons, stripFormBlacklist, stripSlotBlacklist, restrictedSlots)
	return "CFArousalChange_" + modId
endFunction

; Keeps the mod itself registered, like the original
function UnregisterAllCreaturesFromMod(string modId)
	string modName = CreatureFrameworkNative.GetModName(modId)
	CreatureFrameworkNative.UnregisterMod(modId)
	CreatureFrameworkNative.RegisterMod(modId, modName)
	CreatureFrameworkUtility.Log("[Framework] Unregistered all creatures from mod ID \"" + modId + "\"")
endFunction

; The native registry has no JContainers map to hand out
int function GetRegisteredCreatures()
	return 0
endFunction

bool function IsCreatureRegistered(Race raceForm, Armor skinForm, bool checkFake = true)
	return CreatureFrameworkNative.IsCreatureRegistered(raceForm, skinForm, checkFake)
endFunction

bool function IsExactCreatureRegistered(Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.IsExactCreatureRegistered(raceForm, skinForm)
endFunction

; The original read a "creatures" key that was never written, so it always returned 0
int function GetCreaturesRegisteredToMod(string modId)
	return 0
endFunction

bool function IsCreatureRegisteredToMod(string modId, Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.IsCreatureRegisteredToMod(modId, raceForm, skinForm)
endFunction

int function GetRegisteredRaces()
	return JArray.ObjectWithForms(CreatureFrameworkNative.GetRegisteredRaces())
endFunction

int function GetRegisteredRaceCount()
	return CreatureFrameworkNative.GetRegisteredRaces().Length
endFunction

bool function IsRaceRegistered(Race raceForm)
	return CreatureFrameworkNative.IsRaceRegistered(raceForm)
endFunction

int function GetRacesRegisteredToMod(string modId)
	return JArray.ObjectWithForms(CreatureFrameworkNative.GetRacesRegisteredToMod(modId))
endFunction

int function GetRaceCountRegisteredToMod(string modId)
	return CreatureFrameworkNative.GetRacesRegisteredToMod(modId).Length
endFunction

bool function IsRaceRegisteredToMod(string modId, Race raceForm)
	return CreatureFrameworkNative.IsRaceRegisteredToMod(modId, raceForm)
endFunction

int function GetRegisteredSkins()
	return JArray.ObjectWithForms(CreatureFrameworkNative.GetRegisteredSkins())
endFunction

int function GetRegisteredSkinCount()
	return CreatureFrameworkNative.GetRegisteredSkins().Length
endFunction

bool function IsSkinRegistered(Armor skinForm)
	return CreatureFrameworkNative.IsSkinRegistered(skinForm)
endFunction

int function GetSkinsRegisteredToMod(string modId)
	return JArray.ObjectWithForms(CreatureFrameworkNative.GetSkinsRegisteredToMod(modId))
endFunction

int function GetSkinCountRegisteredToMod(string modId)
	return CreatureFrameworkNative.GetSkinsRegisteredToMod(modId).Length
endFunction

bool function IsSkinRegisteredToMod(string modId, Armor skinForm)
	return CreatureFrameworkNative.IsSkinRegisteredToMod(modId, skinForm)
endFunction

int function GetSkinsRegisteredToRace(Race raceForm)
	return JArray.ObjectWithForms(CreatureFrameworkNative.GetSkinsRegisteredToRace(raceForm))
endFunction

int function GetSkinCountRegisteredToRace(Race raceForm)
	return CreatureFrameworkNative.GetSkinsRegisteredToRace(raceForm).Length
endFunction

int function GetModsRegisteredWithRace(Race raceForm)
	return JArray.ObjectWithStrings(CreatureFrameworkNative.GetModsRegisteredWithRace(raceForm))
endFunction

int function GetModCountRegisteredWithRace(Race raceForm)
	return CreatureFrameworkNative.GetModsRegisteredWithRace(raceForm).Length
endFunction

int function GetModsRegisteredWithSkin(Armor skinForm)
	return JArray.ObjectWithStrings(CreatureFrameworkNative.GetModsRegisteredWithSkin(skinForm))
endFunction

int function GetModCountRegisteredWithSkin(Armor skinForm)
	return CreatureFrameworkNative.GetModsRegisteredWithSkin(skinForm).Length
endFunction

int function GetModsRegisteredWithCreature(Race raceForm, Armor skinForm)
	return JArray.ObjectWithStrings(CreatureFrameworkNative.GetModsRegisteredWithCreature(raceForm, skinForm))
endFunction

int function GetModCountRegisteredWithCreature(Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModsRegisteredWithCreature(raceForm, skinForm).Length
endFunction

; Built on demand; 0 when the creature is not registered to that mod
int function GetCreatureModMap(string modId, Race raceForm, Armor skinForm)
	if !CreatureFrameworkNative.IsCreatureRegisteredToMod(modId, raceForm, skinForm)
		return 0
	endIf
	int jRegMap = JMap.Object()
	JMap.SetInt(jRegMap, "type", CreatureFrameworkNative.GetModCreatureType(modId, raceForm, skinForm))
	JMap.SetForm(jRegMap, "normalArmor", CreatureFrameworkNative.GetModCreatureNormalArmor(modId, raceForm, skinForm))
	JMap.SetForm(jRegMap, "arousedArmor", CreatureFrameworkNative.GetModCreatureArousedArmor(modId, raceForm, skinForm))
	JMap.SetInt(jRegMap, "stripArmor", CreatureFrameworkNative.GetModCreatureStripArmor(modId, raceForm, skinForm) as int)
	JMap.SetInt(jRegMap, "stripWeapons", CreatureFrameworkNative.GetModCreatureStripWeapons(modId, raceForm, skinForm) as int)
	JMap.SetObj(jRegMap, "stripFormBlacklist", GetModCreatureStripFormBlacklist(modId, raceForm, skinForm))
	JMap.SetObj(jRegMap, "stripSlotBlacklist", GetModCreatureStripSlotBlacklist(modId, raceForm, skinForm))
	JMap.SetObj(jRegMap, "restrictedSlots", GetModCreatureRestrictedSlots(modId, raceForm, skinForm))
	return jRegMap
endFunction

int function GetModCreatureType(string modId, Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModCreatureType(modId, raceForm, skinForm)
endFunction

Armor function GetModCreatureNormalArmor(string modId, Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModCreatureNormalArmor(modId, raceForm, skinForm)
endFunction

Armor function GetModCreatureArousedArmor(string modId, Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModCreatureArousedArmor(modId, raceForm, skinForm)
endFunction

bool function GetModCreatureStripArmor(string modId, Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModCreatureStripArmor(modId, raceForm, skinForm)
endFunction

bool function GetModCreatureStripWeapons(string modId, Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModCreatureStripWeapons(modId, raceForm, skinForm)
endFunction

int function GetModCreatureStripFormBlacklist(string modId, Race raceForm, Armor skinForm)
	return JArray.ObjectWithForms(CreatureFrameworkNative.GetModCreatureStripFormBlacklist(modId, raceForm, skinForm))
endFunction

int function GetModCreatureStripSlotBlacklist(string modId, Race raceForm, Armor skinForm)
	return JArray.ObjectWithInts(CreatureFrameworkNative.GetModCreatureStripSlotBlacklist(modId, raceForm, skinForm))
endFunction

int function GetModCreatureRestrictedSlots(string modId, Race raceForm, Armor skinForm)
	return JArray.ObjectWithInts(CreatureFrameworkNative.GetModCreatureRestrictedSlots(modId, raceForm, skinForm))
endFunction

string function GetRaceName(Race raceForm)
	return CreatureFrameworkNative.GetRaceName(raceForm)
endFunction

string function GetSkinName(Armor skinForm)
	return CreatureFrameworkNative.GetSkinName(skinForm)
endFunction

;----------------------------------------------------------------
; JSON registration methods					|
;----------------------------------------------------------------

; creatures.d is parsed natively on every data load; this re-reads it on demand
function LoadJSONRegistrations(bool reload = false)
	CreatureFrameworkNative.ReloadRegistrations()
	CreatureFrameworkNative.RequestUpdate()
endFunction

bool function IsJSONLoading()
	return false
endFunction

function ResetLoadedFiles()
endFunction

;----------------------------------------------------------------
; Active mod methods						|
;----------------------------------------------------------------

function SetActiveMod(Race raceForm, Armor skinForm, string modId)
	CreatureFrameworkNative.SetActiveMod(raceForm, skinForm, modId)
	CreatureFrameworkUtility.Log("[Framework] Set the active mod to \"" + modId + "\" for " + CreatureFrameworkUtility.GetDetailedFormName(raceForm) + CreatureFrameworkUtility.GetDetailedFormName(GetSkinOrFake(skinForm)))
endFunction

function SetActiveModUsingIndex(Race raceForm, Armor skinForm, int modIndex)
	string modId = ""
	if modIndex != -1
		string[] mods = CreatureFrameworkNative.GetModsRegisteredWithCreature(raceForm, skinForm)
		if modIndex >= 0 && modIndex < mods.Length
			modId = mods[modIndex]
		endIf
	endIf
	SetActiveMod(raceForm, skinForm, modId)
endFunction

function ClearActiveForMod(string modId)
	Form[] races = CreatureFrameworkNative.GetRegisteredRaces()
	int r = 0
	while r < races.Length
		Race raceForm = races[r] as Race
		Form[] skins = CreatureFrameworkNative.GetSkinsRegisteredToRace(raceForm)
		int s = 0
		while s < skins.Length
			if CreatureFrameworkNative.GetActiveMod(raceForm, skins[s] as Armor) == modId
				CreatureFrameworkNative.SetActiveMod(raceForm, skins[s] as Armor, "")
			endIf
			s += 1
		endWhile
		r += 1
	endWhile
endFunction

string function GetActiveMod(Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetActiveMod(raceForm, skinForm)
endFunction

int function GetActiveModIndex(Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetActiveModIndex(raceForm, skinForm)
endFunction

string function GetActiveModName(Race raceForm, Armor skinForm)
	return CreatureFrameworkNative.GetModName(CreatureFrameworkNative.GetActiveMod(raceForm, skinForm))
endFunction

; Changing the active mod already refreshes the affected creatures natively
function TriggerUpdate(Race raceForm, Armor skinForm)
	CreatureFrameworkNative.RequestUpdate()
endFunction

function TriggerUpdateForActor(Actor actorForm)
	CreatureFrameworkNative.TriggerUpdateForActor(actorForm)
endFunction

;----------------------------------------------------------------
; Arousal methods						|
;----------------------------------------------------------------

bool function IsAroused(Actor actorForm, bool havingSex = false)
	if havingSex || IsOnSexScene(actorForm)
		return true
	endIf
	return Config.GenAroused && CreatureFrameworkNative.GetArousal(actorForm) >= Config.GenArousalThreshold
endFunction

int function GetArousalSource(Actor actorForm, bool havingSex = false)
	if havingSex || IsOnSexScene(actorForm)
		return 1
	elseIf Config.GenAroused && CreatureFrameworkNative.GetArousal(actorForm) >= Config.GenArousalThreshold
		return 2
	endIf
	return 0
endFunction

string function GetArousalSourceText(int arousalSource)
	if arousalSource > -1 && arousalSource < arousalSources.length
		return arousalSources[arousalSource]
	endIf
	return "$None"
endFunction

Function TimeSettingChanged()
	CreatureFrameworkNative.RequestUpdate()
endFunction

Function ArousedSettingChanged()
	CreatureFrameworkNative.RequestUpdate()
Endfunction

;----------------------------------------------------------------
; Event methods							|
;----------------------------------------------------------------

; Called by CreatureFrameworkNG.dll for type 0/2 registrations
function NGFireEvent(string modId, Actor actorForm, bool aroused, bool fromSex, bool fromArousal)
	FireEvent(modId, actorForm, aroused, none, none, fromSex, fromArousal, false)
endFunction

string function FireEvent(string modId, Actor actorForm, bool aroused, Race raceForm = none, Armor skinForm = none, bool fromSex = true, bool fromArousal = false, bool fromUnequip = false, int arousalRating = -1, Armor unequippedArmor = none)
	int eventHandle = ModEvent.Create("CFArousalChange_" + modId)
	if !eventHandle
		CreatureFrameworkUtility.Log("[Framework] Unable to fire event for " + CreatureFrameworkUtility.GetActorName(actorForm) + "; invalid handle")
		return none
	endIf

	ModEvent.PushForm(eventHandle, self)
	string eventId = modId + "_" + actorForm.GetFormID() + "_" + Utility.RandomInt(-100000, 100000)
	ModEvent.PushString(eventHandle, eventId)

	if raceForm == none
		raceForm = actorForm.GetLeveledActorBase().GetRace()
	endIf
	if skinForm == none
		skinForm = GetSkinOrFakeFromActor(actorForm)
		if !CreatureFrameworkNative.IsCreatureRegisteredToMod(modId, raceForm, skinForm) && skinForm != CFFakeSkin
			skinForm = CFFakeSkin
		endIf
	endIf
	if arousalRating == -1 && CreatureFrameworkNative.IsArousalAvailable() && Config.GenAroused
		arousalRating = CreatureFrameworkNative.GetArousal(actorForm)
	endIf

	int eventMap = JMap.Object()
	JMap.SetStr(eventMap, "id", eventId)
	JMap.SetForm(eventMap, "actor", actorForm)
	JMap.SetInt(eventMap, "aroused", aroused as int)
	JMap.SetForm(eventMap, "race", raceForm)
	JMap.SetForm(eventMap, "skin", skinForm)
	JMap.SetInt(eventMap, "arousal", arousalRating)
	JMap.SetForm(eventMap, "unequippedArmor", unequippedArmor)
	JMap.SetInt(eventMap, "fromSex", fromSex as int)
	JMap.SetInt(eventMap, "fromArousal", fromArousal as int)
	JMap.SetInt(eventMap, "fromUnequip", fromUnequip as int)
	JMap.SetObj(jEventsMap, eventId, eventMap)

	CreatureFrameworkUtility.Log("[Framework] Firing event \"" + eventId + "\"")
	ModEvent.Send(eventHandle)
	return eventId
endFunction

function FinishEvent(string eventId)
	JMap.RemoveKey(jEventsMap, eventId)
endFunction

int function GetEventData(string eventId)
	return JMap.GetObj(jEventsMap, eventId)
endFunction

Actor function GetEventActor(string eventId)
	return JMap.GetForm(JMap.GetObj(jEventsMap, eventId), "actor") as Actor
endFunction

bool function IsEventAroused(string eventId)
	return JMap.GetInt(JMap.GetObj(jEventsMap, eventId), "aroused") as bool
endFunction

Race function GetEventRace(string eventId)
	return JMap.GetForm(JMap.GetObj(jEventsMap, eventId), "race") as Race
endFunction

Armor function GetEventSkin(string eventId)
	return JMap.GetForm(JMap.GetObj(jEventsMap, eventId), "skin") as Armor
endFunction

int function GetEventArousal(string eventId)
	return JMap.GetInt(JMap.GetObj(jEventsMap, eventId), "arousal")
endFunction

Armor function GetEventUnequippedArmor(string eventId)
	return JMap.GetForm(JMap.GetObj(jEventsMap, eventId), "unequippedArmor") as Armor
endFunction

bool function IsEventFromSex(string eventId)
	return JMap.GetInt(JMap.GetObj(jEventsMap, eventId), "fromSex") as bool
endFunction

bool function IsEventFromArousal(string eventId)
	return JMap.GetInt(JMap.GetObj(jEventsMap, eventId), "fromArousal") as bool
endFunction

bool function IsEventFromUnequip(string eventId)
	return JMap.GetInt(JMap.GetObj(jEventsMap, eventId), "fromUnequip") as bool
endFunction

function ClearEvents()
	JMap.Clear(jEventsMap)
endFunction

;----------------------------------------------------------------
; Active actor methods						|
;----------------------------------------------------------------

; Detection is native; this asks for an immediate pass over the given actor
bool function ActivateActor(Actor actorForm)
	CreatureFrameworkNative.TriggerUpdateForActor(actorForm)
	return CreatureFrameworkNative.IsCreature(actorForm)
endFunction

bool function IsActorActive(Actor actorForm)
	return actorForm && CreatureFrameworkNative.IsActorActive(actorForm)
endFunction

bool function IsOnSexScene(Actor actorForm)
	return actorForm && Config.GenSexLab && SexLab && CreatureFrameworkNative.IsInScene(actorForm)
endFunction

function ClearInvalidActiveActors()
	CreatureFrameworkNative.RequestUpdate()
endFunction

function RestartActiveActors()
	CreatureFrameworkNative.RequestUpdate()
endFunction

bool function AreActiveActorsRestarting()
	return false
endFunction

function ClearCreatures()
	CreatureFrameworkNative.ClearCreatures()
Endfunction

;----------------------------------------------------------------
; Puppet methods						|
;----------------------------------------------------------------

function SetPuppet(Actor actorForm)
	puppet = actorForm
	CreatureFrameworkUtility.Log("[Framework] Function Setpuppet " + CreatureFrameworkUtility.GetActorName(puppet))
endFunction

Actor function GetPuppet()
	return puppet
endFunction

;----------------------------------------------------------------
; Form DB methods						|
;----------------------------------------------------------------

function ClearFormDB()
	JDB.SetObj("CFForm", 0)
endFunction

function ClearLogFormDB()
	JDB.SetObj("CFFormLog", 0)
endFunction

function ResetFormDBClearTimer()
endFunction

;----------------------------------------------------------------
; Utility methods						|
;----------------------------------------------------------------

int function GetVersion()
	return CreatureFrameworkUtility.GetVersion()
endFunction

string function GetVersionString()
	return CreatureFrameworkUtility.GetVersionString()
endFunction

bool function IsSexLabInstalled()
	return SexLab != none
endFunction

bool function IsSexLabEnabled()
	return SexLab && Config.GenSexLab
endFunction

bool function IsArousedInstalled()
	return CreatureFrameworkNative.IsArousalAvailable()
endFunction

bool function IsArousedEnabled()
	return CreatureFrameworkNative.IsArousalAvailable() && Config.GenAroused
endFunction

bool function IsValidForUpdate(Actor actorForm)
	if !actorForm
		return false
	endIf
	return !((actorForm.GetFlyingState() > 0 && actorForm.GetFlyingState() < 5) || actorForm.GetCurrentScene() != none || actorForm.IsInKillMove())
endFunction

bool function IsCreature(Actor actorForm)
	return actorForm && CreatureFrameworkNative.IsCreature(actorForm)
endFunction

bool function IsCreatureRace(Race raceForm)
	if !raceForm
		return false
	endIf
	return !(raceForm.HasKeyword(ActorTypeNPC) || raceForm.HasKeyword(CFIgnoreThisCreature))
endFunction

Armor function GetSkinOrFake(Armor skinForm)
	if skinForm
		return skinForm
	endIf
	return CFFakeSkin
endFunction

Armor function GetSkinOrFakeFromActor(Actor actorForm)
	if actorForm
		Armor skinForm = actorForm.GetActorBase().GetSkin()
		if skinForm
			return skinForm
		endIf
		skinForm = actorForm.GetLeveledActorBase().GetSkin()
		if skinForm
			return skinForm
		endIf
		skinForm = actorForm.GetRace().GetSkin()
		if skinForm
			return skinForm
		endIf
	endIf
	return CFFakeSkin
endFunction

; Kept for API callers; the native backend does its own armour handling
function RemoveArmors(Actor actorForm, bool removeNormal = true, bool removeAroused = true)
	if !actorForm
		return
	endIf
	bool inScene = CreatureFrameworkNative.IsInScene(actorForm)
	int i = 0
	while i < actorForm.GetNumItems()
		Form item = actorForm.GetNthForm(i)
		if (removeNormal && item.HasKeyword(CFArmorNormal)) || (removeAroused && item.HasKeyword(CFArmorAroused) && !inScene)
			actorForm.UnequipItem(item, false, true)
			actorForm.RemoveItem(item, 5, true)
		else
			i += 1
		endIf
	endWhile
endFunction

int[] function GetArmorSlots()
	return armorSlots
endFunction

function Dump()
	CreatureFrameworkNative.Dump("CreatureFrameworkNG.json")
	CreatureFrameworkUtility.Log("[Framework] Dumped framework data to Skyrim directory")
endFunction

;----------------------------------------------------------------
; mucking about prevention					|
;----------------------------------------------------------------

function PreventMuckingAbout()
	muckingAboutPreventor += 1
endFunction

function AllowMuckingAbout()
	muckingAboutPreventor -= 1
	if muckingAboutPreventor < 0
		muckingAboutPreventor = 0
	endIf
endFunction

function ResetMuckingAboutPrevention()
	muckingAboutPreventor = 0
endFunction

bool function IsMuckingAboutAllowed()
	return muckingAboutPreventor == 0
endFunction
