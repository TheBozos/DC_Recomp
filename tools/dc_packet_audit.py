"""Audit guarded image code against the port's actual installed native owners.

Direct flow and compiler switch cases are followed; other target sets remain
open, with optional bounded runtime observations. Retail assets stay local.
"""
import argparse
import csv
import json
import pathlib
import struct
import subprocess
import collections

# These ordinary native hooks call their generated body. Registry ownership
# alone therefore cannot retire that body's calls, loads or indirect sites.
# Keep in sync with the forwarding hooks in dc_native.cpp; diagnostic feeder,
# sprite and count wrappers are disabled by the registry audit executable.
COMPILED_PASSTHROUGH = {
    0x1554a0, 0x167170, 0x167b40, 0x167b80, 0x167ae0,
    0x138080, 0x1380b0, 0x138020, 0x12f130, 0x12f1a0,
    0x154320, 0x161ac0, 0x1305f0, 0x1303b0, 0x1304d0, 0x130760,
}


def replaced_body(image, owner, overrides):
    return image == 'dc' and owner in overrides and owner not in COMPILED_PASSTHROUGH


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


def calls(word, start, end, control=None, visited=None):
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
            fact = (control or {}).get(pc, {})
            if fun == 9:
                todo.append(pc + 8)
            if fact.get('kind') == 'compiler_switch':
                # Follow all compiler-recognized cases, keeping the unresolved
                # fallback explicit. This does not prove the table immutable.
                todo.extend(fact['targets'])
            if rs != 31 or fun == 9:
                if fact.get('kind') != 'compiler_return_copy':
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
    if visited is not None:
        visited.update(seen)
    return found, indirect


