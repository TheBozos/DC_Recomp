"""Disc-free adversarial checks for bounded pointer-flow evidence."""
import pathlib
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from dc_alias_audit import analyze, physical, data_pointer_references

GP=0x2a97f0
HANDLE=GP-0x742c
REGIONS=[dict(name='packet',start=HANDLE,end=HANDLE+4),
         dict(name='table',start=0x2a12b0,end=0x2a1378)]


def flow(words,entries=(),**kwargs):
    memory={0x1000+i*4:w for i,w in enumerate(words)}
    return analyze(memory.__getitem__,dict(start=0x1000,end=0x1000+len(words)*4,entries=list(entries)),
                   {},GP,REGIONS,**kwargs)


def writes(report,name='packet'):
    return [e for e in report['events'] if e['kind']=='region_write' and e['region']==name]


class AliasAudit(unittest.TestCase):
    def test_gp_address_copy_and_call_delay(self):
        # addiu t0,gp,-742c; or s0,t0,zero; jal; sw zero,0(s0) in its delay slot.
        r=flow([0x27888bd4,0x01008025,0x0c000800,0xae000000,0x03e00008,0])
        self.assertEqual([(e['pc'],e['address']) for e in writes(r)],[(0x100c,HANDLE)])

    def test_absolute_uncached_alias_and_wide_overlap(self):
        # 0xa02a23c0 is four bytes before the handle. SQ aligns and overlaps it.
        r=flow([0x3c08a02a,0x350823c7,0x7d000000,0x03e00008,0])
        self.assertEqual([(e['address'],e['width']) for e in writes(r)],[(0x2a23c0,16)])
        for view in (0,0x20000000,0x30000000,0x80000000,0xa0000000):
            self.assertEqual(physical(HANDLE|view),HANDLE)
        self.assertEqual(physical(0x70000000),0x70000000)

    def test_stack_spill_reload_and_overlapping_invalidation(self):
        prefix=[0x27bdffe0,0x27888bd4,0x7fa80000]
        suffix=[0x7ba90000,0xad200000,0x03e00008,0]
        self.assertEqual(len(writes(flow(prefix+suffix))),1)
        # A byte store destroys knowledge of the saved pointer, not just a word.
        r=flow(prefix+[0xa3a00003]+suffix)
        self.assertFalse(writes(r))
        self.assertGreater(r['stats'].get('unknown_address_writes',0),0)
        # A word reload can recover the low pointer from a quadword spill.
        self.assertEqual(len(writes(flow(prefix+[0x8fa90000]+suffix[1:]))),1)

    def test_narrow_spill_and_64_bit_shift_do_not_invent_a_pointer(self):
        r=flow([0x27bdffe0,0x27888bd4,0xa3a80000,0x93a90000,0xad200000,0x03e00008,0])
        self.assertFalse(writes(r))
        r=flow([0x27888bd4,0x0008403e,0xad000000,0x03e00008,0])
        self.assertGreater(r['stats'].get('unknown_address_writes',0),0)

    def test_conditional_move_uses_full_64_bit_condition(self):
        # t1=1<<32 has low word zero, but MOVZ must retain the handle in t0.
        r=flow([0x24090001,0x0009483c,0x27888bd4,0x0009400a,0xad000000,0x03e00008,0])
        self.assertTrue(writes(r))
        self.assertIn('conditional_move_upper_word_unknown',r['limitations'])

    def test_register_join_keeps_both_known_aliases(self):
        # t0 points at the handle on one arm, at handle+4 on the other.
        r=flow([0x27888bd4,0x10850004,0,0x25080004,0x10000002,0,
                0x01004025,0xad000000,0x03e00008,0])
        self.assertTrue(writes(r))

    def test_annulled_branch_likely_delay_is_not_a_write(self):
        r=flow([0x27888bd4,0x54000001,0xad000000,0x03e00008,0])
        self.assertFalse(writes(r))

    def test_conditional_link_keeps_untaken_path_and_annuls_delay(self):
        r=flow([0x27888bd4,0x041203fd,0xad000000,0xad000000,0x03e00008,0])
        self.assertEqual([e['pc'] for e in writes(r)],[0x100c])
        self.assertFalse(any(e['kind']=='call' for e in r['events']))
        # Unknown condition: the not-taken path retains t0 instead of killing it.
        r=flow([0x27888bd4,0x049203fd,0,0xad000000,0x03e00008,0])
        self.assertTrue(any(e['pc']==0x100c for e in writes(r)))

    def test_absolute_store_may_alias_symbolic_spill(self):
        r=flow([0x27888bd4,0xafa80000,0x3c090070,0xad200000,0x8fa80000,0xad000000,0x03e00008,0])
        self.assertFalse(writes(r))
        self.assertIn('absolute_store_stack_alias_unresolved',r['limitations'])

    def test_system_effects_are_opaque_including_delay_slots(self):
        for opaque in (0x0000000c,0x0000000d,0x42000018):
            r=flow([0x27888bd4,opaque,0xad000000,0x03e00008,0])
            self.assertFalse(writes(r))
            self.assertIn('system_instruction_effects_unmodeled',r['limitations'])
        r=flow([0x27888bd4,0x10000001,0x0000000c,0xad000000,0x03e00008,0])
        self.assertFalse(writes(r))

    def test_volatile_kill_saved_register_and_callee_effect_limit(self):
        prefix=[0x27888bd4,0x01008025,0x0c000800,0]
        r=flow(prefix+[0xad000000,0xae000000,0x03e00008,0])
        self.assertEqual([e['pc'] for e in writes(r)],[0x1014])
        self.assertIn('callee_memory_and_return_effects_unmodeled',r['limitations'])

    def test_mutable_data_is_not_folded_and_field_is_untyped(self):
        # Load a pointer from gp-relative memory, then store through its field.
        r=flow([0x8f889d00,0x250900a0,0xad200000,0x03e00008,0],roots=(GP-0x6300,))
        field=next(e for e in r['events'] if e['kind']=='untyped_field_write')
        self.assertEqual(field['address'],['root:0x2a34f0+0xa0'])
        self.assertFalse(writes(r))

    def test_constructor_publication_via_absolute_destination(self):
        r=flow([0x3c08002a,0x250812b0,0x3c0901ea,0x25294120,0xad280000,0x03e00008,0])
        e=next(e for e in r['events'] if e['kind']=='pointer_store')
        self.assertEqual((e['region'],e['destination']),('table',['absolute+0x1ea4120']))

    def test_bulk_table_write_and_unknown_callee_are_not_ignored(self):
        r=flow([0x3c04002a,0x248412b0,0x24060004,0x0c040cec,0,0x03e00008,0])
        self.assertTrue(any(e['kind']=='bulk_region_write' and e['region']=='table' for e in r['events']))
        self.assertIn('callee_memory_and_return_effects_unmodeled',r['limitations'])

    def test_native_instruction_is_an_opaque_boundary(self):
        r=flow([0x27888bd4,0xad000000,0x03e00008,0],native_pcs={0x1004})
        self.assertFalse(writes(r))
        self.assertIn('native_instruction_effects_unmodeled',r['limitations'])

    def test_byte_store_in_a_table_and_pointer_return(self):
        r=flow([0x3c08002a,0x250812b1,0xa1000000,0x01001025,0x03e00008,0])
        self.assertEqual(writes(r,'table')[0]['width'],1)
        self.assertTrue(any(e['kind']=='return' and e['value']==['absolute+0x2a12b1'] for e in r['events']))
        # Storing only a byte/partial word of an address is not publication of
        # the complete pointer, though its destination write still matters.
        r=flow([0x3c08002a,0x250812b0,0xa0880000,0xa8880000,0x03e00008,0])
        self.assertFalse(any(e['kind']=='pointer_store' for e in r['events']))

    def test_unknown_arithmetic_widening_and_bounds_are_explicit(self):
        # Loop changes an affine argument offset until the value-set bound widens.
        r=flow([0x24840004,0x1000fffe,0])
        self.assertIn('value_set_widened',r['limitations'])
        r=flow([0x24840004,0x1000fffe,0],max_steps=2)
        self.assertIn('step_bound_reached',r['limitations'])

    def test_known_extra_entry_bypasses_a_skip(self):
        code=[0x10000003,0,0x27888bd4,0xad000000,0x03e00008,0]
        self.assertFalse(writes(flow(code)))
        self.assertTrue(writes(flow(code,entries=[0x1008])))

    def test_data_references_exclude_code_and_keep_virtual_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            blob=bytearray(0x120)
            struct.pack_into('<I',blob,32,0x80);struct.pack_into('<HH',blob,46,40,2)
            struct.pack_into('<10I',blob,0x80,0,1,2,0x2000,0x100,8,0,0,4,0)
            struct.pack_into('<10I',blob,0xa8,0,1,6,0x3000,0x108,4,0,0,4,0)
            struct.pack_into('<3I',blob,0x100,HANDLE|0x20000000,0,HANDLE)
            path=pathlib.Path(directory)/'data.elf';path.write_bytes(blob)
            self.assertEqual(data_pointer_references(path,REGIONS),
                             [dict(address=0x2000,value=HANDLE|0x20000000,region='packet')])


if __name__=='__main__': unittest.main()
