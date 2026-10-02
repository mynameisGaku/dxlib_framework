"""Check the repository's parameter comment layout."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ParameterCommentTests(unittest.TestCase):
    def test_each_parameter_has_its_own_line(self):
        violations = []
        for directory in ('Source', 'Examples', 'Tests', 'Tools/PackageConsumer'):
            for path in sorted((ROOT / directory).rglob('*')):
                if path.suffix not in ('.h', '.hpp', '.cpp', '.inl'):
                    continue
                for number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
                    if len(re.findall(r'@param\b', line)) > 1:
                        violations.append(f'{path.relative_to(ROOT)}:{number}')
        self.assertEqual([], violations, 'Multiple parameter comments on one line: ' + ', '.join(violations))
