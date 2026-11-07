"""Disc-free ReadInfo contract boundaries and synthetic PreProcess snapshots."""
import hashlib
import pathlib
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
import dc_readinfo_audit as audit


class ReadInfoAudit(unittest.TestCase):
    def test_artifact_leaf_guard_binds_current_bytes(self):
        raw=struct.pack('<2I',0x03e00008,0)
        with patch.object(audit,'PREPROCESS_SPAN',(0x1000,0x1008,hashlib.sha256(raw).hexdigest())):
            def read(at,size):
                self.assertEqual((at,size),(0x1000,8));return raw
            audit.guard_preprocess(read)
            for bad in (raw[:-1],raw[:-1]+b'\x01'):
                with self.subTest(bad=bad),self.assertRaisesRegex(ValueError,'artifact contract changed'):
                    audit.guard_preprocess(lambda at,size:bad)

    def test_preprocess_known_exact_effects(self):
        for raw,size,changed,locations in [(b'plain\n..',6,0,[]),
                (b'//x\n..',4,3,[0,1,2]),(b'/*x*/\n..',6,5,list(range(5))),
                (b'/*x*/!',4,5,list(range(5))),(b'/*xx..',4,4,list(range(4))),
                (b'abc/..',4,0,[]),(b'//xx..',4,4,list(range(4)))]:
            with self.subTest(raw=raw):
                r=audit.preprocess(raw,0,size)
                self.assertEqual(r['changed_bytes'],changed)
                self.assertEqual(sorted({w['byte'] for w in r['writes']}),locations)
                self.assertEqual(r['writes_outside_cfg'],[dict(byte=4,pc=0x13b340)] if size==4 and raw.startswith(b'/*x*/') else [])

    def test_lookahead_is_retained_even_without_store(self):
        r=audit.preprocess(b'abc/..',0,4)
        self.assertEqual(r['reads_outside_cfg'],[4]);self.assertFalse(r['writes'])
        self.assertEqual(audit.preprocess(b'//xx..',0,4)['reads_outside_cfg'],[4])

    def test_empty_extent_does_not_read(self):
        self.assertEqual(audit.preprocess(b'',0,0),dict(writes=[],reads_outside_cfg=[],writes_outside_cfg=[],changed_bytes=0))

    def test_missing_lookahead_rejects_incomplete_snapshot(self):
        for raw in (b'/',b'//xx',b'/*x*'):
            with self.subTest(raw=raw),self.assertRaisesRegex(ValueError,'read outside'):
                audit.preprocess(raw,0,len(raw))

    def test_invalid_extent_rejected(self):
        for start,size in [(-1,1),(0,-1),(0,4),(1,3),(0,0xffffffff)]:
            with self.subTest(start=start,size=size),self.assertRaises(ValueError):
                audit.preprocess(b'abc',start,size)

    def test_contained_cfg_can_change_next_header_name(self):
        blob=bytearray(200);blob[80:84]=b'/*x*';blob[84:90]=b'/next\0'
        nodes=[dict(name='base.cfg',node=0,payload=80,size=4),
               dict(name='/next',node=84,payload=164,size=4)]
        r=audit.config_effects(blob,nodes)[0]
        self.assertEqual(r['header_hits'],[dict(byte=84,pc=0x13b340,header='/next',header_node=84)])
        self.assertEqual(blob[84],47)  # read-only scanner
        self.assertEqual(r['payload_sha256'],hashlib.sha256(b'/*x*').hexdigest())

    def test_cfg_scan_is_selected_and_read_only(self):
        blob=bytearray(b'//x\n..');before=bytes(blob)
        nodes=[dict(name='BASE.CFG',node=0,payload=0,size=4),dict(name='model',node=0,payload=0,size=6)]
        self.assertEqual(len(audit.config_effects(blob,nodes)),1)
        self.assertEqual(blob,before)

    def contract(self,*,changed=False,native=False,replaced=False,external=False,callback=0x3000):
        words={0x1000:0x03e00008,0x1004:0}
        digest=hashlib.sha256(struct.pack('<2I',*words.values())).hexdigest()
        if changed:words[0x1004]=1
        p=dict(word=lambda pc:words.get(pc,callback if 0x251470<=pc<0x2514bc else 0),
               natives={0x1004} if native else set(),overrides={0x1000} if replaced else set(),
               externals={0x1000:'stub'} if external else {},functions=[],names={})
        with patch.object(audit,'SPANS',{'dc':((0x1000,0x1008,digest),)}):
            return audit.readinfo_contract({'dc':p,'dun':p})

    def test_contract_keeps_mutable_callback_snapshot(self):
        for target in (0,0x3000,0x1dc2e70):
            r=self.contract(callback=target)
            self.assertEqual(len(r['callbacks']),19)
            self.assertEqual(r['callbacks'][0]['initial_target'],target)
            self.assertFalse(r['cursor_reset']);self.assertTrue(r['command_table']['mutable'])
            self.assertIn('unresolved',r['dispatch_effects'])

    def test_changed_body_rejected(self):
        with self.assertRaisesRegex(ValueError,'contract changed'):self.contract(changed=True)

    def test_native_instruction_rejected(self):
        with self.assertRaisesRegex(ValueError,'native instruction'):self.contract(native=True)

    def test_replaced_owner_rejected(self):
        with self.assertRaisesRegex(ValueError,'owner replaced'):self.contract(replaced=True)

    def test_external_owner_rejected(self):
        with self.assertRaisesRegex(ValueError,'owner replaced'):self.contract(external=True)


if __name__=='__main__':unittest.main()
