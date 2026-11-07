"""Bulk byte ranges and bounded caller-instantiated callee evidence.

This reports possible writes, not successful execution or immutable tables.
Unknown lengths, invalid ranges, opaque calls and exploration bounds stay open.
"""
import collections
import hashlib
import struct

from dc_alias_audit import (BULK_CALLS, NATIVE_BULK, Pointer, UNKNOWN, WIDENED, analyze,
                            constant, decode, describe, encode, physical, scalar, shifted)


def nested_effect_regions(symbols, word, dun_word=None):
    # Layout is established by the complete CSHOT_EFFECT constructor's calls,
    # not inferred from a numeric field match in an unrelated object type.
    guards={0x14353c:0x70808628,0x143550:0xae0200a0,
            0x14368c:0x70808628,0x143690:0x26040010,0x143694:0x0c050d4c,0x14369c:0x260411c0,
            0x1436a0:0x3c020014,0x1436a4:0x24453530,0x1436a8:0x70003628,
            0x1436ac:0x240711b0,0x1436b0:0x24080008,0x1436b4:0x0c0488b4}
    if any(word(pc)!=expected for pc,expected in guards.items()):
        raise ValueError('CSHOT_EFFECT nested constructor layout changed')
    matches=[(pc,size) for name,pc,size,kind,section in symbols if name=='MasekiEffect' and kind==1 and section==8]
    if len(matches)!=1 or matches[0][1]!=5*0xa160:
        raise ValueError('DUN MasekiEffect array layout changed')
    if dun_word is not None:
        binding={0x1dc3730:0x3c0201e6,0x1dc3734:0x2444b380,0x1dc3738:0x3c020014,
                 0x1dc373c:0x24453680,0x1dc3740:0x70003628,0x1dc3744:0x3407a160,
                 0x1dc3748:0x24080005,0x1dc374c:0x0c0488b4,0x1dc3750:0}
        if any(dun_word(pc)!=expected for pc,expected in binding.items()):
            raise ValueError('DUN MasekiEffect constructor binding changed')
    start,size=matches[0];fields=(0xb0,*(0x1260+i*0x11b0 for i in range(8)))
    regions=[dict(name=f'MasekiEffect[{i}].character[{j}]:vptr',start=start+i*0xa160+field,
                  end=start+i*0xa160+field+4) for i in range(5) for j,field in enumerate(fields)]
    return regions,dict(name='MasekiEffect',start=start,end=start+size,stride=0xa160,count=5,
                        character_vptr_offsets=list(fields),limit='constructor-established layout; stored runtime pointer values remain mutable')


