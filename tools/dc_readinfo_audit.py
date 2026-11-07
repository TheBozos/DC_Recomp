"""Guarded ReadInfo effects and a bounded, metadata-only PreProcess walk.

Snapshots never establish callback identity, stack/arena nonaliasing or a stable
pack across nested loaders. No result is fed back into generic alias flow.
"""
import hashlib
import struct

PREPROCESS_SPAN = (0x13b270,0x13b38c,'207bb32a2477bf821da95ef7eef377cff4b421b916d415cf5d3f5b7f90a5397c')

SPANS = {
    'dc': ((0x1397f0,0x13988c,'8d10cd079e083cce0355be9427542534a95ac766688b19a3e4a85b95bf502bdd'),
           (0x139d10,0x13b38c,'f8e61eba9d43eb0b953ad649f6982b9f56844cecabe7c44545a822ffde58c0aa'),
           (0x13f720,0x13f7f8,'2ea2e17460c185e36b991368aed5e44ce83c5cd489ddcf800b26f292e7968e98'),
           (0x1278a0,0x1278fc,'92fc7bf7174a280c024479a8c8786c579ba228986d3bf649c81ef629cd4452d2')),
    'dun': ((0x1dba3c0,0x1dba928,'45de84d8e24aae8d78133ca02f88cef2607c06039b79a2448650a298c0f34d08'),)}


def guard_preprocess(read):
    """Bind the offline walk to the current boot artifact, not a stale report."""
    start,end,digest=PREPROCESS_SPAN
    raw=read(start,end-start)
    if len(raw)!=end-start or hashlib.sha256(raw).hexdigest()!=digest:
        raise ValueError('ReadInfo PreProcess artifact contract changed')


def preprocess(blob, start, size):
    """Walk the guarded leaf on a copy, with disjoint stable input metadata.

    Retains one-past reads/stores, including the read after an exhausted line
    comment. A read beyond the supplied outer snapshot rejects the walk. The
    caller receives byte offsets/PCs only, never CFG payload bytes.
    """
    if not 0 <= start <= start+size <= len(blob) or size > 0x7fffffff:
        raise ValueError('unsupported CFG extent for bounded PreProcess walk')
    data=bytearray(blob);writes=[];reads=[];i=0

    def byte(offset):
        at=start+offset
        if not 0 <= at < len(data):
            raise ValueError('PreProcess read outside outer snapshot')
        if offset >= size:reads.append(at)
        return data[at]

    def store(offset, pc):
        at=start+offset
        if not 0 <= at < len(data):
            raise ValueError('PreProcess store outside outer snapshot')
        data[at]=32;writes.append(dict(byte=at,pc=pc))

    while i<size:
        if byte(i)==47 and byte(i+1)==47:
            while i<size:
                if byte(i) in (10,13):break
                store(i,0x13b2cc);i+=1
        if byte(i)==47 and byte(i+1)==42:
            while i<size:
                if byte(i)==42 and byte(i+1)==47:
                    store(i,0x13b33c);store(i+1,0x13b340);break
                store(i,0x13b350);i+=1
            # The compiled outer increment is skipped after a block comment.
            continue
        i+=1
    return dict(writes=writes,reads_outside_cfg=sorted(set(reads)),
                writes_outside_cfg=[w for w in writes if not start<=w['byte']<start+size],
                changed_bytes=sum(a!=b for a,b in zip(blob,data)))


def config_effects(blob, nodes):
    """Snapshot checks for the CFG members used by the guarded DUN caller."""
    result=[]
    for node in nodes:
        if node['name'].lower() not in ('base.cfg','hand_up.cfg'):continue
        start,size=node['payload'],node['size']
        effect=preprocess(blob,start,size)
        result.append(dict(name=node['name'],node=node['node'],payload=start,size=size,
            payload_sha256=hashlib.sha256(blob[start:start+size]).hexdigest(),
            **effect,header_hits=[dict(w,header=h['name'],header_node=h['node'])
                for w in effect['writes'] for h in nodes if h['node']<=w['byte']<h['node']+80],
            limit='leaf snapshot only; stable size/pointer and disjoint input metadata required; earlier writes, parser overflows and callback effects are not modeled'))
    return result


