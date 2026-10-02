"""The optional profile consumer and canonical validation remain deployable."""
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools'))
from ValidatePackage import CONSUMER_SOURCES
from ValidateDebug import REQUIRED

class InputProfilePackageTests(unittest.TestCase):
    def test_copy_manifest_contains_every_local_consumer_source_once(self):
        sources = {path.name for path in (ROOT / 'Tools/PackageConsumer').glob('*.cpp')}
        self.assertTrue(sources.issubset(set(CONSUMER_SOURCES)))
        self.assertEqual(len(CONSUMER_SOURCES), len(set(CONSUMER_SOURCES)))
        self.assertIn('InputProfile.cpp', CONSUMER_SOURCES)

    def test_profiles_and_allocation_fault_are_required_by_standalone_validation(self):
        self.assertTrue({'InputProfiles', 'InputProfilesAllocationFault'}.issubset(REQUIRED))
