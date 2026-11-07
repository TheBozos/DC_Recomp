"""Exact remaining DUN dispatch spans; synthetic landing bodies, no retail output tracked."""
import hashlib
import json
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from dc_packet_audit import image_words

# start, JR, selector register, selector bias, range check
SWITCHES = [(0x1daf598,0x1daf5bc,4,0,0x1daf59c),
            (0x1db1fe8,0x1db200c,16,195,0x1db1fec),
            (0x1db5390,0x1db53b8,4,0,0x1db5398),
            (0x1dbfb24,0x1dbfb44,18,0,0x1dbfb24)]
# start, end, JALR, object field, table slot, GP object location, conditional s0
CALLS = [(0x1dae874,0x1dae8a0,0x1dae898,0xa0,0xac,0x9d00,False),
         (0x1db6838,0x1db6850,0x1db6848,0x2b8,8,0x9ca8,False),
         (0x1db77c0,0x1db77e0,0x1db77d8,0xa0,8,0x9d00,True),
         (0x1db7990,0x1db79b0,0x1db79a8,0xa0,8,0x9d00,True)]


def main():
    source, raw, patches, facts_path, target = map(pathlib.Path, sys.argv[1:])
    if hashlib.sha1(raw.read_bytes()).hexdigest() != 'ab7e56a4eeb69e178638088de6e57857d48f1d98':
        raise ValueError('DUN fixture requires the expected SCUS-97111 image')
    word = image_words(source, patches)
    facts = {s['pc']: s for f in json.loads(facts_path.read_text())['functions'] for s in f['sites']}
    pieces, functions, tables, switch_rows, call_rows = {}, [], {}, [], []

    def put(pc, words):
        for i, value in enumerate(words):
            at = pc + i * 4
            if at in pieces:
                raise ValueError('overlapping dispatch fixture instructions')
            pieces[at] = value

    # Synthetic full-register RA save/restore. Actual spans and delay slots stay exact.
    save, restore = 0x73e0be28, [0x72e0fe28,0x03e00008,0]
    for start, pc, reg, bias, check in SWITCHES:
        f = facts[pc]
        if f['kind'] != 'compiler_switch' or word(check) >> 26 != 11:
            raise ValueError('DUN compiler switch/range-check facts changed')
        count = word(check) & 0xffff
        if count != len(f['targets']):
            raise ValueError('DUN switch bound does not match its complete table')
        branch = word(check + 4)
        if branch >> 26 != 4 or (branch >> 21 & 31) != 1 or (branch >> 16 & 31) != 0:
            raise ValueError('DUN switch default guard changed')
        offset = struct.unpack('<h', struct.pack('<H', branch & 0xffff))[0]
        default = check + 8 + offset * 4
        landings = sorted(set(f['targets']) | {default})
        if any(b - a < 8 for a, b in zip(landings, landings[1:])):
            raise ValueError('case landings leave insufficient synthetic boundary space')
        put(start - 4, [save]); put(start, [word(at) for at in range(start,pc + 8,4)])
        epilogue = max(landings) + 8
        for n, landing in enumerate(landings, 1):
            put(landing, [0x08000000 | (epilogue >> 2), 0x24020000 | n])
        put(epilogue, restore)
        for i, value in enumerate(f['targets']):
            if word(f['table'] + 4 * i) != value:
                raise ValueError('compiler switch table differs from the prepared image')
            tables[f['table'] + i * 4] = value
        functions.append((f'switch_{pc:x}', start - 4, epilogue + 12))
        # Capture all actual table values/aliases in generated, local metadata.
        switch_rows.append((start - 4,pc,reg,bias,default,f['table'],f['targets'],landings))
    for start, end, pc, field, slot, gp_slot, conditional in CALLS:
        f = facts[pc]
        if (f['kind'],f.get('slot_offset')) != ('call_register',slot):
            raise ValueError('DUN call-slot facts changed')
        put(start - 4,[save]); put(start,[word(at) for at in range(start,end,4)]); put(end,restore)
        functions.append((f'call_{pc:x}',start - 4,end + 12))
        call_rows.append((start - 4,pc,field,slot,gp_slot,conditional))
    # Entire short owner, including both lookup branches, both virtual calls,
    # zero float arguments and final parent attachment.
    start, end = 0x1db9590, 0x1db9658
    put(start,[word(at) for at in range(start,end,4)])
    functions.append(('equip_reattach',start,end))
    base, finish = min(pieces), max(pieces) + 4
    code = bytearray(finish - base)
    for at, value in pieces.items(): struct.pack_into('<I',code,at - base,value)
    data_base, data_end = min(tables), max(tables) + 4
    data = bytearray(data_end - data_base)
    for at, value in tables.items(): struct.pack_into('<I',data,at - data_base,value)
    strings, syms = bytearray(b'\0'), bytearray(16)
    for name, a, b in functions:
        index = len(strings); strings += name.encode() + b'\0'
        syms += struct.pack('<IIIBBH',index,a,b - a,0x12,0,1)
    names = b'\0.text\0.data\0.strtab\0.symtab\0.shstrtab\0'
    offsets = [0x100,0x100+len(code),0x100+len(code)+len(data)]
    str_at = offsets[2]; sym_at = str_at + len(strings); names_at = sym_at + len(syms)
    section_at = (names_at + len(names) + 3) & ~3
    header = struct.pack('<16sHHIIIIIHHHHHH',b'\x7fELF\x01\x01\x01'+bytes(9),2,8,1,base,52,section_at,0x20924000,52,32,2,40,6,5)
    ph = struct.pack('<8I',1,0x100,base,base,len(code),len(code),5,4)
    ph += struct.pack('<8I',1,offsets[1],data_base,data_base,len(data),len(data),4,4)
    body = header + ph + bytes(0x100 - len(header + ph)) + code + data + strings + syms + names
    body += bytes(section_at - len(body))
    sections = [[0]*10,[1,1,6,base,0x100,len(code),0,0,4,0],
                [7,1,2,data_base,offsets[1],len(data),0,0,4,0],
                [13,3,0,0,str_at,len(strings),0,0,1,0],[21,2,0,0,sym_at,len(syms),3,1,4,16],
                [29,3,0,0,names_at,len(names),0,0,1,0]]
    target.mkdir(parents=True,exist_ok=True)
    (target/'fixture.elf').write_bytes(body + b''.join(struct.pack('<10I',*s) for s in sections))
    (target/'fixture.externals').write_text('override 0x128700\noverride 0x128180\noverride 0x133070\n')
    out = '#pragma once\n#include <array>\n#include <cstdint>\n'
    out += 'struct SwitchCase { uint32_t entry,pc,reg,bias,defaultTarget,table,count; std::array<uint32_t,12> targets,markers; uint32_t defaultMarker; };\n'
    out += 'constexpr SwitchCase switches[] = {\n'
    for a,pc,reg,bias,default,table,targets,landings in switch_rows:
        out += '{'+','.join(map(str,[a,pc,reg,bias,default,table,len(targets)]))+',{'+','.join(map(str,targets))+'},{'+','.join(str(landings.index(t)+1) for t in targets)+'},'+str(landings.index(default)+1)+'},\n'
    out += '};\nstruct CallCase { uint32_t entry,pc,field,slot,gpSlot; bool conditional; };\nconstexpr CallCase calls[] = {\n'
    for row in call_rows: out += '{'+','.join(map(str,row[:-1]))+','+str(row[-1]).lower()+'},\n'
    out += '};\n'
    (target/'dispatch_cases.h').write_text(out)


if __name__ == '__main__': main()
