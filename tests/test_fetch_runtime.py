"""Public runtime restoration tests with tiny local ZIPs and mocked HTTP only."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('fetch_runtime', Path(__file__).resolve().parents[1] / 'tools/fetch_runtime.py')
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class RuntimeChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.destination = self.root / 'assets/runtime'
        self.cache = self.root / 'cache'
        self.manifest_path = self.root / 'runtime-manifest.json'
        self.files = {'content.json': b'{"synthetic":true}', 'audio/sample.ogg': b'synthetic audio', 'licenses/NOTICE.txt': b'synthetic notice'}
        self.set_archive(self.files)

    def set_archive(self, members, symlink=None):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as archive:
            for name, data in members.items():
                if name == symlink:
                    info = zipfile.ZipInfo(name)
                    info.create_system = 3
                    info.external_attr = (stat.S_IFLNK | 0o777) << 16
                    archive.writestr(info, data)
                else:
                    archive.writestr(name, data)
        data = stream.getvalue()
        self.parts = [data[:len(data)//2], data[len(data)//2:]]
        self.manifest = {'schema': 'nobihaza.public-runtime.v1', 'entry': 'content.json',
            'archive': {'size': len(data), 'sha256': sha(data), 'parts': []},
            'files': [{'path': n, 'size': len(b), 'sha256': sha(b)} for n, b in self.files.items()]}
        for i, part in enumerate(self.parts, 1):
            name = f'{fetch.RELEASE}.zip.part{i:03d}'
            self.manifest['archive']['parts'].append({'name': name, 'size': len(part), 'sha256': sha(part), 'url': fetch.URL_PREFIX + name})
        self.save_manifest()

    def save_manifest(self):
        self.manifest_path.write_text(json.dumps(self.manifest))

    def restore(self):
        return fetch.restore(self.manifest_path, self.destination, self.cache)

    def downloads(self):
        return patch.object(fetch.urllib.request, 'urlopen', side_effect=[io.BytesIO(p) for p in self.parts])

    def test_exact_restore_reuses_verified_parts_and_preserves_existing_destination(self):
        with self.downloads() as network:
            self.assertEqual(self.restore(), 3)
            self.assertEqual(network.call_count, 2)
        for name, data in self.files.items():
            self.assertEqual((self.destination / name).read_bytes(), data)
        with patch.object(fetch.urllib.request, 'urlopen') as network:
            with self.assertRaises(fetch.Rejected):
                self.restore()
            network.assert_not_called()
        self.destination = self.root / 'second'
        with patch.object(fetch.urllib.request, 'urlopen') as network:
            self.assertEqual(self.restore(), 3)
            network.assert_not_called()

    def test_failed_part_is_removed_verified_part_survives_retry(self):
        with patch.object(fetch.urllib.request, 'urlopen', side_effect=[io.BytesIO(self.parts[0]), io.BytesIO(b'corrupt')]):
            with self.assertRaises(fetch.Rejected):
                self.restore()
        self.assertFalse(self.destination.exists())
        self.assertTrue((self.cache / sha(self.parts[0])).is_file())
        self.assertEqual(list(self.cache.glob('.part-*')), [])
        with patch.object(fetch.urllib.request, 'urlopen', return_value=io.BytesIO(self.parts[1])) as network:
            self.assertEqual(self.restore(), 3)
            self.assertEqual(network.call_count, 1)

    def test_combined_digest_and_file_digest_failure_leave_no_destination(self):
        self.manifest['archive']['sha256'] = '0' * 64
        self.save_manifest()
        with self.downloads(), self.assertRaises(fetch.Rejected):
            self.restore()
        self.assertFalse(self.destination.exists())
        self.set_archive(self.files)
        self.manifest['files'][0]['sha256'] = '0' * 64
        self.save_manifest()
        with self.downloads(), self.assertRaises(fetch.Rejected):
            self.restore()
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.destination.parent.glob('.runtime-*')), [])

    def test_extra_missing_escape_and_linked_members_reject(self):
        variants = [(dict(self.files, extra=b'extra'), None),
                    ({'content.json': self.files['content.json']}, None),
                    (dict(self.files, **{'../escape': b'escape'}), None),
                    (self.files, 'content.json')]
        for members, link in variants:
            with self.subTest(members=list(members), symlink=link):
                self.set_archive(members, link)
                with self.downloads(), self.assertRaises(fetch.Rejected):
                    self.restore()
                self.assertFalse(self.destination.exists())
                self.assertFalse((self.root / 'escape').exists())

    def test_manifest_url_path_duplicate_and_collision_reject_before_download(self):
        for mutation in ('url', 'path', 'duplicate', 'collision', 'size'):
            self.set_archive(self.files)
            if mutation == 'url':
                self.manifest['archive']['parts'][0]['url'] = 'https://example.invalid/data'
            elif mutation == 'path':
                self.manifest['files'][0]['path'] = '../content.json'
            elif mutation == 'duplicate':
                self.manifest['files'].append(self.manifest['files'][0])
            elif mutation == 'collision':
                self.manifest['files'].append(dict(self.manifest['files'][1], path='audio'))
            else:
                self.manifest['archive']['parts'][0]['size'] = 65 * fetch.CHUNK
            self.save_manifest()
            with self.subTest(mutation=mutation), patch.object(fetch.urllib.request, 'urlopen') as network:
                with self.assertRaises(fetch.Rejected):
                    self.restore()
                network.assert_not_called()
                self.assertFalse(self.destination.exists())

    def test_symlinked_cache_destination_and_corrupt_cache_reject(self):
        self.cache.symlink_to(self.root / 'missing')
        with self.assertRaises(fetch.Rejected):
            self.restore()
        self.cache.unlink()
        self.destination.parent.mkdir()
        self.destination.symlink_to(self.root / 'missing')
        with self.assertRaises(fetch.Rejected):
            self.restore()
        self.destination.unlink()
        self.cache.mkdir()
        (self.cache / sha(self.parts[0])).write_bytes(b'corrupt')
        with self.assertRaises(fetch.Rejected):
            self.restore()
        self.assertFalse(self.destination.exists())

    def test_publish_failure_cleans_new_directory_and_keeps_cache(self):
        with self.downloads(), patch.object(fetch.shutil, 'move', side_effect=OSError('interruption')):
            with self.assertRaises(OSError):
                self.restore()
        self.assertFalse(self.destination.exists())
        self.assertEqual(len(list(self.cache.iterdir())), 2)

    def test_cache_inside_destination_rejects_without_creating_it(self):
        self.cache = self.destination / 'cache'
        with self.assertRaises(fetch.Rejected):
            self.restore()
        self.assertFalse(self.destination.exists())


if __name__ == '__main__':
    unittest.main()
