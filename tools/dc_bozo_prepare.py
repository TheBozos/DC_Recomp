"""Prepare DC's resident and MWo3 code images for Bozo's ELF-based recompiler.
No instructions or relocations are executed. Output remains local game-derived data.
"""
from pathlib import Path
import csv
import hashlib
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from wrap_elf import wrap
from symtab_map import MAIN_CODE

SH=struct.Struct('<10I')
def headers(blob):
    off=struct.unpack_from('<I',blob,0x20)[0]
    count=struct.unpack_from('<H',blob,0x30)[0]
    return [list(SH.unpack_from(blob,off+i*40)) for i in range(count)]
def finish(blob, sections, names):
    out=bytearray(blob)
    si=struct.unpack_from('<H',blob,0x32)[0]
    sections[si][4]=len(out); sections[si][5]=len(names)
    out+=names
    out+=bytes((-len(out))%4)
    struct.pack_into('<I',out,0x20,len(out))
    struct.pack_into('<H',out,0x30,len(sections))
    out+=b''.join(SH.pack(*s) for s in sections)
    return out

def main(root,out):
    root=Path(root); out=Path(out); out.mkdir(parents=True,exist_ok=True)
    hashes = {
        'SCUS_971.11': 'd9965d6908a334a5dfc982f3384428ad42048d54',
        'TITLE.BIN': '8da27dd6a4f8d2090216b17d5d64a0c142cfd6d5',
        'DUN.BIN': 'ab7e56a4eeb69e178638088de6e57857d48f1d98',
    }
    for filename, expected in hashes.items():
        if hashlib.sha1((root/'game'/filename).read_bytes()).hexdigest() != expected:
            raise ValueError(f'{filename}: unsupported disc revision; expected NTSC-U version 1.02')
    elf=(root/'game/SCUS_971.11').read_bytes()
    sections=headers(elf)
    si=struct.unpack_from('<H',elf,0x32)[0]
    names=bytearray(elf[sections[si][4]:sections[si][4]+sections[si][5]])
    # The retail ELF calls its entire resident image executable, including VU code and data.
    # Keep its original load image, but describe the EE code spans from its retained symbols.
    sections[4][5]=MAIN_CODE[0][1]-MAIN_CODE[0][0]
    start,end=MAIN_CODE[1]
    name=len(names); names+=b'.init\0'
    sections.append([name,1,6,start,0x100+start-0x100000,end-start,0,0,4,0])
    (out/'dc.elf').write_bytes(finish(elf,sections,names))
    flags=struct.unpack_from('<I',elf,0x24)[0]
    for module in ('title','dun'):
        raw=(root/'game'/f'{module.upper()}.BIN').read_bytes()
        image=bytearray(wrap(raw,flags)); sh=headers(image)
        si=struct.unpack_from('<H',image,0x32)[0]
        names=bytearray(image[sh[si][4]:sh[si][4]+sh[si][5]])
        strings=bytearray(b'\0'); symbols=bytearray(16)
        with (root/'recomp/maps'/f'dc_{module}.csv').open(newline='') as stream:
            for row in csv.DictReader(stream):
                addr=int(row['Start'],0); size=int(row['Size'],0)
                symname=len(strings); strings+=row['Name'].encode('latin-1')+b'\0'
                index=next((i for i,s in enumerate(sh) if s[2]&4 and s[3]<=addr<s[3]+s[5]),None)
                if index is None: raise ValueError(f'{module}: unmapped function {addr:x}')
                symbols+=struct.pack('<IIIBBH',symname,addr,size,0x12,0,index)
        strname=len(names); names+=b'.strtab\0'
        strindex=len(sh); sh.append([strname,3,0,0,len(image),len(strings),0,0,1,0]); image+=strings
        image+=bytes((-len(image))%4)
        symname=len(names); names+=b'.symtab\0'
        sh.append([symname,2,0,0,len(image),len(symbols),strindex,1,4,16]); image+=symbols
        (out/f'{module}.elf').write_bytes(finish(image,sh,names))
        (out/f'{module}.identity').write_bytes(raw[:64])
        print(f'{module}: {len(symbols)//16-1} functions, resident identity {raw[:8]!r}')

if __name__=='__main__': main(*sys.argv[1:])
