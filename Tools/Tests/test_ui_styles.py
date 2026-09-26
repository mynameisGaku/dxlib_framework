"""Packaging contract only; C++ parser tests cover style/token semantics."""
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from MergeUiStyles import merge_styles, write_bundle


class UiStylesTest(unittest.TestCase):
    def test_deterministic_sorted_sources_preserve_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'B.dxfui').write_text('dxfui-style 1\nstyle B {\n foreground = @accent\n}\n', encoding='utf-8')
            (root/'A.dxfui').write_text('# comment\ndxfui-style 1\ntoken accent = #0088ff\n', encoding='utf-8')
            output = root/'bundle.dxfui'
            first, count = merge_styles(root, output)
            self.assertEqual(count, 2)
            self.assertEqual(write_bundle(root, output), 2)
            second, _ = merge_styles(root, output)
            self.assertEqual(first, second)
            self.assertIn(b'source "A.dxfui"\r\n# comment\r\n\r\ntoken', first)
            self.assertLess(first.index(b'A.dxfui'), first.index(b'B.dxfui'))
            self.assertEqual(first.count(b'\n'), first.count(b'\r\n'))

    def test_failure_keeps_previous_output_and_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            source=root/'A.dxfui'
            source.write_bytes(b'dxfui-style 2\n')
            output=root/'bundle.dxfui'
            output.write_bytes(b'previous')
            with self.assertRaises(ValueError): write_bundle(root,output)
            self.assertEqual(output.read_bytes(), b'previous')
            self.assertEqual(source.read_bytes(), b'dxfui-style 2\n')

    def test_limits_invalid_utf8_reserved_markers_and_nul(self):
        for content, limit in [(b'dxfui-style 1\n', 4), (b'\xff', 100),
                               (b'dxfui-style 1\nsource "other"\n', 100),
                               (b'dxfui-style 1\n\0', 100)]:
            with self.subTest(content=content), tempfile.TemporaryDirectory() as directory:
                root=Path(directory)
                (root/'A.dxfui').write_bytes(content)
                with self.assertRaises(ValueError): merge_styles(root,root/'output',limit)

    def test_empty_tree_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            with self.assertRaises(ValueError): merge_styles(root,root/'output')
