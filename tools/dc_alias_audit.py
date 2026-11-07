"""Bounded R5900 pointer-flow evidence, not a complete alias/type analysis.

Values are sets of affine constants/arguments/loads. Joins that exceed the bound,
unsupported computations, unknown memory and callee effects remain explicit.
Mutable image data is never treated as a constant pointer value.
"""
import collections
import dataclasses
import struct


@dataclasses.dataclass(frozen=True, order=True)
class Pointer:
    base: str
    offset: int = 0


UNKNOWN=Pointer('unknown')
WIDENED=Pointer('widened')


def value(base, offset=0):
    return frozenset((Pointer(base, offset),))


def constant(n):
    return value('absolute', n & 0xffffffff)


def physical(n):
    n &= 0xffffffff
    if 0x80000000 <= n < 0xc0000000:
        return n & 0x1fffffff
    if 0x20000000 <= n < 0x40000000:
        return n & 0x0fffffff
    return n


def describe(v):
    if v is None:
        return ['unknown']
    return [p.base if p in (UNKNOWN,WIDENED) else f'{p.base}{p.offset:+#x}' for p in sorted(v)]


def shifted(v, amount):
    if v is None:
        return None
    result=[]
    for p in v:
        if p in (UNKNOWN,WIDENED):result.append(p);continue
        n=(p.offset+amount)&0xffffffff
        result.append(Pointer(p.base,n if p.base=='absolute' or n<0x80000000 else n-0x100000000))
    return frozenset(result)


def scalar(v):
    if v is not None and len(v) == 1:
        p = next(iter(v))
        if p.base == 'absolute':
            return p.offset
    return None


def merge_value(a, b, limit=4):
    if a is None and b is None:return None
    both=(a if a is not None else frozenset((UNKNOWN,))) | (b if b is not None else frozenset((UNKNOWN,)))
    if WIDENED in both or len(both-{UNKNOWN})>limit:return frozenset((WIDENED,))
    return both if both!=frozenset((UNKNOWN,)) else None


# width, write, GPR result, alignment. Partial accesses use an enclosing range;
# their merged result is unknown. SQ/LQ alignment matches the production runtime.
MEMORY = {
    26:(8,False,True,8), 27:(8,False,True,8), 30:(16,False,True,16), 31:(16,True,False,16),
    32:(1,False,True,1), 33:(2,False,True,1), 34:(4,False,True,4), 35:(4,False,True,1),
    36:(1,False,True,1), 37:(2,False,True,1), 38:(4,False,True,4), 39:(4,False,True,1),
    40:(1,True,False,1), 41:(2,True,False,1), 42:(4,True,False,4), 43:(4,True,False,1),
    44:(8,True,False,8), 45:(8,True,False,8), 46:(4,True,False,4),
    49:(4,False,False,1), 53:(8,False,False,1), 54:(16,False,False,16), 55:(8,False,True,1),
    57:(4,True,False,1), 61:(8,True,False,1), 62:(16,True,False,16), 63:(8,True,False,1),
}
PARTIAL = {26,27,34,38,42,44,45,46}
BULK_CALLS = {0x1033b0:'memcpy',0x103460:'memmove',0x103568:'memset'}
NATIVE_BULK = {0x1033b0,0x103568}


def encode(v):
    return None if v is None else [{'base':p.base,'offset':p.offset} for p in sorted(v)]


def decode(v):
    return None if v is None else frozenset(Pointer(p['base'],p['offset']) for p in v)