def bulk_effect(event, regions):
    dst,length=decode(event['destination']),decode(event['length'])
    src=decode(event['source']);operation=event['operation']
    out=dict(event,ranges=[],overlaps=[],unresolved=[],unknown_extent_may_reach=[])
    if operation=='memmove':
        out.update(status='unresolved',unresolved=['compiled_64_bit_size_not_established'],
                   limit='compiled memmove tests a 64-bit size; low-word flow alone cannot establish even a zero-length no-op')
        return out
    if scalar(length)==0:
        out['status']='zero_length';return out
    lengths=[] if length is None else [p.offset for p in length if p.base=='absolute']
    destinations=[] if dst is None else [p.offset for p in dst if p.base=='absolute']
    if length is None or any(p.base!='absolute' for p in length):out['unresolved'].append('length_unknown')
    if dst is None or any(p.base!='absolute' for p in dst):out['unresolved'].append('destination_unknown')
    if 'length_unknown' in out['unresolved']:
        out['unknown_extent_may_reach']=sorted({r['name'] for p in destinations if physical(p)<0x2000000
                                               for r in regions if physical(p)<r['end'] and r['start']<0x2000000})
    if 'destination_unknown' in out['unresolved']:out['unknown_destination_may_alias_watched_ranges']=True
    for original in destinations:
        for count in lengths:
            if count==0:continue
            at=physical(original)
            if original+count>0x100000000:
                out['unresolved'].append('virtual_range_wraps');continue
            if 0x70000000<=original and original+count<=0x70004000:
                out['ranges'].append(dict(start=original,end=original+count,space='scratchpad'));continue
            if 0x11004000<=original and original+count<=0x11005000:
                out['ranges'].append(dict(start=original,end=original+count,space='vu0-data'));continue
            if at+count>0x2000000:
                out['unresolved'].append('not_a_valid_contiguous_guest_ram_range');continue
            out['ranges'].append(dict(start=at,end=at+count,space='ram'))
            for r in regions:
                lo,hi=max(at,r['start']),min(at+count,r['end'])
                if lo>=hi:continue
                overlap=dict(region=r['name'],start=lo,end=hi)
                source=scalar(src)
                if operation!='memset' and source is not None:
                    # Source bytes are an origin hint, not a constant pointer.
                    origin=physical(source)
                    if source+count<=0x100000000 and origin+count<=0x2000000:
                        overlap['source_start']=origin+lo-at
                        overlap['source_end']=origin+hi-at
                out['overlaps'].append(overlap)
    out['unresolved']=sorted(set(out['unresolved']))
    out['status']='unresolved' if out['unresolved'] else 'possible_overlap' if out['overlaps'] else 'disjoint_watched_ranges'
    out['limit']='destination write range if the call succeeds; source validity and copied pointer values are not established'
    return out


def overlay_load_contract(raw, regions, word=None):
    import struct
    if len(raw)<0x80 or raw[:4]!=b'MWo3':raise ValueError('unexpected DUN module header')
    if word is not None:
        guards={0x122df8:0x70a09e28,0x122dfc:0x70808628,0x122e00:0x70808e28,
                0x122e04:0x8c920014,0x122e1c:0x02138021,0x122e20:0x72002628,
                0x122e24:0x70002e28,0x122e28:0x72403628,0x122e2c:0x0c040d5a,
                0x122e34:0x8e240018,0x122e38:0x8e25001c,0x122e44:0x0c048a58,
                0x122e94:0x70a09628,0x122eec:0x72402e28,0x122ef0:0x72203628,0x122ef4:0x0c0460ae,
                0x122f5c:0x0c048ba0,0x122f64:0x70408628,0x122f7c:0x72402628,
                0x122f80:0x72002e28,0x122f84:0x0c048b78}
        if any(word(pc)!=expected for pc,expected in guards.items()):
            raise ValueError('overlay loader range contract changed')
    base=struct.unpack_from('<I',raw,8)[0];bss=struct.unpack_from('<I',raw,20)[0]
    begin,end=struct.unpack_from('<2I',raw,24)
    if base+len(raw)+bss>0x2000000 or not base<=begin<=end<=base+len(raw) or (end-begin)&3:
        raise ValueError('invalid DUN load/constructor ranges')
    ctors=[struct.unpack_from('<I',raw,pc-base)[0] for pc in range(begin,end,4)]
    def overlaps(a,b):return [r['name'] for r in regions if a<r['end'] and r['start']<b]
    return {'base':base,'file_bytes':len(raw),'bss_bytes':bss,'constructors':ctors,
            'file_write':dict(start=base,end=base+len(raw),regions=overlaps(base,base+len(raw))),
            'bss_zero':dict(start=base+len(raw),end=base+len(raw)+bss,regions=overlaps(base+len(raw),base+len(raw)+bss)),
            'order':['successful file read','BSS zero','constructor calls'],
            'limit':'artifact-instantiated loader contract, conditional on successful exact-file read at the declared base; mutable loader destinations and header aliases remain open'}


