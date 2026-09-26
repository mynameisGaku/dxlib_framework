"""Check UI project visibility without compiling Native or changing IDE state.

The ordinary tests inspect registration. Opt-in checks read real CMake file-API
responses (DXF_CHECK_UI_BUILD) or Visual Studio filters (DXF_CHECK_VS_BUILD).
PowerShell solution-export checks run only when PowerShell is installed.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
POWERSHELL = shutil.which('pwsh') or shutil.which('powershell')
PRODUCTS = ('dxf_ui', 'dxf_ui_runtime')
DEVELOPMENT = ('dxf_ui_sample', 'UISample', 'NativeUiSmoke', 'dxf_ui_tests',
               'dxf_ui_runtime_tests', 'dxf_ui_application_tests',
               'dxf_ui_fault_tests', 'dxf_ui_benchmark')
DIRECTORIES = {
    'dxf_ui': ('Source/Ui',),
    'dxf_ui_runtime': ('Source/UiRuntime',),
    'dxf_ui_sample': ('Examples/UiSample',),
    'dxf_ui_tests': ('Tests/Ui',),
    'dxf_ui_runtime_tests': ('Tests/Ui',),
    'dxf_ui_application_tests': ('Tests/Ui', 'Tests/UiNativeSmoke'),
    'dxf_ui_fault_tests': ('Tests/Ui',),
    'dxf_ui_benchmark': ('Tools/UiBenchmark',),
}
STYLES = ('Examples/UiSample/Styles/Button.dxfui', 'Examples/UiSample/Styles/Tokens.dxfui')


def expected_headers(target: str) -> set[str]:
    result = set()
    for directory in DIRECTORIES[target]:
        for extension in ('*.h', '*.hpp'):
            result.update(p.relative_to(ROOT).as_posix() for p in (ROOT / directory).rglob(extension))
    if target == 'dxf_ui_sample':
        result.update(('Examples/GameplaySample/SampleCharacters.h',
                       'Examples/GameplaySample/SampleLevel.h'))
    return result


class UiRegistrationTests(unittest.TestCase):
    def visible_projects(self):
        text = (ROOT / 'Tools/SolutionHelpers.ps1').read_text(encoding='utf-8-sig')
        match = re.search(r'\$VisibleProjects\s*=\s*@\((.*?)\)', text, re.S)
        self.assertIsNotNone(match, 'Missing normal-solution allowlist')
        return set(re.findall(r"'([^']+)'", match.group(1)))

    def test_normal_solution_keeps_ui_product_libraries(self):
        self.assertTrue(set(PRODUCTS) <= self.visible_projects())

    def test_normal_solution_excludes_ui_development_projects(self):
        self.assertFalse(set(DEVELOPMENT) & self.visible_projects())
        text = (ROOT / 'Tools/GenerateProjectFiles.ps1').read_text(encoding='utf-8-sig')
        self.assertIn("'-DDXF_BUILD_UI_SAMPLE='", text)
        self.assertIn('$Development -and -not $Portable', text)

    def test_headers_registered_on_the_target_that_owns_implementation(self):
        text = '\n'.join((ROOT / name).read_text(encoding='utf-8-sig')
                         for name in ('CMake/UiModule.cmake', 'CMake/UiSample.cmake'))
        for target, directories in DIRECTORIES.items():
            if target in PRODUCTS:
                continue  # dxf_public_headers already calls dxf_ide_headers.
            with self.subTest(target=target):
                match = re.search(r'dxf_ide_headers\(\s*' + target + r'\s+([^)]*)\)', text)
                self.assertIsNotNone(match, target)
                for directory in directories:
                    self.assertIn(directory, match.group(1).split())


@unittest.skipUnless(POWERSHELL, 'PowerShell is unavailable')
class UiSolutionExportTests(unittest.TestCase):
    def convert(self, extension, content, development):
        with tempfile.TemporaryDirectory(prefix='ui solution ') as temp:
            root = Path(temp)
            source, destination = root / ('input' + extension), root / ('output' + extension)
            source.write_text(content, encoding='utf-8')
            quote = lambda p: "'" + str(p).replace("'", "''") + "'"
            command = (f'. {quote(ROOT / "Tools/SolutionHelpers.ps1")}; '
                       f'Export-RootSolution -Source {quote(source)} -Destination {quote(destination)} '
                       '-BuildRelative "Build/VisualStudio/"')
            if development:
                command += ' -Development'
            proc = subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass',
                                   '-Command', command], capture_output=True, text=True, timeout=30)
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertEqual(source.read_text(encoding='utf-8'), content)
            return destination.read_text(encoding='utf-8-sig')

    def test_sln_export_keeps_ui_libraries_not_sample(self):
        names = PRODUCTS + ('Starter',) + DEVELOPMENT
        content = 'Microsoft Visual Studio Solution File, Format Version 12.00\n'
        for index, name in enumerate(names):
            guid = '{00000000-0000-0000-0000-%012d}' % index
            content += f'Project("{{TYPE}}") = "{name}", "{name}.vcxproj", "{guid}"\nEndProject\n'
        normal = self.convert('.sln', content, False)
        for name in PRODUCTS + ('Starter',):
            self.assertIn(f'"{name}", "Build\\VisualStudio\\{name}.vcxproj"', normal)
        for name in DEVELOPMENT:
            self.assertNotIn(f'"{name}"', normal)
        development = self.convert('.sln', content, True)
        for name in names:
            self.assertIn(f'"{name}"', development)

    def test_slnx_export_keeps_ui_libraries_not_sample(self):
        names = PRODUCTS + ('Starter',) + DEVELOPMENT
        content = '<Solution>' + ''.join(f'<Project Path="{n}.vcxproj" />' for n in names) + '</Solution>'
        normal = ET.fromstring(self.convert('.slnx', content, False))
        self.assertEqual({n.attrib['Path'] for n in normal.findall('Project')},
                         {f'Build/VisualStudio/{n}.vcxproj' for n in PRODUCTS + ('Starter',)})
        development = ET.fromstring(self.convert('.slnx', content, True))
        self.assertEqual(len(development.findall('Project')), len(names))


class UiGeneratedProjectsTests(unittest.TestCase):
    @unittest.skipUnless(os.getenv('DXF_CHECK_UI_BUILD'), 'DXF_CHECK_UI_BUILD is not set')
    def test_cmake_codemodel_headers_styles_and_groups(self):
        reply = Path(os.environ['DXF_CHECK_UI_BUILD']) / '.cmake/api/v1/reply'
        indexes = list(reply.glob('index-*.json'))
        self.assertTrue(indexes, 'Create codemodel-v2 query before configuring CMake')
        index = json.loads(max(indexes, key=lambda p: p.stat().st_mtime_ns).read_text(encoding='utf-8'))
        model_file = next(x['jsonFile'] for x in index['objects'] if x['kind'] == 'codemodel')
        model = json.loads((reply / model_file).read_text(encoding='utf-8'))
        for configuration in model['configurations']:
            targets = {t['name']: t for t in configuration['targets']}
            for target in DIRECTORIES:
                with self.subTest(configuration=configuration['name'], target=target):
                    self.assertIn(target, targets, 'Use Native OFF / TESTS ON for this check')
                    data = json.loads((reply / targets[target]['jsonFile']).read_text(encoding='utf-8'))
                    sources = {s['path']: s for s in data['sources']}
                    required = expected_headers(target)
                    if target == 'dxf_ui_sample':
                        required.update(STYLES)
                    self.assertFalse(required - sources.keys(), sorted(required - sources.keys()))
                    for path in required:
                        self.assertNotIn('compileGroupIndex', sources[path], path)
                    for path, source in sources.items():
                        if not path.startswith(('Source/', 'Examples/', 'Tests/', 'Tools/')):
                            continue
                        self.assertIn('sourceGroupIndex', source, path)
                        group = data['sourceGroups'][source['sourceGroupIndex']]['name'].replace('\\', '/')
                        self.assertEqual(group, path.rsplit('/', 1)[0], (target, path, group))
                    if target == 'dxf_ui_sample':
                        self.assertNotIn('Examples/UiSample/WindowsMain.cpp', sources)

    @unittest.skipUnless(os.getenv('DXF_CHECK_VS_BUILD'), 'DXF_CHECK_VS_BUILD is not set')
    def test_real_visual_studio_filters(self):
        build = Path(os.environ['DXF_CHECK_VS_BUILD']).resolve()
        for target in DIRECTORIES:
            with self.subTest(target=target):
                path = build / (target + '.vcxproj.filters')
                self.assertTrue(path.is_file(), str(path))
                document = ET.parse(path)
                entries = {}
                for node in document.iter():
                    if node.tag.rsplit('}', 1)[-1] not in ('ClInclude', 'ClCompile', 'None'):
                        continue
                    included = node.get('Include')
                    if not included:
                        continue
                    absolute = (path.parent / included).resolve()
                    filter_node = next((c for c in node if c.tag.rsplit('}', 1)[-1] == 'Filter'), None)
                    if filter_node is not None:
                        entries[absolute] = (filter_node.text or '').replace('\\', '/')
                required = expected_headers(target)
                if target == 'dxf_ui_sample':
                    required.update(STYLES)
                for relative in required:
                    full = (ROOT / relative).resolve()
                    self.assertIn(full, entries, relative)
                    self.assertEqual(entries[full], relative.rsplit('/', 1)[0])


if __name__ == '__main__':
    unittest.main()