def analyze(word, function, sites, gp, regions, roots=(), names=None, max_steps=100000, native_pcs=(),
            input_arguments=None, entry_addresses=None):
    """Intraprocedural evidence; no claim about all unknown-address stores/callees.

    Regions are half-open watched byte ranges. Symbolic field matches are untyped:
    CCharacter data at +0x2b8 must not be called a camera vptr merely by offset.
    """
    start, end = function['start'], function['end']
    names = names or {}
    roots = set(roots)
    events, limitations, stats = {}, set(), collections.Counter()
    initial = [None] * 32
    initial[0], initial[28], initial[29] = constant(0), constant(gp), value('stack')
    for reg in range(4,12):
        initial[reg] = value(f'arg{reg - 4}') if input_arguments is None else input_arguments[reg-4]
    incoming, todo = {}, collections.deque()

    def emit(kind, pc, **fields):
        event = dict(kind=kind, pc=pc, **fields)
        events[repr(event)] = event

    def enqueue(pc, registers, memory):
        if not start <= pc < end:
            return
        state = (tuple(registers), dict(memory))
        old = incoming.get(pc)
        if old is not None:
            joined = tuple(merge_value(a,b) for a,b in zip(old[0],state[0]))
            stack = {key:merge_value(old[1].get(key),memory.get(key)) for key in old[1].keys() | memory.keys()}
            stack = {k:v for k,v in stack.items() if v is not None}
            if any(v is not None and WIDENED in v for v in joined):
                limitations.add('value_set_widened')
            if any(v is not None and UNKNOWN in v for v in joined):
                limitations.add('unknown_value_at_join')
            state = (joined, stack)
            if state == old:
                return
        incoming[pc] = state
        todo.append(pc)

    def addresses(v, imm, alignment):
        v = shifted(v,imm)
        if v is None:
            return None
        result = set()
        for p in v:
            if p in (UNKNOWN,WIDENED):result.add(p)
            elif p.base in ('absolute','stack'):
                off = physical(p.offset) if p.base == 'absolute' else p.offset
                result.add(Pointer(p.base,off & -alignment))
            elif alignment != 1:
                # Unknown object alignment: do not invent an exact aligned EA.
                limitations.add('symbolic_alignment_unknown')
            else:
                result.add(p)
        return frozenset(result) if result else None

    def memory_access(pc, at, width, write, source, bulk=False, complete_pointer=True):
        if width<=0:return
        if at is None or any(p in (UNKNOWN,WIDENED) for p in at):
            stats['unknown_address_writes' if write else 'unknown_address_reads'] += 1
            if at is None:return
        for p in at:
            if p in (UNKNOWN,WIDENED):continue
            if write and p.base not in ('absolute','stack'):
                emit('symbolic_write',pc,destination=encode(frozenset((p,))),width=width,source=encode(source))
            if p.base == 'absolute':
                for region in regions:
                    if p.offset < region['end'] and region['start'] < p.offset + width:
                        emit('bulk_region_write' if bulk else 'region_write' if write else 'region_read',pc,region=region['name'],
                             address=p.offset,width=width,source=describe(source) if write else [])
            elif write and p.base != 'stack' and any(p.offset < f+4 and f < p.offset+width for f in (0xa0,0x2b8)):
                emit('untyped_field_write',pc,address=describe(frozenset((p,))),width=width,source=describe(source))
        if write and source is not None and width>=4 and complete_pointer:
            for p in source:
                if p.base == 'absolute':
                    for region in regions:
                        if region['start'] <= physical(p.offset) < region['end']:
                            emit('pointer_store',pc,region=region['name'],value=p.offset,destination=describe(at),width=width)

    def step(pc, registers, memory):
        w = word(pc)
        op,rs,rt,rd,fun = w>>26,w>>21&31,w>>16&31,w>>11&31,w&63
        imm = (w&0xffff) - (0x10000 if w&0x8000 else 0)
        if op in MEMORY:
            width,write,gpr,alignment = MEMORY[op]
            at = addresses(registers[rs],imm,alignment)
            source = registers[rt] if op in (31,40,41,42,43,44,45,46,63) else None
            memory_access(pc,at,width,write,source,complete_pointer=op not in PARTIAL)
            if write:
                if at is None or any(p.base not in ('absolute','stack') for p in at):
                    memory.clear()  # unknown store may alias a saved pointer
                else:
                    # Numeric and symbolic stack addresses are not proved
                    # disjoint. Forward only this flow's own writes, never ELF
                    # initial data; invalidate on possibly aliasing stores.
                    for p in at:
                        for key in list(memory):
                            base,a,size=key
                            if base!=p.base or (p.offset<a+size and a<p.offset+width):
                                if base!=p.base:limitations.add('absolute_store_stack_alias_unresolved')
                                del memory[key]
                    if len(at)==1 and (p:=next(iter(at))).base in ('stack','absolute') and source is not None and width>=4 and op not in PARTIAL:
                        memory[p.base,p.offset,width]=source
                return
            if gpr and rt:
                result = None
                if op not in PARTIAL and at is not None:
                    values=[]
                    for p in at:
                        if p in (UNKNOWN,WIDENED):
                            values.append(None)
                        elif width>=4:
                            compatible=[v for (base,offset,size),v in memory.items()
                                        if base==p.base and offset==p.offset and size>=4]
                            forwarded=compatible[0] if compatible else None
                            for candidate in compatible[1:]:forwarded=merge_value(forwarded,candidate)
                            if forwarded is not None:
                                values.append(forwarded);continue
                            if p.base=='stack':values.append(None);continue
                            base=f'root:{p.offset:#x}' if p.base=='absolute' and p.offset in roots else f'mem({p.base}{p.offset:+#x})'
                            values.append(value(base) if base.count('mem(')<=2 else None)
                        else:
                            values.append(None)
                    if values:
                        result=values[0]
                        for v in values[1:]: result=merge_value(result,v)
                registers[rt]=result
            return
        dest, result = None, None
        if op==15:
            dest,result=rt,constant((w&0xffff)<<16)
        elif op in (8,9,24,25):
            dest,result=rt,shifted(registers[rs],imm)
        elif op in (12,13,14):
            dest=rt; a=scalar(registers[rs]);b=w&0xffff
            if a is not None: result=constant({12:a&b,13:a|b,14:a^b}[op])
            elif op in (13,14) and b==0: result=registers[rs]
        elif op==0 and fun in (0x20,0x21,0x2c,0x2d,0x22,0x23,0x2e,0x2f):
            dest=rd;a,b=scalar(registers[rs]),scalar(registers[rt]);subtract=fun in (0x22,0x23,0x2e,0x2f)
            if b is not None: result=shifted(registers[rs],-b if subtract else b)
            elif a is not None and not subtract: result=shifted(registers[rt],a)
        elif op==0 and fun in (0x24,0x25,0x26,0x27):
            dest=rd;a,b=scalar(registers[rs]),scalar(registers[rt])
            if a is not None and b is not None:
                result=constant({0x24:a&b,0x25:a|b,0x26:a^b,0x27:~(a|b)}[fun])
            elif fun in (0x25,0x26) and (a==0 or b==0): result=registers[rt] if a==0 else registers[rs]
        elif op==28 and (w&0x7ff)==0x628:
            dest=rd
            if scalar(registers[rt])==0: result=registers[rs]
            elif scalar(registers[rs])==0: result=registers[rt]
        elif op==0 and fun in (0,2,3,0x38,0x3a,0x3b,0x3c,0x3e,0x3f):
            dest=rd;a=scalar(registers[rt]);n=w>>6&31
            if fun in (0x3c,0x3e,0x3f):n+=32
            if n==0:result=registers[rt]
            elif a is not None and fun in (0,0x38,0x3c,2):
                # Only low 32 bits are tracked; a 64-bit right shift would need
                # the unknown upper half, even when the low word is constant.
                result=constant(a>>n if fun==2 else a<<n)
        elif op==0 and fun in (10,11):
            dest=rd;condition=scalar(registers[rt])
            if rt==0:result=registers[rs] if fun==10 else registers[rd]
            elif condition is None or condition==0:
                # MOVZ/MOVN test 64 bits. A nonzero low word proves nonzero;
                # a zero low word does not prove that the upper word is zero.
                result=merge_value(registers[rd],registers[rs])
                if condition==0:limitations.add('conditional_move_upper_word_unknown')
            else:result=registers[rs] if fun==11 else registers[rd]
        elif op in (10,11):
            dest=rt
        elif op in (16,17,18) and rs<=2:
            dest=rt
        elif op in (0,28):
            dest=rd
        if dest:
            if result is None: stats['unknown_register_results']+=1
            registers[dest]=result

    def call(pc, target, registers, memory, transfer='call'):
        args={f'a{i}':describe(registers[i+4]) for i in range(8) if registers[i+4] is not None}
        # Unknown callees remain opaque. Native memcpy/memset have a u32
        # byte-range/return-alias contract; compiled memmove remains open.
        emit('call',pc,target=target,name=names.get(target,''),arguments=args,
             argument_values=[encode(registers[i]) for i in range(4,12)],transfer=transfer)
        returned=None
        if target in BULK_CALLS:
            length=scalar(registers[6]);dst=addresses(registers[4],0,1)
            emit('bulk_call',pc,target=target,operation=BULK_CALLS[target],destination=encode(registers[4]),
                 source=encode(registers[5]),length=encode(registers[6]))
            if target not in NATIVE_BULK:
                limitations.add('compiled_bulk_size_upper_word_unproved')
            elif length is not None:
                memory_access(pc,dst,length,True,None,bulk=True)
            else: limitations.add('bulk_write_length_unknown')
            if target in NATIVE_BULK:returned=registers[4]
        for r in (*range(1,16),24,25,26,27,31): registers[r]=None
        memory.clear()
        registers[2]=returned
        limitations.add('bulk_contents_and_failure_paths_unmodeled' if target in BULK_CALLS else
                        'callee_memory_and_return_effects_unmodeled')

    for entry in ((start,*function.get('entries',())) if entry_addresses is None else entry_addresses):
        if entry&3 or not start<=entry<end:raise ValueError('alias entry outside compiled owner')
        enqueue(entry,initial,{})
    processed=0
    def system_boundary(w):
        return (w>>26==0 and w&63 in (12,13)) or (w>>26==16 and (w>>21&31)>=16)
    while todo:
        if processed>=max_steps:
            limitations.add('step_bound_reached');break
        pc=todo.popleft(); processed+=1
        if pc in native_pcs:
            limitations.add('native_instruction_effects_unmodeled');continue
        registers=list(incoming[pc][0]);memory=dict(incoming[pc][1])
        w=word(pc);op,rs,rt,fun=w>>26,w>>21&31,w>>16&31,w&63
        if system_boundary(w):
            limitations.add('system_instruction_effects_unmodeled');continue
        branch=op in (4,5,6,7,20,21,22,23) or (op==1 and rt in (0,1,2,3,16,17,18,19)) or (op in (16,17,18) and rs==8)
        jump=op in (2,3) or (op==0 and fun in (8,9))
        if not branch and not jump:
            step(pc,registers,memory);enqueue(pc+4,registers,memory);continue
        if pc+8>end:
            limitations.add('missing_delay_slot');continue
        linked=op==3 or (op==0 and fun==9 and (w>>11&31)!=0) or (op==1 and rt in (16,17,18,19))
        target=((pc+4)&0xf0000000 | (w&0x3ffffff)<<2) if op in (2,3) else None
        if op==0: target=scalar(registers[rs])
        if branch:
            imm=(w&0xffff)-(0x10000 if w&0x8000 else 0);target=pc+4+imm*4
        if linked:registers[(w>>11&31) if op==0 else 31]=constant(pc+8)
        saved_registers=list(registers);saved_memory=dict(memory)
        likely = branch and (op in (20,21,22,23) or (op==1 and rt in (2,3,18,19)) or
                             (op in (16,17,18) and (rt&2)))
        taken,fall=True,True
        if op in (4,5,20,21) and rs==rt:
            taken=op in (4,20);fall=not taken
        elif op==1 and rs==0:
            taken=rt in (1,3,17,19);fall=not taken
        elif op in (6,7,22,23) and rs==0:
            taken=op in (6,22);fall=not taken
        # Control flow in a delay slot is explicitly outside this analysis.
        delay=word(pc+4);dop=delay>>26
        delay_executes=not (likely and not taken)
        delay_limit=None
        if delay_executes:
            if pc+4 in native_pcs:delay_limit='native_instruction_effects_unmodeled'
            elif system_boundary(delay):delay_limit='system_instruction_effects_unmodeled'
            elif dop in (1,2,3,4,5,6,7,20,21,22,23) or (dop==0 and delay&63 in (8,9)) or (dop in (16,17,18) and (delay>>21&31)==8):
                delay_limit='control_in_delay_slot'
        if delay_limit:
            limitations.add(delay_limit)
            if branch and likely and fall:enqueue(pc+8,saved_registers,saved_memory)
            continue
        if delay_executes:
            step(pc+4,registers,memory)
        if linked and not branch:
            call(pc,target,registers,memory);enqueue(pc+8,registers,memory)
            continue
        if branch:
            if taken:
                if linked:
                    called=list(registers);called_memory=dict(memory)
                    call(pc,target,called,called_memory);enqueue(pc+8,called,called_memory)
                elif start<=target<end: enqueue(target,registers,memory)
                else:call(pc,target,list(registers),dict(memory),'tail')
            if fall:
                if likely:
                    enqueue(pc+8,saved_registers,saved_memory)
                else:enqueue(pc+8,registers,memory)
        elif op==2:
            if start<=target<end:enqueue(target,registers,memory)
            else:call(pc,target,registers,memory,'tail')
        elif rs==31:
            if registers[2] is not None: emit('return',pc,value=describe(registers[2]))
        elif sites.get(pc,{}).get('kind')=='compiler_switch':
            for t in sites[pc]['targets']:enqueue(t,registers,memory)
            limitations.add('mutable_switch_fallback_unmodeled')
        else:
            limitations.add('indirect_jump_target_unmodeled')
    stats['processed_states']=processed
    return {'events':sorted(events.values(),key=lambda e:(e['pc'],e['kind'],repr(e))),
            'limitations':sorted(limitations),'stats':dict(stats)}


