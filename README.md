# Creature Framework NG

Native SKSE rewrite of Creature Framework 4.0 by Ep1cL3w7z for Skyrim SE 1.5.97 / AE 1.6.x.
Everything Creature Framework did in Papyrus - finding creatures, deciding arousal, swapping the
aroused bodies, stripping and restoring gear - runs inside `CreatureFrameworkNG.dll`. The MCM, the
JSON registrations in `Data/creatures.d` and the script API stay the same, so More Nasty Critters
and other creature mods work unchanged.

Mod page: https://dwemermods.com/mods/4364

`docs/DESIGN.md` cites the original script lines for every rule, bug fix and deliberate difference.
The user-facing changelog is in `dist_template/README - Creature Framework NG.txt`.

## Layout

| Path | Contents |
|---|---|
| `src/` | SKSE plugin (CommonLibSSE-NG) |
| `papyrus/source/` | Shipped scripts; `papyrus/stubs/` stand in for SexLab P+ / SexLab Aroused at compile time and are never shipped |
| `plugin/CreatureFramework.esm` | Form 44 plugin without the LE vanilla-record copies |
| `tests/` | Game-free unit tests for `src/Core` |
| `tools/` | Papyrus build, ESM rebuild, packaging, parity dump compare |
| `translations/` | MCM strings |

## Building

Requires Visual Studio 2022, [xmake](https://xmake.io) 3.x, Python 3 and the Creation Kit Papyrus compiler.

```
git clone --recurse-submodules https://github.com/rustyoldsword/CreatureFrameworkNG.git
cd CreatureFrameworkNG
xmake f -m releasedbg --skyrim_vr=n -y
xmake -y CreatureFrameworkNG
xmake build CoreTests && xmake run CoreTests
powershell -ExecutionPolicy Bypass -File tools\build_papyrus.ps1 -Mods <MO2 mods dir> -Game <Skyrim SE dir>
powershell -ExecutionPolicy Bypass -File tools\package.ps1 -OriginalMod <original Creature Framework mod dir>
```

The Papyrus build imports the original Creature Framework sources from `<Mods>\CreatureFramework`.

## Credits

- Ep1cL3w7z - original Creature Framework
- CommonLibSSE-NG contributors

## License

MIT for the code in this repository, see [LICENSE](LICENSE). Creature Framework itself and its
plugin data remain credited to the original author; third-party libraries keep their own licenses.
