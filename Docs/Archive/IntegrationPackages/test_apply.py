"""Offline migration/audit tests. These are not a full repository build."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('apply',Path(__file__).with_name('apply_render_views.py'))
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
class MigrationTests(unittest.TestCase):
    def test_typed_context_and_backend_are_not_confused(self):
        src='void F(FRenderContext& Render) { Render.Draw(T, P); Render.DrawText(F,T,P); Backend.DrawText(C); }'
        out=a.migrate_cpp(src,'sample.cpp')
        self.assertIn('Render.Get2D().DrawSprite(T, P)',out)
        self.assertIn('Backend.DrawText(C)',out)
        self.assertNotIn('Render.DrawText',out)
    def test_nested_getters_migrate(self):
        out=a.migrate_cpp('App.GetRenderer().GetContext().DrawText(F,T,P);','sample.cpp')
        self.assertIn('GetContext().Get2D().DrawText',out)
    def test_alias_and_pointer(self):
        src='auto& Context = R.GetContext(); Context.Draw(T,P); FRenderContext* Ptr; Ptr->DrawText(F,T,P);'
        out=a.migrate_cpp(src,'sample.cpp')
        self.assertIn('Context.Get2D().DrawSprite',out)
        self.assertIn('Ptr->Get2D().DrawText',out)
    def test_comments_and_strings_stay_unchanged(self):
        src='// Render.Draw(T,P)\nvoid F(FRenderContext& Render){ const char* Text="Render.DrawText(F,T,P)"; Render.Draw(T,P); }'
        out=a.migrate_cpp(src,'sample.cpp')
        self.assertIn('// Render.Draw(T,P)',out)
        self.assertIn('"Render.DrawText(F,T,P)"',out)
    def test_root_batches_are_not_silently_rewritten(self):
        with self.assertRaises(RuntimeError): a.migrate_cpp('FRenderContext C(Q); C.SubmitGenerated(Jobs,1,F);','x.cpp')
    def test_application_binding_is_unambiguous(self):
        src='X::X(): m_Clock(m_Settings.MaxDeltaSeconds)\n{\n}\n'
        self.assertIn('SetExecutionJobs(m_ExecutionJobs)',a.bind_application(src))
        with self.assertRaises(RuntimeError): a.bind_application(src+src)
    def test_migration_idempotent(self):
        src='void F(FRenderContext& Render){Render.Draw(T,P);}'
        once=a.migrate_cpp(src,'x.cpp')
        self.assertEqual(once,a.migrate_cpp(once,'x.cpp'))
    def test_root_registers_both_previous_and_new_regressions(self):
        src="add_library(dxf_support STATIC x.cpp)\nset(DXF_NATIVE_SOURCES a.cpp)\nif(DXF_BUILD_TESTS)\nendif()\n"
        out=a.migrate_root_cmake(src)
        self.assertIn('dxf_add_render_continuation_tests(dxf::support)',out)
        self.assertIn('dxf_add_render_views_tests(dxf::support)',out)
        self.assertIn('DxLibGeometryBackend.cpp',out)
    def test_cmake_anchor_missing_fails(self):
        with self.assertRaises(RuntimeError): a.migrate_root_cmake('add_library(other STATIC)')
    def test_transaction_creates_backups_and_deletes_only_planned_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'repo';root.mkdir()
            (root/'keep').write_bytes(b'keep');(root/'old').write_bytes(b'old')
            backup=Path(tmp)/'backup'
            a.apply_transaction(root,{'old':None,'new/path':b'value'},backup)
            self.assertEqual((root/'keep').read_bytes(),b'keep')
            self.assertFalse((root/'old').exists())
            self.assertEqual((root/'new/path').read_bytes(),b'value')
            self.assertEqual((backup/'old').read_bytes(),b'old')
    def test_write_failure_rolls_back_completed_operations(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'repo';root.mkdir()
            (root/'old').write_bytes(b'original')
            original_replace=Path.replace
            def fail_second(path, target):
                if Path(target).name=='bad': raise OSError('injected write failure')
                return original_replace(path,target)
            with patch.object(Path,'replace',fail_second):
                with self.assertRaises(OSError):
                    a.apply_transaction(root,{'old':b'changed','bad':b'value'},Path(tmp)/'backup')
            self.assertEqual((root/'old').read_bytes(),b'original')
            self.assertFalse((root/'bad').exists())
if __name__=='__main__':unittest.main(verbosity=2)
