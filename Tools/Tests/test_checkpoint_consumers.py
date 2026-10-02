"""Exercise the relocated process runner's success and failure boundaries."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools'))
import VerifyCheckpointConsumers as verifier
from ValidatePackage import CONSUMER_SOURCES
from ValidateDebug import REQUIRED


class CheckpointConsumerTests(unittest.TestCase):
    def test_all_definition_and_local_include_dependencies_are_copied(self):
        directory = ROOT / 'Tools/PackageConsumer'
        for path in directory.glob('*.h'):
            self.assertIn(path.name, CONSUMER_SOURCES)
        for path in directory.joinpath('Data').glob('*checkpoint*.json'):
            self.assertIn('Data/' + path.name, CONSUMER_SOURCES)
        self.assertTrue({'GameCheckpointResumeAllocationFault2D', 'GameCheckpointResumeAllocationFault3D'}.issubset(REQUIRED))

    def invoke(self, failure=None):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            exe = root / 'deployed.exe'
            exe.write_bytes(b'fixed executable')
            calls = []

            def run(logs, name, command, **kwargs):
                calls.append(command)
                mode, dimension = command[1:3]
                save_root = Path(command[4])
                if failure == 'exit':
                    raise RuntimeError('child exited nonzero')
                if failure == 'hash':
                    exe.write_bytes(b'changed executable')
                stages = ('before', 'saved', 'changed') if mode == 'write' else ('continued',)
                output = f'CHECKPOINT_PROCESS_PASSED {mode} dimension={dimension}\n'
                if failure == 'completion':
                    output = ''
                for stage in stages:
                    output += f'EXTERNAL_SAVE_PIXEL stage={stage}\n'
                    if failure != 'asset-check':
                        output += f'EXTERNAL_SAVE_ASSET stage={stage}\n'
                    for view in ('single', 'split', 'odd'):
                        if failure != 'image':
                            (save_root / f'{stage}-{view}.png').write_bytes(b'fake image for runner test only')
                return output

            with patch.object(verifier, 'run_logged', side_effect=run):
                evidence = verifier.run_processes(exe, root, root / 'SaveRoot', root, root, 'native', native=True)
            self.assertEqual(len(calls), 4)
            self.assertEqual([(c[1], c[2]) for c in calls], [('write', '2'), ('read', '2'), ('write', '3'), ('read', '3')])
            self.assertEqual(evidence['exe_sha256'], hashlib.sha256(b'fixed executable').hexdigest())
            self.assertEqual(len(evidence['images']), 24)
            self.assertNotEqual(calls[0][4], calls[2][4])

    def test_four_fresh_processes_and_exact_expected_images(self):
        self.invoke()

    def test_nonzero_missing_checks_images_and_changed_exe_fail(self):
        for failure in ('exit', 'hash', 'completion', 'asset-check', 'image'):
            with self.subTest(failure=failure):
                with self.assertRaises(RuntimeError):
                    self.invoke(failure)
