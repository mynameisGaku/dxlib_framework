"""Compare generated Visual Studio projects and filters with the tracked sources (set comparison).

Reads the generated root solutions (dxlib_framework.slnx / dxlib_framework-development.slnx) and their
.vcxproj / .vcxproj.filters files. Nothing is generated, built or opened. Set DXF_CHECK_VS_SOLUTIONS=1 after
running Tools/GenerateProjectFiles.ps1 (normal) and Tools/GenerateProjectFiles.ps1 -Development.
The expected sets come from `git ls-files` of each owning directory, so a newly added h/cpp is checked without
editing a hand-written list.
"""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
ITEM_TAGS = ('ClCompile', 'ClInclude', 'None')
EXTENSIONS = ('.h', '.hpp', '.inl', '.cpp', '.dxfui', '.json')
# Sources that only enter the product libraries when a build option asks for the CPU benchmark probes.
PROBE_SOURCES = ('WorldInteractionProbe.cpp', 'WorldInteractionProbe.h')
PROBE_OPTION = 'DXF_INTERACTION_BENCHMARK_PROBES'
# Product libraries that must stay in the normal solution with every tracked file of their layer.
PRODUCTS = {
    'dxf_toolbox': ('Source/Toolbox',),
    'dxf_physics': ('Source/Physics',),
    'dxf_gameplay': ('Source/Gameplay',),
    'dxf_scene_content': ('Source/SceneContent',),
}
# Development-only projects.
SAMPLES = ('UISample', 'dxf_ui_sample', 'GameplaySample', 'dxf_gameplay_sample')
# Sources compiled by more than one of the checked projects on purpose (the UI sample reuses the gameplay
# sample's level and characters). Any other shared compilation between the checked projects is a finding.
SHARED = {
    'Examples/GameplaySample/SampleCharacters.cpp': {'dxf_gameplay_sample', 'dxf_ui_sample'},
    'Examples/GameplaySample/SampleLevel.cpp': {'dxf_gameplay_sample', 'dxf_ui_sample'},
}


def tracked(directory: str) -> set[str]:
    output = subprocess.run(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '--', directory], cwd=ROOT, capture_output=True, text=True,
                            check=True).stdout
    return {line for line in output.splitlines() if line.endswith(EXTENSIONS)}


def probe_option(projects: dict[str, Path]) -> bool:
    """Read DXF_INTERACTION_BENCHMARK_PROBES from the CMake cache the solution was generated from.

    The probe sources are only compiled into the product libraries when the option is on, so the
    expected set has to follow the configuration instead of demanding them unconditionally.
    """
    if not projects:
        return False
    cache = next(iter(projects.values())).parent / 'CMakeCache.txt'
    if not cache.is_file():
        return False
    for line in cache.read_text(encoding='utf-8', errors='replace').splitlines():
        if line.startswith(PROBE_OPTION + ':'):
            return line.split('=', 1)[1].strip() == 'ON'
    return False


def expected_files(project: str, probes: bool = False) -> tuple[set[str], set[str]]:
    """Return (files that must be visible, files that must be compiled) for a project."""
    if project in PRODUCTS:
        files = set()
        for directory in PRODUCTS[project]:
            files |= tracked(directory)
        # The allocation fault hook is compiled only into the isolated fault executables.
        files = {f for f in files if '/Testing/' not in f}
        if not probes:
            files = {f for f in files if f.rsplit('/', 1)[-1] not in PROBE_SOURCES}
        return files, {f for f in files if f.endswith('.cpp')}
    ui = tracked('Examples/UiSample')
    gameplay = tracked('Examples/GameplaySample') | tracked('Assets/Content')
    shared = {'Examples/GameplaySample/SampleCharacters.h', 'Examples/GameplaySample/SampleCharacters.cpp',
              'Examples/GameplaySample/SampleLevel.h', 'Examples/GameplaySample/SampleLevel.cpp'}
    if project == 'dxf_ui_sample':
        files = (ui - {'Examples/UiSample/WindowsMain.cpp'}) | shared
        return files, {f for f in files if f.endswith('.cpp')}
    if project == 'UISample':
        # The GUI entry shows the whole sample (display only) and compiles only its entry point.
        files = ui | shared
        return files, {'Examples/UiSample/WindowsMain.cpp'}
    if project == 'dxf_gameplay_sample':
        files = gameplay - {'Examples/GameplaySample/WindowsMain.cpp'}
        return files, {f for f in files if f.endswith('.cpp')}
    if project == 'GameplaySample':
        # Same principle as UISample: the entry project shows the sample and compiles only its entry point.
        return gameplay, {'Examples/GameplaySample/WindowsMain.cpp'}
    raise KeyError(project)


