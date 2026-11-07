"""Disc-free boundaries; artifact/runtime identities remain preconditions."""
import pathlib
import struct
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from dc_loadchara_audit import copy_state, reservation, size_envelope, GLOBAL_START, GLOBAL_END, CAPACITY
from dc_pack_inventory import pack_nodes

REGIONS=[dict(name='resident-vtable',start=0x2a12b0,end=0x2a13b0),
         dict(name='DUN-switch',start=0x1dc2800,end=0x1dc2820),
         dict(name='effect-vptr',start=0x1e5b430,end=0x1e5b434)]

class LoadCharaAudit(unittest.TestCase):
    def test_pack_payload_and_terminal(self):
        blob=bytearray(256);blob[:6]=b'model\0'
        struct.pack_into('<3I',blob,64,80,32,128)
        row=pack_nodes(blob)[0]
        self.assertEqual((row['payload'],row['payload_end'],row['size']),(80,112,32))

    def test_pack_validator_rejects_unbounded_fields(self):
        for relative,size,nxt in [(80,0xffffffff,128),(0xffffffff,32,128),(80,32,0),(64,32,128),(80,32,256)]:
            blob=bytearray(256);blob[:6]=b'model\0';struct.pack_into('<3I',blob,64,relative,size,nxt)
            with self.subTest(fields=(relative,size,nxt)),self.assertRaises(ValueError):pack_nodes(blob)

    def test_pack_validator_name_header_and_limit(self):
        with self.assertRaises(ValueError):pack_nodes(b'model')
        blob=bytearray(b'x'*256)
        with self.assertRaises(ValueError):pack_nodes(blob)
        blob=bytearray(256);blob[:6]=b'model\0';struct.pack_into('<3I',blob,64,80,32,128)
        with self.assertRaises(ValueError):pack_nodes(blob,limit=0)

    def test_signed_size_transition(self):
        for size,expected in [(0,4),(63,4),(64,8),(132224,8268),(25360,1588),(53552,3348),
                              (0x7fffffbf,134217724),(0x7fffffc0,-134217728),
                              (0x80000000,-134217724),(0xffffffbf,-4),(0xffffffc0,0),(0xffffffff,0)]:
            with self.subTest(size=hex(size)): self.assertEqual(reservation(size),expected)

    def test_copy_fits_but_padding_rejected_afterwards(self):
        x=copy_state(GLOBAL_START,CAPACITY-4,CAPACITY,64,REGIONS)
        self.assertTrue(x['copy_fits_child'])
        self.assertFalse(x['allocator_if_inputs_survive']['passes_signed_check'])

    def test_copy_exceeds_child_before_rejection(self):
        x=copy_state(GLOBAL_START,CAPACITY-4,CAPACITY,65,REGIONS)
        self.assertFalse(x['copy_fits_child'])
        self.assertEqual(x['remaining_bytes'],64)
        self.assertFalse(x['allocator_if_inputs_survive']['passes_signed_check'])

    def test_zero_copy_still_reserves(self):
        x=copy_state(GLOBAL_START,CAPACITY,CAPACITY,0,REGIONS)
        self.assertTrue(x['copy_fits_child'])
        self.assertFalse(x['overlaps'])
        self.assertFalse(x['allocator_if_inputs_survive']['passes_signed_check'])

    def test_artifact_sizes_cannot_reach_dispatch_under_valid_state(self):
        for n in (132224,25360,53552):
            x=size_envelope(n,REGIONS)
            self.assertEqual(x['possibly_overlapping_regions'],[])
            self.assertLess(x['write_end_max'],0x1dabd00)
            self.assertTrue(x['may_write_global_allocator_counter'])

    def test_envelope_first_byte_and_empty(self):
        distance=REGIONS[1]['start']-GLOBAL_END
        self.assertFalse(size_envelope(distance,REGIONS)['possibly_overlapping_regions'])
        self.assertEqual(size_envelope(distance+1,REGIONS)['possibly_overlapping_regions'],['DUN-switch'])
        self.assertFalse(size_envelope(0,REGIONS)['possibly_overlapping_regions'])

    def test_missing_cursor_preconditions_remain_explicit(self):
        for used,cap in [(0xffffffff,CAPACITY),(CAPACITY+1,CAPACITY),(0,0xffffffff),(0x10000000,CAPACITY)]:
            x=copy_state(GLOBAL_START,used,cap,4,REGIONS)
            self.assertFalse(x['valid_global_child_state'])
            self.assertIsNone(x['remaining_bytes'])

    def test_arena_extent_and_ram_aliases(self):
        for mirror in (0,0x20000000,0x30000000,0x80000000,0xa0000000):
            self.assertTrue(copy_state(GLOBAL_START|mirror,0,CAPACITY,4,REGIONS)['valid_global_child_state'])
        self.assertFalse(copy_state(GLOBAL_END-16,0,2,4,REGIONS)['valid_global_child_state'])

    def test_negative_cursor_can_reach_earlier_word(self):
        x=copy_state(0x1dc2810,0xffffffff,CAPACITY,4,REGIONS)
        self.assertEqual(x['physical_destination'],0x1dc2800)
        self.assertEqual(x['overlaps'],['DUN-switch'])
        self.assertTrue(x['allocator_if_inputs_survive']['passes_signed_check'])

    def test_signed_add_wrap_can_pass(self):
        x=copy_state(GLOBAL_START,0x7ffffffc,CAPACITY,64,REGIONS)
        self.assertTrue(x['allocator_if_inputs_survive']['arithmetic_wrap'])
        self.assertTrue(x['allocator_if_inputs_survive']['passes_signed_check'])
        self.assertFalse(x['valid_global_child_state'])

    def test_shift_wrap_and_invalid_extent(self):
        self.assertEqual(copy_state(GLOBAL_START,0x10000000,CAPACITY,4,REGIONS)['destination'],GLOBAL_START)
        x=copy_state(GLOBAL_START,0,CAPACITY,0xffffffff,REGIONS)
        self.assertFalse(x['contiguous_ram_destination'])
        self.assertFalse(x['overlaps'])

if __name__=='__main__':unittest.main()
