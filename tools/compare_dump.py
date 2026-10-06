"""Compare Creature Framework registrations: original Dump() vs Creature Framework NG Dump().

The original writes CreatureFramework.json through JContainers (JValue.WriteToFile of the JDB
".CreatureFramework" map); NG writes CreatureFrameworkNG.json from its native registry. Both are
normalised to plain dicts keyed by (plugin, local id) and compared field by field.

Usage: python tools/compare_dump.py <original.json> <ng.json>
Exit code 0 when the registrations match.
"""

import json
import re
import sys

FORM_RE = re.compile(r"^__formData\|([^|]*)\|(.+)$")


def norm_form(value):
    """'__formData|Skyrim.esm|0xD53' -> ('skyrim.esm', 0xD53); anything empty -> None."""
    if not isinstance(value, str):
        return None
    match = FORM_RE.match(value)
    if not match:
        return None
    try:
        form_id = int(match.group(2), 0)
    except ValueError:
        return None
    plugin = match.group(1).lower()
    # JContainers may keep load-order bits in the id; only the local part identifies the form
    form_id &= 0xFFF if plugin.endswith(".esl") else 0xFFFFFF
    # JContainers writes a None form as "__formData|Skyrim.esm|0x0"
    if form_id == 0:
        return None
    return (plugin, form_id)


def fmt_form(form):
    return "None" if form is None else f"{form[0]}|0x{form[1]:X}"


def strip_meta(node):
    if isinstance(node, dict):
        return {k: v for k, v in node.items() if k != "__metaInfo"}
    return node


def as_list(node):
    """JContainers arrays may be written as {"__metaInfo":..,"values":[..]} or as a plain list; 0 means none."""
    node = strip_meta(node)
    if isinstance(node, list):
        return node
    if isinstance(node, dict):
        for key in ("values", "__values"):
            if isinstance(node.get(key), list):
                return node[key]
    return []


def form_map(node):
    """JFormMap -> {form: value}."""
    result = {}
    for key, value in (strip_meta(node) or {}).items():
        form = norm_form(key)
        if form is not None:
            result[form] = value
    return result


def str_map(node):
    return {k.lower(): v for k, v in (strip_meta(node) or {}).items()}


def get_ci(node, key, default=None):
    for k, v in (strip_meta(node) or {}).items():
        if k.lower() == key.lower():
            return v
    return default


def normalise(root):
    root = strip_meta(root)
    out = {"mods": {}, "races": {}, "skins": {}, "creatures": {}}

    for mod_id, info in str_map(get_ci(root, "mods", {})).items():
        out["mods"][mod_id] = {
            "name": get_ci(info, "name", ""),
            "races": sorted(fmt_form(norm_form(f)) for f in as_list(get_ci(info, "races"))),
            "skins": sorted(fmt_form(norm_form(f)) for f in as_list(get_ci(info, "skins"))),
        }

    for race, info in form_map(get_ci(root, "races", {})).items():
        out["races"][fmt_form(race)] = {
            "name": get_ci(info, "name", ""),
            "mods": sorted(str(m).lower() for m in as_list(get_ci(info, "mods"))),
            "skins": sorted(fmt_form(norm_form(f)) for f in as_list(get_ci(info, "skins"))),
        }

    for skin, info in form_map(get_ci(root, "skins", {})).items():
        out["skins"][fmt_form(skin)] = {
            "name": get_ci(info, "name", ""),
            "mods": sorted(str(m).lower() for m in as_list(get_ci(info, "mods"))),
        }

    for race, by_skin in form_map(get_ci(root, "creatures", {})).items():
        for skin, entry in form_map(by_skin).items():
            regs = {}
            for mod_id, reg in str_map(get_ci(entry, "mods", {})).items():
                regs[mod_id] = {
                    "type": int(get_ci(reg, "type", 0) or 0),
                    "normalArmor": fmt_form(norm_form(get_ci(reg, "normalArmor"))),
                    "arousedArmor": fmt_form(norm_form(get_ci(reg, "arousedArmor"))),
                    "stripArmor": int(get_ci(reg, "stripArmor", 0) or 0),
                    "stripWeapons": int(get_ci(reg, "stripWeapons", 0) or 0),
                    "stripFormBlacklist": sorted(fmt_form(norm_form(f)) for f in as_list(get_ci(reg, "stripFormBlacklist"))),
                    "stripSlotBlacklist": sorted(int(v) for v in as_list(get_ci(reg, "stripSlotBlacklist"))),
                    "restrictedSlots": sorted(int(v) for v in as_list(get_ci(reg, "restrictedSlots"))),
                }
            out["creatures"][f"{fmt_form(race)} / {fmt_form(skin)}"] = {
                "activeMod": str(get_ci(entry, "activeMod", "") or "").lower(),
                "mods": regs,
            }
    return out


def diff(section, left, right, problems):
    for key in sorted(set(left) | set(right)):
        if key not in right:
            problems.append(f"[{section}] only in original: {key}")
        elif key not in left:
            problems.append(f"[{section}] only in NG: {key}")
        elif left[key] != right[key]:
            problems.append(f"[{section}] {key}\n    original: {left[key]}\n    NG:       {right[key]}")


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    with open(sys.argv[1], encoding="utf-8-sig") as f:
        original = normalise(json.load(f))
    with open(sys.argv[2], encoding="utf-8-sig") as f:
        ng = normalise(json.load(f))

    problems = []
    for section in ("mods", "races", "skins", "creatures"):
        diff(section, original[section], ng[section], problems)

    for section in ("mods", "races", "skins", "creatures"):
        print(f"{section:10} original {len(original[section]):4}   NG {len(ng[section]):4}")
    if problems:
        print(f"\n{len(problems)} differences:")
        for line in problems[:200]:
            print(line)
        return 1
    print("\nRegistrations match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