def straightline_array_count(program, event):
    """Prove a positive/zero 64-bit count from an adjacent zero-based immediate.

    Compiler-known entries, branches/cases and preceding delay slots must not
    bypass the definition. Arbitrary unproved interior entries remain out of scope.
    """
    pc=event['pc'];word=program['word']
    owner=next((f for f in program['functions'] if f['start']<=pc<f['end']),None)
    if owner is None or pc-4<owner['start'] or pc in owner.get('entries',()):return None
    try:
        if word(pc)!=0x0c0488b4 or word(pc+4)!=0:return None
        definition=word(pc-4);op=definition>>26
        if (definition>>21&31)!=0 or (definition>>16&31)!=8 or op not in (9,13):return None
        n=definition&0xffff
        if op==9 and n>=0x8000:return None
        if any(at in program['natives'] for at in (pc-4,pc,pc+4)):return None
        for at in range(owner['start'],owner['end'],4):
            w=word(at);opcode=w>>26;rs=w>>21&31;rt=w>>16&31
            branch=opcode in (1,4,5,6,7,20,21,22,23) or (opcode in (16,17,18) and rs==8)
            transfer=branch or opcode in (2,3) or (opcode==0 and w&63 in (8,9))
            if at==pc-8 and transfer:return None
            if opcode in (2,3) and ((at+4)&0xf0000000 | (w&0x3ffffff)<<2)==pc:return None
            if branch:
                imm=(w&0xffff)-(0x10000 if w&0x8000 else 0)
                if at+4+imm*4==pc:return None
        if any(pc in s.get('targets',()) for s in program['facts'].values()):return None
        return n
    except (KeyError,ValueError):return None


