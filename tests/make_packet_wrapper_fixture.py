"""Extract an owner's original packet wrapper for before/after codegen tests.

Retail ELF and generated C++ stay local. Both versions are generated from the
same function bytes; the adapted version uses exactly the production patch list
entries in this function, with all helper calls made explicit fixture boundaries.
"""
import hashlib
import pathlib
import struct
import sys

kind = sys.argv[1]
source, patches, target = map(pathlib.Path, sys.argv[2:])
data = source.read_bytes()
if hashlib.sha1(data).hexdigest() != 'd9965d6908a334a5dfc982f3384428ad42048d54':
    raise SystemExit('wrapper reference requires the expected SCUS-97111 image')
base, end, helpers = {
    'water': (0x160f20, 0x1610fc, [0x161100, 0x1281b0, 0x1607e0]),
    'effect': (0x163480, 0x1638ec, [0x1312d0, 0x12e990, 0x1110b0, 0x12e740, 0x15d4b0]),
    'frame_close': (0x12d1dc, 0x12d268, [0x120de0, 0x120c28, 0x112f98, 0x104698, 0x113f40]),
    'frame_submit': (0x12d5c0, 0x12d6a8, [0x113e48, 0x112ea0, 0x10cc58, 0x10c3a8, 0x1145a0, 0x10c6f8]),
    'frame_draw': (0x12ed80, 0x12edec, [0x120c28, 0x120f78]),
    'frame_member': (0x12a110, 0x12a18c, [0x120c28, 0x120f78]),
    'mdt_draw': (0x1360e0, 0x136200, [0x127900, 0x1033b0, 0x135000]),
    'message': (0x153310, 0x153f34, [0x133070, 0x151ad0, 0x1312d0, 0x15bc10,
        0x152030, 0x152cb0, 0x152930, 0x14e380, 0x14c220, 0x14c5a0, 0x150810,
        0x152ee0, 0x14c4f0, 0x15c310]),
    'edit_state': (0x17990c, 0x17996c, [0x120d70, 0x177fa0]),
    'menu_state': (0x1f61fc, 0x1f624c, [0x22bdf0, 0x12e280, 0x120d70, 0x120c28, 0x22d0e0]),
    'loading': (0x154320, 0x154770, [0x10cc58, 0x120c18, 0x1110b0, 0x15c310,
        0x120de0, 0x120c28, 0x1145d0, 0x10c6f8, 0x113e48, 0x114c50, 0x112ea0]),
    'texanime': (0x167170, 0x167820, [0x167b40, 0x12f210, 0x1110b0]),
    'fire': (0x161ac0, 0x162280, [0x1312d0, 0x124670, 0x1246a0, 0x1217b8,
        0x121648, 0x1217e8, 0x12e740, 0x15dca0, 0x15d9a0, 0x1046e8, 0x1046f8]),
    'texanim_state': (0x19f9b0, 0x19fb70, [0x1110b0, 0x1312d0, 0x12f210]),
    'item_state': (0x1b1ef0, 0x1b20bc, [0x1312d0, 0x140660]),
    'language': (0x244c70, 0x244d54, [0x120d70, 0x120c28, 0x2451f0, 0x245240, 0x244d60, 0x244e40]),
    'fishing': (0x1ab0f0, 0x1ab73c, [0x12e280, 0x133070, 0x12e290, 0x122ff0, 0x122fd0,
        0x1217a0, 0x1217e8, 0x1217b8, 0x121648, 0x121600, 0x12a230, 0x128560,
        0x127ee0, 0x128180, 0x12ed80]),
    'shadow_fast': (0x1303b0, 0x1304d0, [0x121830, 0x12eec0, 0x12edf0, 0x120d70, 0x12ed80]),
    'shadow_fast2': (0x1304d0, 0x1305f0, [0x121830, 0x12eec0, 0x12edf0, 0x120d70, 0x12ed80]),
    'shadow': (0x1305f0, 0x130760, [0x121830, 0x121840, 0x12eec0, 0x12edf0, 0x120d70, 0x12ed80]),
    'debug_font': (0x13df40, 0x13e4dc, [0x1312d0, 0x13dea0, 0x13df00, 0x12f130, 0x15c310]),
    'packet_init': (0x12c720, 0x12c800, [0x120c08, 0x120c18]),
    'begin_frame': (0x12cd90, 0x12d070, [0x1110b0, 0x120c18, 0x121830, 0x1218a8, 0x12fd30, 0x130000]),
    'resident_chain': (0x1416ac, 0x1416d8, [0x140da0, 0x120d70]),
    'loading_init': (0x153fc0, 0x154314, [0x105058, 0x112c08, 0x120c08, 0x12c700, 0x13f3d0, 0x154770, 0x154950]),
    'packet_reserve': (0x125ae0, 0x125bb8, [0x127a00, 0x12c720, 0x104698]),
    'buffer_clear': (0x125bc0, 0x125e14, [0x127a00, 0x12c720]),
    'packet_handle': (0x12c238, 0x12c25c, [0x10c3a8]),
    'packet_get': (0x12e280, 0x12e28c, []),
}[kind]
phoff = struct.unpack_from('<I', data, 28)[0]
for i in range(struct.unpack_from('<H', data, 44)[0]):
    p = struct.unpack_from('<8I', data, phoff + i * 32)
    if p[0] == 1 and p[2] <= base and end <= p[2] + p[4]:
        code = data[p[1] + base - p[2]:p[1] + end - p[2]]
        break
