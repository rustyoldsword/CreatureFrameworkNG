Creature Framework NG 4.1
=========================

A native SKSE rewrite of Creature Framework 4.0 for Skyrim Special Edition / Anniversary Edition.
Everything Creature Framework did in Papyrus - finding creatures, deciding arousal, swapping the
aroused bodies, stripping and restoring gear - now runs inside CreatureFrameworkNG.dll.
The MCM, the JSON registrations in Data\creatures.d and the script API stay the same, so
More Nasty Critters and other creature mods work without changes.

This is a standalone replacement: it contains CreatureFramework.esm and all scripts.
Do NOT install the original Creature Framework alongside it.


Requirements
------------
- Skyrim SE 1.5.97 or AE 1.6.x
- SKSE64
- Address Library for SKSE Plugins
- JContainers SE
- SkyUI (MCM)
- Optional: SexLab Framework / SexLab P+, SexLab Aroused or OSL Aroused


Installation
------------
1. Remove or disable the original Creature Framework.
2. Install this archive with your mod manager.
3. Keep CreatureFramework.esm where the original was in your load order.
4. Existing saves: load, wait ~10 seconds for "Creature Framework" to initialise. MCM choices of
   active mods and gear stripped by the old version are carried over automatically.


What changed compared to Creature Framework 4.0
-----------------------------------------------
Performance
- No more cloak quest restarted every 15 seconds and no Utility.Wait loops; nearby creatures are
  checked natively once per second at negligible cost.
- The Papyrus VM is no longer busy with Creature Framework, so other script mods are not delayed.
- Bodies switch as soon as a SexLab scene is set up and switch back about a second after it ends.

Fixed bugs of 4.0
- Weapons taken away for a scene were never given back (the restore passed the actor instead of
  the weapon). They now return when the scene ends.
- Creatures that were merely aroused lost their weapons outside scenes. Gear is only taken off
  during SexLab scenes now.
- The MCM "default" key on a creature did not disable it.
- "Armour swap and events" registrations never swapped armour.
- Unregistering a mod could erase an unrelated mod from race and skin lists.
- Event handlers for SexLab Aroused exposure never handled creatures.

Plugin
- Form version 44 in every record (4.0 shipped form 43).
- Removed five Skyrim LE copies of vanilla records (MudcrabRace, MQ101Alduin, MQ206AncientAlduin,
  MQ101Horse, MQ101CartHorse) that reverted Special Edition data. Their only purpose was the
  CFIgnoreThisCreature keyword, which the DLL now adds at runtime.

Behaviour
- Default arousal threshold is 70 instead of 35. With OSL Aroused most creatures sit above 35 and
  were aroused all the time. Saves still on the old default 35 are moved to 70 once.
- JSON registrations are re-read on every game start, so edited or removed JSON files take effect.


Settings
--------
MCM > Creature Framework: same pages as 4.0. The "Cloak cooldown" slider no longer matters.
Data\SKSE\Plugins\CreatureFrameworkNG.ini: pass interval and verbose logging.


Credits
-------
- Original Creature Framework: Jappa (original author) and MadMansGun (4.0), with permission for
  modification.
- CommonLibSSE-NG, SKSE team, Address Library by meh321, JContainers.
- Source code: included in Scripts\Source; C++ source available on request.
