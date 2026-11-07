"""Conditional LoadChara2 copy bounds; no runtime or immutable-pack assumption.

The state evaluator takes explicit pre-copy values. The guarded report describes
the compiled path; it does not promote artifact bytes or post-copy checks into
facts about all calls. Byte writes can change the cursor, size slot or code data.
"""
import hashlib
import struct

from dc_alias_audit import physical

GLOBAL_START, GLOBAL_END = 0x2ab080, 0x1c74980
CAPACITY = 0x33450
SITES = ((0x1dba710,0x1dba73c,0x1dba768,0x1d8,0x1dc3188),
         (0x1dba7b0,0x1dba7e0,0x1dba80c,0x1d8,0x1dc3198),
         (0x1dba830,0x1dba860,0x1dba88c,0x1dc,0x1dc31a8))
SPANS = {
    'dc': ((0x1278a0,0x1278fc,'92fc7bf7174a280c024479a8c8786c579ba228986d3bf649c81ef629cd4452d2'),
           (0x125a90,0x125adc,'bdf1076c9cfabcad04fe1cc3c8d2391097d881fa439627b9f0a45264ca3b6fef'),
           (0x127a00,0x127b34,'ee899148bd9c6712118f28d4f2ad989063d3d57a71830cf231c7165898540e2a'),
           (0x13f720,0x13f7f8,'2ea2e17460c185e36b991368aed5e44ce83c5cd489ddcf800b26f292e7968e98')),
    'dun': ((0x1dba6cc,0x1dba770,'4f5beb5925007335e9b0105d16a5fd808aeb652f2f08609ff9bd3f51842d430f'),
            (0x1dba7a0,0x1dba814,'a2beb138842117881249c6c58aa0cbdc01f96620a1dd76047319d1bccc337390'),
            (0x1dba814,0x1dba894,'8223c14c6cf67bc6e83391bf08a98bce0f8c2b47625524dbe97ef979a9ef6c54'),
            (0x1dac32c,0x1dac344,'5a3cff0839bb43ab7c3023904c7e8040f9cdc66650429d7576305e388e27632b'))}


def signed(n):
    n &= 0xffffffff
    return n - 0x100000000 if n & 0x80000000 else n


def reservation(size):
    """Exact SRA6 / ADDIU1 / SLL6 / SRA4, including 32-bit overflow."""
    return signed(((signed(size) >> 6) + 1) << 6) >> 4


def copy_state(base, used, capacity, size, regions):
    """Evaluate explicit snapshot values; subsequent allocator state is unknown.

    The allocator prediction requires its inputs and the stack size to survive
    the copy. In particular a copy of arbitrary size can overwrite its metadata.
    """
    base,used,capacity,size = (n & 0xffffffff for n in (base,used,capacity,size))
    destination = (base + ((used << 4) & 0xffffffff)) & 0xffffffff
    at = physical(destination)
    mapped = destination + size <= 0x100000000 and at + size <= 0x2000000
    ram_start = physical(base)
    valid_arena = (GLOBAL_START <= ram_start <= GLOBAL_END and
                   0 <= signed(used) <= signed(capacity) and
                   ram_start + capacity * 16 <= GLOBAL_END)
    remaining = (capacity-used)*16 if valid_arena else None
    delta = reservation(size)
    new = signed(used + delta)
    return dict(destination=destination,physical_destination=at,size=size,
                contiguous_ram_destination=mapped,valid_global_child_state=valid_arena,
                remaining_bytes=remaining,copy_fits_child=valid_arena and size<=remaining,
                reservation_qwords=delta,
                allocator_if_inputs_survive=dict(new_used=new,passes_signed_check=new<=signed(capacity),
                                                arithmetic_wrap=signed(used)+delta!=new),
                overlaps=[r['name'] for r in regions if size and mapped and at<r['end'] and r['start']<at+size],
                limit='snapshot destination only; source validity, post-copy metadata and successful lookup are not established')


def size_envelope(size, regions):
    """All valid child cursors, including the one-past-end cursor.

    This deliberately permits a child overflow: it bounds dispatch effects,
    not memory safety. No concrete allocation base or cursor is assumed.
    """
    if not 0 <= size <= 0xffffffff:
        raise ValueError('pack size must be an unsigned word')
    end = GLOBAL_END + size
    return dict(size=size,destination_start_min=GLOBAL_START,destination_start_max=GLOBAL_END,
                write_end_max=end,
                possibly_overlapping_regions=[r['name'] for r in regions
                    if size and GLOBAL_START<r['end'] and r['start']<end],
                may_write_global_allocator_counter=bool(size),
                conditions=['base and complete child arena within global payload',
                            '0 <= used qwords <= capacity qwords',
                            'successful lookup yields this exact size at the call',
                            'destination follows the guarded base+(used<<4) path'],
                limit='conditional direct-write envelope only; a child overflow may corrupt following allocations/global cursor and thereby affect later calls; no proof of child capacity, runtime pack identity or cursor validity')


def loadchara_contract(programs, regions):
    for image, spans in SPANS.items():
        program=programs[image]
        for start,end,digest in spans:
            if any(start<=pc<end for pc in program['natives']):
                raise ValueError('LoadChara2 contract contains a native instruction hook')
            body=b''.join(struct.pack('<I',program['word'](pc)) for pc in range(start,end,4))
            if hashlib.sha256(body).hexdigest()!=digest:
                raise ValueError('LoadChara2 size/cursor contract changed')
        for target in ((0x1278a0,0x125a90,0x127a00,0x127aa0,0x13f720) if image=='dc' else (0x1dba3c0,0x1dac1c0)):
            if target in program['overrides'] or target in program['externals']:
                raise ValueError('LoadChara2 contract owner replaced')
    # This is a symbolic precondition, not a new resolution of generic flow.
    return dict(sites=[dict(lookup_pc=a,copy_pc=b,allocation_pc=c,size_stack_offset=d,name_address=e)
                       for a,b,c,d,e in SITES],
                arena=dict(object=0x1f06660,base_offset=0,used_offset=8,capacity_offset=12,
                           nominal_capacity_qwords=CAPACITY,global_payload=[GLOBAL_START,GLOBAL_END]),
                destination='u32(base + u32(used << 4))',
                native_copy_size='unsigned low 32 bits of the raw pack size slot',
                post_copy_reservation='s32(u32(((s32(size) >> 6) + 1) << 6)) >> 4',
                allocator_check='s32(u32(used + reservation)) <= s32(capacity)',
                size_interpretation_examples=[dict(size=n,reservation_qwords=reservation(n))
                    for n in (0,63,64,0x7fffffbf,0x7fffffc0,0x80000000,0xffffffbf,0xffffffc0,0xffffffff)],
                unknown_size_envelope=size_envelope(0xffffffff,regions),
                limit='three copies remain unresolved; pack-derived sizes and cursor bounds are preconditions, not enforced pre-copy guards; no effect is fed back into alias flow')
