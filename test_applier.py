"""Installer regressions. Tests use only disposable Git repositories in temporary directories."""
from __future__ import annotations
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('scene_installer', HERE / 'apply_scene_tasks.py')
assert spec is not None and spec.loader is not None
installer = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = installer
spec.loader.exec_module(installer)

# A fixture archive contains only exact pre-patch files required by this installer.
# It is not an installation payload and is never copied to a real repository.
FIXTURE = HERE / 'Testing' / 'installer_fixture.zip'


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='dxf-task-applier-test-')
        self.addCleanup(self.temp.cleanup)
        self.parent = Path(self.temp.name)
        self.root = self.parent / 'repo'
        self.root.mkdir()
        import zipfile
        with zipfile.ZipFile(FIXTURE) as archive:
            archive.extractall(self.root)
        (self.root / 'Tests').mkdir(exist_ok=True)
        self.command('init', '-q')
        self.command('config', 'user.email', 'fixture@example.invalid')
        self.command('config', 'user.name', 'Installer fixture')
        self.command('config', 'core.autocrlf', 'false')
        self.command('config', 'core.safecrlf', 'false')
        (self.root / '.gitattributes').write_text('* text=auto\n', encoding='utf-8')
        self.command('add', '.')
        self.command('commit', '-qm', 'Local disposable fixture')

    def command(self, *args):
        return installer.git(self.root, *args)

    def snapshot(self):
        return {p.relative_to(self.root).as_posix(): p.read_bytes()
                for p in self.root.rglob('*') if p.is_file()}

    def test_dry_run_is_read_only(self):
        # Git may refresh the index stat cache; source/index semantic contents must not change.
        status = self.command('status', '--porcelain=v1')
        tree = self.command('write-tree')
        source = {k: v for k, v in self.snapshot().items() if not k.startswith('.git/')}
        plan = installer.build_plan(self.root)
        self.assertEqual(len(plan), 12)
        self.assertEqual(status, self.command('status', '--porcelain=v1'))
        self.assertEqual(tree, self.command('write-tree'))
        self.assertEqual(source, {k: v for k, v in self.snapshot().items() if not k.startswith('.git/')})
        self.assertEqual(list(self.parent.glob('repo.scene-task-backup-*')), [])

    def test_apply_backup_and_idempotence(self):
        head = self.command('rev-parse', 'HEAD')
        tree = self.command('write-tree')
        plan = installer.build_plan(self.root)
        backup = installer.apply_plan(self.root, plan)
        self.assertIsNotNone(backup)
        for change in plan:
            self.assertEqual((self.root / change.relative).read_bytes(), change.after)
            if change.before is not None:
                self.assertEqual((backup / change.relative).read_bytes(), change.before)
        self.assertEqual(installer.build_plan(self.root), [])
        self.assertEqual(head, self.command('rev-parse', 'HEAD'))
        self.assertEqual(tree, self.command('write-tree'))

    def test_unrelated_assets_are_preserved(self):
        path = self.root / 'user-notes.txt'
        path.write_text('// unrelated renderer integration\n')
        installer.apply_plan(self.root, installer.build_plan(self.root))
        self.assertEqual(path.read_text(), '// unrelated renderer integration\n')

    def test_unstaged_target_edit_is_rejected(self):
        path = self.root / 'Source/Runtime/Public/Dxf/TaskDispatcher.h'
        path.write_bytes(path.read_bytes() + b'// user edit\n')
        with self.assertRaises(ValueError):
            installer.build_plan(self.root)

    def test_staged_target_with_original_working_bytes_is_rejected(self):
        rel = 'Source/Runtime/Public/Dxf/TaskDispatcher.h'
        path = self.root / rel
        original = path.read_bytes()
        path.write_bytes(original + b'// staged edit\n')
        self.command('add', rel)
        path.write_bytes(original)
        with self.assertRaisesRegex(ValueError, 'Overlapping'):
            installer.build_plan(self.root)

    def test_untracked_new_target_collision_is_rejected(self):
        path = self.root / 'Tests/SceneTaskIntegrationTests.cpp'
        path.write_text('// user test\n')
        with self.assertRaises(ValueError):
            installer.build_plan(self.root)

    def test_changed_dependency_is_rejected(self):
        path = self.root / 'Source/Toolbox/Public/Toolbox/JobSystem.h'
        path.write_bytes(path.read_bytes() + b'// different dependency\n')
        with self.assertRaisesRegex(ValueError, 'Dependency'):
            installer.build_plan(self.root)

    def test_missing_dependency_is_rejected(self):
        (self.root / 'Source/Toolbox/Public/Toolbox/JobSystem.h').unlink()
        with self.assertRaisesRegex(ValueError, 'Dependency'):
            installer.build_plan(self.root)

    def test_corrupt_payload_is_rejected(self):
        package = self.parent / 'package'
        shutil.copytree(HERE / 'Payload', package / 'Payload')
        shutil.copytree(HERE / 'Variants', package / 'Variants')
        shutil.copy2(HERE / 'MANIFEST.json', package / 'MANIFEST.json')
        target = package / 'Payload/Tests/SceneTaskIntegrationTests.cpp'
        target.write_text('broken')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            installer.build_plan(self.root, package)

    def test_crlf_is_preserved(self):
        self.command("config", "core.autocrlf", "true")
        rel = 'Source/Runtime/Public/Dxf/TaskDispatcher.h'
        path = self.root / rel
        path.write_bytes(path.read_bytes().replace(b'\n', b'\r\n'))
        self.assertEqual(self.command('diff', '--name-only', '--', rel), b'')
        installer.apply_plan(self.root, installer.build_plan(self.root))
        after = path.read_bytes()
        self.assertIn(b'\r\n', after)
        self.assertNotIn(b'\n', after.replace(b'\r\n', b''))
        self.assertEqual(installer.build_plan(self.root), [])

    def test_apply_rolls_back_on_write_failure(self):
        plan = installer.build_plan(self.root)
        before = {k: v for k, v in self.snapshot().items() if not k.startswith('.git/')}
        real = installer.atomic_write
        count = 0
        def failing(path, data, mode):
            nonlocal count
            count += 1
            if count == 4:
                raise OSError('Injected write failure')
            return real(path, data, mode)
        with patch.object(installer, 'atomic_write', side_effect=failing):
            with self.assertRaisesRegex(RuntimeError, 'rolled back'):
                installer.apply_plan(self.root, plan)
        after = {k: v for k, v in self.snapshot().items() if not k.startswith('.git/')}
        self.assertEqual(before, after)
        self.assertEqual(self.command('status', '--porcelain=v1'), b'')
        self.assertEqual(len(list(self.parent.glob('repo.scene-task-backup-*'))), 1)

    def test_change_after_planning_is_rejected(self):
        plan = installer.build_plan(self.root)
        path = self.root / 'Source/Runtime/Private/Dxf/SceneNavigator.cpp'
        path.write_bytes(path.read_bytes() + b'// edited while installer waits\n')
        with self.assertRaisesRegex(ValueError, 'after validation'):
            installer.apply_plan(self.root, plan)
        self.assertEqual(list(self.parent.glob('repo.scene-task-backup-*')), [])

    def test_symlink_is_rejected(self):
        path = self.root / 'Source/Runtime/Public/Dxf/TaskDispatcher.h'
        outside = self.parent / 'outside.h'
        path.rename(outside)
        try:
            path.symlink_to(outside)
        except OSError as error:
            self.skipTest(f'OS did not permit symlink creation: {error}')
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            installer.build_plan(self.root)

    def test_subdirectory_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'repository root'):
            installer.build_plan(self.root / 'Source')

    def test_path_traversal_is_rejected(self):
        for rel in ('../outside', '/outside', 'x/../outside', 'x\\outside', './x'):
            with self.assertRaises(ValueError):
                installer.safe_path(self.root, rel)

    def install_renderer_before(self, header_only=False):
        import zipfile
        with zipfile.ZipFile(HERE / 'Testing/renderer_before.zip') as archive:
            for rel in archive.namelist():
                if not header_only or rel.endswith('.h'):
                    (self.root / rel).write_bytes(archive.read(rel))
        self.command('add', '.')
        self.command('commit', '-qm', 'Disposable already-integrated renderer fixture')

    def test_existing_renderer_integration_is_preserved(self):
        self.install_renderer_before()
        installer.apply_plan(self.root, installer.build_plan(self.root))
        header = (self.root / 'Source/Runtime/Public/Dxf/Application.h').read_text()
        implementation = (self.root / 'Source/Runtime/Private/Dxf/Application.cpp').read_text()
        self.assertIn('Dxf/RenderSystem.h', header)
        self.assertNotIn('FRenderSystem2D', header)
        self.assertEqual(implementation.count('m_Renderer.SetExecutionJobs(m_ExecutionJobs)'), 1)
        self.assertIn('m_Scenes(m_Assets, m_Audio, m_pGame.Get(), &m_TaskDispatcher)', implementation)
        self.assertEqual(installer.build_plan(self.root), [])

    def test_mixed_renderer_profiles_are_rejected(self):
        self.install_renderer_before(header_only=True)
        with self.assertRaisesRegex(ValueError, 'profiles do not match'):
            installer.build_plan(self.root)

    def test_rollback_preserves_concurrent_edit(self):
        plan = installer.build_plan(self.root)
        first = self.root / plan[0].relative
        real = installer.atomic_write
        count = 0
        def failing(path, data, mode):
            nonlocal count
            count += 1
            if count == 3:
                first.write_bytes(b'concurrent editor data')
                raise OSError('Injected write failure after concurrent edit')
            return real(path, data, mode)
        with patch.object(installer, 'atomic_write', side_effect=failing):
            with self.assertRaisesRegex(RuntimeError, 'Concurrent edit prevents rollback'):
                installer.apply_plan(self.root, plan)
        self.assertEqual(first.read_bytes(), b'concurrent editor data')



if __name__ == '__main__':
    unittest.main(verbosity=2)