def relative(project: Path, include: str) -> str | None:
    if not include:
        # Generated entries without a path are not repository files; they never belong to a set.
        return None
    path = (project.parent / include.replace('\\', '/')).resolve()
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return None


def solution(stem: str) -> dict[str, Path]:
    path = ROOT / (stem + '.slnx')
    tree = ET.parse(path).getroot()
    return {Path(node.attrib['Path']).stem: (ROOT / node.attrib['Path']).resolve()
            for node in tree.iter() if node.tag == 'Project' and node.attrib['Path'].endswith('.vcxproj')}


def items(project: Path) -> tuple[dict[str, str], set[str]]:
    """Return ({relative path: item tag}, {relative paths compiled})."""
    tree = ET.parse(project).getroot()
    found: dict[str, str] = {}
    compiled: set[str] = set()
    for tag in ITEM_TAGS:
        for node in tree.findall('.//m:' + tag, NS):
            path = relative(project, node.attrib.get('Include', ''))
            if path is None:
                continue
            found[path] = tag
            if tag == 'ClCompile' and node.find('m:ExcludedFromBuild', NS) is None:
                compiled.add(path)
    return found, compiled


def filters(project: Path) -> dict[str, str]:
    tree = ET.parse(Path(str(project) + '.filters')).getroot()
    result: dict[str, str] = {}
    for tag in ITEM_TAGS:
        for node in tree.findall('.//m:' + tag, NS):
            path = relative(project, node.attrib.get('Include', ''))
            if path is None:
                continue
            result[path] = (node.findtext('m:Filter', '', NS) or '').replace('\\', '/')
    return result


@unittest.skipUnless(os.getenv('DXF_CHECK_VS_SOLUTIONS'), 'DXF_CHECK_VS_SOLUTIONS is not set')
class GeneratedFilterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.normal = solution('dxlib_framework')
        cls.development = solution('dxlib_framework-development')
        cls.probes = probe_option(cls.development)

    def check_project(self, name: str, projects: dict[str, Path]):
        self.assertIn(name, projects)
        project = projects[name]
        found, compiled = items(project)
        shown = filters(project)
        visible, compile = expected_files(name, self.probes)
        # Tracked but missing from the project, or from its filters.
        self.assertFalse(visible - found.keys(), (name, 'missing from project', sorted(visible - found.keys())))
        self.assertFalse(visible - shown.keys(), (name, 'missing from filters', sorted(visible - shown.keys())))
        # In the filters but not a project item, or the other way round (for repository files).
        repository = {p for p in found if p.startswith(('Source/', 'Examples/', 'Tests/', 'Tools/'))}
        self.assertFalse(shown.keys() - found.keys(), (name, 'filters without project item',
                                                       sorted(shown.keys() - found.keys())))
        self.assertFalse(repository - shown.keys(), (name, 'project item without filter',
                                                     sorted(repository - shown.keys())))
        # Each file sits in the filter of its real folder.
        wrong = {p: shown[p] for p in visible if shown[p] != p.rsplit('/', 1)[0]}
        self.assertFalse(wrong, (name, 'wrong filter', wrong))
        # Compiled set: every expected cpp compiled, nothing else from the owning folders.
        self.assertFalse(compile - compiled, (name, 'not compiled', sorted(compile - compiled)))
        extra = {p for p in compiled & visible if p not in compile}
        self.assertFalse(extra, (name, 'compiled although display only', sorted(extra)))
        return compiled

    def test_products_stay_in_the_normal_solution(self):
        for name in PRODUCTS:
            with self.subTest(project=name):
                self.check_project(name, self.normal)
                self.check_project(name, self.development)

    def test_samples_are_development_only(self):
        for name in SAMPLES:
            with self.subTest(project=name):
                self.assertNotIn(name, self.normal)
                self.check_project(name, self.development)

    def test_ui_sample_entry_shows_the_sample_without_compiling_it(self):
        found, compiled = items(self.development['UISample'])
        sample_cpp = {p for p in tracked('Examples/UiSample') if p.endswith('.cpp')}
        self.assertTrue(sample_cpp - {'Examples/UiSample/WindowsMain.cpp'}, 'the sample has implementation files')
        self.assertTrue(sample_cpp <= found.keys(), sorted(sample_cpp - found.keys()))
        self.assertEqual(compiled & sample_cpp, {'Examples/UiSample/WindowsMain.cpp'})

    def test_no_unintended_double_compilation(self):
        owners: dict[str, set[str]] = {}
        for name in tuple(PRODUCTS) + SAMPLES:
            _, compiled = items(self.development[name])
            for path in compiled:
                owners.setdefault(path, set()).add(name)
        shared = {path: names for path, names in owners.items() if len(names) > 1}
        self.assertEqual(shared, SHARED)


if __name__ == '__main__':
    unittest.main()