def contextual_writes(programs, seed_calls, regions, objects, gp, roots, max_depth=4,max_contexts=1536,array_limit=16):
    """Instantiate direct callees with caller values. Never run a replaced body.

    Each callee still treats return/memory effects of its calls as opaque. Child
    contexts add evidence; they are not complete summaries or a memory fixed point.
    """
    pending=collections.deque();contexts=[];boundaries=[];seen={}
    def related(arguments):
        for v in arguments:
            if v is None:continue
            for p in v:
                if p.base.startswith('root:'):return True
                if p.base=='absolute' and any(r['start']<=physical(p.offset)<r['end'] for r in (*regions,*objects)):return True
        return False
    def enqueue(source,event,depth,parent=None,condition=None,active_overlay=None):
        args=[decode(v) for v in event['argument_values']]
        if source!='dun' and parent is None and event['target'] not in (0x125990,0x125a90) and not related(args):return
        namespace=f'{source}:{event.get("owner",0):x}:{parent}'
        args=[None if v is None else frozenset(Pointer(f'caller:{namespace}:{p.base}',p.offset)
                if p.base=='stack' or p.base.startswith('arg') else p for p in v) for v in args]
        if active_overlay is None and source in ('title','dun'):active_overlay=source
        item=(source,event,args,depth,parent,condition,active_overlay)
        if parent is not None and related(args):pending.appendleft(item)
        else:pending.append(item)
    priority={0x1222d0,0x125990,0x125a90,0x124bf0,0x1246f0,0x13f720,0x143530,0x157010,0x143680}
    def control_data_argument(row):
        return any(p.base=='absolute' and any(r['start']<=physical(p.offset)<r['end'] for r in regions
                    if r['name'].startswith(('packet:','__vt__','DUN-switch:')))
                   for raw in row[1]['argument_values'] for p in (decode(raw) or ()))
    seeds=sorted(seed_calls,key=lambda row:(not control_data_argument(row),row[1]['target'] not in (0x125990,0x13f720),row[1]['target'] not in priority,
                 not related([decode(v) for v in row[1]['argument_values']]),row[0]!='dun',row[1]['pc']))
    for source,event in seeds:enqueue(source,event,0)
    while pending:
        source,event,args,depth,parent,condition,active_overlay=pending.popleft();target=event['target']
        path=dict(source_image=source,call_pc=event['pc'],target=target,parent=parent,depth=depth)
        if target in NATIVE_BULK:continue  # Native-u32 byte contract; memmove remains a compiled callee.
        if target is None:
            boundaries.append(dict(path,reason='computed_callee'));continue
        if target>=0x1dabd00:
            selected=active_overlay or 'dun'
            if active_overlay is None:condition='requires_selected_DUN';active_overlay='dun'
        else:selected='dc'
        program=programs[selected];kind=program['externals'].get(target)
        if kind=='stub' or (event.get('transfer','call')=='call' and (kind=='override' or target in program['overrides'])):
            boundaries.append(dict(path,target_image=selected,reason='native_or_sdk_boundary'));continue
        owner=next((f for f in program['functions'] if target==f['start'] or target in f.get('entries',())),None)
        if owner is None:
            boundaries.append(dict(path,target_image=selected,reason='no_compiler_entry'));continue
        count_proof=straightline_array_count(programs[source],event) if target==0x1222d0 else None
        if target==0x1222d0 and count_proof is not None and scalar(args[4])==count_proof:
            body=b''.join(struct.pack('<I',program['word'](pc)) for pc in range(0x1222d0,0x1223fc,4))
            if hashlib.sha256(body).hexdigest()!='dc1f0d3ccf0538499373499d9d31be35b5f1d81b1a4bfeafa4db5fbaca20306b':
                raise ValueError('constructor-array normal-loop contract changed')
        if depth>max_depth or len(contexts)>=max_contexts:
            boundaries.append(dict(path,target_image=selected,reason='context_depth_bound' if depth>max_depth else 'context_count_bound'));continue
        key=(selected,target,repr([encode(v) for v in args]),condition,active_overlay)
        if key in seen:
            contexts[seen[key]]['also_reached_from'].append(path);continue
        index=len(contexts);seen[key]=index
        row=dict(path,id=index,target_image=selected,active_overlay=active_overlay,condition=condition,
                 arguments=[encode(v) for v in args],also_reached_from=[])
        canonical_array=target==0x1222d0 and count_proof is not None and scalar(args[4])==count_proof
        if target==0x1222d0:row['array_count_64_proof']=count_proof if canonical_array else None
        contexts.append(row)
        flow=analyze(program['word'],owner,program['facts'],gp,regions,roots,program['names'],
                     native_pcs=program['natives'],input_arguments=args,entry_addresses=(target,))
        row.update(limitations=flow['limitations'],stats=flow['stats'],writes=[],symbolic_writes=[],bulk_calls=[])
        for e in flow['events']:
            if e['kind']=='symbolic_write':row['symbolic_writes'].append(e)
            if e['kind'] in ('region_write','pointer_store','untyped_field_write'):row['writes'].append(e)
            if e['kind']=='bulk_call':row['bulk_calls'].append(bulk_effect(e,regions))
            if e['kind']=='call' and not (canonical_array and e['pc']==0x12233c):
                enqueue(selected,dict(e,owner=owner['start']),depth+1,index,condition,active_overlay)
        # __construct_array's normal loop calls ctor(element,1). Exception and
        # destructor paths remain separate opaque effects, not an exhaustive loop proof.
        if target==0x1222d0:
            ctor,stride,count=scalar(args[1]),scalar(args[3]),scalar(args[4])
            if not canonical_array:
                boundaries.append(dict(path,parent=index,reason='array_count_64_unproved'))
            elif ctor is None or stride is None or count is None:
                boundaries.append(dict(path,parent=index,reason='array_constructor_arguments_unknown'))
            else:
                for i in range(min(count,array_limit)):
                    child=[shifted(args[0],i*stride),constant(1),*[None]*6]
                    enqueue(selected,dict(pc=0x12233c,target=ctor,argument_values=[encode(v) for v in child],transfer='call',owner=target),depth+1,index,condition,active_overlay)
                if count>array_limit:boundaries.append(dict(path,parent=index,reason='array_element_bound',count=count))
    return dict(contexts=contexts,boundaries=boundaries,max_depth=max_depth,max_contexts=max_contexts,array_limit=array_limit,
                limit='bounded possible callee writes; no conclusion of no writes from an empty event list; calls, exceptions, unknown stores and skipped contexts remain open')
