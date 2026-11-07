"""Extract original overlay owners; compare exact image-scoped production patches.

Only address metadata is checked in. Retail bytes and generated code stay local.
"""
import hashlib
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from overlay_packet_cases import CASES

kind = sys.argv[1]
source, raw_image, patches, target = map(pathlib.Path, sys.argv[2:])
case = CASES[kind]
image, start, end, groups, helpers, floats, flags, span = case
expected_hash = {'title': '8da27dd6a4f8d2090216b17d5d64a0c142cfd6d5',
                 'dun': 'ab7e56a4eeb69e178638088de6e57857d48f1d98'}[image]
if hashlib.sha1(raw_image.read_bytes()).hexdigest() != expected_hash:
    raise SystemExit('overlay fixture requires the expected SCUS-97111 ' + image + ' image')
data = source.read_bytes()
phoff = struct.unpack_from('<I', data, 28)[0]
for i in range(struct.unpack_from('<H', data, 44)[0]):
    p = struct.unpack_from('<8I', data, phoff + i * 32)
    if p[0] == 1 and p[2] <= start and end <= p[2] + p[4]:
        code = data[p[1] + start - p[2]:p[1] + end - p[2]]
        break
else:
    raise SystemExit('overlay owner is not file backed')
base = start
if span:
    # Exact mid-function instructions, surrounded by a synthetic save/return.
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
target.with_suffix('.externals').write_text(''.join(f'override 0x{pc:x}\n' for pc in helpers))
selected = [line for line in patches.read_text().splitlines()
            if line.split('#')[0].strip() and start <= int(line.split()[0], 16) < end]
sites = {pc + offset for pc, _ in groups for offset in (0, 4)}
if {int(line.split()[0], 16) for line in selected} != sites or len(selected) != len(sites):
    raise SystemExit('overlay fixture requires its complete production patch subset')
target.with_suffix('.patches').write_text('\n'.join(selected) + '\n')
def array(name, values):
    return f'constexpr std::array<uint32_t, {len(values)}> {name} = {{' + ','.join(hex(x) for x in values) + '};\n'
target.with_name('overlay_case.h').write_text(
    '#include <array>\n#include <cstdint>\n'
    f'#include "{kind}_ref_functions.h"\n#include "{kind}_native_functions.h"\n'
    f'#define REFERENCE {kind}_ref_function_table\n#define ADAPTED {kind}_native_function_table\n'
    f'constexpr bool isolatedSpan = {str(span).lower()};\n'
    f'constexpr uint32_t owner = 0x{int(kind.split("_")[1], 16):x};\n'
    + array('helpers', helpers) + array('phaseOffsets', floats) + array('flagOffsets', flags))
