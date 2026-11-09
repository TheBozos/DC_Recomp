"""Bind native services, preserving DC's soft-float libm and port overrides."""
import pathlib,sys,re
if sys.argv[1]=='--overrides':
    root=pathlib.Path(sys.argv[2]); out=pathlib.Path(sys.argv[3])
    addresses=set()
    for path in (root/'runtime').glob('dc_*.cpp'):
        addresses.update(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',path.read_text()))
    native_sites=(0x12C19C,0x15434C,0x12D268,0x1AB290,0x13DFA8,0x13DFF0,0x13E314,0x13E420,0x1DF2A0,0x1DC3F4,
                  0x14797C,0x147998,0x147A70,0x147AB4,0x147B10,0x147B24,0x147B48,
                  0x121264,0x121278,0x121334,0x12134C,0x121530,0x121564) # guarded port boundaries
    out.write_text(''.join(f'override 0x{x:x}\n' for x in sorted(addresses) if 0x100000 <= x < 0x245300 and x not in native_sites)
                   + ''.join(f'native 0x{x:x}\n' for x in native_sites))
    sys.exit(0)
src,out=sys.argv[1:3]
lines=[]
for line in pathlib.Path(src).read_text().splitlines():
    if any(line.endswith('# '+name) for name in ('atan','cos','fabs','floor','sin','atan2','sqrt')): continue
    lines.append(line)
lines.append('stub 0x12d070 # DC frame boundary')
pathlib.Path(out).write_text('\n'.join(lines)+'\n')
if len(sys.argv)>3:
    # The shared SDK bindings apply to overlays too; resident-only inline sites do not.
    pathlib.Path(sys.argv[3]).write_text('\n'.join(line for line in lines if not line.startswith('native '))+'\n')
