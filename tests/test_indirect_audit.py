"""Disc-free switch/call facts, instrumentation mismatch and image-scoped traces."""
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from dc_instrument_indirect import instrument
from dc_packet_audit import replaced_body

ANALYZER, TRACER = sys.argv[1:3]
del sys.argv[1:3]
GAME_MODULES = '--game-modules' in sys.argv
if GAME_MODULES:
    sys.argv.remove('--game-modules')


def fixture(path):
    # A bounded two-way switch and a separate register call. Synthetic code.
    words = [0x2c810002, 0x10200008, 0, 0x00041080, 0x3c030000, 0x24632000,
             0x00431021, 0x8c420000, 0x00400008, 0, 0x03e00008, 0,
             0x03e00008, 0, 0x00a0f809, 0, 0x03e00008, 0]
    code = struct.pack('<' + 'I' * len(words), *words)
    data = struct.pack('<2I', 0x1028, 0x1030)
    strings = b'\0synthetic\0'
    syms = bytes(16) + struct.pack('<IIIBBH', 1, 0x1000, len(code), 0x12, 0, 1)
    names = b'\0.text\0.data\0.strtab\0.symtab\0.shstrtab\0'
    sections_at = 0x280
    header = struct.pack('<16sHHIIIIIHHHHHH', b'\x7fELF\x01\x01\x01' + bytes(9),
                         2, 8, 1, 0x1000, 52, sections_at, 0x20924000, 52, 32, 2, 40, 6, 5)
    ph = struct.pack('<8I', 1, 0x100, 0x1000, 0x1000, len(code), len(code), 5, 4)
    ph += struct.pack('<8I', 1, 0x200, 0x2000, 0x2000, len(data), len(data), 4, 4)
    out = bytearray(sections_at)
    out[:len(header + ph)] = header + ph
    out[0x100:0x100 + len(code)] = code
    out[0x200:0x208] = data
    out[0x210:0x210 + len(strings)] = strings
    out[0x220:0x220 + len(syms)] = syms
    out[0x240:0x240 + len(names)] = names
    rows = [[0]*10, [1,1,6,0x1000,0x100,len(code),0,0,4,0],
            [7,1,2,0x2000,0x200,len(data),0,0,4,0], [13,3,0,0,0x210,len(strings),0,0,1,0],
            [21,2,0,0,0x220,len(syms),3,1,4,16], [29,3,0,0,0x240,len(names),0,0,1,0]]
    path.write_bytes(out + b''.join(struct.pack('<10I', *r) for r in rows))


class IndirectAudit(unittest.TestCase):
    def test_forwarding_override_keeps_compiled_body_in_audit(self):
        overrides = {0x1554a0, 0x12ed80}
        self.assertFalse(replaced_body('dc', 0x1554a0, overrides))
        self.assertTrue(replaced_body('dc', 0x12ed80, overrides))
        self.assertFalse(replaced_body('title', 0x12ed80, overrides))

    def test_compiler_facts_and_instrumentation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source, patches, target = [root / n for n in ('input.elf', 'empty.patches', 'facts.json')]
            fixture(source); patches.write_text('')
            subprocess.run([ANALYZER, str(source), str(patches), str(target)], check=True)
            functions = {f['start']: f for f in json.loads(target.read_text())['functions']}
            sites = functions[0x1000]['sites']
            switch = next(s for s in sites if s['pc'] == 0x1020)
            self.assertEqual(switch['kind'], 'compiler_switch')
            self.assertEqual(switch['targets'], [0x1028, 0x1030])
            self.assertEqual(next(s for s in sites if s['pc'] == 0x1038)['kind'], 'call_register')
            body = 'static void body_0x1000() {\n' + '\n'.join(
                f'const uint32_t target = BZ_W({s["reg"]});' for s in sites) + '\n}\nvoid x_entry() {}'
            output, count = instrument(body, functions, 2)
            self.assertEqual(count, 2)
            self.assertIn('dcTraceIndirect(rt, ctx, 2u, 0x1020u, BZ_W(2))', output)
            with self.assertRaises(ValueError):
                instrument(body.replace('BZ_W(2)', 'BZ_W(9)', 1), functions, 2)
            # Guard failure must stop facts generation as it stops production codegen.
            patches.write_text('0x1000 0xffffffff 0x00000000\n')
            self.assertNotEqual(subprocess.run([ANALYZER, str(source), str(patches), str(target)],
                                               capture_output=True).returncode, 0)

    def test_probe_identity_and_cpu_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            target = pathlib.Path(directory) / 'trace.json'
            env = dict(os.environ, DC_INDIRECT_TRACE=str(target))
            subprocess.run([TRACER], check=True, env=env)
            report = json.loads(target.read_text())
            self.assertTrue(report['complete'])
            self.assertEqual(len(report['edges']), 4 if GAME_MODULES else 2)
            first, second = [e for e in report['edges'] if e['pc'] == 0x1000]
            self.assertEqual((first['image'], first['count'], first['first_args'][0], first['last_args'][0]), (0,2,7,9))
            self.assertEqual((second['image'], second['count']), (1,1))
            if GAME_MODULES:
                modules = [e for e in report['edges'] if e['pc'] == 0x2000]
                self.assertEqual({(e['image'], e['target_image'], e['target']) for e in modules},
                                 {(0,1,0x1dabd80),(0,2,0x1dabd80)})
            target.unlink()
            env['DC_INDIRECT_TRACE'] = ''
            subprocess.run([TRACER], check=True, env=env)
            self.assertFalse(target.exists())


if __name__ == '__main__':
    unittest.main()
