"""Joint利用者のソースを、再配置検証器が欠落なく持ち込むことを確認する。"""
from pathlib import Path
import sys
import unittest
import json
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools'))
from ValidatePackage import CONSUMER_SOURCES
import ValidatePackage as package
class JointPackageTests(unittest.TestCase):
    def test_joint_files_are_copied(self):
        for name in ('JointConsumer.h', 'JointConsumer.cpp', 'FJointConsumerResult.h', 'PhysicsJoint.h', 'PhysicsJoint.cpp', 'PhysicsMechanism.h', 'PhysicsMechanism.cpp', 'MechanismConsumer.h', 'MechanismConsumer.cpp', 'FMechanismConsumerResult.h'):
            self.assertIn(name, CONSUMER_SOURCES)
            self.assertTrue((ROOT / 'Tools/PackageConsumer' / name).is_file())
        self.assertEqual(len(CONSUMER_SOURCES), len(set(CONSUMER_SOURCES)))

class ConsumerGeneratorTests(unittest.TestCase):
    def test_non_windows_uses_ninja(self):
        with patch.object(package.sys, 'platform', 'linux'):
            self.assertEqual(package.consumer_build_generator(), ('Ninja', [], False))

    def test_windows_uses_matching_vs_multi_configuration(self):
        installation = json.dumps([{'installationVersion': '18.1.2'}])
        capability = json.dumps({'generators': [{'name': 'Visual Studio 18 2026'}, {'name': 'Ninja'}]})
        with patch.object(package.sys, 'platform', 'win32'), patch.dict(package.os.environ, {'ProgramFiles(x86)': 'C:/VS Installer'}), patch.object(package.subprocess, 'check_output', side_effect=[installation, capability]):
            self.assertEqual(package.consumer_build_generator(), ('Visual Studio 18 2026', ['-A', 'x64'], True))

    def test_missing_cpp_tools_is_a_failure(self):
        with patch.object(package.sys, 'platform', 'win32'), patch.dict(package.os.environ, {'ProgramFiles(x86)': 'C:/VS Installer'}), patch.object(package.subprocess, 'check_output', return_value='[]'):
            with self.assertRaisesRegex(RuntimeError, r'C\+\+ tools'):
                package.consumer_build_generator()

    def test_incompatible_cmake_is_a_failure(self):
        with patch.object(package.sys, 'platform', 'win32'), patch.dict(package.os.environ, {'ProgramFiles(x86)': 'C:/VS Installer'}), patch.object(package.subprocess, 'check_output', side_effect=[json.dumps([{'installationVersion': '18.0'}]), json.dumps({'generators': [{'name': 'Ninja'}]})]):
            with self.assertRaisesRegex(RuntimeError, 'Visual Studio 18'):
                package.consumer_build_generator()