def data_pointer_references(path, regions):
    """Aligned words in allocated, non-executable file-backed ELF sections only."""
    data=path.read_bytes();shoff=struct.unpack_from('<I',data,32)[0]
    size,count=struct.unpack_from('<HH',data,46)
    found=[]
    for i in range(count):
        s=struct.unpack_from('<10I',data,shoff+i*size)
        if s[1]!=1 or not s[2]&2 or s[2]&4:continue
        for offset in range((-s[3])&3,s[5]-3,4):
            pointer=struct.unpack_from('<I',data,s[4]+offset)[0]
            for region in regions:
                if region['start']<=physical(pointer)<region['end']:
                    found.append({'address':s[3]+offset,'value':pointer,'region':region['name']})
    return found


def audit_images(root, images, functions, overrides, replaced_body, image_words, symbols):
    """Run the bounded flow over compiled owners, preserving source-image identity."""
    from dc_callee_audit import nested_effect_regions, bulk_effect, contextual_writes, overlay_load_contract
    gp = next(pc for name,pc,_,_,_ in symbols if name=='_gp')
    regions = [{'name':f'packet:{offset:#x}', 'start':gp+offset, 'end':gp+offset+4}
               for offset in (-0x742c,-0x73f8,-0x73f4)]
    roots = (gp-0x6300,gp-0x6358)
    regions += [dict(name=name,start=pc,end=pc+4) for name,pc in zip(('NowWeapon','NowCamera'),roots)]
    # Names are not unique across overlays. DUN objects must have section 8.
    objects = [dict(name=name,start=pc,end=pc+size) for name,pc,size,kind,section in symbols
               if kind==1 and section==8 and name in
               ('MainWeapon','DefaultWeapon','CrashWeapon','CharaMain','CharaHand','MainCamera','SubCamera')]
    for obj in objects:
        field = 0x2b8 if obj['name'] in ('MainCamera','SubCamera') else 0xa0
        regions.append(dict(name=obj['name']+':vptr',start=obj['start']+field,end=obj['start']+field+4))
    regions += [dict(name=name,start=pc,end=pc+size) for name,pc,size,kind,_ in symbols
                if kind==1 and name.startswith('__vt__') and size]
    tables = {s['table']:s['targets'] for f in functions['dun'] for s in f['sites'] if s['kind']=='compiler_switch'}
    regions += [dict(name=f'DUN-switch:{pc:#x}',start=pc,end=pc+len(targets)*4) for pc,targets in sorted(tables.items())]
    resident_word=image_words(images/'dc.elf',root/'recomp/dc_native.patches')
    nested,layout=nested_effect_regions(symbols,resident_word,image_words(images/'dun.elf',root/'recomp/dun_native.patches'))
    regions+=nested;objects.append(layout)
    result={'gp':gp,'regions':regions,'dun_objects':objects,'images':{},'admitted_packet_alias_accesses':[],
            'limit':'bounded recognized low-word flow only; unknown stores, widenings, callee effects, heap/data aliases and computed transfers prevent complete alias/target-set closure'}
    result['nested_effect_layout']=layout
    result['overlay_load_contract']=overlay_load_contract((root/'game/DUN.BIN').read_bytes(),regions,resident_word)
    programs={};seed_calls=[];bulk_calls=[]

    def interesting_argument(arguments):
        for values in arguments.values():
            for v in values:
                if v.startswith('root:'):return True
                if v.startswith('absolute+'):
                    address=physical(int(v[len('absolute+'):],16))
                    if any(r['start']<=address<r['end'] for r in (*regions,*objects)):return True
        return False

    for image, owners in functions.items():
        word=image_words(images/f'{image}.elf',root/f'recomp/{image}_native.patches')
        names={f['start']:f.get('name','') for f in owners}
        names.update({pc:name for name,pc,_,kind,section in symbols if kind==2 and
                      (section==4 or section=={'title':6,'dun':8}.get(image))})
        facts={s['pc']:s for f in owners for s in f['sites']}
        external_file='dc_externals.txt' if image=='dc' else 'dc_overlay_externals.txt'
        natives={int(fields[1],16) for line in (images/external_file).read_text().splitlines()
                 if len(fields:=line.split())>=2 and fields[0]=='native'}
        externals={int(fields[1],16):fields[0] for line in (images/external_file).read_text().splitlines()
                   if len(fields:=line.split())>=2 and fields[0] in ('stub','override')}
        if any(externals.get(pc)!='stub' for pc in NATIVE_BULK):
            raise ValueError('native bulk binding contract changed')
        programs[image]=dict(word=word,functions=owners,facts=facts,names=names,natives=natives,
                             externals=externals,overrides=overrides)
        if set(BULK_CALLS)&overrides:raise ValueError('bulk memory contract replaced by a port override')
        events=[];stats=collections.Counter();limits=collections.Counter()
        for f in owners:
            flow=analyze(word,f,facts,gp,regions,roots,names,native_pcs=natives)
            native=replaced_body(image,f['start'],overrides)
            stats.update(flow['stats']);limits.update(flow['limitations'])
            for event in flow['events']:
                if event['kind']=='symbolic_write':continue  # Expanded callees retain these; baseline report stays focused.
                if event['kind']=='call' and not native:seed_calls.append((image,dict(event,owner=f['start'])))
                if event['kind']=='bulk_call':
                    bulk_calls.append(dict(bulk_effect(event,regions),image=image,owner=f['start'],native_replaced=native))
                if event['kind']=='return' and not interesting_argument({'v0':event['value']}):continue
                if event['kind']=='call' and event['target'] not in (*BULK_CALLS,0x1db9660,0x1db9590) and not interesting_argument(event['arguments']):
                    continue
                event=dict(event,owner=f['start'],native_replaced=native)
                events.append(event)
                if event['kind'] in ('region_read','region_write') and event['region'].startswith('packet:') and not native:
                    result['admitted_packet_alias_accesses'].append(dict(event,image=image))
        result['images'][image]={'events':events,'stats':dict(stats),'functions_with_limitations':dict(limits),
                                 'native_instruction_pcs':sorted(natives),
                                 'data_pointer_references':data_pointer_references(images/f'{image}.elf',regions),
                                 'overlay_data_limit':'DUN-named data ranges require selected DUN; TITLE coincidences are not DUN runtime writes'}
    result['bulk_calls']=bulk_calls
    from dc_loadchara_audit import loadchara_contract
    result['loadchara_copy_contract']=loadchara_contract(programs,regions)
    from dc_readinfo_audit import readinfo_contract
    result['readinfo_effect_contract']=readinfo_contract(programs)
    result['callee_writes']=contextual_writes(programs,seed_calls,regions,objects,gp,roots)
    result['admitted_bulk_packet_overlaps']=[b for b in bulk_calls if not b['native_replaced'] and
                                          any(r['region'].startswith('packet:') for r in b['overlaps'])]
    result['admitted_callee_packet_overlaps']=[]
    for context in result['callee_writes']['contexts']:
        for event in context['writes']:
            if event['kind']=='region_write' and event['region'].startswith('packet:'):
                result['admitted_callee_packet_overlaps'].append(dict(context=context['id'],event=event))
        for bulk in context['bulk_calls']:
            if any(r['region'].startswith('packet:') for r in bulk['overlaps']):
                result['admitted_callee_packet_overlaps'].append(dict(context=context['id'],event=bulk))
    return result
