"""Rebuild CreatureFramework.esm for Skyrim SE/AE.

- Drops the five vanilla overrides (MudcrabRace, MQ101Alduin, MQ206AncientAlduin, MQ101Horse,
  MQ101CartHorse). They are Skyrim LE copies whose only intended change is CFIgnoreThisCreature;
  CreatureFrameworkNG.dll adds that keyword at runtime instead, so vanilla SE data and localized
  names stay untouched.
- Sets form version 44 on every record header, TES4 included.
- Leaves every CF-owned record byte-identical.

Usage: python tools/rebuild_esm.py <input.esm> <output.esm>
"""

import struct
import sys

DROPPED = {0x000BA545, 0x00032B94, 0x0004377F, 0x000654E5, 0x000B9E1D}
FORM_VERSION = 44


def read_records(buf, start, end):
    """Top-level items: ('REC', header, data) or ('GRUP', header, [items])."""
    items = []
    off = start
    while off + 24 <= end:
        typ = buf[off:off + 4]
        size = struct.unpack_from('<I', buf, off + 4)[0]
        if typ == b'GRUP':
            header = bytearray(buf[off:off + 24])
            items.append(('GRUP', header, read_records(buf, off + 24, off + size)))
            off += size
        else:
            header = bytearray(buf[off:off + 24])
            items.append(('REC', header, bytes(buf[off + 24:off + 24 + size])))
            off += 24 + size
    if off != end:
        raise ValueError(f'trailing bytes at {off:#x} (end {end:#x})')
    return items


def write_items(items):
    out = bytearray()
    for kind, header, payload in items:
        if kind == 'GRUP':
            body = write_items(payload)
            header = bytearray(header)
            struct.pack_into('<I', header, 4, 24 + len(body))
            out += header + body
        else:
            header = bytearray(header)
            struct.pack_into('<I', header, 4, len(payload))
            struct.pack_into('<H', header, 20, FORM_VERSION)
            out += header + payload
    return bytes(out)


def count_items(items):
    total = 0
    for kind, _, payload in items:
        total += 1
        if kind == 'GRUP':
            total += count_items(payload)
    return total


def main():
    src, dst = sys.argv[1], sys.argv[2]
    buf = open(src, 'rb').read()
    items = read_records(buf, 0, len(buf))
    tes4 = items[0]
    assert tes4[0] == 'REC' and tes4[1][:4] == b'TES4'

    removed = []
    kept_groups = []
    for kind, header, payload in items[1:]:
        assert kind == 'GRUP', 'unexpected top-level record'
        records = []
        for sub in payload:
            fid = struct.unpack_from('<I', sub[1], 12)[0]
            if sub[0] == 'REC' and fid in DROPPED:
                removed.append((sub[1][:4].decode(), fid))
            else:
                records.append(sub)
        if records:
            kept_groups.append(('GRUP', header, records))
        else:
            removed.append((f'GRUP {header[8:12].decode()}', 0))

    new_items = [tes4] + kept_groups
    # HEDR: float version, uint32 numRecords (records + groups, TES4 excluded), uint32 nextObjectId
    tes4_data = bytearray(tes4[2])
    assert tes4_data[:4] == b'HEDR'
    old_count = struct.unpack_from('<I', tes4_data, 10)[0]
    new_count = count_items(new_items) - 1
    struct.pack_into('<I', tes4_data, 10, new_count)
    new_items[0] = ('REC', tes4[1], bytes(tes4_data))

    out = write_items(new_items)
    open(dst, 'wb').write(out)

    print('removed:', ', '.join(f'{t} {f:08X}' if f else t for t, f in removed))
    print(f'HEDR record count {old_count} -> {new_count}')
    print(f'size {len(buf)} -> {len(out)} bytes')


if __name__ == '__main__':
    main()
