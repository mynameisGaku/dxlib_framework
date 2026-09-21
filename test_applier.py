import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('apply_debug_tools', Path(__file__).with_name('apply_debug_tools.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class ApplierTests(unittest.TestCase):
    def test_renderer_type_and_header_are_migrated(self):
        self.assertEqual(m.migrate_cpp('#include "Dxf/RenderSystem2D.h"\nFRenderSystem2D R;', 'x.cpp'),
                         '#include "Dxf/RenderSystem.h"\nFRenderSystem R;')
    def test_typed_context_uses_only_get2d(self):
        text = 'void F(FRenderContext& R) { R.Draw(T, P); R.DrawText(F, S, P); }'
        updated = m.migrate_cpp(text, 'x.cpp')
        self.assertIn('R.Get2D().DrawSprite(', updated)
        self.assertIn('R.Get2D().DrawText(', updated)
        self.assertEqual(updated, m.migrate_cpp(updated, 'x.cpp'))
    def test_backend_methods_are_not_migrated(self):
        text = 'void F(IRenderBackend& B) { B.DrawText(C); }'
        self.assertEqual(text, m.migrate_cpp(text, 'x.cpp'))
    def test_comments_and_literals_do_not_make_fake_calls(self):
        text = 'void F(FRenderContext& R) { /* R.Draw(T,P); */ const char* S="R.DrawText()"; }'
        self.assertEqual(text, m.migrate_cpp(text, 'x.cpp'))
    def test_unknown_root_batch_is_rejected(self):
        with self.assertRaises(RuntimeError):
            m.migrate_cpp('void F(FRenderContext& R) { R.SubmitGenerated(J, N, F); }', 'x.cpp')
    def test_bind_after_construction_and_refuse_changed_anchor(self):
        text='FApplication::FApplication() : m_Clock(m_Settings.MaxDeltaSeconds)\n{\n}'
        updated=m.bind_application(text)
        self.assertIn('GetContext().SetExecutionJobs_Internal(&m_ExecutionJobs)',updated)
        with self.assertRaises(RuntimeError): m.bind_application(updated)
    def test_cmake_registers_only_new_owner_and_module_before_install(self):
        text='add_library(dxf_support Source/DxLibSupport/Private/Dxf/RenderSystem2D.cpp)\nif(DXF_INSTALL)\nendif()'
        result=m.migrate_cmake(text)
        self.assertNotIn('RenderSystem2D.cpp',result)
        self.assertLess(result.index('include(CMake/DebugTools.cmake)'),result.index('if(DXF_INSTALL)'))
    def test_cmake_missing_or_duplicate_anchors_refused(self):
        with self.assertRaises(RuntimeError): m.migrate_cmake('if(DXF_INSTALL)\nendif()')
    def test_path_escape_is_refused(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(RuntimeError):m.checked_path(Path(t),'../unsafe.cpp')
    def test_line_endings_hash_identically(self):
        self.assertEqual(m.normalized_blob(b'a\r\nb\r\n'),m.normalized_blob(b'a\nb\n'))
    def test_success_backups_existing_and_deletes_only_requested(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'repo';root.mkdir();(root/'old').write_bytes(b'old')
            m.apply_transaction(root,{'old':None,'new':b'new'},Path(t)/'backup')
            self.assertFalse((root/'old').exists());self.assertEqual((root/'new').read_bytes(),b'new')
            self.assertEqual((Path(t)/'backup/old').read_bytes(),b'old')
    def test_midwrite_exception_rolls_back_completed_operations(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'repo';root.mkdir();(root/'keep').write_bytes(b'before')
            original=Path.replace;calls=0
            def replace(path,target):
                nonlocal calls
                calls+=1
                if calls==2:raise OSError('injected write error')
                return original(path,target)
            with patch.object(Path,'replace',replace):
                with self.assertRaises(OSError):m.apply_transaction(root,{'keep':b'after','new':b'new'},Path(t)/'backup')
            self.assertEqual((root/'keep').read_bytes(),b'before');self.assertFalse((root/'new').exists())

    def test_rollback_preserves_user_edit_after_our_first_write(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'repo';root.mkdir();(root/'keep').write_bytes(b'before')
            original=Path.replace;calls=0
            def replace(path,target):
                nonlocal calls
                calls+=1
                if calls==2:
                    (root/'keep').write_bytes(b'user concurrent edit')
                    raise OSError('injected second write error')
                return original(path,target)
            with patch.object(Path,'replace',replace):
                with self.assertRaises((OSError,RuntimeError)):
                    m.apply_transaction(root,{'keep':b'after','new':b'new'},Path(t)/'backup')
            self.assertEqual((root/'keep').read_bytes(),b'user concurrent edit')
            self.assertEqual((Path(t)/'backup/keep').read_bytes(),b'before')

if __name__=='__main__':unittest.main()
