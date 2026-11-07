"""Disc-free checks for byte-range contracts and contextual callee boundaries."""
import pathlib
import struct
import sys
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from dc_alias_audit import constant, value, encode
from dc_callee_audit import bulk_effect, contextual_writes, overlay_load_contract, nested_effect_regions

REGIONS=[dict(name='table',start=0x4000,end=0x4004)]


def bulk(dst=0x4000,n=4,operation='memcpy',src=0x5000):
    return dict(kind='bulk_call',pc=0x1000,operation=operation,destination=encode(None if dst is None else constant(dst)),
                source=encode(constant(src)),length=encode(None if n is None else constant(n)))


def event(target,arg=0x4000):
    args=[constant(arg),*[None]*7]
    return dict(pc=0x1db0000,target=target,owner=0x1db0000,transfer='call',argument_values=[encode(v) for v in args])


def programs(code,overrides=(),externals=None):
    # address -> instruction list, all in the resident image unless supplied otherwise.
    words={a+i*4:w for a,body in code.items() for i,w in enumerate(body)}
    fs=[dict(start=a,end=a+len(body)*4,entries=(),sites=[]) for a,body in code.items()]
    p=dict(word=words.__getitem__,functions=fs,facts={},names={},natives=(),overrides=set(overrides),externals=externals or {})
    empty=dict(p,functions=[])
    return {'dc':p,'dun':empty,'title':empty}


