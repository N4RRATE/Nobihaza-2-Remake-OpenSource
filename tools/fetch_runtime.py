#!/usr/bin/env python3
"""Restore the reviewed public runtime into a new directory, without overwriting saves."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile
import urllib.request
import zipfile


RELEASE = 'runtime-v1.0.0-rc.1'
URL_PREFIX = 'https://github.com/N4RRATE/Nobihaza-2-Remake-OpenSource/releases/download/' + RELEASE + '/'
CHUNK = 1024 * 1024


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def no_links(path):
    require(not any(p.is_symlink() for p in (path.absolute(), *path.absolute().parents)), 'Symlink path')


def relative(name):
    require(isinstance(name, str) and name and '\\' not in name and ':' not in name and '\0' not in name, 'Invalid runtime path')
    path = PurePosixPath(name)
    require(not path.is_absolute() and path.as_posix() == name and all(p not in ('.', '..') for p in path.parts), 'Unsafe runtime path')
    require(not {'.git', '.env', 'saves', 'credentials', '.codex'}.intersection(path.parts), 'Private runtime path')
    return path


def checked_record(record):
    require(type(record['size']) is int and record['size'] >= 0, 'Invalid recorded size')
    require(isinstance(record['sha256'], str) and re.fullmatch('[a-f0-9]{64}', record['sha256']), 'Invalid recorded digest')


def load_manifest(path):
    no_links(path)
    require(path.is_file() and path.stat().st_size <= 16 * CHUNK, 'Invalid runtime manifest')
    manifest = json.loads(path.read_bytes())
    require(set(manifest) == {'schema', 'entry', 'archive', 'files'} and manifest['schema'] == 'nobihaza.public-runtime.v1'
            and manifest['entry'] == 'content.json', 'Unsupported runtime manifest')
    archive = manifest['archive']
    require(set(archive) == {'size', 'sha256', 'parts'}, 'Invalid archive record')
    checked_record(archive)
    require(isinstance(archive['parts'], list) and archive['parts'], 'Missing runtime parts')
    for index, part in enumerate(archive['parts'], 1):
        require(set(part) == {'name', 'size', 'sha256', 'url'}, 'Invalid part record')
        checked_record(part)
        require(part['name'] == f'{RELEASE}.zip.part{index:03d}' and 0 < part['size'] <= 64 * CHUNK, 'Invalid runtime part')
        require(part['url'] == URL_PREFIX + part['name'], 'Unapproved runtime URL')
    require(sum(p['size'] for p in archive['parts']) == archive['size'], 'Parts do not match archive size')
    require(isinstance(manifest['files'], list) and manifest['files'], 'Empty runtime files')
    names = set()
    for item in manifest['files']:
        require(set(item) == {'path', 'size', 'sha256'}, 'Invalid runtime file record')
        relative(item['path'])
        checked_record(item)
        require(item['path'] not in names, 'Duplicate runtime file')
        names.add(item['path'])
    require(manifest['entry'] in names, 'Missing runtime entry')
    require(not any(str(p) in names for n in names for p in PurePosixPath(n).parents), 'Runtime file/directory collision')
    return manifest


def matches(path, record):
    no_links(path)
    if not path.exists():
        return False
    require(path.is_file() and stat.S_ISREG(path.stat().st_mode), 'Nonregular cached part')
    if path.stat().st_size != record['size']:
        return False
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(CHUNK), b''):
            sha.update(data)
    return sha.hexdigest() == record['sha256']


def cached_part(part, cache):
    path = cache / part['sha256']
    if matches(path, part):
        return path
    require(not path.exists(), 'Corrupt cached part: remove ' + str(path) + ' and retry')
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix='.part-', dir=cache, delete=False) as target:
            temporary = Path(target.name)
            sha, size = hashlib.sha256(), 0
            with urllib.request.urlopen(part['url'], timeout=60) as response:
                for data in iter(lambda: response.read(CHUNK), b''):
                    size += len(data)
                    require(size <= part['size'], 'Runtime part exceeds recorded size')
                    sha.update(data)
                    target.write(data)
            require(size == part['size'] and sha.hexdigest() == part['sha256'], 'Runtime part digest/size mismatch')
        no_links(path)
        require(not path.exists(), 'Cache changed during download')
        temporary.rename(path)
        return path
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def extract(archive_path, destination, files):
    expected = {item['path']: item for item in files}
    with zipfile.ZipFile(archive_path) as archive:
        seen, found = set(), set()
        for info in archive.infolist():
            name = info.filename[:-1] if info.is_dir() else info.filename
            relative(name)
            require(name not in seen, 'Duplicate ZIP member')
            seen.add(name)
            mode = stat.S_IFMT(info.external_attr >> 16)
            require(mode in ((0, stat.S_IFDIR) if info.is_dir() else (0, stat.S_IFREG)) and not info.flag_bits & 1, 'Linked/nonregular/encrypted ZIP member')
            if info.is_dir():
                require(any(n.startswith(name + '/') for n in expected), 'Extra ZIP directory')
                continue
            require(name in expected and info.file_size == expected[name]['size'], 'Extra or changed ZIP file')
            found.add(name)
        require(found == set(expected), 'Missing ZIP files')
        for info in archive.infolist():
            if info.is_dir():
                continue
            target = destination / info.filename
            target.parent.mkdir(parents=True, exist_ok=True)
            sha, size = hashlib.sha256(), 0
            with archive.open(info) as source, target.open('xb') as output:
                for data in iter(lambda: source.read(CHUNK), b''):
                    size += len(data)
                    require(size <= expected[info.filename]['size'], 'Expanded runtime file exceeds recorded size')
                    sha.update(data)
                    output.write(data)
            require(size == expected[info.filename]['size'] and sha.hexdigest() == expected[info.filename]['sha256'], 'Runtime file digest mismatch')


def restore(manifest_path, destination, cache):
    manifest = load_manifest(manifest_path)
    no_links(destination)
    no_links(cache)
    require(not destination.exists(), 'Destination already exists; choose a new directory')
    require(destination.resolve() != cache.resolve() and destination.resolve() not in cache.resolve().parents,
            'Download cache must be outside the new runtime destination')
    destination.parent.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    require(cache.is_dir(), 'Invalid download cache')
    parts = [cached_part(part, cache) for part in manifest['archive']['parts']]
    with tempfile.TemporaryDirectory(prefix='.runtime-', dir=destination.parent) as temporary:
        staging = Path(temporary)
        joined = staging / 'runtime.zip'
        sha, size = hashlib.sha256(), 0
        with joined.open('xb') as output:
            for part, record in zip(parts, manifest['archive']['parts']):
                require(matches(part, record), 'Cached part changed before assembly')
                with part.open('rb') as source:
                    for data in iter(lambda: source.read(CHUNK), b''):
                        size += len(data)
                        sha.update(data)
                        output.write(data)
        require(size == manifest['archive']['size'] and sha.hexdigest() == manifest['archive']['sha256'], 'Combined runtime digest mismatch')
        runtime = staging / 'runtime'
        runtime.mkdir()
        extract(joined, runtime, manifest['files'])
        no_links(destination)
        destination.mkdir()  # Reserve the new name without replacing an existing path.
        try:
            for path in runtime.iterdir():
                shutil.move(str(path), str(destination / path.name))
        except BaseException:
            shutil.rmtree(destination)
            raise
    return len(manifest['files'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--manifest', type=Path, default=root / 'runtime-manifest.json')
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--cache', type=Path, default=root / '.runtime-downloads')
    args = parser.parse_args()
    try:
        count = restore(args.manifest, args.destination, args.cache)
    except (Rejected, OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Rejected: {error}\n')
    print(f'{count} runtime files verified in {args.destination}; start with {args.destination / "content.json"}')


if __name__ == '__main__':
    main()
