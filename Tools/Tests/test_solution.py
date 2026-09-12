"""Check both Visual Studio formats without requiring CMake or the DxLib SDK."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

TOOLS = Path(__file__).resolve().parents[1]
POWERSHELL = shutil.which('pwsh') or shutil.which('powershell')
KEEP = '{11111111-1111-1111-1111-111111111111}'
REMOVE = '{22222222-2222-2222-2222-222222222222}'


@unittest.skipUnless(POWERSHELL, 'PowerShell is unavailable')
class SolutionTests(unittest.TestCase):
    def convert(self, extension, content, development=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / ('original' + extension)
            destination = root / ('root' + extension)
            source.write_text(content, encoding='utf-8')
            # Arguments are literal PowerShell strings, including workspace paths with apostrophes.
            quote = lambda value: "'" + str(value).replace("'", "''") + "'"
            command = f'. {quote(TOOLS / "SolutionHelpers.ps1")}; Export-RootSolution -Source {quote(source)} -Destination {quote(destination)} -BuildRelative "Build/VisualStudio/"'
            if development:
                command += ' -Development'
            result = subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', command], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(source.read_text(encoding='utf-8'), content)
            return destination.read_text(encoding='utf-8-sig')

    def test_sln_removes_projects_and_dangling_guids(self):
        content = f'''Microsoft Visual Studio Solution File, Format Version 12.00
Project("{{TYPE}}") = "Starter", "Examples\\Starter.vcxproj", "{KEEP}"
    ProjectSection(ProjectDependencies) = postProject
        {REMOVE} = {REMOVE}
    EndProjectSection
EndProject
Project("{{TYPE}}") = "ALL_BUILD", "ALL_BUILD.vcxproj", "{REMOVE}"
EndProject
Global
    GlobalSection(ProjectConfigurationPlatforms) = postSolution
        {KEEP}.Debug|x64.ActiveCfg = Debug|x64
        {REMOVE}.Debug|x64.ActiveCfg = Debug|x64
    EndGlobalSection
EndGlobal
'''
        normal = self.convert('.sln', content)
        self.assertNotIn(REMOVE, normal)
        self.assertIn(KEEP, normal)
        self.assertIn('Build\\VisualStudio\\Examples\\Starter.vcxproj', normal)
        development = self.convert('.sln', content, True)
        self.assertIn('ALL_BUILD', development)
        self.assertIn(REMOVE, development)

    def test_slnx_removes_helpers_and_rebases_dependencies(self):
        content = '''<Solution>
  <Project Path="Starter.vcxproj" />
  <Project Path="dxf_foundation.vcxproj">
    <BuildDependency Project="Starter.vcxproj" />
    <BuildDependency Project="ZERO_CHECK.vcxproj" />
  </Project>
  <Project Path="ZERO_CHECK.vcxproj" />
</Solution>
'''
        normal = ET.fromstring(self.convert('.slnx', content))
        self.assertEqual([node.attrib['Path'] for node in normal.findall('Project')], ['Build/VisualStudio/Starter.vcxproj', 'Build/VisualStudio/dxf_foundation.vcxproj'])
        self.assertEqual([node.attrib['Project'] for node in normal.findall('.//BuildDependency')], ['Build/VisualStudio/Starter.vcxproj'])
        development = ET.fromstring(self.convert('.slnx', content, True))
        self.assertEqual(len(development.findall('Project')), 3)


if __name__ == '__main__':
    unittest.main()
