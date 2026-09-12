"""Portable release-tool tests; these do not execute PowerShell or the real SDK."""
from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / 'PackageRelease.py'
spec = importlib.util.spec_from_file_location('package_release', SCRIPT)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'Source').mkdir()
        (self.root / 'Source' / 'Probe.h').write_text('#pragma once\n', encoding='utf-8')
        (self.root / 'README.md').write_text('test\n', encoding='utf-8')

    def test_only_distribution_inputs_are_included(self):
        for directory in ('Build', 'ThirdParty', '.git'):
            (self.root / directory).mkdir()
            (self.root / directory / 'private.txt').write_text('exclude')
        files = release.collect_paths(self.root)
        self.assertEqual([p.relative_to(self.root).as_posix() for p in files], ['README.md', 'Source/Probe.h'])

    def test_fonts_are_never_packaged(self):
        (self.root / 'Source' / 'font.ttf').write_bytes(b'not-a-real-font')
        with self.assertRaisesRegex(ValueError, 'font'):
            release.collect_paths(self.root)

    def test_symbolic_links_are_rejected(self):
        link = self.root / 'Source' / 'outside.h'
        try:
            link.symlink_to(self.root / 'README.md')
        except OSError:
            self.skipTest('Symlink creation not permitted')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            release.collect_paths(self.root)

    def test_archive_has_valid_manifest_and_reproducible_bytes(self):
        first, second = self.root / 'first.zip', self.root / 'second.zip'
        release.make_archive(self.root, first)
        release.make_archive(self.root, second)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        with zipfile.ZipFile(first) as archive:
            manifest = json.loads(archive.read('dxlib_framework/DistributionManifest.json'))
            for record in manifest['files']:
                data = archive.read('dxlib_framework/' + record['path'])
                self.assertEqual(len(data), record['bytes'])
                self.assertEqual(hashlib.sha256(data).hexdigest(), record['sha256'])
            self.assertEqual(len(manifest['files']), 2)

    def test_tools_bytecode_is_not_distributed(self):
        cache = self.root / 'Tools' / '__pycache__'
        cache.mkdir(parents=True)
        (cache / 'x.pyc').write_bytes(b'cached')
        self.assertFalse(any('__pycache__' in p.parts for p in release.collect_paths(self.root)))

    def test_archive_cannot_overwrite_source(self):
        with self.assertRaisesRegex(ValueError, 'overwrite'):
            release.make_archive(self.root, self.root / 'README.md')


if __name__ == '__main__':
    unittest.main()
