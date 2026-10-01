"""一時コピーだけを変異させ、build失敗でも正常版へ戻し失敗を維持する。"""
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

class MechanismMutationHarnessTests(unittest.TestCase):
    def test_failed_build_restores_bytes_and_does_not_count_red(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'Tools/ValidateMechanismMutations.py'
            script.parent.mkdir()
            shutil.copyfile(ROOT / 'Tools/ValidateMechanismMutations.py', script)
            shutil.copyfile(ROOT / 'Tools/ValidationSupport.py', script.parent / 'ValidationSupport.py')
            relative = 'Source/Physics/Private/Dxf/MechanismConstraintMath.h'
            source = root / relative
            source.parent.mkdir(parents=True)
            original = (ROOT / relative).read_bytes()
            source.write_bytes(original)
            logs = root / 'logs'
            # build失敗、復元build成功、復元後正常。この試行は必ず失敗を維持する。
            replies = [subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 0), subprocess.CompletedProcess([], 0)]
            argv = [str(script), '--build', str(root / 'build'), '--logs', str(logs), '1']
            with patch.object(sys, 'argv', argv), patch.object(subprocess, 'run', side_effect=replies) as invoked:
                with self.assertRaisesRegex(SystemExit, 'Mutation failed'):
                    runpy.run_path(str(script), run_name='__main__')
                self.assertEqual(invoked.call_count, 3)
            result = json.loads((logs / 'K-M01.json').read_text())
            self.assertEqual(source.read_bytes(), original)
            self.assertNotIn('red', result)
            self.assertEqual(result['build']['exit'], 1)
            self.assertEqual(result['green']['exit'], 0)
            self.assertEqual(result['baseline_hashes'], result['restored_hashes'])
            self.assertEqual((logs / 'K-M01-backup' / relative).read_bytes(), original)

    def test_bad_id_rejects_before_writing_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            logs = Path(directory) / 'should-not-exist'
            result = subprocess.run([sys.executable, str(ROOT / 'Tools/ValidateMechanismMutations.py'),
                                     '--build', directory, '--logs', str(logs), '25'], capture_output=True)
            self.assertEqual(result.returncode, 2)
            self.assertFalse(logs.exists())
