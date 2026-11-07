"""Audit guarded image code against the port's actual installed native owners.

Direct control flow only: indirect jumps are reported as a coverage limit, not
assumed reachable/absent. Retail assets are read locally and never emitted.
"""
import argparse
import csv
import json
import pathlib
import struct
import subprocess


def symbols(path):
    return [(row[0], int(row[1], 16), int(row[2], 16)) for row in csv.reader(path.open())
            if len(row) >= 3 and row[1].startswith('0x')]


def image_words(path, patches):
    data = path.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    segments = [struct.unpack_from('<8I', data, phoff + i * 32)
                for i in range(struct.unpack_from('<H', data, 44)[0])]

    def word(pc):
        for p in segments:
            if p[0] == 1 and p[2] <= pc and pc + 4 <= p[2] + p[4]:
                return struct.unpack_from('<I', data, p[1] + pc - p[2])[0]
        raise ValueError(f'unbacked instruction {pc:#x}')

    replacements = {}
    for line in patches.read_text().splitlines():
        fields = line.split('#')[0].split()
        if not fields:
            continue
        pc, expected, replacement = [int(f, 16) for f in fields]
        if pc in replacements or word(pc) != expected:
            raise ValueError(f'invalid production patch guard {pc:#x}')
        replacements[pc] = replacement
    return lambda pc: replacements[pc] if pc in replacements else word(pc)


def calls(word, start, end):
    todo, seen, found, indirect = [start], set(), [], []
    while todo:
        pc = todo.pop()
        if pc in seen or not start <= pc < end:
            continue
        seen.add(pc)
        w = word(pc)
        op, rs, rt, fun = w >> 26, w >> 21 & 31, w >> 16 & 31, w & 63
        if op in (2, 3):
            target = ((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2)
            if op == 3:
                found.append((pc, target))
                todo.append(pc + 8)
            elif start <= target < end:
                todo.append(target)
            else:
                found.append((pc, target))  # direct tail call
            seen.add(pc + 4)
            continue
        if op == 0 and fun in (8, 9):
            if fun == 9:
                todo.append(pc + 8)
            if rs != 31 or fun == 9:
                indirect.append(pc)
            seen.add(pc + 4)
            continue
        branch = (op in (4, 5, 6, 7, 20, 21, 22, 23) or
                  (op == 1 and rt in (0, 1, 2, 3, 16, 17, 18, 19)) or
                  (op in (16, 17, 18) and rs == 8))
        if branch:
            imm = w & 0xffff
            if imm & 0x8000:
                imm -= 65536
            if op not in (5, 21) or rs != rt:
                todo.append(pc + 4 + imm * 4)
            if op not in (4, 20) or rs != rt:
                todo.append(pc + 8)
            seen.add(pc + 4)
            continue
        todo.append(pc + 4)
    return found, indirect


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=pathlib.Path, required=True)
    parser.add_argument('--registry', type=pathlib.Path, required=True)
    parser.add_argument('--images', type=pathlib.Path)
    args = parser.parse_args()
    root = args.root
    main_symbols = symbols(root / 'recomp/maps/dc_main.csv')
    sdk = {pc: name for name, pc, _ in main_symbols if name.startswith(('sceVif1Pk', 'sceGifPk'))}
    guarded = set(sdk) | {0x12c800, 0x150bb0, 0x150f50}
    result = subprocess.run([str(args.registry), *map(hex, sorted(guarded))],
                            check=True, text=True, stdout=subprocess.PIPE)
    overrides = {int(line.split()[1], 16) for line in result.stdout.splitlines() if line.startswith('override ')}
    rejected = {int(line.split()[1], 16) for line in result.stdout.splitlines() if line.startswith('reject ')}
    if rejected != guarded:
        raise ValueError('packet/readback guards are incomplete')
    report = {'rejected_boundaries': len(rejected), 'active_callers': [], 'native_owned_callers': [],
              'legacy_balloon_edges': [],
              'indirect_sites': [], 'limit': 'direct CFG only; indirect targets and inline cursor dataflow require separate review'}
    if args.images:
        # A registry entry cannot intercept a generated direct C++ call unless
        # codegen declared it external. Check resident and overlay dispatch.
        for filename in ('dc_externals.txt', 'dc_overlay_externals.txt'):
            declared = {int(fields[1], 16) for line in (args.images / filename).read_text().splitlines()
                        if len(fields := line.split()) >= 2 and fields[0] == 'override'}
            if not guarded <= declared:
                raise ValueError(f'{filename} would bypass a production rejection boundary')
        for image in ('dc', 'title', 'dun'):
            rows = main_symbols if image == 'dc' else symbols(root / f'recomp/maps/dc_{image}.csv')
            word = image_words(args.images / f'{image}.elf', root / f'recomp/{image}_native.patches')
            for name, start, end in rows:
                if start in sdk:
                    continue
                found, indirect = calls(word, start, end)
                for pc, target in found:
                    if target in (0x150bb0, 0x150f50):
                        report['legacy_balloon_edges'].append({'image': image, 'owner': hex(start),
                                                              'site': hex(pc), 'callee': hex(target)})
                        if image != 'dc' or start not in (0x150f50, 0x151ad0) or start not in overrides:
                            raise ValueError('legacy balloon helper has an unowned direct caller')
                hits = [{'site': hex(pc), 'callee': sdk[target]} for pc, target in found if target in sdk]
                if hits:
                    report['native_owned_callers' if start in overrides and image == 'dc' else 'active_callers'].append(
                        {'image': image, 'owner': name, 'address': hex(start), 'calls': hits})
                if indirect and not (image == 'dc' and start in overrides):
                    report['indirect_sites'].append({'image': image, 'owner': hex(start), 'sites': list(map(hex, indirect))})
    print(json.dumps(report, indent=2))
    return bool(report['active_callers'])


if __name__ == '__main__':
    raise SystemExit(main())
