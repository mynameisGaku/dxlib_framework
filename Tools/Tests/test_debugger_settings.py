"""Check the Visual Studio F5 settings generated for NativeModelSmoke.

The synthetic tests always run. The generated-project test needs a Visual Studio build
directory and runs only when DXF_CHECK_VS_BUILD points to one, for example
Build/VisualStudio-development after GenerateProjectFiles.bat -Development.
"""
from pathlib import Path
import os
import re
import unittest
import xml.etree.ElementTree as ET

NAMESPACE = '{http://schemas.microsoft.com/developer/msbuild/2003}'
CONFIGURATIONS = ('Debug', 'Release')
OUTPUT_PREFIX = 'model-smoke-vs-'


def split_arguments(text):
    """Split a debugger command line into arguments; double quotes group paths with spaces."""
    return [quoted if quoted else plain for quoted, plain in re.findall(r'"([^"]*)"|(\S+)', text)]


def debugger_settings(project_text):
    """Return {configuration: {property: value}} for LocalDebugger* properties."""
    root = ET.fromstring(project_text)
    settings = {}
    for element in root.iter():
        if not element.tag.startswith(NAMESPACE + 'LocalDebugger'):
            continue
        match = re.search(r"'([^'|]+)\|x64'", element.get('Condition', ''))
        configuration = match.group(1) if match else ''
        settings.setdefault(configuration, {})[element.tag[len(NAMESPACE):]] = element.text or ''
    return settings


def model_smoke_errors(project_text, source_root, binary_root):
    """List mismatches against the expected NativeModelSmoke arguments and working directory."""
    normalize = lambda value: str(value).replace('\\', '/').rstrip('/').lower()
    settings = debugger_settings(project_text)
    errors = []
    for configuration in CONFIGURATIONS:
        values = settings.get(configuration, {})
        raw = values.get('LocalDebuggerCommandArguments', '')
        arguments = split_arguments(raw)
        if raw.count('"') != 4 or len(arguments) != 2:
            errors.append(f'{configuration}: expected two quoted arguments, got {raw!r}')
            continue
        if normalize(arguments[0]) != normalize(source_root):
            errors.append(f'{configuration}: first argument is not the source root: {arguments[0]!r}')
        expected_output = f'{normalize(binary_root)}/{OUTPUT_PREFIX}{configuration}'.lower()
        if normalize(arguments[1]) != expected_output:
            errors.append(f'{configuration}: output directory is {arguments[1]!r}')
        if '$<' in raw or '${' in raw:
            errors.append(f'{configuration}: unexpanded CMake expression in {raw!r}')
        working = values.get('LocalDebuggerWorkingDirectory', '')
        if normalize(working) != f'{normalize(binary_root)}/{configuration}'.lower():
            errors.append(f'{configuration}: working directory is {working!r}')
    return errors


def sample_project(arguments, working):
    """Build a minimal vcxproj text in the shape the Visual Studio generator writes."""
    groups = ''.join(
        f'<PropertyGroup><LocalDebuggerCommandArguments Condition="\'$(Configuration)|$(Platform)\'==\'{c}|x64\'">'
        f'{arguments.format(c)}</LocalDebuggerCommandArguments>'
        f'<LocalDebuggerWorkingDirectory Condition="\'$(Configuration)|$(Platform)\'==\'{c}|x64\'">'
        f'{working.format(c)}</LocalDebuggerWorkingDirectory></PropertyGroup>' for c in CONFIGURATIONS)
    return f'<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">{groups}</Project>'


class SyntheticDebuggerSettingsTests(unittest.TestCase):
    SOURCE = 'C:/Work Space/dxlib framework'
    BINARY = 'C:/Work Space/dxlib framework/Build/VisualStudio-development'

    def test_quoted_paths_with_spaces_stay_two_arguments(self):
        self.assertEqual(split_arguments('"C:/a b/c" "D:/e f"'), ['C:/a b/c', 'D:/e f'])

    def test_expected_settings_pass(self):
        project = sample_project(f'&quot;{self.SOURCE}&quot; &quot;{self.BINARY}/model-smoke-vs-{{}}&quot;',
                                 self.BINARY + '/{}')
        self.assertEqual(model_smoke_errors(project, self.SOURCE, self.BINARY), [])

    def test_missing_arguments_fail(self):
        project = '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><PropertyGroup /></Project>'
        self.assertEqual(len(model_smoke_errors(project, self.SOURCE, self.BINARY)), 2)

    def test_unquoted_paths_with_spaces_fail(self):
        project = sample_project(f'{self.SOURCE} {self.BINARY}/model-smoke-vs-{{}}', self.BINARY + '/{}')
        self.assertTrue(model_smoke_errors(project, self.SOURCE, self.BINARY))

    def test_shared_output_or_wrong_order_fails(self):
        shared = sample_project(f'&quot;{self.SOURCE}&quot; &quot;{self.BINARY}/model-smoke&quot;', self.BINARY + '/{}')
        swapped = sample_project(f'&quot;{self.BINARY}/model-smoke-vs-{{}}&quot; &quot;{self.SOURCE}&quot;',
                                 self.BINARY + '/{}')
        self.assertTrue(model_smoke_errors(shared, self.SOURCE, self.BINARY))
        self.assertTrue(model_smoke_errors(swapped, self.SOURCE, self.BINARY))

    def test_wrong_configuration_directory_fails(self):
        project = sample_project(f'&quot;{self.SOURCE}&quot; &quot;{self.BINARY}/model-smoke-vs-Debug&quot;',
                                 self.BINARY + '/Debug')
        self.assertTrue(model_smoke_errors(project, self.SOURCE, self.BINARY))


@unittest.skipUnless(os.environ.get('DXF_CHECK_VS_BUILD'), 'DXF_CHECK_VS_BUILD is not set')
class GeneratedDebuggerSettingsTests(unittest.TestCase):
    def test_native_model_smoke_debugger_settings(self):
        build = Path(os.environ['DXF_CHECK_VS_BUILD']).resolve()
        cache = (build / 'CMakeCache.txt').read_text(encoding='utf-8', errors='replace')
        source = re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$', cache, re.M).group(1).strip()
        project = (build / 'NativeModelSmoke.vcxproj').read_text(encoding='utf-8-sig')
        self.assertEqual(model_smoke_errors(project, source, build), [])
        # Only NativeModelSmoke receives the model-smoke arguments.
        others = [path.name for path in build.glob('*.vcxproj')
                  if path.name != 'NativeModelSmoke.vcxproj' and OUTPUT_PREFIX in path.read_text(encoding='utf-8-sig')]
        self.assertEqual(others, [])


if __name__ == '__main__':
    unittest.main()
