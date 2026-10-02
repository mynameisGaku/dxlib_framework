"""Reject incomplete native save evidence, even when a process prints a success token."""
from contextlib import nullcontext
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import VerifyNativeSaveProcesses as runner


class NativeSaveEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'
        (self.source / 'Assets').mkdir(parents=True)
        (self.source / 'Assets' / 'original.txt').write_bytes(b'original')
        self.probe = self.root / 'probe.exe'
        self.probe.write_bytes(b'fixed executable')
        self.logs = self.root / 'new logs'
        self.logs.mkdir()
        self.calls = []

    def complete_process(self, logs, name, command, **kwargs):
        self.calls.append((name, command, kwargs['cwd']))
        dimension, mode = name.split('D-')
        images = Path(command[2])
        phases = ('before', 'saved', 'changed') if mode == 'write' else ('continued',)
        for phase in phases:
            for layout in ('single', 'split', 'odd'):
                (images / f'{phase}-{layout}.png').write_bytes(b'test image')
        return f'SAVE_COURSE_NATIVE_PASSED dimension={dimension} mode={mode}'

    def verify(self, process):
        with patch.object(runner, 'validation_report', return_value=nullcontext()), patch.object(runner, 'run_logged', side_effect=process):
            runner.verify(self.probe, self.source, self.logs)

    def test_four_independent_invocations_share_only_dimension_save_root(self):
        self.verify(self.complete_process)
        self.assertEqual([call[0] for call in self.calls], ['2D-write', '2D-read', '3D-write', '3D-read'])
        commands = [call[1] for call in self.calls]
        self.assertEqual(commands[0][5], commands[1][5])
        self.assertEqual(commands[2][5], commands[3][5])
        self.assertNotEqual(commands[0][5], commands[2][5])
        self.assertTrue(all(cwd != self.source for _, _, cwd in self.calls))
        evidence = json.loads((self.logs / 'ProcessEvidence.json').read_text(encoding='utf-8'))
        self.assertEqual(evidence['readback_images'], 24)

    def test_success_token_does_not_hide_incomplete_images(self):
        def incomplete(*args, **kwargs):
            output = self.complete_process(*args, **kwargs)
            (Path(args[2][2]) / 'before-odd.png').unlink()
            return output
        with self.assertRaisesRegex(RuntimeError, 'Incomplete native readback'):
            self.verify(incomplete)
        self.assertFalse((self.logs / 'ProcessEvidence.json').exists())

    def test_child_failure_never_runs_reader_or_emits_success_evidence(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.verify(lambda *a, **k: (_ for _ in ()).throw(subprocess.CalledProcessError(7, 'probe')))
        self.assertFalse((self.logs / 'ProcessEvidence.json').exists())

    def test_changed_executable_is_rejected_before_next_process(self):
        def changed(*args, **kwargs):
            output = self.complete_process(*args, **kwargs)
            Path(args[2][0]).write_bytes(b'changed executable')
            return output
        with self.assertRaisesRegex(RuntimeError, 'executable changed'):
            self.verify(changed)
        self.assertEqual(len(self.calls), 1)

    def test_original_assets_change_is_rejected(self):
        def changed(*args, **kwargs):
            output = self.complete_process(*args, **kwargs)
            (self.source / 'Assets' / 'original.txt').write_bytes(b'changed')
            return output
        with self.assertRaisesRegex(RuntimeError, 'Original Assets changed'):
            self.verify(changed)
        self.assertFalse((self.logs / 'ProcessEvidence.json').exists())
