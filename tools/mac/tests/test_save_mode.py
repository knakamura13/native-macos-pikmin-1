import importlib.util
import os
import pathlib
import shutil
import sys
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('save_mode', ROOT / 'save_mode.py')
save_mode = importlib.util.module_from_spec(spec)
sys.modules['save_mode'] = save_mode
spec.loader.exec_module(save_mode)


class SaveModeTests(unittest.TestCase):
    def test_central_path_uses_application_support(self):
        home = pathlib.Path('/tmp/example-home')
        self.assertEqual(
            save_mode.central_save_dir(home),
            home / 'Library' / 'Application Support' / 'Open Nectar' / 'save',
        )

    def test_migration_copies_and_leaves_legacy(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = pathlib.Path(tmp) / 'home'
            legacy = home / '.local' / 'share' / 'pikmin-native' / 'save' / 'card0'
            legacy.mkdir(parents=True)
            (legacy / '01.gci').write_text('one', encoding='utf-8')
            save_mode.migrate_central_save(home)
            central = save_mode.central_save_dir(home) / 'card0' / '01.gci'
            self.assertTrue(central.is_file())
            self.assertEqual(central.read_text(encoding='utf-8'), 'one')
            self.assertEqual((legacy / '01.gci').read_text(encoding='utf-8'), 'one')

    def test_pikmin2_conflict_keeps_newer_and_backups_older(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = pathlib.Path(tmp) / 'home'
            central = save_mode.central_save_dir(home)
            p2_install = home / 'pikmin2'
            p2_save = p2_install / 'save'
            (central / 'card0').mkdir(parents=True)
            (p2_save / 'card0').mkdir(parents=True)
            (central / 'card0' / 'Pikmin2_SaveData').write_text('old', encoding='utf-8')
            p2_file = p2_save / 'card0' / 'Pikmin2_SaveData'
            p2_file.write_text('new', encoding='utf-8')
            future = time.time() + 60
            os.utime(p2_file, (future, future))
            config = home / '.config'
            (config / 'open-nectar').mkdir(parents=True)
            (config / 'open-nectar' / 'pikmin2_dir').write_text(str(p2_install) + '\n', encoding='utf-8')
            save_mode.migrate_central_save(home, config_home=config)
            self.assertEqual((central / 'card0' / 'Pikmin2_SaveData').read_text(encoding='utf-8'), 'new')
            backups = list((home / 'OpenNectar-archive.noindex' / 'save-migration-backups').glob('*/save/card0/Pikmin2_SaveData'))
            self.assertTrue(backups)

    def test_automation_forces_throwaway(self):
        env = {'PIKMIN_REPLAY_TEST': '1'}
        self.assertTrue(save_mode.automation_or_cheat_forces_throwaway(env))

    def test_resolve_default_is_central(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = pathlib.Path(tmp)
            args = argparse_namespace()
            resolution = save_mode.resolve_save_mode(
                home=home, args=args, env={}, build_output=None, cmake_cache=None, interactive=False,
            )
            self.assertEqual(resolution.mode, 'central')
            self.assertEqual(resolution.save_dir, save_mode.central_save_dir(home))

    def test_read_only_mode_sets_flag(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = pathlib.Path(tmp)
            args = argparse_namespace(read_only_save=True)
            resolution = save_mode.resolve_save_mode(
                home=home, args=args, env={}, build_output=None, cmake_cache=None, interactive=False,
            )
            self.assertTrue(resolution.read_only)
            self.assertEqual(resolution.mode, 'readonly')

    def test_sandbox_never_aliases_central(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = pathlib.Path(tmp)
            central = save_mode.central_save_dir(home)
            (central / 'card0').mkdir(parents=True)
            (central / 'card0' / 'keep.txt').write_text('keep', encoding='utf-8')
            out = home / 'build-out'
            args = argparse_namespace(sandbox_save=True)
            resolution = save_mode.resolve_save_mode(
                home=home, args=args, env={}, build_output=out, cmake_cache=None, interactive=False,
            )
            marker = resolution.save_dir / 'card0' / 'touch.txt'
            marker.parent.mkdir(parents=True, exist_ok=True)
            marker.write_text('sandbox', encoding='utf-8')
            self.assertTrue(marker.exists())
            self.assertFalse((central / 'card0' / 'touch.txt').exists())

    def test_hash_detects_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp) / 'save'
            (root / 'card0').mkdir(parents=True)
            file_path = root / 'card0' / 'a.gci'
            file_path.write_text('a', encoding='utf-8')
            before = save_mode.hash_save_tree(root)
            file_path.write_text('b', encoding='utf-8')
            after = save_mode.hash_save_tree(root)
            self.assertNotEqual(before, after)


def argparse_namespace(**kwargs):
    class Args:
        pass

    args = Args()
    args.fresh_save = kwargs.get('fresh_save', False)
    args.sandbox_save = kwargs.get('sandbox_save', False)
    args.read_only_save = kwargs.get('read_only_save', False)
    args.save_name = kwargs.get('save_name')
    args.save_dir = kwargs.get('save_dir')
    args.choose_save = kwargs.get('choose_save', False)
    return args


if __name__ == '__main__':
    unittest.main()
