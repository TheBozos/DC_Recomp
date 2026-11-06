"""Build DC's sound libraries as native C++ using Bozo's offline IRX tools."""
from pathlib import Path
import sys,subprocess
root=Path(sys.argv[1]); out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'BozoEngine/tools/native_irx'))
from relocate import relocate
from recompile import Module
lines=['#pragma once','#include "bozo/native/compiled_libraries.h"','inline void addDcAudioImages(bozo::native::irx::CompiledLibraries &host) {']
for name,base in [('LIBSD',0x45a00),('SDRDRV',0x4a400),('MODMIDI',0x4c500),('MODHSYN',0x50200),('EZMIDI',0x5db00)]:
 info=relocate(root/'game/MODULES'/f'{name}.IRX',base,out/name)
 m=Module(str(out/(name+'.json')));exports,imports=m.tables()
 subprocess.run([sys.executable,'-I',str(root/'BozoEngine/tools/native_irx/recompile.py'),str(out/(name+'.json')),'--out',str(out)],check=True)
 data=(out/(name+'.bin')).read_bytes()
 lines+=['{','bozo::native::irx::Image image;',f'image.name="{info["name"]}"; image.hash=0x{info["hash"]}ull;',f'image.base={base};image.entry={info["entry"]};image.gp={info["gp"]};']
 lines+=['static const uint8_t bytes[]={']+[' '.join(f'0x{v:02x},' for v in data[i:i+32]) for i in range(0,len(data),32)]+['};','image.bytes.assign(std::begin(bytes),std::end(bytes));']
 for label,entries in [('imports',imports),('exports',exports)]:
  lines += [f'image.{label}={{']+[f'{{0x{addr:x},"{lib}",{ordinal}}},' for addr,(lib,ordinal) in entries.items()]+['};']
 lines += [f'image.nativeLibsd={str(name=="LIBSD").lower()};','host.addImage(std::move(image));','}']
lines+=['}']
(out/'dc_audio_modules.h').write_text('\n'.join(lines)+'\n')