def readinfo_contract(programs):
    for image,spans in SPANS.items():
        p=programs[image]
        for start,end,digest in spans:
            if any(start<=pc<end for pc in p['natives']):
                raise ValueError('ReadInfo contract contains a native instruction hook')
            if any(start<=pc<end for pc in set(p['overrides'])|set(p['externals'])):
                raise ValueError('ReadInfo contract owner replaced')
            raw=b''.join(struct.pack('<I',p['word'](pc)) for pc in range(start,end,4))
            if hashlib.sha256(raw).hexdigest()!=digest:
                raise ValueError('ReadInfo header/cursor contract changed')
    p=programs['dc'];callbacks=[]
    for i in range(19):
        target=p['word'](0x251470+4*i)
        owner=next((f for f in p['functions'] if f['start']==target),None)
        direct=[]
        if owner is not None:
            for pc in range(owner['start'],owner['end'],4):
                w=p['word'](pc)
                if w>>26==3:
                    direct.append(dict(pc=pc,target=((pc+4)&0xf0000000)|((w&0x3ffffff)<<2)))
        callbacks.append(dict(index=i,slot=0x251470+4*i,initial_target=target,
            initial_name=p['names'].get(target,''),
            direct_jal_candidates=direct,
            compiled_sites=[] if owner is None else owner['sites'],
            limit='mutable initial ELF word only; no closed nested effects or runtime target set'))
    dun_calls=[]
    for pc,wrapper,address,condition in ((0x1dba544,0x139850,0x1dc3168,'normal path'),
            (0x1dba5ac,0x139820,0x1dc3178,'selector == 1'),
            (0x1dba61c,0x139820,0x1dc3178,'selector == 3')):
        raw=b''.join(struct.pack('<I',programs['dun']['word'](address+i)) for i in range(0,64,4))
        dun_calls.append(dict(pc=pc,wrapper=wrapper,name_address=address,condition=condition,
            initial_name=raw.split(b'\0',1)[0].decode('latin-1') if b'\0' in raw else None,
            name_mutable=True))
    return dict(
        dun_calls=dun_calls,
        arena=0x1f06660,
        wrappers=[dict(entry=0x139850,model_arena='a3',motion_arena='a3',shadow_arena='a3',image_arena='t1'),
                  dict(entry=0x139820,model_arena='a3',motion_arena='t0',shadow_arena='t1',image_arena=0),
                  dict(entry=0x1397f0,computed_slot=0xb8,limit='virtual dispatch; target is not fixed')],
        parser_globals=dict(start=0x2a247c,end_exclusive=0x2a24dc),config_pointer=0x2a24dc,config_size=0x2a24e0,
        allocation_name_lists=[0x1c7ae70,0x1c7af00,0x1c7af90,0x1c7b020],
        image_slots=[0x1c7b0b0,0x1c7b0c0],cursor_reset=False,
        descriptors=dict(start=0x250f60,end=0x25146c,stride=68,mutable=True),
        command_table=dict(start=0x251470,end=0x2514bc,load_pc=0x139f30,call_pc=0x139f34,mutable=True),
        callbacks=callbacks,
        image_command=dict(entry=0x13aa50,lookup_pc=0x13aa9c,allocation_pc=0x13aac0,copy_pc=0x13aae8,
            request='s32(size) >> 4, then add 1 in 32 bits',check_order='allocation check precedes copy',
            limit='signed/wrapping allocator still accepts some invalid states; mapped range is not logical arena validity'),
        header_preservation='unproved across complete ReadInfo; PreProcess can store at payload+size, and aliased globals/stack/arenas or nested callbacks can overwrite headers',
        dispatch_effects='unresolved; IMG can copy to a DUN table under a synthetic aliased arena, and command targets are reloaded from mutable RAM',
        parser_limits=['SearchCommand and GetArg have unbounded writes to local 256-byte token buffers',
                       'descriptor sentinel traversal has no explicit 16-argument bound',
                       'MODEL calls a mutable character virtual slot before lookup and nested model loading'],
        limit='guarded local contracts and initial callback inventory only; no runtime change, immutable table, closed dispatch set, complete callback summary or unconditional nonalias proof')
