"""Content consumer copy set and bounded data fixtures are part of deployment."""
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools'))
from ValidatePackage import CONSUMER_SOURCES
class ContentPackageTests(unittest.TestCase):
    def test_content_files_copy_and_compile(self):
        cmake = (ROOT / 'Tools/PackageConsumer/CMakeLists.txt').read_text(encoding='utf-8')
        for name in ('ContentConsumer.h', 'ContentConsumer.cpp', 'ContentNativeConsumer.cpp', 'Data/prefab2d.dxfprefab.json', 'Data/prefab3d.dxfprefab.json', 'Data/scene2d.dxfscene.json', 'Data/scene3d.dxfscene.json'):
            self.assertIn(name, CONSUMER_SOURCES)
            self.assertTrue((ROOT / 'Tools/PackageConsumer' / name).is_file())
            if name.endswith('.cpp'):
                self.assertIn(name, cmake)
        for dimension in (2, 3):
            data = json.loads((ROOT / f'Tools/PackageConsumer/Data/prefab{dimension}d.dxfprefab.json').read_text(encoding='utf-8'))
            self.assertEqual(data['dimension'], dimension)
            self.assertEqual({j['kind'] for j in data['joints']}, {'Distance', 'Revolute', 'Fixed', 'Prismatic'})

    def test_assets_are_fingerprinted_and_changed_run_is_rejected(self):
        import tempfile
        from ValidationSupport import source_manifest, validation_report
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'Assets').mkdir()
            data = root / 'Assets/data.json'
            data.write_text('{}', encoding='utf-8')
            before = source_manifest(root)[0]
            logs = root / 'Logs'
            with self.assertRaisesRegex(RuntimeError, 'manifest changed'):
                with validation_report(logs, {}, root=root):
                    data.write_text('{"changed":true}', encoding='utf-8')
            summary = json.loads((logs / 'Summary.json').read_text(encoding='utf-8'))
            self.assertEqual(summary['status'], 'failed')
            self.assertTrue(summary['source_changed'])
            self.assertNotEqual(before, summary['final_source_sha256'])
