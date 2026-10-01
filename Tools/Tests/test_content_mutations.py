"""Content変異のbuild失敗と無効IDを、製品に触らない一時コピーで確認する。"""
from pathlib import Path
import json
import runpy
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]

class ContentMutationHarnessTests(unittest.TestCase):
    def test_failed_build_restores_bytes_without_red(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'Tools/ValidateContentMutations.py'
            script.parent.mkdir()
            shutil.copyfile(ROOT / 'Tools/ValidateContentMutations.py', script)
            relative = 'Source/SceneContent/Private/Dxf/PrefabRuntime.h'
            source = root / relative
            source.parent.mkdir(parents=True)
            original = (ROOT / relative).read_bytes()
            source.write_bytes(original)
            binary = root / 'build/Debug/dxf_scene_content_tests.exe'
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b'fake-test-binary')
            logs = root / 'logs'
            replies = [subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 0), subprocess.CompletedProcess([], 0)]
            argv = [str(script), '--build', str(root / 'build'), '--logs', str(logs), '1']
            with patch.object(sys, 'path', [str(ROOT / 'Tools')] + sys.path), patch.object(sys, 'argv', argv), patch.object(subprocess, 'run', side_effect=replies) as invoked:
                with self.assertRaisesRegex(SystemExit, 'Mutation did not complete'):
                    runpy.run_path(str(script), run_name='__main__')
                self.assertEqual(invoked.call_count, 3)
            result = json.loads((logs / 'B-M01.json').read_text())
            self.assertNotIn('red', result)
            self.assertEqual(result['build']['exit'], 1)
            self.assertEqual(result['green']['exit'], 0)
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual(result['baseline_hashes'], result['restored_hashes'])
            self.assertEqual((logs / 'B-M01-backup' / relative).read_bytes(), original)

    def test_invalid_id_starts_no_run(self):
        with tempfile.TemporaryDirectory() as directory:
            logs = Path(directory) / 'should-not-exist'
            result = subprocess.run([sys.executable, str(ROOT / 'Tools/ValidateContentMutations.py'), '--build', directory, '--logs', str(logs), '19'], capture_output=True)
            self.assertEqual(result.returncode, 2)
            self.assertFalse(logs.exists())
