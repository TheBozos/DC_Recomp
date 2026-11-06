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
}[kind]
phoff = struct.unpack_from('<I', data, 28)[0]
for i in range(struct.unpack_from('<H', data, 44)[0]):
    p = struct.unpack_from('<8I', data, phoff + i * 32)
    if p[0] == 1 and p[2] <= base and end <= p[2] + p[4]:
        code = data[p[1] + base - p[2]:p[1] + end - p[2]]
        break
else:
    raise SystemExit('wrapper is not file backed')
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
    helpers + [0x120c80, 0x120e38, 0x120ec0, 0x121028, 0x120ed8, 0x120e90]))
selected = [line for line in patches.read_text().splitlines()
            if line.split('#')[0].strip() and base <= int(line.split()[0], 16) < end]
if len(selected) != 4:
    raise SystemExit('wrapper fixture expects the complete four guarded production adaptations')
target.with_suffix('.patches').write_text('\n'.join(selected) + '\n')