class CalleeAudit(unittest.TestCase):
    def test_full_and_partial_byte_overlap_and_source(self):
        r=bulk_effect(bulk(0x3fff,2),REGIONS)
        self.assertEqual(r['status'],'possible_overlap')
        self.assertEqual(r['overlaps'],[dict(region='table',start=0x4000,end=0x4001,source_start=0x5001,source_end=0x5002)])
        self.assertFalse(bulk_effect(bulk(0x3ffc,4),REGIONS)['overlaps'])

    def test_zero_length_never_writes_even_unknown_destination(self):
        self.assertEqual(bulk_effect(bulk(None,0),REGIONS)['status'],'zero_length')
        self.assertFalse(bulk_effect(bulk(0x4001,0),REGIONS)['overlaps'])

    def test_unknown_length_and_mixed_paths_stay_open(self):
        unknown=bulk_effect(bulk(n=None),REGIONS)
        self.assertIn('length_unknown',unknown['unresolved'])
        self.assertIn('table',unknown['unknown_extent_may_reach'])
        e=bulk();e['destination']=encode(constant(0x4000)|value('unknown'))
        r=bulk_effect(e,REGIONS)
        self.assertTrue(r['overlaps']);self.assertEqual(r['status'],'unresolved')

    def test_mirrors_invalid_ranges_and_scratchpad(self):
        self.assertTrue(bulk_effect(bulk(0xa0004000),REGIONS)['overlaps'])
        self.assertTrue(bulk_effect(bulk(0x20004000),REGIONS)['overlaps'])
        self.assertIn('virtual_range_wraps',bulk_effect(bulk(0xfffffffe,4),REGIONS)['unresolved'])
        self.assertIn('not_a_valid_contiguous_guest_ram_range',bulk_effect(bulk(0x1fffffe,4),REGIONS)['unresolved'])
        self.assertEqual(bulk_effect(bulk(0x70000000),REGIONS)['ranges'][0]['space'],'scratchpad')

    def test_fill_has_no_copied_pointer_source(self):
        self.assertNotIn('source_start',bulk_effect(bulk(operation='memset'),REGIONS)['overlaps'][0])

    def test_compiled_memmove_low_zero_is_not_proof_of_a_noop(self):
        r=bulk_effect(bulk(n=0,operation='memmove'),REGIONS)
        self.assertEqual(r['status'],'unresolved')
        self.assertIn('compiled_64_bit_size_not_established',r['unresolved'])

    def test_context_reaches_write_in_called_owner(self):
        p=programs({0x2000:[0x248400a0,0xac800000,0x03e00008,0]})
        r=contextual_writes(p,[('dun',event(0x2000,0x3f60))],REGIONS,[],0,())
        self.assertTrue(any(e['kind']=='region_write' for c in r['contexts'] for e in c['writes']))

    def test_native_boundary_is_not_original_compiled_body(self):
        p=programs({0x2000:[0xac800000,0x03e00008,0]},overrides={0x2000},externals={0x2000:'override'})
        r=contextual_writes(p,[('dun',event(0x2000))],REGIONS,[],0,())
        self.assertFalse(r['contexts']);self.assertEqual(r['boundaries'][0]['reason'],'native_or_sdk_boundary')

    def test_nested_context_bound_is_explicit(self):
        p=programs({0x2000:[0x0c000840,0,0x03e00008,0],0x2100:[0xac800000,0x03e00008,0]})
        r=contextual_writes(p,[('dun',event(0x2000))],REGIONS,[],0,(),max_depth=0)
        self.assertTrue(any(b['reason']=='context_depth_bound' for b in r['boundaries']))

    def test_known_control_destination_is_prioritized_with_a_small_budget(self):
        p=programs({0x2000:[0xac800000,0x03e00008,0],0x2100:[0x03e00008,0]})
        regions=[dict(name='packet:global',start=0x4000,end=0x4004)]
        r=contextual_writes(p,[('dun',event(0x2100,0x6000)),('dc',event(0x2000))],regions,[],0,(),max_contexts=1)
        self.assertTrue(any(w['kind']=='region_write' for c in r['contexts'] for w in c['writes']))
        self.assertTrue(any(b['reason']=='context_count_bound' for b in r['boundaries']))

    def test_extra_entry_does_not_seed_owner_start(self):
        p=programs({0x2000:[0xac800000,0,0,0,0x03e00008,0]});p['dc']['functions'][0]['entries']=[0x2010]
        r=contextual_writes(p,[('dun',event(0x2010))],REGIONS,[],0,())
        self.assertFalse(r['contexts'][0]['writes'])

    def test_caller_stack_is_not_the_callee_frame(self):
        p=programs({0x2000:[0x27bdfff0,0x24084000,0xafa80000,0x8c890000,0xad200000,0x03e00008,0]})
        e=event(0x2000);e['argument_values'][0]=encode(value('stack',-16))
        r=contextual_writes(p,[('dun',e)],REGIONS,[],0,())
        self.assertFalse(any(w['kind']=='region_write' for c in r['contexts'] for w in c['writes']))

    def test_resident_overlay_target_requires_selected_identity(self):
        p=programs({0x1dabd00:[0xac800000,0x03e00008,0]});p['dun']=p['dc'];p['dc']=dict(p['dc'],functions=[])
        r=contextual_writes(p,[('dc',event(0x1dabd00))],REGIONS,[],0,())
        self.assertEqual((r['contexts'][0]['target_image'],r['contexts'][0]['condition']),('dun','requires_selected_DUN'))

    def test_active_overlay_survives_a_resident_callee(self):
        p=programs({0x2000:[0x0c76af40,0,0x03e00008,0]}) # jal 0x1dabd00
        overlay=programs({0x1dabd00:[0xac800000,0x03e00008,0]})['dc']
        p['dun']=overlay;p['title']=dict(overlay,word=lambda pc:0x03e00008 if pc==0x1dabd00 else 0)
        r=contextual_writes(p,[('title',event(0x2000))],REGIONS,[],0,())
        self.assertFalse(any(w['kind']=='region_write' for c in r['contexts'] for w in c['writes']))
        self.assertTrue(any(c['target_image']=='title' for c in r['contexts']))

    def test_overlay_file_and_bss_writes_are_separate(self):
        raw=bytearray(0x100);raw[:4]=b'MWo3'
        struct.pack_into('<I',raw,8,0x1000);struct.pack_into('<I',raw,20,0x100)
        struct.pack_into('<2I',raw,24,0x1080,0x1084);struct.pack_into('<I',raw,0x80,0x10c0)
        r=overlay_load_contract(raw,[dict(name='switch',start=0x10a0,end=0x10a4),dict(name='vptr',start=0x1150,end=0x1154)])
        self.assertEqual(r['file_write']['regions'],['switch']);self.assertEqual(r['bss_zero']['regions'],['vptr'])
        self.assertEqual(r['constructors'],[0x10c0])

    def test_constructor_layout_cannot_be_guessed_from_offset(self):
        with self.assertRaisesRegex(ValueError,'layout changed'):
            nested_effect_regions([],lambda pc:0)


if __name__=='__main__':unittest.main()
