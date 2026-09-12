"""Keep STL out of public APIs and hidden aliases without flagging documentation."""
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from CheckNoStl import scan_text


class NoStlTests(unittest.TestCase):
    def test_rejects_headers_aliases_and_namespace_imports(self):
        source = '#include <vector>\nusing FVector = std::vector<int>;\nusing namespace std;\nnamespace Hidden = std;\n'
        self.assertEqual(len(scan_text(Path('Source/Foundation/Probe.h'), source)), 4)

    def test_only_toolbox_can_use_language_support(self):
        source = '#include <new>\n#include <initializer_list>\nstd::align_val_t Alignment;\nstd::initializer_list<int> Values;\n'
        self.assertFalse(scan_text(Path('Source/Toolbox/Public/Probe.h'), source))
        self.assertEqual(len(scan_text(Path('Source/Foundation/Probe.h'), source)), 4)

    def test_toolbox_cannot_wrap_stl(self):
        self.assertEqual(len(scan_text(Path('Source/Toolbox/Probe.h'), '#include <memory>\nusing T = std::shared_ptr<int>;')), 2)

    def test_comments_and_literals_are_not_code(self):
        source = '// std::vector\n/** std::shared_ptr */\nconst char* Text = "std::string";\nconst char* Raw = R"tag(std::vector)tag";\n'
        self.assertFalse(scan_text(Path('Source/Probe.h'), source))

    def test_raw_string_include_is_not_a_dependency(self):
        self.assertFalse(scan_text(Path('Source/Probe.h'), 'const char* Text = R"(\n#include <vector>\n)";'))

    def test_digit_separators_do_not_hide_namespace_use(self):
        self.assertEqual(len(scan_text(Path('Source/Probe.h'), "auto Count = 1'000; std::vector<int> Values; auto Other = 2'000;")), 1)


if __name__ == '__main__':
    unittest.main()
