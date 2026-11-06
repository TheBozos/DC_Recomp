"""Extract the owner's original visibility loop for differential testing only.

The generated ELF/C++ stays in the build directory. No retail instructions are
embedded in this script. Two synthetic instructions preserve/return the caller RA
around the exact loop, excluding unrelated frame-end code and port adaptations.
"""
import hashlib
import pathlib
import struct
import sys

source, target = map(pathlib.Path, sys.argv[1:])
data = source.read_bytes()
if hashlib.sha1(data).hexdigest() != 'd9965d6908a334a5dfc982f3384428ad42048d54':
    raise SystemExit('visibility reference requires the expected SCUS-97111 image')
start, end = 0x12d268, 0x12d438
phoff = struct.unpack_from('<I', data, 28)[0]
count = struct.unpack_from('<H', data, 44)[0]
for i in range(count):
    p = struct.unpack_from('<8I', data, phoff + i * 32)
    if p[0] == 1 and p[2] <= start and end <= p[2] + p[4]:
        original = data[p[1] + start - p[2]:p[1] + end - p[2]]
        break
else:
    raise SystemExit('visibility loop is not file backed')
base = start - 4
code = struct.pack('<I', 0x03e0b825) + original + struct.pack('<3I', 0x02e0f825, 0x03e00008, 0)
strings = b'\0visibility_reference\0'
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
    [0x1278a0, 0x1145a0, 0x113498, 0x113758, 0x112f98]))
