"""Synthetic-only export checks; no original content, production build or user saves."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('public_source', Path(__file__).resolve().parents[1] / 'tools/prepare_public_source.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class ExportChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / 'src').mkdir()
        (self.root / 'src/a.cpp').write_text('int value() { return 7; }\n')
        self.manifest = {'version': 1, 'status': 'partial-source-review-candidate', 'blockers': ['synthetic missing input'],
                         'files': [{'source': 'src/a.cpp', 'path': 'src/a.cpp', 'sha256': exporter.digest((self.root / 'src/a.cpp').read_bytes()), 'basis': 'synthetic'}]}
        self.lock = self.root / 'manifest.json'
        self.destination = self.root / 'candidate'
        self.save_manifest()

    def save_manifest(self):
        self.lock.write_text(json.dumps(self.manifest))

    def run_export(self):
        return exporter.export(self.root, self.lock, self.destination)

    def test_exact_copy_and_no_overwrite(self):
        self.assertEqual(self.run_export(), 2)
        self.assertEqual(exporter.audit(self.destination, self.manifest), 2)
        with self.assertRaises(exporter.Rejected):
            self.run_export()
        self.assertEqual((self.destination / 'src/a.cpp').read_bytes(), (self.root / 'src/a.cpp').read_bytes())

    def test_changed_input_never_creates_candidate(self):
        (self.root / 'src/a.cpp').write_text('changed')
        with self.assertRaises(exporter.Rejected):
            self.run_export()
        self.assertFalse(self.destination.exists())

    def test_symlink_input_and_output_reject(self):
        (self.root / 'src/a.cpp').unlink()
        (self.root / 'src/a.cpp').symlink_to(self.lock)
        with self.assertRaises(exporter.Rejected):
            self.run_export()
        (self.root / 'src/a.cpp').unlink()
        (self.root / 'src/a.cpp').write_text('int value() { return 7; }\n')
        self.destination.symlink_to(self.root / 'missing')
        with self.assertRaises(exporter.Rejected):
            self.run_export()

    def test_extra_or_tampered_output_reject(self):
        self.run_export()
        extra = self.destination / 'extra.txt'
        extra.write_text('extra')
        with self.assertRaises(exporter.Rejected):
            exporter.audit(self.destination, self.manifest)
        extra.unlink()
        (self.destination / 'src/a.cpp').write_text('changed')
        with self.assertRaises(exporter.Rejected):
            exporter.audit(self.destination, self.manifest)

    def test_path_escape_duplicate_and_private_reject(self):
        for name in ('../secret', '/absolute', 'private/input.json', '.git/config', 'src/a_program.inc'):
            with self.subTest(name=name), self.assertRaises(exporter.Rejected):
                exporter.relative(name)
        self.manifest['files'].append(dict(self.manifest['files'][0]))
        self.save_manifest()
        with self.assertRaises(exporter.Rejected):
            self.run_export()

    def test_linked_parent_and_extra_empty_directory_reject(self):
        (self.root / 'linked').symlink_to(self.root / 'src', target_is_directory=True)
        self.manifest['files'][0]['source'] = 'linked/a.cpp'
        self.save_manifest()
        with self.assertRaises(exporter.Rejected):
            self.run_export()
        self.manifest['files'][0]['source'] = 'src/a.cpp'
        self.save_manifest()
        self.run_export()
        (self.destination / 'empty-extra').mkdir()
        with self.assertRaises(exporter.Rejected):
            exporter.audit(self.destination, self.manifest)

    def test_sensitive_input_and_file_directory_collision_reject(self):
        with self.assertRaises(exporter.Rejected):
            exporter.inspect_bytes(('/' + 'Users/person/private.txt').encode(), 'synthetic')
        with self.assertRaises(exporter.Rejected):
            exporter.inspect_bytes(('-----BEGIN ' + 'PRIVATE KEY-----').encode(), 'synthetic')
        other = dict(self.manifest['files'][0], source='src/b.cpp', path='src')
        self.manifest['files'].append(other)
        self.save_manifest()
        with self.assertRaises(exporter.Rejected):
            self.run_export()

    def test_failed_copy_cleans_new_destination_and_preserves_sources(self):
        from unittest.mock import patch
        with patch.object(exporter.shutil, 'move', side_effect=OSError('synthetic interruption')):
            with self.assertRaises(OSError):
                self.run_export()
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.root.glob('.public-source-*')), [])
        self.assertEqual((self.root / 'src/a.cpp').read_text(), 'int value() { return 7; }\n')

    def test_complete_contract_allows_only_named_includes(self):
        self.manifest.update(version=2, status='complete-source-candidate',
                             content_contract='nobihaza.public-complete-source.v1')
        for name in sorted(exporter.COMPLETE_CONTENT_INCLUDES):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('// synthetic reviewed content\n')
            self.manifest['files'].append({'source': name, 'path': name,
                'sha256': exporter.digest(path.read_bytes()), 'basis': 'synthetic content'})
        self.save_manifest()
        self.assertEqual(self.run_export(), 9)
        for name in ('src/game/other_program.inc', 'tools/other.inc', '../src/game/dora_program.inc',
                     'private/src/game/dora_program.inc'):
            with self.subTest(name=name), self.assertRaises(exporter.Rejected):
                exporter.relative(name, complete=True)
        self.manifest['files'][-1]['path'] = 'src/renamed.cpp'
        self.save_manifest()
        with self.assertRaises(exporter.Rejected):
            exporter.load_manifest(self.lock)

    def test_old_contract_and_invalid_complete_contract_reject_includes(self):
        name = 'src/game/dora_program.inc'
        self.manifest['files'][0].update(source=name, path=name)
        self.save_manifest()
        with self.assertRaises(exporter.Rejected):
            exporter.load_manifest(self.lock)
        self.manifest.update(version=2, status='complete-source-candidate')
        self.save_manifest()
        with self.assertRaises(exporter.Rejected):
            exporter.load_manifest(self.lock)

    def test_complete_content_keeps_input_secret_hash_and_link_guards(self):
        name = 'src/game/dora_program.inc'
        path = self.root / name
        path.parent.mkdir(parents=True)
        self.manifest.update(version=2, status='complete-source-candidate',
                             content_contract='nobihaza.public-complete-source.v1')
        self.manifest['files'][0].update(source=name, path=name)
        for data in ('changed without review', '/' + 'Users/person/file.cpp',
                     'ghp_' + 'a' * 30):
            path.write_text(data)
            if data != 'changed without review':
                self.manifest['files'][0]['sha256'] = exporter.digest(path.read_bytes())
            self.save_manifest()
            with self.subTest(data=data), self.assertRaises(exporter.Rejected):
                self.run_export()
            self.assertFalse(self.destination.exists())
        path.unlink()
        path.symlink_to(self.root / 'src/a.cpp')
        with self.assertRaises(exporter.Rejected):
            self.run_export()


if __name__ == '__main__':
    unittest.main()
