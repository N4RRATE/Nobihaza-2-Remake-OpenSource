#!/usr/bin/env python3
"""Export only reviewed bytes to a new local directory; never publish anything."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


COMPLETE_CONTENT_INCLUDES = frozenset({
    'src/game/dekisugi_program.inc', 'src/game/dora_program.inc',
    'src/game/cash_program.inc', 'src/game/warehouse_g4_program.inc',
    'src/game/final_boss_program.inc', 'tools/g3-route-doors.inc',
    'tools/g3-access-cold-room.inc',
})


def relative(name, complete=False):
    require(isinstance(name, str) and re.fullmatch(r'[A-Za-z0-9_./-]+', name), 'Invalid relative path')
    p = PurePosixPath(name)
    require(bool(p.parts) and not p.is_absolute() and str(p) == name and all(x not in ('.', '..') for x in p.parts), 'Unsafe relative path')
    forbidden = {'.git', '.scratch', 'private', 'build', '__pycache__', '.env', 'research', 'reference', 'history', 'saves', '.codex', '.vscode', 'credentials'}
    require(not forbidden.intersection(p.parts) and not p.name.startswith('core'), 'Private or generated path')
    require((complete and name in COMPLETE_CONTENT_INCLUDES) or
            (not p.name.endswith('_program.inc') and (p.suffix != '.inc' or name == 'src/game/warehouse_visual.inc')), 'Original source projection')
    return p


def no_links(path):
    p = path.absolute()
    require(not any(x.is_symlink() for x in (p, *p.parents)), 'Symlink path')


def regular(root, name, complete=False):
    p = root
    for part in relative(name, complete).parts:
        p = p / part
        require(not p.is_symlink(), 'Symlink input: ' + name)
    require(p.is_file() and stat.S_ISREG(p.stat().st_mode), 'Not a regular input: ' + name)
    return p


def inspect_bytes(data, name):
    text = data.decode('utf-8')
    patterns = (r'/' + r'Users/[^/\s]+', r'[A-Za-z]:\\' + r'Users\\',
                r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
                r'\bgh[pousr]_[A-Za-z0-9]{20,}', r'\bgithub_pat_[A-Za-z0-9_]{20,}',
                r'N4RRATE/' + r'Nobihaza-2-Remake-chuban-geren-kaifa')
    require(not any(re.search(p, text) for p in patterns), 'Personal path, secret or private repository reference: ' + name)


def load_manifest(path):
    no_links(path)
    require(path.is_file(), 'Manifest must be a regular file')
    raw = path.read_bytes()
    m = json.loads(raw)
    complete = m.get('version') == 2 and m.get('status') == 'complete-source-candidate'
    require(complete or (m.get('version') == 1 and m.get('status') in ('partial-source-review-candidate', 'source-with-local-inputs-candidate')), 'Unsupported manifest')
    if complete:
        require(m.get('content_contract') == 'nobihaza.public-complete-source.v1', 'Missing complete source contract')
    require(isinstance(m.get('requirements', m.get('blockers')), list) and m.get('requirements', m.get('blockers')), 'Missing honest local-input/build requirements')
    require(isinstance(m.get('files'), list) and m['files'], 'Empty whitelist')
    sources, outputs = set(), set()
    for entry in m['files']:
        require(set(entry) == {'source', 'path', 'sha256', 'basis'}, 'Unexpected entry fields')
        source, output = str(relative(entry['source'], complete)), str(relative(entry['path'], complete))
        if complete and (source in COMPLETE_CONTENT_INCLUDES or output in COMPLETE_CONTENT_INCLUDES):
            require(source == output, 'Content include cannot be renamed')
        require(source not in sources and output not in outputs and output != 'PUBLIC-SOURCE-MANIFEST.json', 'Duplicate whitelist entry')
        require(re.fullmatch('[a-f0-9]{64}', entry['sha256']) and bool(entry['basis']), 'Missing review/hash')
        sources.add(source)
        outputs.add(output)
    require(not any(str(parent) in outputs for name in outputs for parent in PurePosixPath(name).parents), 'File/directory collision')
    return m, raw


def audit(destination, manifest):
    expected = {e['path']: e['sha256'] for e in manifest['files']}
    expected['PUBLIC-SOURCE-MANIFEST.json'] = digest(json.dumps(manifest, ensure_ascii=False, indent=2).encode() + b'\n')
    found = set()
    for p in destination.rglob('*'):
        require(not p.is_symlink(), 'Symlink in candidate')
        if p.is_dir():
            require(any(name.startswith(p.relative_to(destination).as_posix() + '/') for name in expected), 'Extra directory in candidate')
            continue
        name = p.relative_to(destination).as_posix()
        require(name in expected and stat.S_ISREG(p.stat().st_mode), 'Extra/nonregular candidate file: ' + name)
        require(digest(p.read_bytes()) == expected[name], 'Changed candidate: ' + name)
        found.add(name)
    require(found == set(expected), 'Missing candidate files')
    return len(found)


def export(root, manifest_path, destination):
    no_links(root)
    no_links(destination)
    require(root.is_dir(), 'Invalid source root')
    root = root.resolve()
    manifest, _ = load_manifest(manifest_path)
    complete = manifest['version'] == 2
    require(not destination.exists() and not destination.is_symlink(), 'Destination already exists; never overwrite a candidate')
    require(destination.parent.is_dir() and not destination.parent.is_symlink(), 'Invalid destination parent')
    destination = destination.parent.resolve() / destination.name
    # Read and validate everything before creating the candidate, preserving the checked bytes.
    payload = []
    for entry in manifest['files']:
        source = regular(root, entry['source'], complete)
        data = source.read_bytes()
        require(digest(data) == entry['sha256'], 'Unreviewed changed input: ' + entry['source'])
        inspect_bytes(data, entry['source'])
        payload.append((entry['path'], data, entry['source']))
    temporary = Path(tempfile.mkdtemp(prefix='.public-source-', dir=destination.parent))
    try:
        for name, data, source in payload:
            require(digest(regular(root, source, complete).read_bytes()) == digest(data), 'Input changed during export: ' + source)
            target = temporary / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (temporary / 'PUBLIC-SOURCE-MANIFEST.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
        count = audit(temporary, manifest)
        # mkdir reserves the final name without replacing an existing directory or symlink.
        destination.mkdir()
        try:
            for p in temporary.iterdir():
                shutil.move(str(p), str(destination / p.name))
        except BaseException:
            shutil.rmtree(destination)
            raise
        return count
    finally:
        shutil.rmtree(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--manifest', type=Path, default=Path(__file__).with_name('public-source-manifest.json'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--audit', action='store_true')
    args = parser.parse_args()
    try:
        if args.audit:
            manifest, _ = load_manifest(args.manifest)
            no_links(args.output)
            require(args.output.is_dir(), 'Missing candidate')
            count = audit(args.output, manifest)
        else:
            count = export(args.root, args.manifest, args.output)
        print(f'{count} exact candidate files checked; local source candidate, not a game publication')
    except (Rejected, OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f'Rejected: {error}\n')


if __name__ == '__main__':
    main()