else:
    raise SystemExit('wrapper is not file backed')
span_start = base
if kind in {'frame_close', 'frame_submit', 'edit_state', 'menu_state', 'resident_chain', 'packet_handle'}:
    # Isolated mid-function spans: synthetic save/return around the exact retail
    # instructions. No device value, frame boundary or completion is fabricated.
    base -= 4
    code = struct.pack('<I', 0x03e0b825) + code + struct.pack('<3I', 0x02e0f825, 0x03e00008, 0)
strings = ('\0' + kind + '_reference\0').encode()
symbols = bytes(16) + struct.pack('<IIIBBH', 1, base, len(code), 0x12, 0, 1)
names = b'\0.text\0.strtab\0.symtab\0.shstrtab\0'
body = code + strings + symbols + names
section_at = (0x100 + len(body) + 3) & ~3
header = struct.pack('<16sHHIIIIIHHHHHH', b'\x7fELF\x01\x01\x01' + bytes(9), 2, 8, 1,
                     base, 52, section_at, 0x20924000, 52, 32, 1, 40, 5, 4)
ph = struct.pack('<8I', 1, 0x100, base, base, len(code), len(code), 5, 4)
out = header + ph + bytes(0x100 - 84) + body
out += bytes(section_at - len(out))
str_at = 0x100 + len(code)
sections = [[0]*10, [1, 1, 6, base, 0x100, len(code), 0, 0, 4, 0],
            [7, 3, 0, 0, str_at, len(strings), 0, 0, 1, 0],
            [15, 2, 0, 0, str_at + len(strings), len(symbols), 2, 1, 4, 16],
            [23, 3, 0, 0, str_at + len(strings) + len(symbols), len(names), 0, 0, 1, 0]]
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(out + b''.join(struct.pack('<10I', *s) for s in sections))
target.with_suffix('.externals').write_text(''.join(f'override 0x{pc:x}\n' for pc in
    helpers + [0x120c80, 0x120e38, 0x120ec0, 0x121028, 0x120ed8, 0x120e90])
    + ('native 0x15434c\n' if kind == 'loading' else ''))
target.with_suffix('.native.externals').write_text(target.with_suffix('.externals').read_text()
    + ('native 0x1ab290\n' if kind == 'fishing' else '')
    + (''.join(f'native 0x{pc:x}\n' for pc in (0x13dfa8,0x13dff0,0x13e314,0x13e420)) if kind == 'debug_font' else ''))
selected = [line for line in patches.read_text().splitlines()
            if line.split('#')[0].strip() and span_start <= int(line.split()[0], 16) < end]
expected = {'frame_close': 2, 'frame_draw': 0, 'frame_member': 0, 'mdt_draw': 2,
            'message': 3, 'edit_state': 2, 'menu_state': 2, 'loading': 13, 'fire': 6, 'language': 2, 'fishing': 9,
            'debug_font': 14, 'packet_init': 2, 'begin_frame': 5, 'resident_chain': 2, 'loading_init': 3,
            'packet_reserve': 2, 'buffer_clear': 4, 'packet_handle': 3, 'packet_get': 1, 'frame_submit': 12}.get(kind, 4)
handle_sites = {int(line.split()[0], 16) for line in selected
                if int(line.split()[0], 16) != 0x12e280
                and int(line.split()[1], 16) & 0xffe0ffff == 0x8f808bd4
                and int(line.split()[2], 16) == (0x24000000 | (int(line.split()[1], 16) & 0x1f0000))}
if len(selected) - len(handle_sites) != expected:
    raise SystemExit('wrapper fixture requires its complete guarded production adaptation subset')
target.with_suffix('.patches').write_text('\n'.join(selected) + '\n')
# Frame-submit/loading references retain their published native changes in BOTH
# versions. Only the new packet/environment retirement differs. Other references
# use original bytes.
baseline_sites = {0x12d65c, 0x12d670} if kind == 'frame_submit' else set()
if kind == 'begin_frame':
    # Published CPU-diagnostic retirement is held constant in both versions.
    baseline_sites = {0x12cdac}
if kind == 'loading':
    # Compare against the published native field-query boundary in BOTH versions.
    baseline_sites = {0x15434c, 0x154350}
baseline = [line for line in selected if int(line.split()[0], 16) in baseline_sites]
if len(baseline) != len(baseline_sites):
    raise SystemExit('wrapper reference is missing a published native adaptation')
target.with_suffix('.baseline.patches').write_text('\n'.join(baseline) + '\n')
