"""Resident inline native sites must not leak into overlapping overlay builds."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "tools" / "dc_bozo_externals.py"


class ExternalImages(unittest.TestCase):
    def test_inline_sites_are_not_callable_overrides(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            (root / "runtime").mkdir()
            (root / "runtime" / "dc_fixture.cpp").write_text(
                "0x12C19C 0x15434C 0x12D268 0x12DAE0")
            output = root / "overrides.txt"
            subprocess.run([sys.executable, "-I", str(SCRIPT), "--overrides", str(root), str(output)], check=True)
            entries = output.read_text().splitlines()
            for pc in ("12c19c", "15434c", "12d268"):
                self.assertIn("native 0x" + pc, entries)
                self.assertNotIn("override 0x" + pc, entries)
            self.assertIn("override 0x12dae0", entries)

    def test_image_boundaries(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            source, resident, overlay = (root / name for name in ("source.txt", "resident.txt", "overlay.txt"))
            source.write_text("stub 0x1000 # memcpy\n"
                              "stub 0x2000 # sin\n"
                              "override 0x3000\n"
                              "native 0x12c19c\n"
                              "native 0x15434c\n"
                              "native 0x12d268\n")
            subprocess.run([sys.executable, "-I", str(SCRIPT), str(source), str(resident), str(overlay)], check=True)
            base, extra = resident.read_text().splitlines(), overlay.read_text().splitlines()
            self.assertEqual([line for line in base if line.startswith("native ")],
                             ["native 0x12c19c", "native 0x15434c", "native 0x12d268"])
            self.assertFalse(any(line.startswith("native ") for line in extra))
            self.assertEqual([line for line in base if not line.startswith("native ")], extra)
            self.assertIn("stub 0x1000 # memcpy", extra)
            self.assertIn("override 0x3000", extra)
            self.assertNotIn("stub 0x2000 # sin", extra)


if __name__ == "__main__":
    unittest.main()
