"""Bind native services, preserving DC's soft-float libm and port overrides."""
import pathlib,sys,re
if sys.argv[1]=='--overrides':
    root=pathlib.Path(sys.argv[2]); out=pathlib.Path(sys.argv[3])
    addresses=set()
    for path in (root/'runtime').glob('dc_*.cpp'):
        addresses.update(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',path.read_text()))
    out.write_text(''.join(f'override 0x{x:x}\n' for x in sorted(addresses) if 0x100000 <= x < 0x245300))
    sys.exit(0)
src,out=sys.argv[1:3]
lines=[]
for line in pathlib.Path(src).read_text().splitlines():
    if any(line.endswith('# '+name) for name in ('atan','cos','fabs','floor','sin','atan2','sqrt')): continue
    lines.append(line)
lines.append('stub 0x12d070 # DC frame boundary')
pathlib.Path(out).write_text('\n'.join(lines)+'\n')