def indirect_category(site, name):
    if site['kind'] == 'jump_register' and name in ('__sjthrow', '__throw', '__rethrow'):
        return 'exception_runtime_jump'
    if site['kind'] != 'call_register':
        return site['kind']
    if 'slot_base_reg' in site:
        return {28: 'global_function_slot', 29: 'stack_function_slot'}.get(site['slot_base_reg'], 'memory_function_slot')
    if site.get('definition_op') in ('addu', 'daddu', 'or', 'paddub'):
        return 'register_copy_call'
    return 'register_target_call'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=pathlib.Path, required=True)
    parser.add_argument('--registry', type=pathlib.Path, required=True)
    parser.add_argument('--images', type=pathlib.Path)
    parser.add_argument('--trace', type=pathlib.Path, action='append', default=[])
    args = parser.parse_args()
    if args.trace and not args.images:
        parser.error('--trace requires --images from the matching build')
    root = args.root
    main_symbols = symbols(root / 'recomp/maps/dc_main.csv')
    sdk = {pc: name for name, pc, _ in main_symbols if name.startswith(('sceVif1Pk', 'sceGifPk'))}
    guarded = set(sdk) | {0x12c720, 0x12c800, 0x150bb0, 0x150f50}
    result = subprocess.run([str(args.registry), *map(hex, sorted(guarded))],
                            check=True, text=True, stdout=subprocess.PIPE)
    overrides = {int(line.split()[1], 16) for line in result.stdout.splitlines() if line.startswith('override ')}
    rejected = {int(line.split()[1], 16) for line in result.stdout.splitlines() if line.startswith('reject ')}
    if rejected != guarded:
        raise ValueError('packet/readback guards are incomplete')
    report = {'rejected_boundaries': len(rejected), 'active_callers': [], 'native_owned_callers': [],
              'legacy_balloon_edges': [],
              'installed_overrides': list(map(hex, sorted(overrides))), 'indirect_sites': [],
              'compiled_passthrough_overrides': list(map(hex, sorted(overrides & COMPILED_PASSTHROUGH))),
              'retired_initializer_callers': [], 'packet_global_accesses': [],
              'limit': 'compiler switch cases expand CFG; fallback targets, mutable slots and inline cursor dataflow remain open; traces prove observed edges only'}
    facts, entries, externals = {}, {}, {}
    if args.images:
        # A registry entry cannot intercept a generated direct C++ call unless
        # codegen declared it external. Check resident and overlay dispatch.
        for filename in ('dc_externals.txt', 'dc_overlay_externals.txt'):
            declared = {int(fields[1], 16) for line in (args.images / filename).read_text().splitlines()
                        if len(fields := line.split()) >= 2 and fields[0] == 'override'}
            if not guarded <= declared:
                raise ValueError(f'{filename} would bypass a production rejection boundary')
            externals.update((int(fields[1], 16), fields[0]) for line in (args.images / filename).read_text().splitlines()
                             if len(fields := line.split()) >= 2 and fields[0] in ('override', 'stub'))
        for image in ('dc', 'title', 'dun'):
            names = {pc: name for name, pc, _ in (main_symbols if image == 'dc' else symbols(root / f'recomp/maps/dc_{image}.csv'))}
            compiled = json.loads((args.images / f'{image}.indirect.json').read_text())['functions']
            facts[image] = {s['pc']: dict(s, owner=f['start'], end=f['end']) for f in compiled for s in f['sites']}
            for site in facts[image].values():
                site['category'] = indirect_category(site, names.get(site['owner'], ''))
            entries[image] = compiled
            word = image_words(args.images / f'{image}.elf', root / f'recomp/{image}_native.patches')
            for function in compiled:
                start, end = function['start'], function['end']
                name = names.get(start, f'compiler_function_{start:x}')
                if start in sdk:
                    continue
                visited = set()
                found, indirect = calls(word, start, end, facts[image], visited)
                native_owned = replaced_body(image, start, overrides)
                if not native_owned:
                    for pc in sorted(visited):
                        w = word(pc)
                        if w >> 21 & 31 == 28 and w & 0xffff in (0x8bd4, 0x8c08, 0x8c0c) and w >> 26 in (35,43):
                            access = 'read' if w >> 26 == 35 else 'write'
                            report['packet_global_accesses'].append({'image': image, 'owner': hex(start),
                                                                    'site': hex(pc), 'offset': hex(w & 0xffff), 'access': access})
                            if access == 'write':
                                raise ValueError('admitted code still initializes/selects a retired packet handle')
                for pc, target in found:
                    if target == 0x12c720:
                        report['retired_initializer_callers'].append({'image': image, 'owner': hex(start), 'site': hex(pc)})
                        if not native_owned:
                            raise ValueError('admitted code still calls the retired packet-buffer initializer')
                    if target in (0x150bb0, 0x150f50):
                        report['legacy_balloon_edges'].append({'image': image, 'owner': hex(start),
                                                              'site': hex(pc), 'callee': hex(target)})
                        if image != 'dc' or start not in (0x150f50, 0x151ad0) or start not in overrides:
                            raise ValueError('legacy balloon helper has an unowned direct caller')
                hits = [{'site': hex(pc), 'callee': sdk[target]} for pc, target in found if target in sdk]
                if hits:
                    report['native_owned_callers' if native_owned else 'active_callers'].append(
                        {'image': image, 'owner': name, 'address': hex(start), 'calls': hits})
                if indirect and not native_owned:
                    report['indirect_sites'].append({'image': image, 'owner': hex(start), 'sites': list(map(hex, indirect)),
                                                     'details': [facts[image][pc] for pc in indirect]})
        report['indirect_summary'] = dict(collections.Counter(
            s['category'] for owner in report['indirect_sites'] for s in owner['details']))
        report['inventory_summary'] = dict(collections.Counter(f['category'] for image, group in facts.items()
            for f in group.values() if f['kind'] != 'return' and not replaced_body(image, f['owner'], overrides)))
        reached = {(owner['image'], s['pc']) for owner in report['indirect_sites'] for s in owner['details']}
        report['cfg_unreached_sites'] = [{'image': image, 'pc': pc, 'category': fact['category']} for image, group in facts.items()
            for pc, fact in group.items() if fact['kind'] != 'return' and not replaced_body(image, fact['owner'], overrides)
            and (image, pc) not in reached]
        report['observed_edges'] = []
        observed_sites = set()
        for path in args.trace:
            trace = json.loads(path.read_text())
            if trace.get('schema') != 1 or not trace.get('complete'):
                raise ValueError(f'{path} is not a complete audit run')
            for edge in trace['edges']:
                image = ('dc', 'title', 'dun')[edge['image']]
                fact = facts[image].get(edge['pc'])
                if not fact or fact['kind'] == 'return':
                    raise ValueError('trace does not match the compiled indirect site inventory')
                target = edge['target']
                if target in guarded or any(a <= target < b for n, a, b in main_symbols if n.startswith(('sceVif1Pk','sceGifPk'))):
                    raise ValueError('indirect trace reached a retired packet entry/interior')
                target_image = ('dc', 'title', 'dun', 'unselected')[edge['target_image']]
                disposition = 'unclassified_address'
                if target in overrides:
                    disposition = 'native_override'
                elif externals.get(target) == 'stub':
                    disposition = 'sdk_external'
                elif fact['kind'] == 'compiler_switch' and target in fact['targets'] and target_image == image:
                    disposition = 'compiler_switch_case'
                elif target_image in entries:
                    owner = next((f for f in entries[target_image] if f['start'] <= target < f['end']), None)
                    if owner:
                        disposition = 'compiled_entry' if target == owner['start'] else 'compiled_interior_or_continuation'
                if disposition == 'unclassified_address' and target in externals:
                    disposition = 'unresolved_declared_external'
                observed_sites.add((image, edge['pc']))
                report['observed_edges'].append(dict(edge, source_image=image, selected_target_image=target_image,
                                                     kind=fact['kind'], disposition=disposition, trace=str(path)))
        report['observed_site_count'] = len(observed_sites)
        report['unclassified_observed_edges'] = [e for e in report['observed_edges'] if e['disposition'] in
                                                ('unclassified_address', 'unresolved_declared_external')]
        report['unobserved_sites'] = [{'image': image, 'pc': pc, 'kind': fact['kind']} for image, group in facts.items()
                                      for pc, fact in group.items() if fact['kind'] != 'return'
                                      and not replaced_body(image, fact['owner'], overrides)
                                      and (image, pc) not in observed_sites]
    print(json.dumps(report, indent=2))
    return bool(report['active_callers'] or report.get('unclassified_observed_edges'))


if __name__ == '__main__':
    raise SystemExit(main())
