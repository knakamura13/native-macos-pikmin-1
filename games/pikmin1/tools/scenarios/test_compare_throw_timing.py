import pathlib
import tempfile
import unittest
from compare_throw_timing import compare


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.sources = {name: pathlib.Path(self.temp.name)/name for name in ('macOS','Linux','Windows')}
        for directory in self.sources.values():
            directory.mkdir()
            for rate, k in ((30,15),(60,18)):
                (directory/f'scenario_throw_release_{rate}.log').write_text(
                    f'[netplay-det] reseed day=0 stage=0 sessionSeed=0 hash=0x123 tick=0\n'
                    f'[scenario] throw_release calibration: K = {k} ticks\n[scenario] PASS throw_release\n')

    def path(self):
        return self.sources['Windows']/'scenario_throw_release_60.log'

    def test_equal(self):
        self.assertEqual(compare(self.sources)['Linux'], {'30':15,'60':18})

    def test_changed_timing(self):
        self.path().write_text(self.path().read_text().replace('18 ticks','19 ticks'))
        with self.assertRaisesRegex(ValueError,'differs'):
            compare(self.sources)

    def test_missing_log(self):
        self.path().unlink()
        with self.assertRaises(OSError):
            compare(self.sources)

    def test_bad_logs(self):
        original = self.path().read_text()
        for text in ('', original.replace('[scenario] PASS','[scenario] FAIL'),
                     original.replace('sessionSeed=0','sessionSeed=1'),
                     original+original, original.replace('18 ticks','0 ticks')):
            with self.subTest(text=text):
                self.path().write_text(text)
                with self.assertRaises(ValueError):
                    compare(self.sources)

    def test_single_source(self):
        with self.assertRaisesRegex(ValueError,'two distinct'):
            compare({'macOS': self.sources['macOS']})


if __name__ == '__main__':
    unittest.main()
