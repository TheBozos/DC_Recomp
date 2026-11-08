"""Player-facing migration and timing regression checks; no game data required."""
import configparser
import sys
import tempfile
import io
import subprocess
import tarfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import make_release
import dc_build
from unittest.mock import patch
from types import SimpleNamespace

REQUIRED = (
    'DC Launcher.exe', 'README.txt', 'CHANGELOG.md', 'LICENSES/TMB_Recomp/LICENSE',
    'LICENSES/BozoEngine/LICENSE', 'LICENSES/DC_Recomp/LICENSE', 'source/VERSION', 'source/release.ini', 'kit/kit_manifest.json',
    'kit/python/python.exe', 'source/tools/dc_build.py', 'source/BozoEngine/CMakeLists.txt',
    'kit/deps/sdl3-src/CMakeLists.txt', 'kit/deps/imgui-src/imgui.cpp',
)


class ReleaseTests(unittest.TestCase):
    def test_retry_drops_failed_feature_checks_after_toolchain_relocation_changes(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            cache = root / 'CMakeCache.txt'
            cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=' + dc_build.ROOT +
                             '\nBOZO_ENGINE_DIR:PATH=engine\nHAVE_XINPUT_H:INTERNAL=\n', encoding='utf-8')
            (root / 'CMakeFiles').mkdir()
            (root / 'CMakeFiles/old-check').write_bytes(b'failed check')
            stamp = root / '.dc-toolchain-key'
            stamp.write_text('before relocation fix', encoding='utf-8')
            build = dc_build.Build.__new__(dc_build.Build)
            build.cmake_build = folder
            build.disc_dir = folder
            build.game = folder
            build.args = SimpleNamespace(build_type='Release', debug_info='no')
            build.ffmpeg_root = folder
            build.toolchain_key = lambda: 'relocated compiler'
            build.tool = lambda name: name
            build.elf = lambda: str(root / 'own-elf')
            build.fetch_args = lambda: []
            def configure(_command):
                self.assertFalse(cache.exists())
                self.assertFalse((root / 'CMakeFiles').exists())
            build.run = configure
            build.step_configure()
            self.assertEqual(stamp.read_text(encoding='utf-8').strip(), 'relocated compiler')


    def test_source_export_drains_archive_padding_before_waiting(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            archive = base / 'source.tar'
            content = b'native source archive\n'
            with tarfile.open(archive, 'w') as tar:
                entry = tarfile.TarInfo('source.txt')
                entry.size = len(content)
                tar.addfile(entry, io.BytesIO(content))
            producer = base / 'producer.py'
            producer.write_text('import sys\nfrom pathlib import Path\n'
                                'sys.stdout.buffer.write(Path(sys.argv[1]).read_bytes())\n'
                                'sys.stdout.buffer.write(bytes(1048576))\n', encoding='utf-8')
            command = [sys.executable, str(producer), str(archive)]
            code = ('import sys; sys.path.insert(0, %r); import make_release; '
                    'make_release.extract_tar_stream(%r, %r, %r)' %
                    (str(ROOT / 'tools'), command, str(base / 'out'), folder))
            subprocess.run([sys.executable, '-c', code], check=True, timeout=15)
            self.assertEqual((base / 'out/source.txt').read_bytes(), content)


    def test_source_cleanup_cannot_escape_the_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            root.mkdir()
            outside = Path(folder) / 'outside'
            outside.mkdir()
            (outside / 'keep.txt').write_bytes(b'outside source')
            (root / 'release_files.txt').write_text('../outside/keep.txt\n', encoding='utf-8')
            with patch.object(dc_build, 'ROOT', str(root)), self.assertRaises(dc_build.BuildError):
                dc_build.remove_stale_sources()
            self.assertEqual((outside / 'keep.txt').read_bytes(), b'outside source')


    def test_update_removes_the_retired_source_component_and_keeps_player_data(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            root.mkdir()
            (root / 'release_files.txt').write_text('tools/kept.py\n', encoding='utf-8')
            for name in ('tools/kept.py', 'tools/removed.py', 'PS2Recomp/runtime.cpp', 'PS2Recomp/__pycache__/old.pyc',
                         'custom/untouched.txt', 'game/save.dat'):
                file = root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'preserve or retire by ownership')
            with patch.object(dc_build, 'ROOT', str(root)):
                dc_build.remove_stale_sources()
            self.assertFalse((root / 'PS2Recomp').exists())
            self.assertFalse((root / 'tools/removed.py').exists())
            for name in ('tools/kept.py', 'custom/untouched.txt', 'game/save.dat'):
                self.assertEqual((root/name).read_bytes(), b'preserve or retire by ownership')


    def test_update_never_follows_a_source_directory_link(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            external = Path(folder) / 'external'
            root.mkdir()
            external.mkdir()
            (root / 'tools').mkdir()
            (root / 'release_files.txt').write_text('tools/kept.py\n', encoding='utf-8')
            (external / 'keep.txt').write_bytes(b'outside source')
            if sys.platform == 'win32':
                import _winapi
                _winapi.CreateJunction(str(external), str(root / 'PS2Recomp'))
                _winapi.CreateJunction(str(external), str(root / 'tools/linked'))
            else:
                (root / 'PS2Recomp').symlink_to(external, target_is_directory=True)
                (root / 'tools/linked').symlink_to(external, target_is_directory=True)
            with patch.object(dc_build, 'ROOT', str(root)):
                dc_build.remove_stale_sources()
            self.assertEqual((external / 'keep.txt').read_bytes(), b'outside source')
            self.assertFalse((root / 'PS2Recomp').exists())
            self.assertTrue((root / 'tools/linked').exists())


    def test_only_native_engine_is_a_submodule(self):
        config = configparser.ConfigParser()
        config.read(ROOT / '.gitmodules')
        self.assertEqual(config.sections(), ['submodule "BozoEngine"'])
        self.assertEqual(config[config.sections()[0]]['path'], 'BozoEngine')


    def test_release_rejects_game_executables(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'bad.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                for name in REQUIRED:
                    archive.writestr('DC_Recomp_Bozo/' + name, b'fixture')
                archive.writestr('DC_Recomp_Bozo/source/hidden.dat', b'\x7fELF' + b'\0' * 100)
            self.assertTrue(any('an ELF file' in error for error in make_release.verify_zip(path)))



class DiscBuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        dc_build.load_engine()

    def test_overlay_corruption_is_rejected_even_with_a_matching_executable(self):
        import hashlib
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name in dc_build.DISC_HASHES:
                (root / name).write_bytes(name.encode())
            hashes = {n: hashlib.sha1((root/n).read_bytes()).hexdigest() for n in dc_build.DISC_HASHES}
            b = dc_build.Build.__new__(dc_build.Build)
            b.args = SimpleNamespace(disc=folder)
            with patch.object(dc_build, 'DISC_HASHES', hashes):
                b.step_verify()
                for name in ('TITLE.BIN', 'DUN.BIN'):
                    original = (root/name).read_bytes()
                    (root/name).write_bytes(b'corrupted')
                    with self.assertRaisesRegex(dc_build.BuildError, name): b.step_verify()
                    (root/name).write_bytes(original)

    def test_only_required_disc_inputs_are_extracted(self):
        for name in dc_build.DISC_FILES: self.assertTrue(dc_build.Build.wanted(name.lower()))
        for name in ('save.dat', 'OTHER.BIN', '../SCUS_971.11', 'MODULES/UNUSED.IRX'):
            self.assertFalse(dc_build.Build.wanted(name))

    def test_same_sized_old_image_is_replaced(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            selected = root / 'selected.iso'; selected.write_bytes(b'new image')
            game = root / 'game'; game.mkdir()
            (game/'disc.iso').write_bytes(b'old image')
            disc = SimpleNamespace(image=SimpleNamespace(size=2048, sector_size=2048), close=lambda: None)
            b = dc_build.Build.__new__(dc_build.Build); b.game = str(game)
            with patch.object(dc_build.ps2_disc, 'Disc', return_value=disc): b.place_image(str(selected))
            self.assertEqual((game/'disc.iso').read_bytes(), b'new image')

    def test_archive_requires_tmb_attribution_and_version(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder)/'release.zip'
            with zipfile.ZipFile(p,'w') as z:
                for name in REQUIRED: z.writestr('DC_Recomp_Bozo/'+name,b'fixture')
            self.assertEqual(make_release.verify_zip(p), [])
            with zipfile.ZipFile(p,'a') as z: z.writestr('DC_Recomp_Bozo/source/DATA.HED',b'game')
            self.assertTrue(any('DATA.HED' in e for e in make_release.verify_zip(p)))

if __name__ == '__main__': unittest.main()
